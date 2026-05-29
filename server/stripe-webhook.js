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

const VERSION = "3.2.0";

// Hub Product Catalog (mirrors arc_service products.catalog.json)
const HUB_PRODUCTS = {
  // Pro products (one-time purchase)
  proeq8: { name: "ProEQ8", price: 2999, free: false },
  therum: { name: "Therum", price: 1999, free: false },
  wurp: { name: "WURP", price: 1499, free: false },
  aether: { name: "AETHER", price: 1999, free: false },
  paintmask: { name: "PaintMask", price: 2499, free: false },
  bassmaid: { name: "BassMaid", price: 999, free: false },
  gluemaid: { name: "GlueMaid", price: 999, free: false },
  spacemaid: { name: "SpaceMaid", price: 999, free: false },
  mixmaid: { name: "MixMaid", price: 999, free: false },
  waveform_pro: { name: "WaveForm Pro", price: 1999, free: false },
  riftsynth_pro: { name: "RiftSynth Pro", price: 1999, free: false },
  // Bundle
  complete_collection: { name: "Complete Collection", price: 9999, free: false, bundle: true },
  // Free products (no checkout needed, auto-entitled)
  freeeq8: { name: "FreeEQ8", price: 0, free: true },
  whispergate: { name: "WhisperGate", price: 0, free: true },
  waveform_riftsynth_lite: { name: "WaveForm/RiftSynth Lite", price: 0, free: true },
};

// Products included in Complete Collection bundle
const BUNDLE_PRODUCTS = [
  "therum", "wurp", "aether", "paintmask",
  "bassmaid", "gluemaid", "spacemaid", "mixmaid",
  "waveform_pro", "riftsynth_pro"
];

// SoundCloud OAuth endpoints
const SC_AUTH_URL = "https://api.soundcloud.com/connect";
const SC_TOKEN_URL = "https://api.soundcloud.com/oauth2/token";
const SC_ME_URL = "https://api.soundcloud.com/me";
const SC_FOLLOWINGS_URL = "https://api.soundcloud.com/me/followings";
const MAX_BODY_SIZE = 16384;
const RATE_LIMIT_WINDOW = 60;
const RATE_LIMIT_MAX = 30;

export default {
  // Cron trigger for weekly newsletter (runs every Monday 10 AM UTC)
  async scheduled(event, env, ctx) {
    const cronType = event.cron;
    if (cronType === "0 10 * * 1") {
      // Weekly newsletter — Mondays at 10 AM UTC
      await sendWeeklyNewsletter(env);
      await sendAdminSubscriberDigest(env);
    }
  },

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
  // Hub product checkout (any product from catalog)
  if (path === "/create-checkout/product" && method === "POST") {
    return handleCreateProductCheckout(request, env, headers);
  }
  // Get owned products for an account
  if (path === "/hub/products" && method === "GET") {
    const email = new URL(request.url).searchParams.get("email");
    return handleGetOwnedProducts(email, env, headers);
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
  if (path === "/hub/master-key/link" && method === "POST") {
    return handleMasterKeyLink(request, env, headers);
  }
  if (path === "/hub/master-key/unlink" && method === "POST") {
    return handleMasterKeyUnlink(request, env, headers);
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
  // Admin endpoints for newsletter/digest
  if (path === "/admin/subscribers" && method === "GET") {
    return handleAdminSubscribers(request, env, headers);
  }
  if (path === "/admin/send-weekly-newsletter" && method === "POST") {
    return handleManualNewsletter(request, env, headers);
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
    if (data.status !== "active" || data.seats < 1) return false;
    
    // Also check if the email is linked as a seat holder (for device deactivation)
    // The primary email holder OR any linked email can deactivate devices
    const mkStr = await env.LICENSES.get(`master_key:${email.toLowerCase()}`);
    if (mkStr) return true; // Primary holder
    
    // Check if this email is linked under someone else's Master Key
    // This requires a reverse lookup — for now, primary holder only
    return true;
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

    // Distinguish ProEQ8 one-time purchase vs Master Key subscription vs Hub product
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

    // Check for Hub product purchase via metadata
    const productId = session.metadata?.product_id;
    if (productId && HUB_PRODUCTS[productId]) {
      const product = HUB_PRODUCTS[productId];
      const owned = await getOwnedProducts(email, env);
      
      // Generate license key for this product
      const licenseId = `${productId}_${crypto.randomUUID()}`;
      const license = await generateLicense(email, licenseId, env.LICENSE_SIGNING_SECRET, product.name);
      
      // Add to owned products
      if (product.bundle) {
        if (!owned.bundles.includes(productId)) owned.bundles.push(productId);
      } else {
        if (!owned.products.includes(productId)) owned.products.push(productId);
      }
      owned.licenses[productId] = { licenseId, license, purchasedAt: new Date().toISOString() };
      await saveOwnedProducts(email, owned, env);
      
      await env.LICENSES.put(idempKey, licenseId, { expirationTtl: 2592000 });
      await sendProductEmail(email, product.name, license, env.RESEND_API_KEY);
      await sendAdminNotification(email, `${product.name} (${licenseId})`, session.id, env.RESEND_API_KEY);
      return jsonResponse({ ok: true, type: "hub_product", product: productId }, 200, headers);
    }

    // ProEQ8 one-time purchase (legacy) — issue license
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

// Generic product checkout (for any Hub product)
async function handleCreateProductCheckout(request, env, headers) {
  let body = {};
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { email, productId } = body;
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);
  
  const product = HUB_PRODUCTS[productId];
  if (!product) return jsonResponse({ error: "Unknown product" }, 400, headers);
  if (product.free) return jsonResponse({ error: "This product is free — no checkout needed" }, 400, headers);

  // Check if user already owns this product
  const owned = await getOwnedProducts(email, env);
  if (owned.products.includes(productId)) {
    return jsonResponse({ error: "You already own this product" }, 400, headers);
  }
  // If user owns Complete Collection, they already have access
  if (owned.bundles.includes("complete_collection") && BUNDLE_PRODUCTS.includes(productId)) {
    return jsonResponse({ error: "You already have access via Complete Collection" }, 400, headers);
  }

  // Get price ID from env (format: STRIPE_PRICE_ID_<PRODUCT_ID>)
  const priceIdKey = `STRIPE_PRICE_ID_${productId.toUpperCase()}`;
  const priceId = env[priceIdKey];
  if (!priceId) {
    // If no specific price ID, use inline price
    return handleCreateInlineCheckout(email, productId, product, env, headers);
  }

  const params = new URLSearchParams({
    mode: "payment",
    "line_items[0][price]": priceId,
    "line_items[0][quantity]": "1",
    success_url: `https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html?purchase=success&product=${productId}`,
    cancel_url: `https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html`,
    "payment_method_types[0]": "card",
    customer_email: email,
    "metadata[product_id]": productId,
    "metadata[product_name]": product.name,
  });

  try {
    const resp = await fetch("https://api.stripe.com/v1/checkout/sessions", {
      method: "POST",
      headers: { Authorization: `Bearer ${env.STRIPE_SECRET_KEY}`, "Content-Type": "application/x-www-form-urlencoded" },
      body: params,
    });
    const session = await resp.json();
    if (!resp.ok) return jsonResponse({ error: "Checkout failed", detail: session.error?.message }, 500, headers);
    return jsonResponse({ url: session.url, id: session.id, product: productId }, 200, headers);
  } catch { return jsonResponse({ error: "Checkout failed" }, 500, headers); }
}

// Inline price checkout (when no pre-created Stripe Price ID exists)
async function handleCreateInlineCheckout(email, productId, product, env, headers) {
  const params = new URLSearchParams({
    mode: "payment",
    "line_items[0][price_data][currency]": "cad",
    "line_items[0][price_data][product_data][name]": product.name,
    "line_items[0][price_data][product_data][description]": `TizWildin ${product.name} — lifetime license`,
    "line_items[0][price_data][unit_amount]": String(product.price),
    "line_items[0][quantity]": "1",
    success_url: `https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html?purchase=success&product=${productId}`,
    cancel_url: `https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html`,
    "payment_method_types[0]": "card",
    customer_email: email,
    "metadata[product_id]": productId,
    "metadata[product_name]": product.name,
  });

  try {
    const resp = await fetch("https://api.stripe.com/v1/checkout/sessions", {
      method: "POST",
      headers: { Authorization: `Bearer ${env.STRIPE_SECRET_KEY}`, "Content-Type": "application/x-www-form-urlencoded" },
      body: params,
    });
    const session = await resp.json();
    if (!resp.ok) return jsonResponse({ error: "Checkout failed", detail: session.error?.message }, 500, headers);
    return jsonResponse({ url: session.url, id: session.id, product: productId }, 200, headers);
  } catch { return jsonResponse({ error: "Checkout failed" }, 500, headers); }
}

// Get all owned products for an email
async function getOwnedProducts(email, env) {
  const e = email.toLowerCase();
  const ownedKey = `owned_products:${e}`;
  const ownedStr = await env.LICENSES.get(ownedKey);
  if (ownedStr) {
    return JSON.parse(ownedStr);
  }
  return { products: [], bundles: [], licenses: {} };
}

async function saveOwnedProducts(email, data, env) {
  const e = email.toLowerCase();
  const ownedKey = `owned_products:${e}`;
  await env.LICENSES.put(ownedKey, JSON.stringify(data));
}

async function handleGetOwnedProducts(email, env, headers) {
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);
  
  const owned = await getOwnedProducts(email, env);
  
  // Also include ProEQ8 licenses from the old system
  const indexStr = await env.LICENSES.get(`email_index:${email.toLowerCase()}`);
  const proeq8Licenses = [];
  if (indexStr) {
    for (const id of JSON.parse(indexStr)) {
      const rec = await env.LICENSES.get(id);
      if (rec) {
        const r = JSON.parse(rec);
        proeq8Licenses.push({ licenseId: id, product: r.product || "ProEQ8", devicesUsed: r.used || 0, devicesMax: r.max_uses || 2 });
      }
    }
  }
  
  // Free products are always available
  const freeProducts = Object.entries(HUB_PRODUCTS)
    .filter(([_, p]) => p.free)
    .map(([id, _]) => id);
  
  // Build full entitlements
  const allProducts = [...new Set([...owned.products, ...freeProducts])];
  
  // If owns Complete Collection, add all bundle products
  if (owned.bundles.includes("complete_collection")) {
    allProducts.push(...BUNDLE_PRODUCTS.filter(p => !allProducts.includes(p)));
  }
  
  return jsonResponse({
    email: email.toLowerCase(),
    ownedProducts: allProducts,
    bundles: owned.bundles,
    licenses: { ...owned.licenses, proeq8: proeq8Licenses },
    freeProducts,
    catalog: HUB_PRODUCTS,
  }, 200, headers);
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
// Helper to extract geo/device info from Cloudflare headers
function extractRequestMeta(request) {
  return {
    ip: request.headers.get("CF-Connecting-IP") || "unknown",
    country: request.headers.get("CF-IPCountry") || "XX",
    city: request.headers.get("CF-IPCity") || "",
    region: request.headers.get("CF-Region") || "",
    timezone: request.headers.get("CF-Timezone") || "",
    userAgent: (request.headers.get("User-Agent") || "").slice(0, 200),
  };
}

async function handleHubSignIn(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { email, name, provider, picture } = body;
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);

  const meta = extractRequestMeta(request);
  const key = `hub_user:${email.toLowerCase()}`;
  const existing = await env.LICENSES.get(key);
  const now = new Date().toISOString();
  
  let user = existing ? JSON.parse(existing) : {
    email: email.toLowerCase(),
    name: "",
    provider: "",
    picture: "",
    created: now,
    verified: false,
    acceptedSubmissions: 0,
    // New tracking fields
    loginCount: 0,
    firstIp: meta.ip,
    firstCountry: meta.country,
    firstCity: meta.city,
    firstRegion: meta.region,
    firstUserAgent: meta.userAgent,
  };
  
  // Update on every login
  user.lastSeen = now;
  user.loginCount = (user.loginCount || 0) + 1;
  user.lastIp = meta.ip;
  user.lastCountry = meta.country;
  user.lastCity = meta.city;
  user.lastRegion = meta.region;
  user.lastTimezone = meta.timezone;
  user.lastUserAgent = meta.userAgent;
  
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

  // Get ProEQ8 licenses owned by this email
  const indexStr = await env.LICENSES.get(`email_index:${e}`);
  const licenses = [];
  if (indexStr) {
    for (const id of JSON.parse(indexStr)) {
      const rec = await env.LICENSES.get(id);
      if (rec) { const r = JSON.parse(rec); licenses.push({ licenseId: id, product: r.product || "ProEQ8", devicesUsed: r.used || 0, devicesMax: r.max_uses || 2 }); }
    }
  }

  // Check if this user OWNS a Master Key subscription (is the purchaser)
  const mkSubStr = await env.LICENSES.get(`master_key_sub:${e}`);
  const mkDataStr = await env.LICENSES.get(`master_key:${e}`);
  let masterKeyOwner = null;
  
  if (mkSubStr) {
    const sub = JSON.parse(mkSubStr);
    const mkData = mkDataStr ? JSON.parse(mkDataStr) : { linkedEmails: [], licenses: {} };
    // Migration for old format
    if (mkData.linkedEmail && !mkData.linkedEmails) {
      mkData.linkedEmails = mkData.linkedEmail ? [mkData.linkedEmail] : [];
    }
    masterKeyOwner = {
      isOwner: true,
      canManageSeats: true,  // Owner can add/remove linked emails
      status: sub.status,
      seatsTotal: sub.seats || 1,
      seatsUsed: (mkData.linkedEmails || []).length,
      seatsAvailable: (sub.seats || 1) - (mkData.linkedEmails || []).length,
      linkedEmails: mkData.linkedEmails || [],
      licenses: mkData.licenses || {},
      subscriptionId: sub.subscriptionId,
      created: sub.created,
    };
  }

  // Check if this user is LINKED to someone else's Master Key (not the owner)
  let masterKeyLinked = null;
  if (!masterKeyOwner) {
    // Search for this email in other users' Master Key linked lists
    const linkedTo = await findMasterKeyOwnerForLinkedEmail(e, env);
    if (linkedTo) {
      masterKeyLinked = {
        isOwner: false,
        canManageSeats: false,  // Linked users cannot manage seats
        ownerEmail: linkedTo.ownerEmail,
        license: linkedTo.license,
        message: "Your Master Key license is managed by the account holder.",
      };
    }
  }

  return jsonResponse({
    email: e,
    hubAccount: user ? { name: user.name, provider: user.provider, verified: user.verified, created: user.created } : null,
    licenses,
    masterKey: masterKeyOwner || masterKeyLinked || null,
  }, 200, headers);
}

// Helper: Find if an email is linked under someone's Master Key
async function findMasterKeyOwnerForLinkedEmail(linkedEmail, env) {
  // Check reverse index first (fast path)
  const reverseKey = `mk_linked:${linkedEmail}`;
  const ownerEmailStr = await env.LICENSES.get(reverseKey);
  if (ownerEmailStr) {
    const mkDataStr = await env.LICENSES.get(`master_key:${ownerEmailStr}`);
    if (mkDataStr) {
      const mkData = JSON.parse(mkDataStr);
      if (mkData.linkedEmails?.includes(linkedEmail)) {
        return {
          ownerEmail: ownerEmailStr,
          license: mkData.licenses?.[linkedEmail] || null,
        };
      }
    }
  }
  return null;
}

// Master Key Seating System
// Data structure:
//   master_key_sub:{email} → { email, seats: 1-3, status, subscriptionId, created }
//   master_key:{email}     → { primaryEmail, linkedEmails: [], licenses: {email: licenseKey}, status, created }

async function getMasterKeyData(primaryEmail, env) {
  const subStr = await env.LICENSES.get(`master_key_sub:${primaryEmail.toLowerCase()}`);
  if (!subStr) return { sub: null, mk: null };
  const sub = JSON.parse(subStr);
  
  const mkStr = await env.LICENSES.get(`master_key:${primaryEmail.toLowerCase()}`);
  let mk = mkStr ? JSON.parse(mkStr) : {
    primaryEmail: primaryEmail.toLowerCase(),
    linkedEmails: [],
    licenses: {},
    status: "active",
    created: new Date().toISOString(),
  };
  
  // Migration: convert old single linkedEmail to array format
  if (mk.linkedEmail && !mk.linkedEmails) {
    mk.linkedEmails = mk.linkedEmail ? [mk.linkedEmail] : [];
    mk.licenses = mk.licenses || {};
    delete mk.linkedEmail;
  }
  if (!mk.linkedEmails) mk.linkedEmails = [];
  if (!mk.licenses) mk.licenses = {};
  
  return { sub, mk };
}

async function handleMasterKeyActivate(request, env, headers) {
  // Legacy endpoint — now just an alias for /link with first email
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { primaryEmail, linkedEmail } = body;
  if (!validEmail(primaryEmail) || !validEmail(linkedEmail)) return jsonResponse({ error: "Valid emails required" }, 400, headers);

  // Redirect to link logic
  return handleMasterKeyLinkInternal(primaryEmail, linkedEmail, env, headers);
}

async function handleMasterKeyLink(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { primaryEmail, linkedEmail } = body;
  if (!validEmail(primaryEmail) || !validEmail(linkedEmail)) return jsonResponse({ error: "Valid emails required" }, 400, headers);

  return handleMasterKeyLinkInternal(primaryEmail, linkedEmail, env, headers);
}

async function handleMasterKeyLinkInternal(primaryEmail, linkedEmail, env, headers) {
  const { sub, mk } = await getMasterKeyData(primaryEmail, env);
  
  if (!sub || sub.status !== "active") {
    return jsonResponse({ error: "No active subscription" }, 403, headers);
  }
  
  const maxSeats = sub.seats || 1;
  const normalizedLinked = linkedEmail.toLowerCase();
  const normalizedPrimary = primaryEmail.toLowerCase();
  
  // Check if already linked
  if (mk.linkedEmails.includes(normalizedLinked)) {
    return jsonResponse({ 
      ok: true, 
      message: "Already linked",
      linkedEmail: normalizedLinked,
      license: mk.licenses[normalizedLinked] || null,
      seatsUsed: mk.linkedEmails.length,
      seatsTotal: maxSeats,
    }, 200, headers);
  }
  
  // Check seat availability
  if (mk.linkedEmails.length >= maxSeats) {
    return jsonResponse({ 
      error: "All seats used", 
      seatsUsed: mk.linkedEmails.length, 
      seatsTotal: maxSeats,
      linkedEmails: mk.linkedEmails,
    }, 403, headers);
  }
  
  // Generate license for this linked email
  const license = await generateLicense(normalizedLinked, `mk_${crypto.randomUUID()}`, env.LICENSE_SIGNING_SECRET, "MasterKey");
  
  // Add to linked emails
  mk.linkedEmails.push(normalizedLinked);
  mk.licenses[normalizedLinked] = license;
  mk.lastUpdated = new Date().toISOString();
  
  await env.LICENSES.put(`master_key:${normalizedPrimary}`, JSON.stringify(mk));
  
  // Store reverse index so linked user can find their owner
  await env.LICENSES.put(`mk_linked:${normalizedLinked}`, normalizedPrimary);
  
  return jsonResponse({ 
    ok: true, 
    linkedEmail: normalizedLinked,
    license,
    seatsUsed: mk.linkedEmails.length,
    seatsTotal: maxSeats,
    linkedEmails: mk.linkedEmails,
  }, 200, headers);
}

async function handleMasterKeyUnlink(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { primaryEmail, linkedEmail } = body;
  if (!validEmail(primaryEmail) || !validEmail(linkedEmail)) return jsonResponse({ error: "Valid emails required" }, 400, headers);

  const { sub, mk } = await getMasterKeyData(primaryEmail, env);
  
  if (!sub) {
    return jsonResponse({ error: "No subscription" }, 403, headers);
  }
  
  const normalizedLinked = linkedEmail.toLowerCase();
  const normalizedPrimary = primaryEmail.toLowerCase();
  const idx = mk.linkedEmails.indexOf(normalizedLinked);
  
  if (idx === -1) {
    return jsonResponse({ error: "Email not linked", linkedEmails: mk.linkedEmails }, 404, headers);
  }
  
  // Remove from linked emails
  mk.linkedEmails.splice(idx, 1);
  delete mk.licenses[normalizedLinked];
  mk.lastUpdated = new Date().toISOString();
  
  await env.LICENSES.put(`master_key:${normalizedPrimary}`, JSON.stringify(mk));
  
  // Remove reverse index
  await env.LICENSES.delete(`mk_linked:${normalizedLinked}`);
  
  return jsonResponse({ 
    ok: true, 
    unlinkedEmail: normalizedLinked,
    seatsUsed: mk.linkedEmails.length,
    seatsTotal: sub.seats || 1,
    linkedEmails: mk.linkedEmails,
  }, 200, headers);
}

async function handleMasterKeyReset(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }

  const { primaryEmail } = body;
  if (!validEmail(primaryEmail)) return jsonResponse({ error: "Valid email required" }, 400, headers);

  const { sub, mk } = await getMasterKeyData(primaryEmail, env);
  if (!sub) return jsonResponse({ error: "No subscription" }, 404, headers);

  // Clear all linked emails
  mk.linkedEmails = [];
  mk.licenses = {};
  mk.status = "reset";
  mk.resetAt = new Date().toISOString();
  
  await env.LICENSES.put(`master_key:${primaryEmail.toLowerCase()}`, JSON.stringify(mk));
  return jsonResponse({ ok: true, message: "All seats cleared" }, 200, headers);
}

async function handleMasterKeyStatus(email, env, headers) {
  if (!validEmail(email)) return jsonResponse({ error: "Valid email required" }, 400, headers);
  
  const { sub, mk } = await getMasterKeyData(email, env);
  
  if (!sub) {
    return jsonResponse({ hasMasterKey: false }, 200, headers);
  }
  
  return jsonResponse({ 
    hasMasterKey: true, 
    status: sub.status,
    seatsTotal: sub.seats || 1,
    seatsUsed: mk.linkedEmails.length,
    seatsAvailable: (sub.seats || 1) - mk.linkedEmails.length,
    linkedEmails: mk.linkedEmails,
    licenses: mk.licenses,
    subscriptionId: sub.subscriptionId,
    created: sub.created,
  }, 200, headers);
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
  const meta = extractRequestMeta(request);
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
      loginCount: 1,
      firstIp: meta.ip,
      firstCountry: meta.country,
      firstCity: meta.city,
      firstRegion: meta.region,
      firstUserAgent: meta.userAgent,
      lastIp: meta.ip,
      lastCountry: meta.country,
      lastCity: meta.city,
      lastRegion: meta.region,
      lastTimezone: meta.timezone,
      lastUserAgent: meta.userAgent,
    }));
    const cnt = await env.LICENSES.get("hub_meta:user_count") || "0";
    await env.LICENSES.put("hub_meta:user_count", String(parseInt(cnt) + 1));
  } else {
    const user = JSON.parse(existingUser);
    user.lastSeen = now;
    user.loginCount = (user.loginCount || 0) + 1;
    user.lastIp = meta.ip;
    user.lastCountry = meta.country;
    user.lastCity = meta.city;
    user.lastRegion = meta.region;
    user.lastTimezone = meta.timezone;
    user.lastUserAgent = meta.userAgent;
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

  const meta = extractRequestMeta(request);
  const key = `hub_user:${email.toLowerCase()}`;
  const existing = await env.LICENSES.get(key);
  const now = new Date().toISOString();
  
  let user = existing ? JSON.parse(existing) : {
    email: email.toLowerCase(),
    name: "",
    provider: "",
    picture: "",
    created: now,
    verified: false,
    loginCount: 0,
    firstIp: meta.ip,
    firstCountry: meta.country,
    firstCity: meta.city,
    firstRegion: meta.region,
    firstUserAgent: meta.userAgent,
  };
  
  // Update on every login
  user.lastSeen = now;
  user.loginCount = (user.loginCount || 0) + 1;
  user.lastIp = meta.ip;
  user.lastCountry = meta.country;
  user.lastCity = meta.city;
  user.lastRegion = meta.region;
  user.lastTimezone = meta.timezone;
  user.lastUserAgent = meta.userAgent;
  
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
    // Accept all valid products (ProEQ8, MasterKey, or any Hub product)
    if (!payload.product || !payload.license_id) return null;
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

async function sendProductEmail(email, productName, license, apiKey) {
  const html = `<h2>${productName} — License Activated</h2>
<p>Thank you for your purchase! Your license key:</p>
<pre style="background:#f4f4f4;padding:12px;border-radius:4px;word-break:break-all;font-size:11px;">${license}</pre>
<p>Your license is tied to your TizWildin HUB account (<strong>${email}</strong>). You can view all your products at <a href="https://garebear99.github.io/TizWildinEntertainmentHUB/pages/account.html">your account page</a>.</p>
<p>— TizWildin</p>`;
  await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: { Authorization: `Bearer ${apiKey}`, "Content-Type": "application/json" },
    body: JSON.stringify({ from: "TizWildin <onboarding@resend.dev>", to: [email], subject: `Your ${productName} License`, html }),
  });
}

// Weekly Newsletter System
const GIVEAWAY_TARGET = 1000;
const HUB_URL = "https://garebear99.github.io/TizWildinEntertainmentHUB/";
const SC_URL = "https://soundcloud.com/tizwildin";

async function getAllHubUsers(env) {
  // List all hub_user: keys from KV
  const users = [];
  let cursor = null;
  do {
    const list = await env.LICENSES.list({ prefix: "hub_user:", cursor, limit: 1000 });
    for (const key of list.keys) {
      const data = await env.LICENSES.get(key.name);
      if (data) {
        try {
          const user = JSON.parse(data);
          users.push(user);
        } catch {}
      }
    }
    cursor = list.cursor;
  } while (cursor);
  return users;
}

function buildNewsletterHtml(totalUsers) {
  const remaining = Math.max(0, GIVEAWAY_TARGET - totalUsers);
  const progress = Math.min(100, Math.floor((totalUsers / GIVEAWAY_TARGET) * 100));
  const progressBar = remaining > 0
    ? `<div style="background:#eee;border-radius:8px;height:24px;margin:16px 0;"><div style="background:linear-gradient(90deg,#6366f1,#8b5cf6);height:100%;border-radius:8px;width:${progress}%;"></div></div>`
    : `<div style="background:#22c55e;border-radius:8px;height:24px;margin:16px 0;text-align:center;color:#fff;line-height:24px;">🎉 TARGET REACHED!</div>`;

  return `
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><meta name="viewport" content="width=device-width"></head>
<body style="font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;max-width:600px;margin:0 auto;padding:20px;color:#1a1a2e;">
  <h1 style="color:#6366f1;">🎛️ TizWildin Weekly</h1>
  <p>Hey! Thanks for being part of the TizWildin community.</p>
  
  <hr style="border:none;border-top:1px solid #e5e5e5;margin:24px 0;">
  
  <h2 style="color:#1a1a2e;">📢 Help Us Grow</h2>
  <p>We're building free professional audio plugins, sample packs, MIDI tools, games, and a full creator ecosystem.</p>
  <p>Share the HUB with a friend:</p>
  <p>
    <a href="${HUB_URL}" style="color:#6366f1;">🌐 TizWildin HUB</a><br>
    <a href="${SC_URL}" style="color:#6366f1;">🔊 SoundCloud</a>
  </p>
  
  <hr style="border:none;border-top:1px solid #e5e5e5;margin:24px 0;">
  
  <h2 style="color:#1a1a2e;">🎁 1,000 User Giveaway</h2>
  <p><strong>Current:</strong> ${totalUsers} users &nbsp;|&nbsp; <strong>Target:</strong> ${GIVEAWAY_TARGET}</p>
  ${progressBar}
  <p>${remaining > 0 ? `Only <strong>${remaining}</strong> more to go! When we hit 1,000, giveaways begin — free plugin upgrades, exclusive sample packs, and more.` : `<strong>We hit ${GIVEAWAY_TARGET}!</strong> Giveaways are now LIVE. Check the HUB for details.`}</p>
  
  <hr style="border:none;border-top:1px solid #e5e5e5;margin:24px 0;">
  
  <h2 style="color:#1a1a2e;">📻 Coming Soon: 24/7 Radio</h2>
  <p>A 24/7 live radio stream is coming to YouTube with continuous music, song submissions, and badge-gated daily submissions for supporters.</p>
  
  <hr style="border:none;border-top:1px solid #e5e5e5;margin:24px 0;">
  
  <p style="color:#666;font-size:14px;">Free plugins. Free packs. Real DSP. Open source.<br>Great sound shouldn't cost anything.</p>
  <p style="color:#666;font-size:14px;">— Gary Doman (GareBear99 / TizWildin)</p>
  <p style="color:#999;font-size:12px;">Reply "unsubscribe" to stop these emails.</p>
</body>
</html>`;
}

function buildAdminDigestHtml(users) {
  const now = new Date().toISOString();
  
  // Country breakdown
  const countryStats = {};
  users.forEach(u => {
    const c = u.lastCountry || u.firstCountry || "XX";
    countryStats[c] = (countryStats[c] || 0) + 1;
  });
  const topCountries = Object.entries(countryStats)
    .sort((a, b) => b[1] - a[1])
    .slice(0, 10)
    .map(([c, n]) => `${c}: ${n}`)
    .join(", ");
  
  // Provider breakdown
  const providerStats = {};
  users.forEach(u => {
    const p = u.provider || "unknown";
    providerStats[p] = (providerStats[p] || 0) + 1;
  });
  const providerBreakdown = Object.entries(providerStats)
    .sort((a, b) => b[1] - a[1])
    .map(([p, n]) => `${p}: ${n}`)
    .join(", ");
  
  // Active users (logged in last 7 days)
  const weekAgo = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString();
  const activeLastWeek = users.filter(u => (u.lastSeen || "") >= weekAgo).length;
  
  const rows = users
    .sort((a, b) => (b.created || "").localeCompare(a.created || ""))
    .map(u => {
      const location = [u.lastCity, u.lastRegion, u.lastCountry].filter(Boolean).join(", ") || "—";
      const ua = (u.lastUserAgent || "").slice(0, 50) + ((u.lastUserAgent || "").length > 50 ? "..." : "");
      return `<tr>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;">${u.email}</td>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;">${u.provider || "—"}</td>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;">${u.name || "—"}</td>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;">${location}</td>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;">${u.loginCount || 1}</td>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;">${(u.created || "").slice(0, 10)}</td>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;">${(u.lastSeen || "").slice(0, 10)}</td>
        <td style="padding:4px 8px;border-bottom:1px solid #eee;font-size:10px;color:#666;">${u.lastIp || "—"}</td>
      </tr>`;
    })
    .join("");

  return `
<!DOCTYPE html>
<html>
<head><meta charset="utf-8"></head>
<body style="font-family:monospace;padding:20px;">
  <h2>TizWildin HUB — Admin Subscriber Report</h2>
  <p>Generated: ${now}</p>
  <p><strong>Total users: ${users.length}</strong> | Active last 7 days: ${activeLastWeek}</p>
  <p>Giveaway target: ${GIVEAWAY_TARGET} | Remaining: ${Math.max(0, GIVEAWAY_TARGET - users.length)}</p>
  <hr>
  <h3>📊 Stats</h3>
  <p><strong>Top Countries:</strong> ${topCountries}</p>
  <p><strong>Providers:</strong> ${providerBreakdown}</p>
  <hr>
  <h3>👥 Full User List</h3>
  <table style="border-collapse:collapse;width:100%;font-size:11px;">
    <tr style="background:#f4f4f4;">
      <th style="padding:6px;text-align:left;">Email</th>
      <th style="padding:6px;text-align:left;">Provider</th>
      <th style="padding:6px;text-align:left;">Name</th>
      <th style="padding:6px;text-align:left;">Location</th>
      <th style="padding:6px;text-align:left;">Logins</th>
      <th style="padding:6px;text-align:left;">Joined</th>
      <th style="padding:6px;text-align:left;">Last Seen</th>
      <th style="padding:6px;text-align:left;">IP</th>
    </tr>
    ${rows}
  </table>
</body>
</html>`;
}

async function sendWeeklyNewsletter(env) {
  const users = await getAllHubUsers(env);
  if (users.length === 0) return { sent: 0 };
  
  // Stop sending newsletters once we pass 1000 users
  if (users.length >= GIVEAWAY_TARGET) {
    // Still send but could add a flag to stop entirely if desired
  }

  const html = buildNewsletterHtml(users.length);
  const emails = users.map(u => u.email).filter(e => e && e.includes("@") && !e.endsWith("@soundcloud.local"));
  
  // Resend supports up to 100 recipients per call in batch mode
  // For larger lists, we send individually (Resend free tier is 100/day, paid is higher)
  let sent = 0;
  const subject = `🎛️ TizWildin Weekly — ${new Date().toISOString().slice(0, 10)}`;
  
  for (const email of emails) {
    try {
      await fetch("https://api.resend.com/emails", {
        method: "POST",
        headers: { Authorization: `Bearer ${env.RESEND_API_KEY}`, "Content-Type": "application/json" },
        body: JSON.stringify({
          from: "TizWildin <onboarding@resend.dev>",
          to: [email],
          subject,
          html,
        }),
      });
      sent++;
    } catch {}
  }
  return { sent, total: emails.length };
}

async function sendAdminSubscriberDigest(env) {
  const users = await getAllHubUsers(env);
  const html = buildAdminDigestHtml(users);
  const subject = `TizWildin Admin Digest — ${new Date().toISOString().slice(0, 10)} (${users.length} users)`;
  
  await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: { Authorization: `Bearer ${env.RESEND_API_KEY}`, "Content-Type": "application/json" },
    body: JSON.stringify({
      from: "TizWildin Admin <onboarding@resend.dev>",
      to: [ADMIN],
      subject,
      html,
    }),
  });
}

// Admin endpoint: Get subscriber list (protected by simple check)
async function handleAdminSubscribers(request, env, headers) {
  const url = new URL(request.url);
  const adminKey = url.searchParams.get("key");
  // Simple protection — require RESEND_API_KEY as admin key (you can change this)
  if (adminKey !== env.RESEND_API_KEY?.slice(0, 16)) {
    return jsonResponse({ error: "Unauthorized" }, 401, headers);
  }
  
  const users = await getAllHubUsers(env);
  
  // Country breakdown
  const countryStats = {};
  users.forEach(u => {
    const c = u.lastCountry || u.firstCountry || "XX";
    countryStats[c] = (countryStats[c] || 0) + 1;
  });
  
  // Active users (logged in last 7 days)
  const weekAgo = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString();
  const activeLastWeek = users.filter(u => (u.lastSeen || "") >= weekAgo).length;
  
  return jsonResponse({
    total: users.length,
    activeLastWeek,
    giveawayTarget: GIVEAWAY_TARGET,
    remaining: Math.max(0, GIVEAWAY_TARGET - users.length),
    countryBreakdown: countryStats,
    users: users.map(u => ({
      email: u.email,
      name: u.name,
      provider: u.provider,
      created: u.created,
      lastSeen: u.lastSeen,
      loginCount: u.loginCount || 1,
      location: {
        country: u.lastCountry || u.firstCountry,
        city: u.lastCity || u.firstCity,
        region: u.lastRegion || u.firstRegion,
        timezone: u.lastTimezone,
      },
      lastIp: u.lastIp,
      userAgent: u.lastUserAgent,
    })),
  }, 200, headers);
}

// Admin endpoint: Manually trigger newsletter (protected)
async function handleManualNewsletter(request, env, headers) {
  let body;
  try { body = await parseBody(request); } catch (e) { return jsonResponse({ error: e.message }, 400, headers); }
  
  const adminKey = body.key;
  if (adminKey !== env.RESEND_API_KEY?.slice(0, 16)) {
    return jsonResponse({ error: "Unauthorized" }, 401, headers);
  }
  
  const newsletterResult = await sendWeeklyNewsletter(env);
  await sendAdminSubscriberDigest(env);
  return jsonResponse({ ok: true, ...newsletterResult }, 200, headers);
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
