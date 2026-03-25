/**
 * Integration test for the ProEQ8 license activation server.
 *
 * Tests the /activate, /deactivate, and /health endpoints using
 * direct function calls (no wrangler needed).
 *
 * Run: node test-activation.js
 */

// We test the logic directly by simulating the Worker environment.
// This avoids needing wrangler dev running.

const TEST_SIGNING_SECRET = "test-secret-key-for-testing-only-32chars!!";

// ── Helpers (replicate the server's crypto using Node.js) ────────

async function generateTestLicense(email, licenseId) {
  const { subtle } = globalThis.crypto || (await import("node:crypto")).webcrypto;

  const payload = JSON.stringify({
    product: "ProEQ8",
    email: email,
    license_id: licenseId,
    expires: "2099-12-31",
    issued: new Date().toISOString(),
  });

  const payloadB64 = Buffer.from(payload).toString("base64");

  const key = await subtle.importKey(
    "raw",
    new TextEncoder().encode(TEST_SIGNING_SECRET),
    { name: "HMAC", hash: "SHA-256" },
    false,
    ["sign"]
  );

  const sigBytes = await subtle.sign("HMAC", key, new TextEncoder().encode(payloadB64));
  const sigB64 = Buffer.from(sigBytes).toString("base64");

  return `${payloadB64}.${sigB64}`;
}

function generateBadLicense() {
  const payload = JSON.stringify({
    product: "ProEQ8",
    email: "bad@test.com",
    license_id: "bad-id",
    expires: "2099-12-31",
  });
  const payloadB64 = Buffer.from(payload).toString("base64");
  return `${payloadB64}.INVALIDSIGNATURE`;
}

// ── Mock KV Store ────────────────────────────────────────────────

class MockKV {
  constructor() {
    this.store = new Map();
  }
  async get(key) {
    return this.store.get(key) || null;
  }
  async put(key, value, opts) {
    this.store.set(key, value);
  }
  async delete(key) {
    this.store.delete(key);
  }
  dump() {
    const obj = {};
    for (const [k, v] of this.store) obj[k] = v;
    return obj;
  }
}

// ── Import the worker module ─────────────────────────────────────

let worker;
async function loadWorker() {
  worker = await import("./stripe-webhook.js");
}

function makeRequest(method, path, body) {
  const url = `http://localhost${path}`;
  const init = { method };
  if (body) {
    init.body = JSON.stringify(body);
    init.headers = { "Content-Type": "application/json" };
  }
  return new Request(url, init);
}

// ── Tests ────────────────────────────────────────────────────────

let passed = 0;
let failed = 0;

function assert(condition, message) {
  if (condition) {
    console.log(`  ✅ ${message}`);
    passed++;
  } else {
    console.log(`  ❌ ${message}`);
    failed++;
  }
}

async function runTests() {
  await loadWorker();

  const kv = new MockKV();
  const env = {
    LICENSES: kv,
    LICENSE_SIGNING_SECRET: TEST_SIGNING_SECRET,
    MAX_DEVICES_PER_LICENSE: "2",
    STRIPE_SECRET_KEY: "sk_test_fake",
    STRIPE_PRICE_ID: "price_fake",
    STRIPE_WEBHOOK_SECRET: "whsec_fake",
    RESEND_API_KEY: "re_fake",
    SUCCESS_URL: "https://example.com/success",
    CANCEL_URL: "https://example.com/cancel",
  };

  // ── Test 1: Health check ──
  console.log("\n── Test 1: GET /health ──");
  {
    const req = makeRequest("GET", "/health");
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 200, "Status 200");
    assert(data.status === "ok", "Status field is 'ok'");
    assert(data.version === "2.0.0", "Version is 2.0.0");
  }

  // ── Test 2: 404 for unknown routes ──
  console.log("\n── Test 2: GET /unknown ──");
  {
    const req = makeRequest("GET", "/unknown");
    const resp = await worker.default.fetch(req, env);
    assert(resp.status === 404, "Returns 404");
  }

  // ── Test 3: Activate with valid key ──
  console.log("\n── Test 3: Activate device 1 ──");
  const licenseId = "test-license-001";
  const licenseKey = await generateTestLicense("user@test.com", licenseId);

  // Pre-seed the KV with a license record (simulating webhook flow)
  await kv.put(
    licenseId,
    JSON.stringify({
      email: "user@test.com",
      max_uses: 2,
      used: 0,
      devices: [],
      created: new Date().toISOString(),
    })
  );

  {
    const req = makeRequest("POST", "/activate", {
      license_key: licenseKey,
      device_id: "device-aaa",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 200, "Status 200");
    assert(data.ok === true, "ok is true");
    assert(data.used === 1, "Used count is 1");
    assert(data.max === 2, "Max is 2");
  }

  // ── Test 4: Activate device 2 ──
  console.log("\n── Test 4: Activate device 2 ──");
  {
    const req = makeRequest("POST", "/activate", {
      license_key: licenseKey,
      device_id: "device-bbb",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 200, "Status 200");
    assert(data.ok === true, "ok is true");
    assert(data.used === 2, "Used count is 2");
  }

  // ── Test 5: Re-activate same device (should succeed) ──
  console.log("\n── Test 5: Re-activate device 1 (idempotent) ──");
  {
    const req = makeRequest("POST", "/activate", {
      license_key: licenseKey,
      device_id: "device-aaa",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 200, "Status 200");
    assert(data.ok === true, "ok is true");
    assert(data.message === "Device already activated", "Already activated message");
  }

  // ── Test 6: Activate device 3 (should fail - limit reached) ──
  console.log("\n── Test 6: Activate device 3 (limit reached) ──");
  {
    const req = makeRequest("POST", "/activate", {
      license_key: licenseKey,
      device_id: "device-ccc",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 403, "Status 403");
    assert(data.error === "Activation limit reached", "Limit reached error");
    assert(data.used === 2, "Used is still 2");
  }

  // ── Test 7: Deactivate device 1 ──
  console.log("\n── Test 7: Deactivate device 1 ──");
  {
    const req = makeRequest("POST", "/deactivate", {
      license_key: licenseKey,
      device_id: "device-aaa",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 200, "Status 200");
    assert(data.ok === true, "ok is true");
    assert(data.used === 1, "Used count dropped to 1");
    assert(data.message === "Device deactivated", "Deactivated message");
  }

  // ── Test 8: Now device 3 can activate ──
  console.log("\n── Test 8: Activate device 3 (now has room) ──");
  {
    const req = makeRequest("POST", "/activate", {
      license_key: licenseKey,
      device_id: "device-ccc",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 200, "Status 200");
    assert(data.ok === true, "ok is true");
    assert(data.used === 2, "Used count is 2 again");
  }

  // ── Test 9: Invalid license key ──
  console.log("\n── Test 9: Activate with invalid key ──");
  {
    const badKey = generateBadLicense();
    const req = makeRequest("POST", "/activate", {
      license_key: badKey,
      device_id: "device-zzz",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 401, "Status 401");
    assert(data.error === "Invalid license key", "Invalid key error");
  }

  // ── Test 10: Missing fields ──
  console.log("\n── Test 10: Activate with missing fields ──");
  {
    const req = makeRequest("POST", "/activate", {
      license_key: licenseKey,
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 400, "Status 400");
    assert(data.error.includes("Missing"), "Missing field error");
  }

  // ── Test 11: Deactivate non-existent device ──
  console.log("\n── Test 11: Deactivate device not in list ──");
  {
    const req = makeRequest("POST", "/deactivate", {
      license_key: licenseKey,
      device_id: "device-nonexistent",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 200, "Status 200");
    assert(data.message === "Device was not activated", "Not activated message");
  }

  // ── Test 12: CORS preflight ──
  console.log("\n── Test 12: OPTIONS preflight ──");
  {
    const req = makeRequest("OPTIONS", "/activate");
    const resp = await worker.default.fetch(req, env);
    assert(resp.status === 204, "Status 204");
    assert(resp.headers.get("Access-Control-Allow-Origin") === "*", "CORS header present");
    assert(resp.headers.get("Access-Control-Allow-Methods").includes("POST"), "Allows POST");
  }

  // ── Test 13: License not found in KV ──
  console.log("\n── Test 13: Activate with key for missing license record ──");
  {
    const orphanKey = await generateTestLicense("orphan@test.com", "nonexistent-license-id");
    const req = makeRequest("POST", "/activate", {
      license_key: orphanKey,
      device_id: "device-orphan",
    });
    const resp = await worker.default.fetch(req, env);
    const data = await resp.json();
    assert(resp.status === 404, "Status 404");
    assert(data.error === "License not found", "License not found error");
  }

  // ── Summary ──
  console.log(`\n${"═".repeat(50)}`);
  console.log(`  ${passed} passed, ${failed} failed`);
  console.log(`${"═".repeat(50)}\n`);

  process.exit(failed > 0 ? 1 : 0);
}

runTests().catch((e) => {
  console.error("Test runner error:", e);
  process.exit(1);
});
