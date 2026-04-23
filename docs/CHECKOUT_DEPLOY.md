# ProEQ8 Checkout — Deploy Guide

A Warp-quality three-page checkout flow for ProEQ8 ($20 USD), backed by:

- Google Identity Services for authentication (email only; no password)
- Stripe Checkout (hosted) for payment capture
- Cloudflare Worker at `server/stripe-webhook.js` for license issuance
- Resend for license-delivery email
- A public **transparency log** on `TizWildinEntertainmentHUB` that
  records a SHA-256 hash of every issued license (no PII) so anyone
  can audit that a purchase existed

## Files

```
server/pages/proeq8/
├── index.html        # landing + Google Sign-In + hero feature card
├── checkout.js       # Google → worker /api/checkout → Stripe
├── success.html      # polls worker /api/license-for-session until ready
└── styles.css        # dark Warp-style theme

server/stripe-webhook.js     # Cloudflare Worker (existing)
server/wrangler.toml         # Cloudflare Worker config (existing)
```

The three HTML pages are static and deploy to Cloudflare Pages
(recommended) or GitHub Pages. They talk only to the worker; card
data never touches these pages.

## One-time setup (what you need to do)

1. **Stripe account** — create a Product "ProEQ8" at $20 USD, one-time.
   Note its `price_id`.
2. **Stripe webhook endpoint** — add
   `https://proeq8-checkout.tizwildin.workers.dev/webhook/stripe` and
   subscribe to:
    - `checkout.session.completed` (issues license)
    - `charge.refunded`            (revokes license)
    - `charge.dispute.closed`       (revokes license on lost dispute)
   Copy the signing secret.
3. **Google Cloud project** — enable Google Identity Services, create
   an OAuth 2.0 Client ID (type: "Web"), add authorized origin
   `https://proeq8.tizwildin.com`. Copy the Client ID.
4. **Resend** — verify the `tizwildin.com` sender domain. Create an
   API key.
5. **GitHub** — create a fine-grained PAT scoped to
   `GareBear99/TizWildinEntertainmentHUB` with write-access to
   `transparency/licenses.ndjson`.
6. **Cloudflare secrets** — from the `server/` directory:

    ```bash
    wrangler secret put STRIPE_SECRET_KEY
    wrangler secret put STRIPE_WEBHOOK_SECRET
    wrangler secret put STRIPE_PRICE_ID_PROEQ8
    wrangler secret put RESEND_API_KEY
    wrangler secret put LICENSE_SIGNING_SECRET
    wrangler secret put GOOGLE_CLIENT_ID
    wrangler secret put GITHUB_HUB_TOKEN
    wrangler secret put ALLOWED_ORIGINS   # e.g. https://proeq8.tizwildin.com,https://garebear99.github.io
    ```

7. **Replace the `__GOOGLE_CLIENT_ID__` placeholder** in
   `server/pages/proeq8/index.html` with your real client ID before
   publishing (or template it at build time).

## Deploy

Worker:

```bash
cd server
wrangler deploy
```

Static pages (Cloudflare Pages — zero-config):

```bash
wrangler pages deploy server/pages/proeq8 --project-name proeq8-checkout
```

Point `proeq8.tizwildin.com` (CNAME) at the Pages project. Done.

## Money-safety guarantees

- **Stripe is system-of-record.** Every payment has a Stripe event ID
  that is immutable and cryptographically attested.
- **Cloudflare KV** holds only derived state: license row +
  activation slots. No card data, no PII. KV writes are all keyed by
  `license_id` which is itself derived from the Stripe event ID.
- **Transparency log** on TizWildinEntertainmentHUB is append-only.
  Each successful license issue is recorded as a single line:

    ```
    {"ts":"2026-04-23T07:30:00Z","product":"ProEQ8","license_hash":"<sha256>","stripe_event":"evt_..."}
    ```

  No email, no name, no IP. Anyone auditing the repo can verify that
  N purchases were recorded and cross-check the Stripe event IDs.
- **Revocation** closes the loop: `charge.refunded` and
  `charge.dispute.closed` webhooks set `revoked:true` in the KV row.
  The plugin's next `/verify` call receives 403 and the license
  deactivates on the next 7-day check (or immediately if online).
- **CORS allowlist** restricts state-changing endpoints (`/api/checkout`,
  `/activate`, `/deactivate`) to the Pages origin + the plugin (which
  sends no Origin header, so it still passes).
- **Rate limit** on `/activate` and `/verify`: 60 req/10 min per
  license_id via KV counter with TTL.

## Plugin wiring

The in-plugin **Buy ProEQ8 — $20 USD** button in the License dialog
(and the upgrade banner that surfaces during demo-mute windows)
launches `https://proeq8.tizwildin.com/` via
`juce::URL::launchInDefaultBrowser`. No credentials or state flow
through the plugin at purchase time — the only state ever handed to
the plugin is the license key the user pastes back in.
