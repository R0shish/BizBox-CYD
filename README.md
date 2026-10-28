# BizBox-CYD

An open-source business hub for a shop counter, built on the ESP32 "Cheap Yellow Display" (ESP32-2432S028, 2.8" 320×240 touchscreen). It shows your payment QR, watches for payment emails, and reads each payment out loud ("rupees five hundred received"). It also runs a small orders board, a customer-facing promo board, today's stats, and a phone dashboard at `http://bizbox.local`.

![Screens](previews/screen-previews.png)

## What it does

- **Pay:** your eSewa / Fonepay / Khalti merchant QR, big and scannable
- **Payment alerts:** checks Gmail over IMAP (or a Google Apps Script webhook, or a local API call), then shows a full-screen "payment received" and speaks the amount
- **Orders:** a kitchen-style queue (NEW → PREP → READY → PAID), tap to advance
- **Board:** welcome screen + today's special for customers, auto-flips with the QR
- **Stats:** today's total, payment count, average ticket, sales by hour, resets at midnight Nepal time
- **Setup:** volume, brightness, Wi-Fi info, voice test
- **Phone dashboard:** everything above plus QR, brand, Gmail and Wi-Fi settings; first boot opens a setup hotspot (`BizBox-Setup`) with a scan-to-join QR

## Build

Arduino IDE (or arduino-cli) with:

| Library | Version | Notes |
|---|---|---|
| ESP32 Arduino core | 2.0.x or 3.x | both supported |
| TFT_eSPI | 2.5+ | `User_Setup.h` must have your CYD pins, `USE_HSPI_PORT` and `SMOOTH_FONT` |
| ArduinoJson | 7.x | |
| ESP8266Audio | any | only its `AudioOutput` base class is used |
| ESP8266SAM | any | speech synth |
| QRCode (ricmoo) | any | provides `qrcode.h` |

Board: "ESP32 Dev Module". If the sketch is too big, set **Partition Scheme → Huge APP**.

> Updating TFT_eSPI through the Library Manager overwrites `User_Setup.h`. Back it up first.

## Files

| File | What's in it |
|---|---|
| `esp.ino` | hardware (touch, backlight, audio), storage, payment polling, touch handling, Wi-Fi, setup/loop |
| `model.h` | shared state: config, orders, today's numbers |
| `ui.h` | the whole screen UI: palette, fonts, components, every screen |
| `web.h` | the phone dashboard (HTML/CSS/JS) and REST API |
| `speaker.h` | voice output straight to the DAC on GPIO26 |
| `fonts.h` | generated anti-aliased fonts (Space Grotesk + JetBrains Mono) |
| `previews/` | rendered screenshots of every screen |
| `tools/` | font generator, screen simulator, desktop compile check |

## Local API

| Method | Path | Body | Does |
|---|---|---|---|
| GET | `/api/state` | | today's numbers, orders, status as JSON |
| POST | `/api/payment` | `{"amount":500,"note":"eSewa"}` | logs + announces a payment |
| POST | `/api/order` | `{"title":"2x Latte","amount":450,"status":"NEW"}` | adds an order to the screen |
| GET | `/api/clear` | | removes paid orders |
| GET | `/api/reset` | | zeroes today's numbers |

## Tools

```bash
tools/check.sh          # compile the sketch against stand-in libraries (core 2.x and 3.x) + audio test
tools/preview.sh        # re-render previews/ from the real UI code
tools/preview.sh --web  # also screenshot the phone dashboard (needs Playwright)
python3 tools/fonts/gen_fonts.py   # regenerate fonts.h after changing sizes/weights
```

Fonts: Space Grotesk and JetBrains Mono, SIL Open Font License 1.1 (see `tools/fonts/`).
