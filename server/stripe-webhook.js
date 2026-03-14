/**
 * Stripe Webhook — Cloudflare Worker
 *
 * Handles `checkout.session.completed` events from Stripe.
 * Generates a signed license key and emails it to the customer via Resend.
 *
 * Environment variables (set in Cloudflare dashboard or wrangler.toml secrets):
 *   STRIPE_WEBHOOK_SECRET  — Stripe webhook signing secret (whsec_...)
 *   STRIPE_SECRET_KEY      — Stripe secret key (sk_live_... or sk_test_...)
 *   RESEND_API_KEY         — Resend.com API key for sending email
 *   LICENSE_SIGNING_SECRET — Shared HMAC-SHA256 secret for signing license keys
 *                            (must match LicenseValidator.h in the plugin)
 */

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    // Health check endpoint (GET)
    if (url.pathname === "/health") {
      return new Response(JSON.stringify({ status: "ok", version: "1.1.0" }), {
        headers: { "Content-Type": "application/json" },
      });
    }

    // ── Create Stripe Checkout session (POST /create-checkout) ─────
    if (url.pathname === "/create-checkout" && request.method === "POST") {
      return handleCreateCheckout(env);
    }

    // All other routes require POST
    if (request.method !== "POST") {
      return new Response("Method Not Allowed", { status: 405 });
    }

    if (url.pathname !== "/webhook/stripe") {
      return new Response("Not Found", { status: 404 });
    }

    // ── Verify Stripe signature ───────────────────────────────────
    const body = await request.text();
    const sig = request.headers.get("Stripe-Signature");

    if (!sig) {
      return new Response("Missing Stripe-Signature header", { status: 400 });
    }

    const event = await verifyStripeWebhook(body, sig, env.STRIPE_WEBHOOK_SECRET);
    if (!event) {
      return new Response("Invalid signature", { status: 400 });
    }

    // ── Handle checkout.session.completed ──────────────────────────
    if (event.type === "checkout.session.completed") {
      const session = event.data.object;
      const email = session.customer_details?.email || session.customer_email;

      if (!email) {
        console.error("No customer email found in session:", session.id);
        return new Response("No email", { status: 400 });
      }

      // Generate license key
      const license = await generateLicense(email, env.LICENSE_SIGNING_SECRET);

      // Send license email via Resend
      await sendLicenseEmail(email, license, env.RESEND_API_KEY);

      console.log(`License sent to ${email} for session ${session.id}`);
      return new Response(JSON.stringify({ ok: true }), { status: 200 });
    }

    // Acknowledge other event types
    return new Response(JSON.stringify({ received: true }), { status: 200 });
  },
};

// ── Stripe webhook signature verification ───────────────────────────
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

    // Compute expected signature: HMAC-SHA256(timestamp + "." + payload)
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

    if (expected !== signature) return null;

    return JSON.parse(payload);
  } catch (e) {
    console.error("Webhook verification failed:", e);
    return null;
  }
}

// ── Create Stripe Checkout session ─────────────────────────────────
// Creates a Stripe Checkout session for ProEQ8 purchase.
// The frontend redirects the user to the returned URL.
async function handleCreateCheckout(env) {
  try {
    const resp = await fetch("https://api.stripe.com/v1/checkout/sessions", {
      method: "POST",
      headers: {
        Authorization: `Bearer ${env.STRIPE_SECRET_KEY}`,
        "Content-Type": "application/x-www-form-urlencoded",
      },
      body: new URLSearchParams({
        "mode": "payment",
        "line_items[0][price]": env.STRIPE_PRICE_ID,
        "line_items[0][quantity]": "1",
        "success_url": env.SUCCESS_URL || "https://github.com/GareBear99/FreeEQ8?purchase=success",
        "cancel_url": env.CANCEL_URL || "https://github.com/GareBear99/FreeEQ8?purchase=cancelled",
        "payment_method_types[0]": "card",
      }),
    });

    const session = await resp.json();
    if (!resp.ok) {
      console.error("Stripe checkout creation failed:", session);
      return new Response(JSON.stringify({ error: session.error?.message || "Unknown error" }), {
        status: 500,
        headers: { "Content-Type": "application/json" },
      });
    }

    return new Response(JSON.stringify({ url: session.url, id: session.id }), {
      status: 200,
      headers: {
        "Content-Type": "application/json",
        "Access-Control-Allow-Origin": "*",
      },
    });
  } catch (e) {
    console.error("Checkout creation error:", e);
    return new Response(JSON.stringify({ error: "Internal error" }), { status: 500 });
  }
}

// ── License key generation (HMAC-SHA256 signed) ──────────────────────
// Format: <base64_payload>.<base64_signature>
// Payload: { "product": "ProEQ8", "email": "...", "expires": "..." }
// Signature: HMAC-SHA256(LICENSE_SIGNING_SECRET, payloadB64) → Base64
// Verified client-side by LicenseValidator.h using the same shared secret.
async function generateLicense(email, signingSecret) {
  const payload = JSON.stringify({
    product: "ProEQ8",
    email: email,
    expires: getExpiryDate(), // 1 year from now
    issued: new Date().toISOString(),
  });

  const payloadB64 = btoa(payload);

  // HMAC-SHA256 signature
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

// ── Email delivery via Resend ───────────────────────────────────────
async function sendLicenseEmail(email, licenseKey, resendApiKey) {
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
