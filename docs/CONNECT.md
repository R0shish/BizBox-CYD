# Connect: plugging BizBox into your own system

Connect is a small, generic layer that lets any management system (an order dashboard, a POS, an ERP, an n8n or Zapier flow, a Google Sheet) work with the box:

| Direction | What | How |
|---|---|---|
| device → system | a payment came in, an order was created, someone tapped an order to change its status | **webhooks**: the box POSTs JSON to your URL |
| system → device | orders to show on the screen, status updates, "say this", "announce Rs 500", "show the orders screen" | **pull**: the box GETs your URL every few seconds |

Why pull rather than push? The box sits behind the shop router's NAT, so a cloud system can't reach it. The box makes every connection itself: webhooks out, polling in. Nothing has to be opened on the router.

Set it up in the **Connect** tab of the phone dashboard (`http://bizbox.local`). The tab also has *Send test event* and *Test pull* buttons and shows the last result of each.

---

## 1. Outbound webhooks

### Events

| Event | When | Notes |
|---|---|---|
| `payment.received` | A payment was announced: from Gmail, Apps Script or `POST /api/payment` | Not sent for payments your own system announced through a pull command (no echo). *Send test event* sends this with `"test": true`. |
| `order.created` | An order was added on the device: from the dashboard or `POST /api/order` | Orders that came from your system (pull) and orders created by a payment are not echoed. |
| `order.status_changed` | Staff tapped an order on the screen (NEW → PREP → READY → PAID → NEW) | Includes `ext_id`, so status taps on orders from your system go back to it. |

Each event can be switched off in the Connect tab.

### Request

```
POST <your webhook URL>
Content-Type: application/json
User-Agent: BizBox/3.1
X-BizBox-Event: payment.received
X-BizBox-Delivery: evt_5f2a91c3_17
X-BizBox-Timestamp: 1730100000
X-BizBox-Signature: sha256=5d1c…         (only when a signing secret is set)
<your auth header>                       (optional, e.g. "Authorization: Bearer abc")
```

The default body:

```json
{
  "event": "payment.received",
  "id": "evt_5f2a91c3_17",
  "ts": 1730100000,
  "device": "bizbox",
  "shop": "Roast & Co",
  "test": false,
  "data": {
    "amount": 1250.5,
    "amount_paisa": 125050,
    "currency": "Rs",
    "note": "eSewa",
    "source": "gmail",
    "order_id": 112,
    "ext_id": "",
    "title": "Rs 1,250.50 received from SITA",
    "status": "PAID",
    "old_status": ""
  }
}
```

Every event has the same `data` keys; ones that don't apply are empty (`""` or `0`).

| field | meaning |
|---|---|
| `id` | Delivery ID: the same across retries of one event. Use it to ignore duplicates. |
| `ts` | Unix time (UTC) when the event happened; `0` if the clock wasn't synced yet. |
| `data.source` | `gmail`, `apps_script`, `api`, `dashboard`, `screen` or `test` |
| `data.order_id` | The order number shown on the screen (`#112`) |
| `data.ext_id` | Your system's order ID, for orders that came from a pull; empty for local orders |
| `data.status` / `old_status` | `NEW`, `PREP`, `READY` or `PAID` |

### Custom body template

Leave *Body template* empty for the JSON above, or write your own with `{{placeholders}}`. Values are JSON-escaped, so put them inside quotes for strings:

```json
{"text": "{{shop}}: {{currency}} {{amount}} from {{note}}", "order": "{{ext_id}}", "raw": {{body}}}
```

Placeholders: `{{event}} {{id}} {{ts}} {{time}} {{device}} {{shop}} {{test}} {{amount}} {{amount_paisa}} {{currency}} {{note}} {{source}} {{order_id}} {{ext_id}} {{title}} {{status}} {{old_status}} {{body}}`.
`{{time}}` is local time `2026-10-28T13:42:05+05:45`. `{{body}}` is the whole default JSON body. Unknown placeholders become empty. Templates are capped at 1000 characters.

### Retries

- Any `2xx` reply is success.
- Network errors, timeouts (5 s), `408`, `429` and `5xx` are retried 10 s, 1 min, 5 min and 15 min later (5 attempts in all), then dropped.
- Other `4xx` replies are dropped at once (your endpoint rejected the event).
- Up to 8 events wait in RAM. When that's full the oldest is dropped. The queue does not survive a reboot.
- Delivery runs on the network task, so a slow endpoint never freezes the screen.

### Verifying the signature

If you set a signing secret, check `X-BizBox-Signature` against
`"sha256=" + hex(HMAC_SHA256(secret, timestamp + "." + raw_body))`, where `timestamp` is the `X-BizBox-Timestamp` header. Reject requests whose timestamp is more than a few minutes old.

```js
// Node / Express (use express.raw so you hash the exact bytes)
const crypto = require('crypto');
function verify(req, secret) {
  const ts = req.get('X-BizBox-Timestamp');
  const mac = crypto.createHmac('sha256', secret).update(ts + '.' + req.body).digest('hex');
  const ok = crypto.timingSafeEqual(Buffer.from('sha256=' + mac), Buffer.from(req.get('X-BizBox-Signature') || ''));
  return ok && Math.abs(Date.now() / 1000 - Number(ts)) < 300;
}
```

```python
# Python / Flask
import hmac, hashlib, time
def verify(request, secret: bytes) -> bool:
    ts = request.headers.get("X-BizBox-Timestamp", "0")
    mac = hmac.new(secret, ts.encode() + b"." + request.get_data(), hashlib.sha256).hexdigest()
    return hmac.compare_digest("sha256=" + mac, request.headers.get("X-BizBox-Signature", "")) \
        and abs(time.time() - int(ts)) < 300
```

---

## 2. Inbound pull

Every *N* seconds (default 15, minimum 5) the box asks your system what's new:

```
GET <your pull URL>?cursor=<last cursor>
Accept: application/json
X-BizBox-Device: bizbox
<your auth header>
```

`cursor` is empty the first time. Reply with `200` and JSON, or `204 No Content` when there's nothing new. With the default paths the reply looks like this:

```json
{
  "cursor": "2026-10-28T13:42:05Z#481",
  "orders": [
    {"id": "PL-1042", "title": "2x A5 flyers, matte", "amount": 450, "status": "pending"},
    {"id": "PL-1043", "title": "Business cards", "amount": "1,200.50", "status": "ready"},
    {"id": "PL-1031", "status": "cancelled"}
  ],
  "commands": [
    {"id": "c-77", "type": "say", "text": "Order ten forty two is ready"},
    {"id": "c-78", "type": "screen", "screen": "orders"},
    {"id": "c-79", "type": "announce", "amount": 500, "note": "Cash"}
  ]
}
```

### Cursor: nothing is processed twice

- Send back a `cursor` that marks how far the box has read (a timestamp, an auto-increment ID, anything). The box stores it in flash and sends it on the next pull. Return only what changed after it.
- The cursor is saved only after every order and command in the reply has been handed to the screen. If the box loses power in between, you'll see the old cursor again and resend the same items. Orders are idempotent (matched by `id`), and commands with an `id` are skipped if they were run in the last 32 commands.
- At most 20 orders and 10 commands are taken per pull, so page bigger backlogs through the cursor.
- *Reset cursor* in the Connect tab starts again from empty.

### Orders → screen

| setting | default | meaning |
|---|---|---|
| Orders path | `orders` | where the array is in your JSON |
| ID path | `id` | your order ID (stored on the device as `ext_id`, max 39 bytes) |
| Title path | `title` | text shown on the screen (cut to 40 bytes) |
| Amount path | `amount` | rupees: a number (`450`, `499.5`) or a numeric string (`"1,200.50"`) |
| Status path | `status` | your status value, mapped through the status map |

Paths are dot-separated and can index arrays: `data.items`, `customer.name`, `lines.0.total`. An empty path means "the item itself".

The **status map** turns your values into the screen's four statuses, plus `REMOVE`:

```
NEW=new,pending,received,placed; PREP=preparing,processing,in_progress,printing; READY=ready,ready_for_pickup,done_printing; PAID=paid,completed,delivered,picked_up; REMOVE=cancelled,canceled,deleted,archived
```

Matching ignores case and surrounding spaces. The device's own names (`NEW`, `PREP`, `READY`, `PAID`) always work, even if they're not in the map.

- An unknown `id` with a status adds a new order. An unknown status counts as `NEW`.
- A known `id` updates only the fields present in the item (title, amount, status). An unknown status leaves the current one.
- `REMOVE` takes the order off the screen.
- Updates from your system don't change today's totals and aren't echoed back as webhooks.
- When the screen list (10 orders) is full, the oldest PAID order makes room first.

### Commands

The type field is always `type`.

| `type` | fields | does |
|---|---|---|
| `say` | `text` (ASCII, max 120 chars) | Speaks the text (SAM voice). No screen change. |
| `screen` | `screen`: `pay`, `orders`, `board`, `stats`, `setup` or `1`–`5` | Switches the screen. |
| `announce` | `amount` (rupees), `note` | Full "payment received" takeover and voice, logged as a PAID order and counted in today's totals. **Only runs if the reply is signed** (below). |

### Signed replies (required for `announce`)

Anyone who can tamper with your HTTPS response could otherwise make the box announce a payment that never happened. So `announce` commands run only when the reply has
`X-BizBox-Signature: sha256=<hex HMAC_SHA256(secret, raw_body)>` computed with the same signing secret as the webhooks. Everything else works unsigned. Tick *Require signed replies* to reject all unsigned replies.

```js
// Express: send a signed reply
const body = JSON.stringify(reply);
res.set('X-BizBox-Signature', 'sha256=' + crypto.createHmac('sha256', secret).update(body).digest('hex'));
res.type('application/json').send(body);
```

### TLS

HTTPS endpoints are encrypted, but the box doesn't verify their certificates (there's no CA store for arbitrary hosts on the ESP32 within the flash budget). Integrity comes from the HMAC signatures, so use a signing secret. Plain `http://` works too, on your LAN for example.

---

## 3. Copy-paste examples

Replace `bizbox.local` with the box's IP if mDNS doesn't resolve on your network. `TOKEN` is the API token from the dashboard's System tab (used only by the device's own REST API, not by Connect).

### curl: catch webhooks while testing

```bash
# Terminal 1: print whatever the box sends (needs Python 3)
python3 -c 'import http.server as h
class H(h.BaseHTTPRequestHandler):
  def do_POST(s):
    n=int(s.headers["Content-Length"]); print(dict(s.headers)); print(s.rfile.read(n).decode()); s.send_response(200); s.end_headers()
h.HTTPServer(("",8080),H).serve_forever()'
# Connect tab: Webhook URL = http://<your computer's IP>:8080/  ->  "Send test event"
```

### curl: the device's own REST API (same network)

```bash
curl -H "Authorization: Bearer TOKEN" http://bizbox.local/api/state
curl -H "Authorization: Bearer TOKEN" -H "Content-Type: application/json" \
     -d '{"title":"2x Latte","amount":450,"status":"NEW"}' http://bizbox.local/api/order
curl -H "Authorization: Bearer TOKEN" -H "Content-Type: application/json" \
     -d '{"amount":500,"note":"Cash"}' http://bizbox.local/api/payment
curl -X POST -H "Authorization: Bearer TOKEN" http://bizbox.local/api/test
```

### A static pull endpoint (for a quick test)

Put this file on any web server, or a GitHub Gist "raw" URL, and use it as the pull URL:

```json
{"cursor": "1", "orders": [{"id": "T-1", "title": "Hello from Connect", "amount": 99, "status": "ready"}],
 "commands": [{"id": "c1", "type": "say", "text": "Connect works"}]}
```

The box shows the order once and says the line once. The cursor `1` comes back, so the same items repeat on every pull, and orders are idempotent, but the `say` command is skipped after the first time thanks to its `id`.

### n8n

- **Webhooks in:** add a *Webhook* node (POST, path `bizbox`), copy its production URL into *Webhook URL*. Optionally verify the signature in a *Crypto* node (HMAC, SHA256, hex) over `{{$json.headers["x-bizbox-timestamp"]}}.{{JSON.stringify($json.body)}}`; for an exact match enable *Raw Body* on the Webhook node.
- **Pull out:** a second *Webhook* node (GET, path `bizbox-pull`, *Respond: Using 'Respond to Webhook' node*), then build the reply (orders from your DB, a cursor such as the newest `updated_at`), then *Respond to Webhook* with JSON. Use `{{$json.query.cursor}}` to filter.

### Zapier

- **Webhooks in:** trigger *Webhooks by Zapier → Catch Hook*, paste its URL into *Webhook URL*, then add any action (Google Sheets row, Slack message, …). Zapier doesn't check HMAC; leave the secret empty or check it in a *Code by Zapier* step.
- **Pull:** Zapier can't answer the box's GET with dynamic data. Use n8n, Apps Script, or your own server for the pull side.

### Google Apps Script (both directions)

Deploy as a Web app (*Execute as: Me*, *Who has access: Anyone*). Use the `/exec` URL for both the webhook and the pull URL.

```js
const SECRET = 'change-me';          // same as the Connect tab's signing secret
const SHEET = SpreadsheetApp.getActive();

// Webhooks from the box -> a row per event
// (Apps Script can't read request headers, so put a shared token in the URL,
//  e.g. .../exec?k=SECRET, and check e.parameter.k)
function doPost(e) {
  if (e.parameter.k !== SECRET) return ContentService.createTextOutput('no');
  const ev = JSON.parse(e.postData.contents);
  SHEET.getSheetByName('events').appendRow([new Date(), ev.event, ev.data.amount, ev.data.note, ev.data.ext_id, ev.data.status]);
  return ContentService.createTextOutput('ok');
}

// Pull: orders from the 'orders' sheet (id | title | amount | status | updated)
function doGet(e) {
  const since = Number(e.parameter.cursor || 0);
  const rows = SHEET.getSheetByName('orders').getDataRange().getValues().slice(1);
  const fresh = rows.filter(r => new Date(r[4]).getTime() > since);
  const cursor = Math.max(since, ...fresh.map(r => new Date(r[4]).getTime()));
  const body = JSON.stringify({
    cursor: String(cursor),
    orders: fresh.slice(0, 20).map(r => ({id: String(r[0]), title: r[1], amount: r[2], status: r[3]}))
  });
  return ContentService.createTextOutput(body).setMimeType(ContentService.MimeType.JSON);
}
```

Apps Script web apps can't set response headers, so their replies can't be signed: `say`, `screen` and orders work, `announce` doesn't. They also redirect once to `script.googleusercontent.com`, which the box follows.

---

## 4. Vendor presets

A preset fills the Connect tab's fields for a known system: paths, the status map, default events. Presets are plain JSON in [`presets.h`](../presets.h), in the block marked `VENDOR PRESETS`. Add an object there and it appears in the *Preset* dropdown. Secrets and URLs are never part of a preset.

```c
// in presets.h, inside the JSON array
{"name": "My POS", "pull_orders": "data.orders", "pull_id": "uuid", "pull_title": "summary",
 "pull_amount": "total", "pull_status": "state", "pull_commands": "data.commands", "pull_cursor": "next",
 "status_map": "NEW=open; PREP=cooking; READY=ready; PAID=closed; REMOVE=void", "events": 7}
```

`events` is a bitmask: 1 = `payment.received`, 2 = `order.created`, 4 = `order.status_changed`.

---

## 5. Reference

### Settings (NVS keys, namespace `cydbiz`)

| key | setting |
|---|---|
| `cxHook` | webhook URL |
| `cxHdr` | extra header, `Name: value` (secret: never shown again) |
| `cxSec` | signing secret (secret) |
| `cxTpl` | body template |
| `cxEvt` | event bitmask (default 7) |
| `cxPull` | pull URL |
| `cxEvery` | pull interval, seconds |
| `cxPOrd` `cxPId` `cxPTit` `cxPAmt` `cxPSt` `cxPCmd` `cxPCur` | JSON paths |
| `cxMap` | status map |
| `cxSign` | require signed pull replies |
| `cxCur` | last cursor (state) |

### Device side, in code

- `connect.h`: webhook queue (loop task enqueues, network task delivers), the pull client (network task), JSON-path mapping, HMAC.
- `presets.h`: the vendor presets.
- Status in `GET /api/state` under `connect`: `{"hook": "...", "pull": "...", "queue": 0}`.
- Dashboard endpoints (Bearer token): `POST /api/connect/test` queues a test `payment.received`; `POST /api/connect/pull` pulls now.
