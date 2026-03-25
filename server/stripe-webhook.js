/**
 * Stripe Webhook + License Activation Server — Cloudflare Worker
 *
 * Handles:
 *   - POST /webhook/stripe     — Stripe checkout.session.completed → generate license → email
 *   - POST /create-checkout    — Create Stripe Checkout session
 *   - POST /activate           — Activate a license on a device (max 2 per license)
 *   - POST /deactivate         — Release a device slot
 *   - GET  /health             — Health check
 *
 * Environment variables (set via wrangler secret put):
 *   STRIPE_WEBHOOK_SECRET  — Stripe webhook signing secret (whsec_...)
 *   STRIPE_SECRET_KEY      — Stripe secret key (sk_live_... or sk_test_...)
 *   STRIPE_PRICE_ID        — Stripe price ID for ProEQ8
 *   RESEND_API_KEY         — Resend.com API key for sending email
 *   LICENSE_SIGNING_SECRET — Shared HMAC-SHA256 secret (must match LicenseValidator.h)
 *
 * KV Namespaces (bound in wrangler.toml):
 *   LICENSES — License activation state: { email, max_uses, used, devices[], created }
 *              Also stores idempotency keys: session:<stripe_session_id> → license_id
 *
 * Environment vars (wrangler.toml [vars]):
 *   MAX_DEVICES_PER_LICENSE — Default: "2"
 *   SUCCESS_URL, CANCEL_URL — Stripe Checkout redirect URLs
 */

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    // ── CORS preflight ───────────────────────────────────────────
    if (request.method === "OPTIONS") {
      return corsResponse(new Response(null, { status: 204 }));
    }

    // ── GET /health ──────────────────────────────────────────────
    if (url.pathname === "/health" && request.method === "GET") {
      return corsResponse(jsonResponse({ status: "ok", version: "2.0.0" }));
    }

    // ── POST /create-checkout ────────────────────────────────────
    if (url.pathname === "/create-checkout" && request.method === "POST") {
      return corsResponse(await handleCreateCheckout(env));
    }

    // ── POST /activate ───────────────────────────────────────────
    if (url.pathname === "/activate" && request.method === "POST") {
      return corsResponse(await handleActivate(request, env));
    }

    // ── POST /deactivate ─────────────────────────────────────────
    if (url.pathname === "/deactivate" && request.method === "POST") {
      return corsResponse(await handleDeactivate(request, env));
    }

    // ── POST /webhook/stripe ─────────────────────────────────────
    if (url.pathname === "/webhook/stripe" && request.method === "POST") {
      return await handleStripeWebhook(request, env);
    }

    if (request.method !== "POST" && request.method !== "GET") {
      return new Response("Method Not Allowed", { status: 405 });
    }

    return new Response("Not Found", { status: 404 });
  },
};

// ═══════════════════════════════════════════════════════════════════
//  STRIPE WEBHOOK HANDLER
// ═══════════════════════════════════════════════════════════════════

async function handleStripeWebhook(request, env) {
  const body = await request.text();
  const sig = request.headers.get("Stripe-Signature");

  if (!sig) {
    return new Response("Missing Stripe-Signature header", { status: 400 });
  }

  const event = await verifyStripeWebhook(body, sig, env.STRIPE_WEBHOOK_SECRET);
  if (!event) {
    return new Response("Invalid signature", { status: 400 });
  }

  if (event.type === "checkout.session.completed") {
    const session = event.data.object;
    const email = session.customer_details?.email || session.customer_email;

    if (!email) {
      console.error("No customer email found in session:", session.id);
      return new Response("No email", { status: 400 });
    }

    // ── Idempotency: check if we already processed this session ──
    const idempotencyKey = `session:${session.id}`;
    const existingLicenseId = await env.LICENSES.get(idempotencyKey);
    if (existingLicenseId) {
      console.log(`Session ${session.id} already processed (license: ${existingLicenseId})`);
      return jsonResponse({ ok: true, duplicate: true });
    }

    // ── Generate license ──
    const licenseId = crypto.randomUUID();
    const maxDevices = parseInt(env.MAX_DEVICES_PER_LICENSE || "2", 10);

    const license = await generateLicense(email, licenseId, env.LICENSE_SIGNING_SECRET);

    // ── Store license record in KV ──
    const licenseRecord = {
      email,
      max_uses: maxDevices,
      used: 0,
      devices: [],
      created: new Date().toISOString(),
      stripe_session_id: session.id,
    };
    await env.LICENSES.put(licenseId, JSON.stringify(licenseRecord));

    // ── Store idempotency key (TTL: 30 days) ──
    await env.LICENSES.put(idempotencyKey, licenseId, {
      expirationTtl: 60 * 60 * 24 * 30,
    });

    // ── Send license email ──
    await sendLicenseEmail(email, license, maxDevices, env.RESEND_API_KEY);

    console.log(`License ${licenseId} sent to ${email} for session ${session.id}`);
    return jsonResponse({ ok: true });
  }

  // Acknowledge other event types
  return jsonResponse({ received: true });
}

// ═══════════════════════════════════════════════════════════════════
//  ACTIVATION ENDPOINT
// ═══════════════════════════════════════════════════════════════════

async function handleActivate(request, env) {
  let body;
  try {
    body = await request.json();
  } catch {
    return jsonResponse({ error: "Invalid JSON body" }, 400);
  }

  const { license_key, device_id } = body;
  if (!license_key || !device_id) {
    return jsonResponse({ error: "Missing license_key or device_id" }, 400);
  }

  // 1. Validate HMAC signature on the key
  const payload = await validateLicenseSignature(license_key, env.LICENSE_SIGNING_SECRET);
  if (!payload) {
    return jsonResponse({ error: "Invalid license key" }, 401);
  }

  // 2. Check expiration
  if (payload.expires) {
    const expiry = new Date(payload.expires + "T23:59:59Z");
    if (expiry < new Date()) {
      return jsonResponse({ error: "License expired" }, 401);
    }
  }

  // 3. Look up license record by license_id
  const licenseId = payload.license_id;
  if (!licenseId) {
    return jsonResponse({ error: "License key missing license_id (legacy key?)" }, 400);
  }

  const recordStr = await env.LICENSES.get(licenseId);
  if (!recordStr) {
    return jsonResponse({ error: "License not found" }, 404);
  }

  const record = JSON.parse(recordStr);

  // 4. Check if device is already activated
  if (record.devices.includes(device_id)) {
    return jsonResponse({
      ok: true,
      message: "Device already activated",
      used: record.used,
      max: record.max_uses,
    });
  }

  // 5. Check activation limit
  if (record.devices.length >= record.max_uses) {
    return jsonResponse(
      {
        error: "Activation limit reached",
        message: `This license allows ${record.max_uses} devices. Deactivate another device first.`,
        used: record.used,
        max: record.max_uses,
      },
      403
    );
  }

  // 6. Activate: add device, increment count, save
  record.devices.push(device_id);
  record.used = record.devices.length;
  await env.LICENSES.put(licenseId, JSON.stringify(record));

  console.log(`Activated device ${device_id} for license ${licenseId} (${record.used}/${record.max_uses})`);

  return jsonResponse({
    ok: true,
    message: "Activation successful",
    used: record.used,
    max: record.max_uses,
  });
}

// ═══════════════════════════════════════════════════════════════════
//  DEACTIVATION ENDPOINT
// ═══════════════════════════════════════════════════════════════════

async function handleDeactivate(request, env) {
  let body;
  try {
    body = await request.json();
  } catch {
    return jsonResponse({ error: "Invalid JSON body" }, 400);
  }

  const { license_key, device_id } = body;
  if (!license_key || !device_id) {
    return jsonResponse({ error: "Missing license_key or device_id" }, 400);
  }

  // Validate signature
  const payload = await validateLicenseSignature(license_key, env.LICENSE_SIGNING_SECRET);
  if (!payload) {
    return jsonResponse({ error: "Invalid license key" }, 401);
  }

  const licenseId = payload.license_id;
  if (!licenseId) {
    return jsonResponse({ error: "License key missing license_id" }, 400);
  }

  const recordStr = await env.LICENSES.get(licenseId);
  if (!recordStr) {
    return jsonResponse({ error: "License not found" }, 404);
  }

  const record = JSON.parse(recordStr);

  // Remove device if present
  const idx = record.devices.indexOf(device_id);
  if (idx === -1) {
    return jsonResponse({
      ok: true,
      message: "Device was not activated",
      used: record.used,
      max: record.max_uses,
    });
  }

  record.devices.splice(idx, 1);
  record.used = record.devices.length;
  await env.LICENSES.put(licenseId, JSON.stringify(record));

  console.log(`Deactivated device ${device_id} for license ${licenseId} (${record.used}/${record.max_uses})`);

  return jsonResponse({
    ok: true,
    message: "Device deactivated",
    used: record.used,
    max: record.max_uses,
  });
}

// ═══════════════════════════════════════════════════════════════════
//  CREATE CHECKOUT SESSION
// ═══════════════════════════════════════════════════════════════════

async function handleCreateCheckout(env) {
  try {
    const resp = await fetch("https://api.stripe.com/v1/checkout/sessions", {
      method: "POST",
      headers: {
        Authorization: `Bearer ${env.STRIPE_SECRET_KEY}`,
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: new URLSearchParams({
        mode: "payment",
        "line_items[0][price]": env.STRIPE_PRICE_ID,
        "line_items[0][quantity]": "1",
        success_url: env.SUCCESS_URL || "https://github.com/GareBear99/FreeEQ8?purchase=success",
        cancel_url: env.CANCEL_URL || "https://github.com/GareBear99/FreeEQ8?purchase=cancelled",
        "payment_method_types[0]": "card",
      }),
    });

    const session = await resp.json();
    if (!resp.ok) {
      console.error("Stripe checkout creation failed:", session);
      return jsonResponse({ error: session.error?.message || "Unknown error" }, 500);
    }

    return jsonResponse({ url: session.url, id: session.id });
  } catch (e) {
    console.error("Checkout creation error:", e);
    return jsonResponse({ error: "Internal error" }, 500);
  }
}

// ═══════════════════════════════════════════════════════════════════
//  STRIPE WEBHOOK SIGNATURE VERIFICATION
// ═══════════════════════════════════════════════════════════════════

async function verifyStripeWebhook(payload, sigHeader, secret) {
  try {
    const parts = sigHeader.split(",").reduce((acc, part) => {
      const [key, value] = part.split("=");
      acc[key.trim()] = value;
      return acc;
    }, {});

    const timestamp = parts["t"];
    const signature = parts["v1"];

    if (!timestamp || !signature) return null;

    // Reject if timestamp is older than 5 minutes
    const now = Math.floor(Date.now() / 1000);
    if (Math.abs(now - parseInt(timestamp)) > 300) return null;

    const signedPayload = `${timestamp}.${payload}`;
    const key = await crypto.subtle.importKey(
      "raw",
      new TextEncoder().encode(secret),
      { name: "HMAC", hash: "SHA-256" },
      false,
      ["sign"]
    );

    const sigBytes = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(signedPayload));
    const expected = Array.from(new Uint8Array(sigBytes))
      .map((b) => b.toString(16).padStart(2, "0"))
      .join("");

    // Timing-safe comparison
    if (expected.length !== signature.length) return null;
    let mismatch = 0;
    for (let i = 0; i < expected.length; i++) {
      mismatch |= expected.charCodeAt(i) ^ signature.charCodeAt(i);
    }
    if (mismatch !== 0) return null;

    return JSON.parse(payload);
  } catch (e) {
    console.error("Webhook verification failed:", e);
    return null;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  LICENSE KEY GENERATION
// ═══════════════════════════════════════════════════════════════════

// Format: <base64_payload>.<base64_signature>
// Payload: { product, email, license_id, expires, issued }
async function generateLicense(email, licenseId, signingSecret) {
  const payload = JSON.stringify({
    product: "ProEQ8",
    email: email,
    license_id: licenseId,
    expires: getExpiryDate(),
    issued: new Date().toISOString(),
  });

  const payloadB64 = btoa(payload);

  const key = await crypto.subtle.importKey(
    "raw",
    new TextEncoder().encode(signingSecret),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"]
  );

  const sigBytes = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
  const sigB64 = btoa(String.fromCharCode(...new Uint8Array(sigBytes)));

  return `${payloadB64}.${sigB64}`;
}

function getExpiryDate() {
  const d = new Date();
  d.setFullYear(d.getFullYear() + 1);
  return d.toISOString().split("T")[0]; // "YYYY-MM-DD"
}

// ═══════════════════════════════════════════════════════════════════
//  LICENSE KEY VALIDATION (server-side, mirrors client HMAC check)
// ═══════════════════════════════════════════════════════════════════

async function validateLicenseSignature(licenseKey, signingSecret) {
  try {
    const dotIdx = licenseKey.indexOf(".");
    if (dotIdx < 0) return null;

    const payloadB64 = licenseKey.substring(0, dotIdx);
    const signatureB64 = licenseKey.substring(dotIdx + 1);

    // Recompute HMAC
    const key = await crypto.subtle.importKey(
      "raw",
      new TextEncoder().encode(signingSecret),
      { name: "HMAC", hash: "SHA-256" },
      false,
      ["sign"]
    );

    const sigBytes = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
    const expectedB64 = btoa(String.fromCharCode(...new Uint8Array(sigBytes)));

    // Timing-safe comparison
    const expected = expectedB64.trim();
    const received = signatureB64.trim();
    if (expected.length !== received.length) return null;
    let mismatch = 0;
    for (let i = 0; i < expected.length; i++) {
      mismatch |= expected.charCodeAt(i) ^ received.charCodeAt(i);
    }
    if (mismatch !== 0) return null;

    // Decode payload
    const payloadStr = atob(payloadB64);
    const payload = JSON.parse(payloadStr);

    if (payload.product !== "ProEQ8") return null;

    return payload;
  } catch (e) {
    console.error("License validation failed:", e);
    return null;
  }
}

// ═══════════════════════════════════════════════════════════════════
//  EMAIL DELIVERY VIA RESEND
// ═══════════════════════════════════════════════════════════════════

async function sendLicenseEmail(email, licenseKey, maxDevices, resendApiKey) {
  const html = `
    <h2>Your ProEQ8 License Key</h2>
    <p>Thank you for purchasing ProEQ8!</p>
    <p>Your license key:</p>
    <pre style="background:#f4f4f4;padding:12px;border-radius:4px;font-size:14px;word-break:break-all;">${licenseKey}</pre>
    <p><strong>To activate:</strong></p>
    <ol>
      <li>Open ProEQ8 in your DAW</li>
      <li>Click the <strong>Activate</strong> button in the sidebar</li>
      <li>Paste the license key above</li>
      <li>Click <strong>Activate</strong></li>
    </ol>
    <p><strong>Important:</strong> This license can be activated on up to <strong>${maxDevices} devices</strong>.
    You can deactivate a device from within the plugin to free up a slot.</p>
    <p>If you have any issues, reply to this email.</p>
    <p>&mdash; TizWildin Entertainment</p>
  `;

  const resp = await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: {
      Authorization: `Bearer ${resendApiKey}`,
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      from: "ProEQ8 <noreply@tizwildin.com>",
      to: [email],
      subject: "Your ProEQ8 License Key",
      html: html,
    }),
  });

  if (!resp.ok) {
    const err = await resp.text();
    console.error("Failed to send email:", err);
    throw new Error("Email delivery failed");
  }
}

// ═══════════════════════════════════════════════════════════════════
//  HELPERS
// ═══════════════════════════════════════════════════════════════════

function jsonResponse(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "Content-Type": "application/json" },
  });
}

function corsResponse(response) {
  const headers = new Headers(response.headers);
  // TODO: Replace "*" with your actual domain(s) before going live
  headers.set("Access-Control-Allow-Origin", "*");
  headers.set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  headers.set("Access-Control-Allow-Headers", "Content-Type");
  return new Response(response.body, {
    status: response.status,
    statusText: response.statusText,
    headers,
  });
}
