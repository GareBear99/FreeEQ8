/**
 * TizWildin License Server — Cloudflare Worker (Production v3.1)
 * 
 * Endpoints:
 *   POST /webhook/stripe        — Stripe checkout completed → license → email
 *   POST /create-checkout       — Stripe Checkout (ProEQ8 $29.99 CAD)
 *   POST /create-checkout/master-key — Master Key ($3 CAD/seat/mo, max 3 seats = $9)
 *   POST /activate              — Activate license (max 2 devices)
 *   POST /deactivate            — Release device slot
 *   POST /verify                — Verify license
 *   POST /recover               — Email recovery
 *   GET  /health                — Health check
 *   GET  /hub/community-stats   — User count
 *   GET  /hub/account           — Account status
 *   POST /hub/signin            — OAuth sign-in tracking
 *   POST /hub/master-key/*      — Master Key management
 *   POST /auth/soundcloud/url   — Get SoundCloud OAuth URL
 *   POST /auth/soundcloud/callback — Exchange OAuth code for token + profile
 *   GET  /auth/soundcloud/follow-status — Check if user follows @TizWildin
 *   POST /auth/google/signin    — Google sign-in tracking
 * 
 * Security: HMAC-SHA256 signing, rate limiting, input validation, security headers
 */

const VERSION = "3.1.0";

// SoundCloud OAuth endpoints
const SC_AUTH_URL = "https://api.soundcloud.com/connect";
const SC_TOKEN_URL = "https://api.soundcloud.com/oauth2/token";
const SC_ME_URL = "https://api.soundcloud.com/me";
const SC_FOLLOWINGS_URL = "https://api.soundcloud.com/me/followings";
const MAX_BODY_SIZE = 16384;
const RATE_LIMIT_WINDOW = 60;
const RATE_LIMIT_MAX = 30;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const method = request.method;

    const securityHeaders = {
      "X-Content-Type-Options": "nosniff",
      "X-Frame-Options": "DENY",
      "Referrer-Policy": "strict-origin-when-cross-origin",
      "X-Request-Id": crypto.randomUUID(),
    };

    if (method === "OPTIONS") {
      return corsResponse(new Response(null, { status: 204, headers: securityHeaders }), request);
    }

    if (!url.pathname.startsWith("/webhook")) {
      const clientIP = request.headers.get("CF-Connecting-IP") || "unknown";
      const rateLimit = await checkRateLimit(clientIP, env);
      if (!rateLimit.allowed) {
        return corsResponse(jsonResponse({ error: "Rate limit exceeded", retry_after: rateLimit.retryAfter }, 429, securityHeaders), request);
      }
    }

    try {
      const response = await routeRequest(url.pathname, method, request, env, securityHeaders);
      return corsResponse(response, request);
    } catch (err) {
      console.error(`[ERROR] ${url.pathname}:`, err.message);
      return corsResponse(jsonResponse({ error: "Internal server error" }, 500, securityHeaders), request);
    }
  },
};

async function routeRequest(path, method, request, env, headers) {
  if (path === "/health" && method === "GET") {
    return jsonResponse({ status: "ok", version: VERSION }, 200, headers);
  }
  if (path === "/webhook/stripe" && method === "POST") {
    return handleStripeWebhook(request, env, headers);
  }
  if (path === "/create-checkout" && method === "POST") {
    return handleCreateCheckout(request, env, headers);
  }
  if (path === "/create-checkout/master-key" && method === "POST") {
    return handleCreateMasterKeyCheckout(request, env, headers);
  }
  if (path === "/activate" && method === "POST") {
    return handleActivate(request, env, headers);
  }
  if (path === "/deactivate" && method === "POST") {
    return handleDeactivate(request, env, headers);
  }
  if (path === "/verify" && method === "POST") {
    return handleVerify(request, env, headers);
  }
  if (path === "/recover" && method === "POST") {
    return handleRecover(request, env, headers);
  }
  if (path === "/hub/signin" && method === "POST") {
    return handleHubSignIn(request, env, headers);
  }
  if (path === "/hub/community-stats" && method === "GET") {
    return handleCommunityStats(env, headers);
  }
  if (path === "/hub/account" && method === "GET") {
    const email = new URL(request.url).searchParams.get("email");
    return handleAccountStatus(email, env, headers);
  }
  if (path === "/hub/master-key/activate" && method === "POST") {
    return handleMasterKeyActivate(request, env, headers);
  }
  if (path === "/hub/master-key/reset" && method === "POST") {
    return handleMasterKeyReset(request, env, headers);
  }
  if (path === "/hub/master-key/status" && method === "GET") {
    const email = new URL(request.url).searchParams.get("email");
    return handleMasterKeyStatus(email, env, headers);
  }
  // SoundCloud OAuth
  if (path === "/auth/soundcloud/url" && method === "POST") {
    return handleSoundCloudAuthUrl(request, env, headers);
  }
  if (path === "/auth/soundcloud/callback" && method === "POST") {
    return handleSoundCloudCallback(request, env, headers);
  }
  if (path === "/auth/soundcloud/follow-status" && method === "GET") {
    const token = new URL(request.url).searchParams.get("token");
    return handleSoundCloudFollowStatus(token, env, headers);
  }
  // Google sign-in tracking
  if (path === "/auth/google/signin" && method === "POST") {
    return handleGoogleSignIn(request, env, headers);
  }
  return jsonResponse({ error: "Not found" }, 404, headers);
}

// Rate Limiting
async function checkRateLimit(clientIP, env) {
  const key = `ratelimit:${clientIP}`;
  const now = Math.floor(Date.now() / 1000);
  const windowStart = now - RATE_LIMIT_WINDOW;
  const data = await env.LICENSES.get(key);
  let requests = data ? JSON.parse(data) : [];
  requests = requests.filter(ts => ts > windowStart);
  if (requests.length >= RATE_LIMIT_MAX) {
    return { allowed: false, retryAfter: Math.min(...requests) + RATE_LIMIT_WINDOW - now };
  }
  requests.push(now);
  await env.LICENSES.put(key, JSON.stringify(requests), { expirationTtl: RATE_LIMIT_WINDOW * 2 });
  return { allowed: true };
}

// Validation
async function parseBody(request) {
  const len = request.headers.get("content-length");
  if (len && parseInt(len) > MAX_BODY_SIZE) throw new Error("Body too large");
  const text = await request.text();
  if (text.length > MAX_BODY_SIZE) throw new Error("Body too large");
  if (!text) return {};
  try { return JSON.parse(text); } catch { throw new Error("Invalid JSON"); }
}

function validEmail(e) { return e && typeof e === "string" && e.length <= 254 && /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(e); }
function validKey(k) { return k && typeof k === "string" && k.length <= 2048 && k.includes("."); }
function validDevice(d) { return d && typeof d === "string" && d.length >= 8 && d.length <= 128; }

async function hasActiveMasterKeySeat(email, env) {
  if (!validEmail(email)) return false;
  const sub = await env.LICENSES.get(`master_key_sub:${email.toLowerCase()}`);
  if (!sub) return false;
  try {
    const data = JSON.parse(sub);
    return data.status === "active" && data.seats >= 1;
  } catch {
    return false;
  }
}

// Stripe Webhook
async function handleStripeWebhook(request, env, headers) {
  const body = await request.text();
  const sig = request.headers.get("Stripe-Signature");
  if (!sig) return jsonResponse({ error: "Missing signature" }, 400, headers);

  const event = await verifyStripeWebhook(body, sig, env.STRIPE_WEBHOOK_SECRET);
  if (!event) return jsonResponse({ error: "Invalid signature" }, 401, headers);

  if (event.type === "checkout.session.completed") {
    const session = event.data.object;
    const email = session.customer_details?.email || session.customer_email;
    if (!validEmail(email)) return jsonResponse({ error: "Invalid email" }, 400, headers);

    const idempKey = `session:${session.id}`;
    if (await env.LICENSES.get(idempKey)) return jsonResponse({ ok: true, duplicate: true }, 200, headers);

    // Distinguish ProEQ8 one-time purchase vs Master Key subscription
    if (session.mode === "subscription") {
      // Master Key subscription checkout — store seat count
      const seats = session.line_items?.data?.[0]?.quantity || parseInt(session.metadata?.seats || "1");
      await env.LICENSES.put(`master_key_sub:${email.toLowerCase()}`, JSON.stringify({
        email: email.toLowerCase(),
        seats: Math.min(3, Math.max(1, seats)),
        status: "active",
        subscriptionId: session.subscription,
        created: new Date().toISOString(),
      }));
      await env.LICENSES.put(idempKey, `master_key_sub:${email.toLowerCase()}`, { expirationTtl: 2592000 });
      await sendMasterKeyEmail(email, seats, env.RESEND_API_KEY);
      await sendAdminNotification(email, `MasterKey (${seats} seats)`, session.id, env.RESEND_API_KEY);
      return jsonResponse({ ok: true, type: "master_key", seats }, 200, headers);
    }

    // ProEQ8 one-time purchase — issue license
    const licenseId = crypto.randomUUID();
    const maxDevices = parseInt(env.MAX_DEVICES_PER_LICENSE || "2", 10);
    const license = await generateLicense(email, licenseId, env.LICENSE_SIGNING_SECRET);

    await env.LICENSES.put(licenseId, JSON.stringify({
      email: email.toLowerCase(), product: "ProEQ8", max_uses: maxDevices,
      used: 0, devices: [], created: new Date().toISOString(), stripe_session_id: session.id,
    }));

    const emailKey = `email_index:${email.toLowerCase()}`;
    const existing = await env.LICENSES.get(emailKey);
    const ids = existing ? JSON.parse(existing) : [];
    if (!ids.includes(licenseId)) ids.push(licenseId);
    await env.LICENSES.put(emailKey, JSON.stringify(ids));
    await env.LICENSES.put(idempKey, licenseId, { expirationTtl: 2592000 });

    await sendLicenseEmail(email, license, maxDevices, env.RESEND_API_KEY);
    await sendAdminNotification(email, licenseId, session.id, env.RESEND_API_KEY);
    return jsonResponse({ ok: true, type: "proeq8" }, 200, headers);
  }
  return jsonResponse({ received: true }, 200, headers);
}

// Checkout
async function handleCreateCheckout(request, env, headers) {
  let body = {};
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const params = new URLSearchParams({
    mode: "payment",
    "line_items[0][price]": env.STRIPE_PRICE_ID,
    "line_items[0][quantity]": "1",
    success_url: env.SUCCESS_URL || "https://garebear99.github.io/FreeEQ8?purchase=success",
    cancel_url: env.CANCEL_URL || "https://garebear99.github.io/FreeEQ8?purchase=cancelled",
    "payment_method_types[0]": "card",
  });
  if (body.email && validEmail(body.email)) params.set("customer_email", body.email);

  try {
    const resp = await fetch("https://api.stripe.com/v1/checkout/sessions", {
      method: "POST",
      headers: { Authorization: `Bearer ${env.STRIPE_SECRET_KEY}`, "Content-Type": "application/x-www-form-urlencoded" },
      body: params,
    });
    const session = await resp.json();
    if (!resp.ok) return jsonResponse({ error: "Checkout failed" }, 500, headers);
    return jsonResponse({ url: session.url, id: session.id }, 200, headers);
  } catch { return jsonResponse({ error: "Checkout failed" }, 500, headers); }
}

async function handleCreateMasterKeyCheckout(request, env, headers) {
  let body = {};
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  // Master Key: 1-3 seats at $3 CAD each (max $9 CAD)
  const seats = Math.min(3, Math.max(1, parseInt(body.seats) || 1));

  const params = new URLSearchParams({
    mode: "subscription",
    "line_items[0][price]": env.MASTER_KEY_PRICE_ID,
    "line_items[0][quantity]": String(seats),
    success_url: `https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html?master_key=success&seats=${seats}`,
    cancel_url: "https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html",
    "payment_method_types[0]": "card",
  });
  if (body.email && validEmail(body.email)) params.set("customer_email", body.email);

  try {
    const resp = await fetch("https://api.stripe.com/v1/checkout/sessions", {
      method: "POST",
      headers: { Authorization: `Bearer ${env.STRIPE_SECRET_KEY}`, "Content-Type": "application/x-www-form-urlencoded" },
      body: params,
    });
    const session = await resp.json();
    if (!resp.ok) return jsonResponse({ error: "Checkout failed" }, 500, headers);
    return jsonResponse({ url: session.url, id: session.id }, 200, headers);
  } catch { return jsonResponse({ error: "Checkout failed" }, 500, headers); }
}

// License Activation
async function handleActivate(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { license_key, device_id } = body;
  if (!validKey(license_key)) return jsonResponse({ error: "Invalid license key" }, 400, headers);
  if (!validDevice(device_id)) return jsonResponse({ error: "Invalid device ID" }, 400, headers);

  const payload = await validateLicenseSignature(license_key, env.LICENSE_SIGNING_SECRET);
  if (!payload) return jsonResponse({ error: "Invalid license key" }, 401, headers);

  if (payload.expires && new Date(payload.expires + "T23:59:59Z") < new Date()) {
    return jsonResponse({ error: "License expired" }, 401, headers);
  }

  const licenseId = payload.license_id;
  if (!licenseId) return jsonResponse({ error: "Invalid key format" }, 400, headers);

  const recordStr = await env.LICENSES.get(licenseId);
  if (!recordStr) return jsonResponse({ error: "License not found" }, 404, headers);

  const record = JSON.parse(recordStr);
  if (record.devices.includes(device_id)) {
    return jsonResponse({ ok: true, message: "Already activated", used: record.used, max: record.max_uses }, 200, headers);
  }
  if (record.devices.length >= record.max_uses) {
    return jsonResponse({ error: "Activation limit reached", used: record.used, max: record.max_uses }, 403, headers);
  }

  record.devices.push(device_id);
  record.used = record.devices.length;
  await env.LICENSES.put(licenseId, JSON.stringify(record));
  return jsonResponse({ ok: true, message: "Activated", used: record.used, max: record.max_uses }, 200, headers);
}

async function handleDeactivate(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { license_key, device_id } = body;
  if (!validKey(license_key) || !validDevice(device_id)) return jsonResponse({ error: "Invalid input" }, 400, headers);

  const payload = await validateLicenseSignature(license_key, env.LICENSE_SIGNING_SECRET);
  if (!payload) return jsonResponse({ error: "Invalid license key" }, 401, headers);

  const recordStr = await env.LICENSES.get(payload.license_id);
  if (!recordStr) return jsonResponse({ error: "License not found" }, 404, headers);

  const record = JSON.parse(recordStr);
  const idx = record.devices.indexOf(device_id);
  if (idx === -1) return jsonResponse({ ok: true, message: "Not activated", used: record.used, max: record.max_uses }, 200, headers);

  // Require active Master Key seat to unlink a device
  const hasSeat = await hasActiveMasterKeySeat(record.email, env);
  if (!hasSeat) {
    return jsonResponse({ error: "Master Key subscription required to unlink devices", needsMasterKey: true }, 403, headers);
  }

  record.devices.splice(idx, 1);
  record.used = record.devices.length;
  await env.LICENSES.put(payload.license_id, JSON.stringify(record));
  return jsonResponse({ ok: true, message: "Deactivated", used: record.used, max: record.max_uses }, 200, headers);
}

async function handleVerify(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ ok: false, error: e.message }, 400, headers); }

  const { license_key, device_id } = body;
  if (!validKey(license_key) || !validDevice(device_id)) return jsonResponse({ ok: false, error: "Invalid input" }, 400, headers);

  const payload = await validateLicenseSignature(license_key, env.LICENSE_SIGNING_SECRET);
  if (!payload) return jsonResponse({ ok: false, error: "Invalid key" }, 401, headers);
  if (payload.expires && new Date(payload.expires + "T23:59:59Z") < new Date()) {
    return jsonResponse({ ok: false, error: "Expired" }, 401, headers);
  }

  const recordStr = await env.LICENSES.get(payload.license_id);
  if (!recordStr) return jsonResponse({ ok: false, error: "Not found" }, 404, headers);

  const record = JSON.parse(recordStr);
  if (!record.devices.includes(device_id)) return jsonResponse({ ok: false, error: "Device not activated" }, 403, headers);
  return jsonResponse({ ok: true, used: record.used, max: record.max_uses }, 200, headers);
}

async function handleRecover(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { email } = body;
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);

  const rateKey = `recover:${email.toLowerCase()}`;
  if (await env.LICENSES.get(rateKey)) return jsonResponse({ error: "Wait 10 minutes" }, 429, headers);
  await env.LICENSES.put(rateKey, "1", { expirationTtl: 600 });

  const indexKey = `email_index:${email.toLowerCase()}`;
  const indexed = await env.LICENSES.get(indexKey);
  if (!indexed) return jsonResponse({ ok: false, message: "No licenses found" }, 200, headers);

  const ids = JSON.parse(indexed);
  const matches = [];
  for (const id of ids) {
    const rec = await env.LICENSES.get(id);
    if (rec) {
      const r = JSON.parse(rec);
      matches.push({ id, product: r.product || "ProEQ8", used: r.used || 0, max: r.max_uses || 2 });
    }
  }
  if (!matches.length) return jsonResponse({ ok: false, message: "No licenses found" }, 200, headers);

  const html = `<h2>Your Licenses</h2><ul>${matches.map(m => `<li><strong>${m.product}</strong> — ${m.id} (${m.used}/${m.max} devices)</li>`).join("")}</ul><p>— TizWildin</p>`;
  await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: { Authorization: `Bearer ${env.RESEND_API_KEY}`, "Content-Type": "application/json" },
    body: JSON.stringify({ from: "TizWildin <onboarding@resend.dev>", to: [email], subject: "Your License Keys", html }),
  });
  return jsonResponse({ ok: true, message: `Sent to ${email}` }, 200, headers);
}

// HUB
async function handleHubSignIn(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { email, name, provider, picture } = body;
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);

  const key = `hub_user:${email.toLowerCase()}`;
  const existing = await env.LICENSES.get(key);
  const now = new Date().toISOString();
  let user = existing ? JSON.parse(existing) : { email: email.toLowerCase(), name: "", provider: "", picture: "", created: now, verified: false, acceptedSubmissions: 0 };
  user.lastSeen = now;
  if (name) user.name = String(name).slice(0, 100);
  if (provider) user.provider = String(provider).slice(0, 50);
  if (picture) user.picture = String(picture).slice(0, 500);
  await env.LICENSES.put(key, JSON.stringify(user));

  if (!existing) {
    const cnt = await env.LICENSES.get("hub_meta:user_count") || "0";
    await env.LICENSES.put("hub_meta:user_count", String(parseInt(cnt) + 1));
  }
  return jsonResponse({ ok: true, email: user.email, verified: user.verified }, 200, headers);
}

async function handleCommunityStats(env, headers) {
  const total = parseInt(await env.LICENSES.get("hub_meta:user_count") || "0");
  return jsonResponse({
    totalUsers: total, capacity: 1200, warningThreshold: 1000,
    capacityPercent: Math.min(100, Math.floor(total / 12)),
    signupsOpen: total < 1200, warningActive: total >= 1000,
    slotsRemaining: Math.max(0, 1200 - total),
    giveawayTarget: 1000, giveawayProgress: Math.min(100, Math.floor(total / 10)),
    remaining: Math.max(0, 1000 - total),
  }, 200, headers);
}

async function handleAccountStatus(email, env, headers) {
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);
  const e = email.toLowerCase();
  const userStr = await env.LICENSES.get(`hub_user:${e}`);
  const user = userStr ? JSON.parse(userStr) : null;

  const indexStr = await env.LICENSES.get(`email_index:${e}`);
  const licenses = [];
  if (indexStr) {
    for (const id of JSON.parse(indexStr)) {
      const rec = await env.LICENSES.get(id);
      if (rec) { const r = JSON.parse(rec); licenses.push({ licenseId: id, product: r.product || "ProEQ8", devicesUsed: r.used || 0, devicesMax: r.max_uses || 2 }); }
    }
  }

  const mkStr = await env.LICENSES.get(`master_key:${e}`);
  const mk = mkStr ? JSON.parse(mkStr) : null;

  return jsonResponse({
    email: e,
    hubAccount: user ? { name: user.name, provider: user.provider, verified: user.verified, created: user.created } : null,
    licenses,
    masterKey: mk ? { linkedEmail: mk.linkedEmail, status: mk.status } : null,
  }, 200, headers);
}

async function handleMasterKeyActivate(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { primaryEmail, linkedEmail } = body;
  if (!validEmail(primaryEmail) || !validEmail(linkedEmail)) return jsonResponse({ error: "Valid emails required" }, 400, headers);

  const subKey = `master_key_sub:${primaryEmail.toLowerCase()}`;
  if (!(await env.LICENSES.get(subKey))) return jsonResponse({ error: "No subscription" }, 403, headers);

  const mk = { primaryEmail: primaryEmail.toLowerCase(), linkedEmail: linkedEmail.toLowerCase(), activatedAt: new Date().toISOString(), status: "active" };
  await env.LICENSES.put(`master_key:${primaryEmail.toLowerCase()}`, JSON.stringify(mk));

  const license = await generateLicense(linkedEmail, `mk_${crypto.randomUUID()}`, env.LICENSE_SIGNING_SECRET, "MasterKey");
  return jsonResponse({ ok: true, linkedEmail: mk.linkedEmail, masterKeyLicense: license }, 200, headers);
}

async function handleMasterKeyReset(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { primaryEmail } = body;
  if (!validEmail(primaryEmail)) return jsonResponse({ error: "Valid email required" }, 400, headers);

  const key = `master_key:${primaryEmail.toLowerCase()}`;
  const existing = await env.LICENSES.get(key);
  if (!existing) return jsonResponse({ error: "Not found" }, 404, headers);

  const mk = JSON.parse(existing);
  mk.linkedEmail = ""; mk.status = "reset"; mk.resetAt = new Date().toISOString();
  await env.LICENSES.put(key, JSON.stringify(mk));
  return jsonResponse({ ok: true }, 200, headers);
}

async function handleMasterKeyStatus(email, env, headers) {
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);
  const mkStr = await env.LICENSES.get(`master_key:${email.toLowerCase()}`);
  if (!mkStr) return jsonResponse({ hasMasterKey: false }, 200, headers);
  const mk = JSON.parse(mkStr);
  return jsonResponse({ hasMasterKey: true, linkedEmail: mk.linkedEmail, status: mk.status }, 200, headers);
}

// SoundCloud OAuth
async function handleSoundCloudAuthUrl(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const clientId = env.SOUNDCLOUD_CLIENT_ID;
  if (!clientId) {
    return jsonResponse({ approved: false, reason: "soundcloud_not_configured" }, 200, headers);
  }

  const redirectUri = body.redirectUri || "https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html";
  const params = new URLSearchParams({
    client_id: clientId,
    redirect_uri: redirectUri,
    response_type: "code",
    scope: "non-expiring",
  });
  return jsonResponse({ approved: true, url: `${SC_AUTH_URL}?${params}` }, 200, headers);
}

async function handleSoundCloudCallback(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { code, redirectUri } = body;
  if (!code) return jsonResponse({ approved: false, reason: "missing_code" }, 400, headers);

  const clientId = env.SOUNDCLOUD_CLIENT_ID;
  const clientSecret = env.SOUNDCLOUD_CLIENT_SECRET;
  if (!clientId || !clientSecret) {
    return jsonResponse({ approved: false, reason: "soundcloud_not_configured" }, 200, headers);
  }

  // Exchange code for access token
  let tokenData;
  try {
    const tokenResp = await fetch(SC_TOKEN_URL, {
      method: "POST",
      headers: { "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({
        grant_type: "authorization_code",
        client_id: clientId,
        client_secret: clientSecret,
        redirect_uri: redirectUri || "https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html",
        code,
      }),
    });
    tokenData = await tokenResp.json();
    if (!tokenResp.ok || !tokenData.access_token) {
      return jsonResponse({ approved: false, reason: "token_exchange_failed", detail: tokenData.error || "unknown" }, 200, headers);
    }
  } catch (e) {
    return jsonResponse({ approved: false, reason: `token_exchange_error: ${e.message}` }, 200, headers);
  }

  const scAccessToken = tokenData.access_token;

  // Fetch user profile
  let profile;
  try {
    const meResp = await fetch(SC_ME_URL, {
      headers: { Authorization: `OAuth ${scAccessToken}` },
    });
    profile = await meResp.json();
    if (!meResp.ok) {
      return jsonResponse({ approved: false, reason: "profile_fetch_failed" }, 200, headers);
    }
  } catch (e) {
    return jsonResponse({ approved: false, reason: `profile_fetch_error: ${e.message}` }, 200, headers);
  }

  const scUserId = String(profile.id || "");
  const scUsername = profile.username || "";
  const scAvatar = profile.avatar_url || "";
  const scEmail = profile.email || profile.primary_email || "";

  // Generate account ID
  const emailForId = scEmail || `sc_${scUserId}@soundcloud.local`;
  const accountId = await makeAccountId(emailForId);

  // Store SC link in KV
  const linkKey = `sc_link:${scUserId}`;
  await env.LICENSES.put(linkKey, JSON.stringify({
    provider: "soundcloud",
    scUserId,
    accountId,
    scUsername,
    scAvatar,
    scEmail,
    scAccessToken,
    linkedAt: new Date().toISOString(),
  }));

  // Also store by account for quick lookup
  await env.LICENSES.put(`sc_account:${accountId}`, linkKey);

  // Track user for community count
  const userKey = `hub_user:${emailForId.toLowerCase()}`;
  const existingUser = await env.LICENSES.get(userKey);
  const now = new Date().toISOString();
  if (!existingUser) {
    await env.LICENSES.put(userKey, JSON.stringify({
      email: emailForId.toLowerCase(),
      name: scUsername,
      provider: "soundcloud",
      picture: scAvatar,
      created: now,
      lastSeen: now,
      verified: false,
    }));
    const cnt = await env.LICENSES.get("hub_meta:user_count") || "0";
    await env.LICENSES.put("hub_meta:user_count", String(parseInt(cnt) + 1));
  } else {
    const user = JSON.parse(existingUser);
    user.lastSeen = now;
    user.picture = scAvatar;
    user.name = scUsername;
    await env.LICENSES.put(userKey, JSON.stringify(user));
  }

  // Create session token (simple signed token)
  const sessionToken = await generateSessionToken(accountId, scAccessToken, env.LICENSE_SIGNING_SECRET);

  return jsonResponse({
    approved: true,
    token: sessionToken,
    accountId,
    scUsername,
    scAvatar,
    scEmail,
    scUserId,
  }, 200, headers);
}

async function handleSoundCloudFollowStatus(token, env, headers) {
  if (!token) return jsonResponse({ following: false, reason: "no_token" }, 200, headers);

  // Decode token to get SC access token
  let scAccessToken;
  try {
    const decoded = await decodeSessionToken(token, env.LICENSE_SIGNING_SECRET);
    if (!decoded || !decoded.scToken) {
      return jsonResponse({ following: false, reason: "invalid_token" }, 200, headers);
    }
    scAccessToken = decoded.scToken;
  } catch {
    return jsonResponse({ following: false, reason: "token_decode_failed" }, 200, headers);
  }

  const tizwildinId = env.TIZWILDIN_SC_USER_ID;
  if (!tizwildinId) {
    return jsonResponse({ following: false, reason: "tizwildin_id_not_configured" }, 200, headers);
  }

  try {
    const resp = await fetch(`${SC_FOLLOWINGS_URL}/${tizwildinId}`, {
      headers: { Authorization: `OAuth ${scAccessToken}` },
    });
    if (resp.status === 200) {
      return jsonResponse({ following: true }, 200, headers);
    } else if (resp.status === 404) {
      return jsonResponse({ following: false }, 200, headers);
    } else {
      return jsonResponse({ following: false, reason: `sc_api_${resp.status}` }, 200, headers);
    }
  } catch (e) {
    return jsonResponse({ following: false, reason: e.message }, 200, headers);
  }
}

// Google sign-in tracking (mirrors hub/signin but for Google)
async function handleGoogleSignIn(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { email, name, picture } = body;
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);

  const key = `hub_user:${email.toLowerCase()}`;
  const existing = await env.LICENSES.get(key);
  const now = new Date().toISOString();
  let user = existing ? JSON.parse(existing) : { email: email.toLowerCase(), name: "", provider: "", picture: "", created: now, verified: false };
  user.lastSeen = now;
  if (name) user.name = String(name).slice(0, 100);
  user.provider = "google";
  if (picture) user.picture = String(picture).slice(0, 500);
  await env.LICENSES.put(key, JSON.stringify(user));

  if (!existing) {
    const cnt = await env.LICENSES.get("hub_meta:user_count") || "0";
    await env.LICENSES.put("hub_meta:user_count", String(parseInt(cnt) + 1));
  }

  const accountId = await makeAccountId(email);
  const token = await generateSessionToken(accountId, null, env.LICENSE_SIGNING_SECRET);
  return jsonResponse({ ok: true, email: user.email, token, accountId }, 200, headers);
}

// Helper: Generate deterministic account ID from email
async function makeAccountId(email) {
  const data = new TextEncoder().encode(email.toLowerCase());
  const hashBuffer = await crypto.subtle.digest("SHA-256", data);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  return hashArray.slice(0, 16).map(b => b.toString(16).padStart(2, "0")).join("");
}

// Helper: Generate session token
async function generateSessionToken(accountId, scToken, secret) {
  const payload = JSON.stringify({ accountId, scToken, issued: new Date().toISOString() });
  const payloadB64 = btoa(payload);
  const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const sig = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
  return `${payloadB64}.${btoa(String.fromCharCode(...new Uint8Array(sig)))}`;
}

// Helper: Decode session token
async function decodeSessionToken(token, secret) {
  try {
    const [payloadB64, sigB64] = token.split(".");
    if (!payloadB64 || !sigB64) return null;
    const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
    const sig = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
    const expected = btoa(String.fromCharCode(...new Uint8Array(sig)));
    if (expected.length !== sigB64.length) return null;
    let mismatch = 0;
    for (let i = 0; i < expected.length; i++) mismatch |= expected.charCodeAt(i) ^ sigB64.charCodeAt(i);
    if (mismatch) return null;
    return JSON.parse(atob(payloadB64));
  } catch { return null; }
}

// Crypto
async function generateLicense(email, licenseId, secret, product = "ProEQ8") {
  const payload = JSON.stringify({ product, email, license_id: licenseId, expires: getExpiry(), issued: new Date().toISOString() });
  const payloadB64 = btoa(payload);
  const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
  const sig = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
  return `${payloadB64}.${btoa(String.fromCharCode(...new Uint8Array(sig)))}`;
}

function getExpiry() { const d = new Date(); d.setFullYear(d.getFullYear() + 1); return d.toISOString().split("T")[0]; }

async function validateLicenseSignature(licenseKey, secret) {
  try {
    const [payloadB64, sigB64] = licenseKey.split(".");
    if (!payloadB64 || !sigB64) return null;
    const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
    const sig = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
    const expected = btoa(String.fromCharCode(...new Uint8Array(sig)));
    if (expected.length !== sigB64.length) return null;
    let mismatch = 0;
    for (let i = 0; i < expected.length; i++) mismatch |= expected.charCodeAt(i) ^ sigB64.charCodeAt(i);
    if (mismatch) return null;
    const payload = JSON.parse(atob(payloadB64));
    if (payload.product !== "ProEQ8" && payload.product !== "MasterKey") return null;
    return payload;
  } catch { return null; }
}

async function verifyStripeWebhook(payload, sigHeader, secret) {
  try {
    const parts = Object.fromEntries(sigHeader.split(",").map(p => p.split("=")));
    const { t, v1 } = parts;
    if (!t || !v1 || Math.abs(Date.now() / 1000 - parseInt(t)) > 300) return null;
    const key = await crypto.subtle.importKey("raw", new TextEncoder().encode(secret), { name: "HMAC", hash: "SHA-256" }, false, ["sign"]);
    const sig = await crypto.subtle.sign("HMAC", key, new TextEncoder().encode(`${t}.${payload}`));
    const expected = Array.from(new Uint8Array(sig)).map(b => b.toString(16).padStart(2, "0")).join("");
    if (expected.length !== v1.length) return null;
    let mismatch = 0;
    for (let i = 0; i < expected.length; i++) mismatch |= expected.charCodeAt(i) ^ v1.charCodeAt(i);
    return mismatch ? null : JSON.parse(payload);
  } catch { return null; }
}

// Email
const ADMIN = "gdoman99@gmail.com";

async function sendLicenseEmail(email, license, maxDevices, apiKey) {
  const html = `<h2>ProEQ8 License</h2><p>Your key:</p><pre style="background:#f4f4f4;padding:12px;border-radius:4px;word-break:break-all;">${license}</pre><p>Paste in ProEQ8 → Activate. Up to ${maxDevices} devices.</p><p>— TizWildin</p>`;
  await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: { Authorization: `Bearer ${apiKey}`, "Content-Type": "application/json" },
    body: JSON.stringify({ from: "ProEQ8 <onboarding@resend.dev>", to: [email], subject: "Your ProEQ8 License Key", html }),
  });
}

async function sendAdminNotification(email, licenseId, sessionId, apiKey) {
  try {
    await fetch("https://api.resend.com/emails", {
      method: "POST",
      headers: { Authorization: `Bearer ${apiKey}`, "Content-Type": "application/json" },
      body: JSON.stringify({ from: "Sales <onboarding@resend.dev>", to: [ADMIN], subject: `[ProEQ8] ${email}`, html: `<p>${email} — ${licenseId}</p>` }),
    });
  } catch {}
}

async function sendMasterKeyEmail(email, seats, apiKey) {
  const html = `<h2>Master Key Subscription Active</h2><p>Your Master Key subscription with <strong>${seats} seat(s)</strong> is now active.</p><p>You can now unlink devices from your ProEQ8 licenses as needed. Manage your subscription in the TizWildin HUB.</p><p>— TizWildin</p>`;
  await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: { Authorization: `Bearer ${apiKey}`, "Content-Type": "application/json" },
    body: JSON.stringify({ from: "TizWildin <onboarding@resend.dev>", to: [email], subject: "Master Key Subscription Confirmed", html }),
  });
}

// Helpers
function jsonResponse(data, status = 200, extra = {}) {
  return new Response(JSON.stringify(data), { status, headers: { "Content-Type": "application/json", ...extra } });
}

const ORIGINS = ["https://garebear99.github.io", "http://localhost:5000", "http://localhost:3000"];

function corsResponse(response, request) {
  const headers = new Headers(response.headers);
  const origin = request?.headers?.get("Origin") || "";
  const allowed = ORIGINS.find(o => origin.startsWith(o)) || ORIGINS[0];
  headers.set("Access-Control-Allow-Origin", allowed);
  headers.set("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  headers.set("Access-Control-Allow-Headers", "Content-Type");
  headers.set("Vary", "Origin");
  return new Response(response.body, { status: response.status, headers });
}
