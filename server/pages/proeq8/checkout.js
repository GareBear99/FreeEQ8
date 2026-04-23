// ProEQ8 checkout page — client glue.
//
// Flow:
//   1. Google Identity Services calls onGoogleSignIn({ credential })
//      where `credential` is a JWT signed by Google containing the user's
//      email + name + picture.
//   2. We decode the (unverified) payload for UI display, then POST the
//      raw JWT to /api/checkout on the Cloudflare Worker. The worker
//      verifies the JWT against Google's public keys server-side before
//      creating a Stripe Checkout Session and returning the redirect URL.
//   3. We redirect the browser to Stripe-hosted Checkout. Card data never
//      touches our code.
//
// The worker URL is hard-coded here for production; change WORKER_ORIGIN
// for local wrangler dev.
(function () {
  "use strict";

  const WORKER_ORIGIN = "https://proeq8-checkout.tizwildin.workers.dev";

  function b64urlToJson(b64) {
    try {
      const pad = "=".repeat((4 - (b64.length % 4)) % 4);
      const json = atob(b64.replace(/-/g, "+").replace(/_/g, "/") + pad);
      return JSON.parse(decodeURIComponent(
        json.split("").map(c => "%" + ("00" + c.charCodeAt(0).toString(16)).slice(-2)).join("")
      ));
    } catch (e) {
      return null;
    }
  }

  function decodeGoogleJwt(credential) {
    const parts = credential.split(".");
    if (parts.length !== 3) return null;
    return b64urlToJson(parts[1]);
  }

  function setStatus(msg) {
    const el = document.getElementById("status-msg");
    if (el) el.textContent = msg;
  }

  async function startCheckout(googleJwt, claims) {
    setStatus("Creating secure Stripe Checkout session…");
    try {
      const r = await fetch(WORKER_ORIGIN + "/api/checkout", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          product_id: "ProEQ8",
          google_credential: googleJwt,
          // If the Google JWT can't be verified on the worker, the worker
          // will refuse. No client-side fallback.
          return_url: "https://proeq8.tizwildin.com/success.html?session_id={CHECKOUT_SESSION_ID}",
          cancel_url: "https://proeq8.tizwildin.com/"
        })
      });
      if (!r.ok) {
        const body = await r.text();
        throw new Error(`Worker returned ${r.status}: ${body}`);
      }
      const data = await r.json();
      if (!data || !data.checkout_url) throw new Error("No checkout_url in worker response");
      window.location.href = data.checkout_url;
    } catch (e) {
      console.error(e);
      setStatus("Could not start checkout — " + e.message + ". Try again or email support.");
    }
  }

  function renderSignedInCard(claims, credential) {
    const region = document.getElementById("signin-region");
    if (!region) return;
    const picture = claims.picture ? `<img src="${claims.picture}" alt="">` : "";
    region.innerHTML = `
      <div class="checkout-user">
        ${picture}
        <div>
          <div class="cu-name">${claims.name || "Signed in"}</div>
          <div class="cu-email">${claims.email || ""}</div>
        </div>
      </div>
      <button class="btn-primary" id="pay-btn">Continue to secure checkout — $20 USD</button>
      <div class="btn-ghost" id="status-msg">Stripe handles card data end-to-end. Your license key arrives at ${claims.email} within 60 seconds of payment.</div>
    `;
    document.getElementById("pay-btn").addEventListener("click", (ev) => {
      ev.currentTarget.classList.add("loading");
      ev.currentTarget.textContent = "Working…";
      startCheckout(credential, claims);
    });
  }

  // Google Identity Services invokes this as the sign-in callback.
  window.onGoogleSignIn = function onGoogleSignIn(response) {
    if (!response || !response.credential) return;
    const claims = decodeGoogleJwt(response.credential);
    if (!claims || !claims.email) {
      setStatus("Google sign-in did not return an email address. Try again.");
      return;
    }
    renderSignedInCard(claims, response.credential);
  };
})();
