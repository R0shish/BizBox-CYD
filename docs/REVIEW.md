# Code review: BizBox-CYD firmware

Scope: every file in the sketch (`esp.ino`, `model.h`, `ui.h`, `web.h`, `speaker.h`; `fonts.h` is generated data) and `tools/`.
Line numbers refer to the code **before** this review's fixes (commit `b3bfaaa`).
Library behaviour was checked against the real sources: arduino-esp32 2.0.17 and 3.3, TFT_eSPI 2.5.44, ArduinoJson 7.4, ESP8266SAM.

Severity:
- **critical**: device hangs, or anyone nearby can fake payments or read credentials.
- **high**: wrong money announced, payments missed or badly delayed, real data corrupted.
- **medium**: data loss, reliability or UX defects you hit in normal use.
- **low**: polish, dead code, hardening.

The **Status** column is updated as fixes land.

## Summary table

| ID | Sev | Where | Problem | Status |
|---|---|---|---|---|
| C1 | critical | esp.ino:433, 441–502 | IMAP reads can block for 100 minutes (core 2 `setTimeout` is in **seconds**), and `while (client.connected())` loops never exit on `NO`/`BAD` | open |
| C2 | critical | web.h:375–403, esp.ino:758–759 | `/api/payment` and `/api/order` have no auth, and they can even be hit cross-site from any web page | open |
| C3 | critical | web.h:244, 255, 104 | Gmail app password and admin PIN are written into the page; the setup hotspot bypasses the PIN | open |
| H0 | high | esp.ino:11 | Doesn't build on core 3.x: `#include "qrcode.h"` resolves to ESP-IDF's QR component, not ricmoo/QRCode (found by the new CI build) | open |
| H1 | high | esp.ino:484–485 | Only one unread email is handled per 15 s poll | open |
| H2 | high | esp.ino:224, 464, 498, 512–514 | Default `UNSEEN` marks *every* unread email as read and logs non-payments as orders; any number in any subject is announced | open |
| H3 | high | esp.ino:408–423 | `extractAmount()` takes the first number: dates, IDs and phone numbers get announced | open |
| H4 | high | esp.ino:425–517, 519–555, 847–850 | The whole poll (TLS handshake plus 5 round trips) runs on the UI loop: touch, web and the BOOT button freeze for seconds every 15 s | open |
| H5 | high | esp.ino:491–494 | Subjects are not MIME-decoded or unfolded, so encoded subjects (`=?UTF-8?B?…?=`) lose the amount | open |
| H6 | high | esp.ino:793–797, web.h:338–343 | The voice test is booked as a real sale and looks and sounds exactly like a real payment | open |
| H7 | high | esp.ino:769–770, 708–714 | If Wi-Fi isn't up at boot (router slower than the ESP after a power cut), the device stays in setup mode forever | open |
| H8 | high | web.h:338, 405–427, esp.ino:756, 760–761 | State-changing GET endpoints (`/test`, `/api/reset`, `/api/clear`) can be triggered by any web page (CSRF) | open |
| M1 | medium | web.h:389–398, esp.ino:387–403 | `announce()` runs inside the HTTP handler: callers wait about 5 s, retry, and announce twice | open |
| M2 | medium | web.h:380, 393 | `doc["amount"] \| 0`: negative amounts wrap to about 4.29 billion; `500.5` and `"500"` are silently dropped | open |
| M3 | medium | model.h:24, esp.ino:346 | Orders are lost on every reboot; `nextOrderId` restarts at 101, so IDs repeat | open |
| M4 | medium | esp.ino:347–352 | A full order list drops the oldest order even when it is unpaid | open |
| M5 | medium | web.h:158–280 | The dashboard is built as one ~20 KB `String` per request (fragmentation, can fail after TLS) | open |
| M6 | medium | ui.h:487 | `fillScreen()` on every change makes the whole panel flash on each tap | open |
| M7 | medium | esp.ino:160, 530, 538, 802 | Apps Script mode: `since` resets to `"0"` on boot and on every Save; a URL that already has a `?` breaks; a missing `latest` becomes `"null"` | open |
| M8 | medium | esp.ino:432, 527 | `setInsecure()` to Gmail: a MITM on the shop Wi-Fi gets the Gmail app password | open |
| M9 | medium | web.h:103–108 | No limit on PIN attempts: a 4-digit PIN can be brute-forced over Wi-Fi in minutes | open |
| M10 | medium | ui.h:61–70 | Every font switch frees, mallocs and re-parses the whole glyph table (dozens per redraw) | open |
| L1 | low | esp.ino:132–136 | Touch driver prints to Serial every 150 ms while touched | open |
| L2 | low | esp.ino:509–513, ui.h:101 | UTF-8 cut in the middle of a character (`substring(0,22)`, `remove(len-1)`) draws garbage glyphs | open |
| L3 | low | esp.ino:767 | `onNotFound` redirects to `192.168.4.1` even on the shop LAN | open |
| L4 | low | web.h:302, 306; esp.ino:218, 221 | `vmode` and `emode` are not range-checked | open |
| L5 | low | web.h:379–382 | `/api/order` accepts any status string, unlimited title length, and invalid JSON (which creates "New Order") | open |
| L6 | low | esp.ino:443 | IMAP `LOGIN` doesn't escape `"` or `\` | open |
| L7 | low | web.h:340 | `/test?amt=` overflows `int` | open |
| L8 | low | esp.ino:14–19, 157–158, 803; tools/check/stubs/AudioOutputI2S.h, driver/i2s.h | Dead code: `FONT_BIG`, `lastPollOk`, `timeSynced`, unused stubs | open |
| L9 | low | esp.ino:777, 839 | Double poll at boot (`setup()` polls, then the first `loop()` polls again) | open |
| L10 | low | web.h:100 | The dashboard polls `/api/state` every 5 s even in a background tab | open |
| L11 | low | ui.h:219–247 | The QR is re-encoded on every Pay redraw | open |
| L12 | low | esp.ino:599–602 | Tapping a PAID order cycles it back to NEW; marking PAID on screen doesn't count the sale while API-PAID does | open |
| L13 | low | web.h | Basic auth over plain HTTP, and no `Host` check (DNS rebinding) | open |

## Details

### C1 · critical · IMAP reads can hang for 100 minutes
`esp.ino:433` calls `client.setTimeout(6000)`. On core 2.x, `WiFiClientSecure::setTimeout(uint32_t seconds)` sets `_timeout = seconds * 1000` (WiFiClientSecure.cpp:378), so every `readStringUntil('\n')` waits up to **6000 s**. On core 3.x the call lands in `Stream::setTimeout(ms)` (6 s). On top of that, the read loops at lines 459, 468, 489 and 499 only break on `Axx OK`. A `NO`/`BAD` reply (a typo in the search filter is enough: `A03 BAD Could not parse command`) keeps them spinning for as long as Gmail keeps the socket open (up to 30 min), with a 6 s or 100 min wait on each empty read.
**Scenario:** the owner types `FROM esewa` (missing quotes are fine, but `UNSEEN FROM "esewa` isn't). From then on the device is frozen: no touch, no dashboard, and the BOOT-button factory reset doesn't work either, because `handleButton()` runs in the same loop.
**Fix:** a line reader with an absolute deadline (`imapReadLine`) and a session deadline. Every tagged reply ends the wait on `OK`/`NO`/`BAD`. The session runs off the UI task (see H4).

### C2 · critical · Anyone can make the box say "payment received"
`/api/payment` and `/api/order` skip `authed()`. `server.arg("plain")` is the raw body whatever the content type, so a plain HTML form with `enctype="text/plain"` on *any website* can produce a valid JSON body: `<input name='{"amount":5000,"note":"x' value='"}'>`.
**Scenario:** a customer on the shop Wi-Fi runs `curl -d '{"amount":5000}' bizbox.local/api/payment`, then walks out after the box says "rupees five thousand received". A malicious page opened on the shopkeeper's phone can do the same.
**Fix:** an API token (128-bit, generated on first boot, rotatable, shown on the dashboard), required as `Authorization: Bearer <token>` on every `/api/*`. Browsers can't add that header cross-site without a CORS preflight, which the device never approves, so this also closes the CSRF path.

### C3 · critical · Secrets in the HTML; setup hotspot skips the PIN
`value='" + esc(cfg.gmailPass) + "'` and `value='" + esc(cfg.adminPin) + "'` put both secrets in the page source. Without a PIN (the default), anyone on the Wi-Fi who opens `bizbox.local` gets a password with full access to the shop's Gmail. `authed()` returns true in `portalMode`, and portal mode is also entered when the router is down at boot (H7). The device then broadcasts an **open** hotspot serving that page.
**Fix:** secrets are never echoed. Password fields show "saved, leave blank to keep", plus an explicit "clear" checkbox for the PIN. The PIN is enforced in portal mode too (when one is set; a forgotten PIN is what the factory reset is for).

### H0 · high · No build on core 3.x
Core 3.x ships ESP-IDF's `esp_qrcode` component with its own `qrcode.h` on the include path, so the sketch got that header instead of ricmoo's, and `QRCode`, `qrcode_initText` and `ECC_LOW` were undefined. The CI added in this branch showed it first.
**Fix:** vendor ricmoo/QRCode (MIT, two files) as `qr_encode.h/.c`. On core 2, ESP8266Audio 2.x also needs core 3's `driver/i2s_std.h`: the README and CI pin 1.9.9 there.

### H1 · high · One email per poll
Only the first UID of `* SEARCH` is fetched. Five payments in a minute take 75 s to come through, and each one is announced up to a minute after the customer left.
**Fix:** process every new matching message in one session (bounded to 10 per session, with an immediate follow-up poll if more are waiting).

### H2 · high · `UNSEEN` eats the whole inbox
The default filter is `UNSEEN`, and `STORE +FLAGS (\Seen)` runs on whatever was found. Non-payment emails become NEW orders with no amount (lines 512–514), and anything with a digit in its subject is "a payment" ("Get 50% off" becomes "rupees fifty received").
**Fix:** only emails whose subject parses as a payment (amount next to a currency or payment keyword, see H3) are announced and marked read. Everything else is left untouched and not logged. The device remembers the highest UID it has examined (`UIDVALIDITY` plus the last UID, both in NVS), so non-payment mail isn't fetched again. On first run it starts from `UIDNEXT`, so an old backlog is never announced.

### H3 · high · Wrong amounts announced
`extractAmount("12/05 Rs 500")` returns 12, and `"Txn 7731 NPR 500"` returns 7731. `"9801234567 paid Rs 500"` overflows `toInt()` (32-bit `long`) and announces about two hundred crore. `"Rs 1,500.50"` drops the paisa.
**Fix:** `parseAmount()` in `textutil.h`:
- It scores every number by context: a currency marker before it (`Rs`, `Rs.`, `NPR`, `NRs`, `INR`, `रु`, `₹`, the configured currency) or after it (`rupees`, `/-`), or a payment keyword nearby (`amount`, `received`, `credited`, `paid`, `payment of`).
- It rejects dates (`12/05/2024`, `2024-05-12`), times (`13:42`), IDs (`#123`, `Txn 7731`, `Ref:`, `A/C XX1234`, alphanumeric tokens), phone numbers (9+ ungrouped digits), percentages, and amounts of 1 crore or more.
- It understands `1,500`, `1,25,000`, `500.5`, `500.50` and Devanagari digits.

Host unit tests use real eSewa, Fonepay, Khalti and bank subject formats.

### H4 · high · Polling freezes the device
A Gmail session is a TLS handshake (1–3 s on an ESP32) plus five round trips, all on the Arduino loop task, every 15 s. Taps, swipes and dashboard requests stall, and so does the factory-reset button.
**Fix:** a dedicated FreeRTOS network task (`net.h`) runs IMAP, Apps Script and (phase 4) the Connect pull and webhook deliveries. It hands results to the UI loop through a queue of fixed-size events. Only the loop task ever touches the TFT, the audio, the orders or `cfg`. The task reads a copy of the config taken under a mutex.

### H5 · high · Encoded subjects
Bank and wallet mail with any non-ASCII in the subject (`रु`, `₹`, or just a curly quote) is sent as RFC 2047 encoded words, so `"=?UTF-8?B?UnMgNTAwIHJlY2VpdmVk?="` contains no digits. Long subjects are folded onto continuation lines, which are dropped. `SUBJECT:` in capitals isn't matched.
**Fix:** `mimeDecodeHeader()` handles B and Q encodings, UTF-8 / ISO-8859-1 / ASCII, and the whitespace between adjacent encoded words. Header lines are unfolded and matched case-insensitively.

### H6 · high · The voice test is a real sale
`/test` goes through `addOrder("Voice test", a, "PAID")`, which calls `recordPayment()` and inflates today's total and count. The screen and voice are identical to a genuine payment, so anyone with dashboard access can play a convincing "payment received".
**Fix:** test announcements are flagged. They aren't recorded as sales or orders, the screen says **TEST** in yellow ("TEST PAYMENT · NOT REAL"), and the voice says "test, rupees … received".

### H7 · high · Stuck in setup mode
`connectWifi()` is tried once for 18 s at boot. After a power cut the ESP boots in about 1 s while a home router takes 30–90 s, so the device falls into the setup hotspot and never tries the saved network again.
**Fix:** in setup mode with a saved SSID, the STA side keeps retrying (`WIFI_AP_STA`, every 30 s). Once it gets an IP and nobody is using the setup page, the device restarts into normal mode.

### H8 · high · CSRF on GET endpoints
`<img src="http://bizbox.local/api/reset">` on any page zeroes today's numbers when no PIN is set (and also with a PIN once the browser has cached it). `/test` and `/api/clear` are the same.
**Fix:** all `/api/*` need the Bearer token (C2). The dashboard calls them with `fetch` and the header. Legacy `/test` stays for old bookmarks but only plays a flagged TEST announcement.

### M1 · medium · Announcing inside the HTTP handler
`handleApiPayment` calls `announce()`, which runs about 0.6 s of animation, 1.5–3 s of speech and `delay(1800)` before the response is sent. Integrations with a 3–5 s timeout retry, and the retry announces again.
**Fix:** an announcement queue; the API answers immediately with `202`.

### M2 · medium · Amount parsing in the JSON API
In ArduinoJson 7, `variant | 0` only returns the value when it *is* an int: `500.5` and `"500"` give 0. `{"amount":-5}` becomes `(uint32_t)-5`, about 4.29 billion.
**Fix:** read the value as a number or a numeric string, require 0 < amount < 1 crore, keep paisa, and return `400` with a reason otherwise.

### M3 / M4 · medium · Orders
The list lives only in RAM, and IDs restart at 101 after every reboot. When the list is full (`MAX_ORDERS` = 10), `orders[0]` is dropped even if it's unpaid.
**Fix:** orders and `nextOrderId` are saved to NVS as one versioned blob (debounced). When the list is full, the oldest PAID order is dropped first; an unpaid one goes only if every order is unpaid.

### M5 · medium · 20 KB page String
`h.reserve(20000)` after a TLS session (40+ KB of heap churn) can fail on a fragmented heap, and Arduino `String` then silently truncates. Every dashboard load also fragments the heap further before the next TLS handshake.
**Fix (phase 3):** stream the page with chunked `sendContent()` through a 1 KB buffer; CSS and JS are sent straight from flash.

### M6 · medium · Flicker
`ui_drawScreen()` starts with `fillScreen()`, so a volume tap repaints all 76,800 pixels and the whole panel blinks.
**Fix (phase 3):** screen switches repaint only the content area and the nav. Value changes repaint only the affected row, meter, switch or pill.

### M7 · medium · Apps Script cursor
`since` is a RAM global set to `"0"` at boot and in `pendingRefresh` after every Save. If the script returns payments newer than `since`, every reboot or Save re-announces history. `cfg.url + "?key="` breaks a URL that already has a query string, and `doc["latest"].as<String>()` gives `"null"` when the field is missing.
**Fix:** `since` is persisted (new NVS key `since`), not reset on Save, joined with `?` or `&` as needed, and only updated when `latest` is present.

### M8 · medium · Unverified TLS to Gmail
`setInsecure()` means anyone who can ARP-spoof the shop Wi-Fi can impersonate `imap.gmail.com` and receive the LOGIN line.
**Fix:** pin Google Trust Services roots R1 and R4 (Gmail's chains end there) for IMAP and Apps Script. The arduino-esp32 build has `MBEDTLS_HAVE_TIME_DATE` off, so validation doesn't depend on the clock. There's a dashboard opt-out ("Verify Google's certificate") in case a chain change ever breaks it, and the status line says so.

### M9 · medium · PIN brute force
No delay or lockout on Basic-auth failures.
**Fix:** after 5 wrong PINs, all PIN-protected pages return `429` for 60 s.

### M10 · medium · Font reloads
TFT_eSPI's `loadFont(array)` calls `unloadFont()` (7 `free`s), then re-reads the whole glyph table with `pgm_read_byte` and `malloc`s 7 arrays. A full redraw switches fonts 20–40 times.
**Fix (phase 3):** keep each font's parsed metrics after its first load and swap the public `gFont` / `g*` pointers in and out instead of reloading.

### Low
- **L1:** the `[TOUCH]` debug print is removed.
- **L2:** `utf8Cut()` trims at character boundaries, used for order titles and `ui_fit`.
- **L3:** on the LAN, unknown paths return 404; captive-portal redirects only happen in setup mode.
- **L4:** `vmode` is clamped to 0–2 and `emode` to 0–2, both on load and on save.
- **L5:** `/api/order` returns 400 on invalid JSON. Status must be NEW, PREP, READY or PAID; titles are capped at 40 characters.
- **L6:** IMAP quoted strings are escaped.
- **L7:** `/test?amt=` is parsed with the same bounded parser.
- **L8:** dead code removed.
- **L9:** the first loop poll waits a full interval after the boot poll.
- **L10:** the dashboard pauses refreshes while hidden.
- **L11:** the encoded QR is cached until the QR text changes.
- **L12** (deferred): the order cycle and sale-on-tap semantics are a business decision. Counting a sale when staff tap PAID would double-count orders that are also paid by QR (the payment email books the sale too). Left as is.
- **L13** (deferred): serving HTTPS from the ESP32 isn't practical (RAM, certificate distribution). The PIN plus token is the boundary; set a PIN. A `Host` allow-list would break people who reach the box by IP, a custom DNS name or a reverse proxy.

## Status after the fixes
To be filled in once phase 2 and 3 land.
