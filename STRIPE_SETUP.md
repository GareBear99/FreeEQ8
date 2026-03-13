# ProEQ8 Stripe + License Server Setup

Complete guide to deploying the ProEQ8 payment and license delivery system.

## Architecture

```
Customer → Stripe Checkout → Payment → Stripe Webhook → Cloudflare Worker
                                                            ↓
                                                    Generate HMAC-signed
                                                      license key
                                                            ↓
                                                    Email via Resend
                                                            ↓
                                            Customer pastes key in ProEQ8
                                                            ↓
                                            LicenseValidator.h verifies
                                              HMAC-SHA256 signature
```

## Prerequisites

- [Stripe account](https://stripe.com) (test mode is fine for development)
- [Cloudflare account](https://cloudflare.com) (free tier works)
- [Resend account](https://resend.com) (free tier: 100 emails/day)
- Node.js 18+ and npm
- `wrangler` CLI (installed via `npm install` in `server/`)

## Step 1: Create Stripe Product

1. Go to [Stripe Dashboard → Products](https://dashboard.stripe.com/products)
2. Click **+ Add Product**
3. Set:
   - **Name**: ProEQ8
   - **Description**: Professional 24-Band Parametric EQ Plugin
   - **Price**: $20.00 (one-time)
4. Save and copy the **Price ID** (starts with `price_...`)

## Step 2: Generate Signing Secret

Generate a random secret that will be shared between the server and the plugin:

```bash
openssl rand -hex 32
```

Save this value — you'll need it in both places:
- Server: `LICENSE_SIGNING_SECRET` env var
- Plugin: `LicenseValidator.h` → `licenseSigningSecret` constant

## Step 3: Update Plugin Signing Secret

Edit `Source/LicenseValidator.h` line 73-74:

```cpp
static constexpr const char* licenseSigningSecret =
    "your-64-char-hex-secret-from-step-2";
```

Rebuild both FreeEQ8 and ProEQ8 after this change.

## Step 4: Deploy Cloudflare Worker

```bash
cd server
npm install

# Set secrets (you'll be prompted for values)
wrangler secret put STRIPE_SECRET_KEY      # sk_test_... or sk_live_...
wrangler secret put STRIPE_WEBHOOK_SECRET  # (set after Step 5)
wrangler secret put STRIPE_PRICE_ID        # price_... from Step 1
wrangler secret put RESEND_API_KEY         # re_... from resend.com
wrangler secret put LICENSE_SIGNING_SECRET # hex string from Step 2

# Deploy
npm run deploy
```

Note the deployed URL (e.g., `https://proeq8-license-server.<your-subdomain>.workers.dev`).

## Step 5: Configure Stripe Webhook

1. Go to [Stripe Dashboard → Webhooks](https://dashboard.stripe.com/webhooks)
2. Click **+ Add endpoint**
3. Set:
   - **Endpoint URL**: `https://proeq8-license-server.<your-subdomain>.workers.dev/webhook/stripe`
   - **Events to send**: `checkout.session.completed`
4. Save and copy the **Signing secret** (starts with `whsec_...`)
5. Set the webhook secret on the worker:

```bash
cd server
wrangler secret put STRIPE_WEBHOOK_SECRET  # paste whsec_... value
```

## Step 6: Configure Resend

1. Go to [Resend Dashboard](https://resend.com/domains)
2. Add and verify your domain (e.g., `tizwildin.com`)
3. Create an API key and save it
4. Update the `from` address in `stripe-webhook.js` if needed (currently `noreply@tizwildin.com`)

## Step 7: Test End-to-End

### Test the health endpoint:
```bash
curl https://proeq8-license-server.<your-subdomain>.workers.dev/health
# → {"status":"ok","version":"1.1.0"}
```

### Test checkout creation:
```bash
curl -X POST https://proeq8-license-server.<your-subdomain>.workers.dev/create-checkout
# → {"url":"https://checkout.stripe.com/...","id":"cs_test_..."}
```

### Test with Stripe CLI (webhook):
```bash
stripe listen --forward-to https://proeq8-license-server.<your-subdomain>.workers.dev/webhook/stripe
stripe trigger checkout.session.completed
```

### Test license activation in plugin:
1. Complete a test purchase through the checkout URL
2. Check your email for the license key
3. Open ProEQ8 in your DAW
4. Click **Activate** → paste the key → confirm

## Going Live

1. Switch Stripe to live mode
2. Update `STRIPE_SECRET_KEY` to your live key (`sk_live_...`)
3. Create a new webhook endpoint for live mode and update `STRIPE_WEBHOOK_SECRET`
4. Update `STRIPE_PRICE_ID` to your live price ID
5. Redeploy: `npm run deploy`

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/health` | Health check |
| POST | `/create-checkout` | Create Stripe Checkout session → returns `{ url, id }` |
| POST | `/webhook/stripe` | Stripe webhook → generates license → emails customer |

## Troubleshooting

- **Worker logs**: `cd server && npm run tail`
- **Stripe events**: Dashboard → Developers → Events
- **Email delivery**: Resend Dashboard → Emails
- **License validation fails**: Ensure `LICENSE_SIGNING_SECRET` matches exactly between server and `LicenseValidator.h`
