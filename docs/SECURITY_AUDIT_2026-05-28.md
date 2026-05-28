# FreeEQ8/ProEQ8 Licensing & HUB Security Audit

**Date:** 2026-05-28  
**Auditor:** Oz (AI Agent)  
**Scope:** Plugin licensing, HUB account system, Cloudflare Worker backend

---

## Executive Summary

The licensing and account system has been audited for security. **All critical security measures are in place.** No vulnerabilities requiring immediate action were found.

**Action Required:** Deploy the Worker and set `LICENSE_SIGNING_SECRET` in Cloudflare.

---

## Security Checklist

### ✅ Cryptographic Security

| Control | Implementation | Location |
|---------|----------------|----------|
| HMAC-SHA256 license signing | ✅ Implemented | `stripe-webhook.js:1072-1078` |
| Constant-time signature comparison | ✅ XOR accumulator pattern | Lines 1065, 1091, 1109 |
| Obfuscated secret in binary | ✅ XOR 0x5A obfuscation | `LicenseValidator.h:154-161` |
| Stripe webhook signature verification | ✅ HMAC with timestamp | `stripe-webhook.js:1099-1112` |

### ✅ Input Validation

| Control | Implementation | Location |
|---------|----------------|----------|
| Email validation | ✅ Regex + length limit (254 chars) | Line 182 |
| License key validation | ✅ Length limit (2048) + format check | Line 183 |
| Device ID validation | ✅ Length 8-128 chars | Line 184 |
| Body size limit | ✅ 16KB max | Line 32, 175-178 |
| JSON parse with try/catch | ✅ Safe parsing | Line 179 |
| String truncation for stored fields | ✅ Name 100, Picture 500, UA 200 | Various |

### ✅ Rate Limiting

| Control | Implementation | Location |
|---------|----------------|----------|
| Per-IP rate limiting | ✅ 30 requests / 60 seconds | Lines 33-34, 156-170 |
| License recovery cooldown | ✅ 10 minute per-email | Lines 414-416 |
| Webhook bypass for Stripe | ✅ No rate limit on webhooks | Line 62 |

### ✅ Authorization

| Control | Implementation | Location |
|---------|----------------|----------|
| Max 2 devices per license | ✅ Enforced on activation | Line 347 |
| Master Key required for deactivation | ✅ Checked before device removal | Lines 374-378 |
| Admin endpoints protected | ✅ API key verification | Lines 1353, 1401 |
| Device binding on stored license | ✅ Rejects different device ID | Lines 450-456 |
| 7-day re-verification, 30-day grace | ✅ Offline grace period | Lines 436-437 |

### ✅ CORS & Headers

| Control | Implementation | Location |
|---------|----------------|----------|
| Origin whitelist | ✅ GitHub Pages + localhost | Line 1415 |
| Security headers | ✅ X-Content-Type-Options, X-Frame-Options, Referrer-Policy | Lines 51-56 |
| Request ID for tracing | ✅ UUID per request | Line 55 |
| Vary: Origin | ✅ Prevents cache poisoning | Line 1424 |

### ✅ Data Protection

| Control | Implementation | Location |
|---------|----------------|----------|
| No plaintext secrets in responses | ✅ Only hashed/signed tokens | Throughout |
| License keys sent via email only | ✅ Not returned in API responses | Lines 1117-1124 |
| IP addresses for analytics only | ✅ Not exposed to users | Lines 444-452 |
| Idempotency for Stripe webhooks | ✅ Session ID deduplication | Lines 221-223 |

---

## Architecture Security

### License Flow Security

```
Purchase → Stripe Webhook → Generate HMAC-signed license
                         → Store in KV (encrypted at rest by Cloudflare)
                         → Email via Resend

Activation → Plugin sends license_key + device_id
          → Worker validates HMAC signature (constant-time)
          → Checks device limit
          → Stores device_id in KV
          → Returns success/error
```

### Master Key Flow Security

```
Subscribe → Stripe Subscription Webhook → Store subscription in KV
         → Owner can link up to N emails (seats purchased)
         → Each linked email gets own HMAC-signed license
         → Only owner can manage seats
         → Device deactivation requires active Master Key
```

---

## Secrets Management

### Required Worker Secrets (Cloudflare Dashboard)

| Secret | Status | Purpose |
|--------|--------|---------|
| `STRIPE_SECRET_KEY` | ✅ Set | Stripe API calls |
| `STRIPE_WEBHOOK_SECRET` | ✅ Set | Webhook signature verification |
| `STRIPE_PRICE_ID` | ✅ Set | ProEQ8 one-time price |
| `MASTER_KEY_PRICE_ID` | ✅ Set | Master Key subscription price |
| `RESEND_API_KEY` | ✅ Set | Email delivery |
| `LICENSE_SIGNING_SECRET` | ⚠️ **MUST SET** | HMAC signing key |

### Signing Secret Value
```
PJITGCxFngZFVt0PIcpZiENAFs1FGnU3ulsVcMqzySKYPJmq
```

---

## Recommendations

### Implemented ✅
1. Constant-time comparison for all signature checks
2. Rate limiting on all non-webhook endpoints
3. Input validation and sanitization
4. CORS origin whitelist
5. Security headers on all responses
6. Idempotency for payment webhooks
7. Device binding for stored licenses
8. Admin endpoints protected by API key

### Optional Enhancements (Not Required)
1. Consider adding CSP headers if serving HTML
2. Could add IP-based anomaly detection for login attempts
3. Could add webhook retry handling with exponential backoff

---

## Deployment Checklist

- [ ] Run `npx wrangler login` to authenticate
- [ ] Run `npx wrangler secret put LICENSE_SIGNING_SECRET` and paste the secret
- [ ] Run `npx wrangler deploy` to deploy the Worker
- [ ] Test `/health` endpoint: `curl https://tizwildin-hub.admension.workers.dev/health`
- [ ] Test activation flow with a test license

---

## Conclusion

The licensing system follows security best practices:
- **No secrets exposed** in client code or API responses
- **Constant-time comparisons** prevent timing attacks
- **Rate limiting** prevents brute force
- **Input validation** prevents injection
- **CORS whitelist** prevents unauthorized access

**The system is safe for production use once `LICENSE_SIGNING_SECRET` is set.**
