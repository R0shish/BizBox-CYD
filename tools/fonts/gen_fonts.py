#!/usr/bin/env python3
"""Generate TFT_eSPI smooth (VLW, anti-aliased) fonts as PROGMEM C arrays.

VLW layout (big-endian int32):
  header: gCount, version(11), size, 0, ascent, descent
  per glyph (7 ints): unicode, height, width, xAdvance, dY(top above baseline), dX, 0
  then 8-bit alpha bitmaps, glyph after glyph
  then name strings (ignored by TFT_eSPI, kept for completeness)
"""
import struct, sys, os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
GROTESK = os.path.join(HERE, "SpaceGrotesk.ttf")
MONO = os.path.join(HERE, "JetBrainsMono.ttf")

ASCII = [chr(c) for c in range(0x21, 0x7F)]
EXTRA = ["°", "·", "…", "→", "←", "↔", "’"]
DIGITS = list("0123456789,.:-+")

FONTS = [
    # name,        file,    weight, px, charset,        tracking
    ("F_LABEL",   MONO,    600, 10, ASCII + EXTRA, 1),
    ("F_MONO",    MONO,    500, 12, ASCII + EXTRA, 0),
    ("F_BODY",    GROTESK, 500, 15, ASCII + EXTRA, 0),
    ("F_HEAD",    GROTESK, 700, 24, ASCII + EXTRA, 0),
    ("F_NUM",     GROTESK, 700, 40, DIGITS,        0),
    ("F_HERO",    GROTESK, 700, 58, DIGITS,        0),
]


def make_font(path, weight, px, chars, tracking):
    f = ImageFont.truetype(path, px)
    try:
        f.set_variation_by_axes([weight])
    except Exception as e:
        print("variation failed", e, file=sys.stderr)
    glyphs = []
    for ch in chars:
        l, t, r, b = f.getbbox(ch, anchor="ls")
        w, h = max(0, r - l), max(0, b - t)
        adv = int(round(f.getlength(ch))) + tracking
        if w == 0 or h == 0:
            continue
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-l, -t), ch, font=f, fill=255, anchor="ls")
        glyphs.append(dict(u=ord(ch), w=w, h=h, adv=adv, dy=-t, dx=l, bmp=img.tobytes()))
    # Non-breaking space: TFT_eSPI guesses the width of " " (0x20), so the UI
    # swaps spaces for U+00A0, which carries the font's real space advance.
    glyphs.append(dict(u=0xA0, w=1, h=1, adv=int(round(f.getlength(" "))) + tracking, dy=1, dx=0, bmp=b"\0"))
    ascent = max(g["dy"] for g in glyphs)
    descent = max(max(g["h"] - g["dy"] for g in glyphs), 0)
    return glyphs, ascent, descent


def vlw_bytes(glyphs, px, ascent, descent, name):
    out = bytearray()
    out += struct.pack(">6i", len(glyphs), 11, px, 0, ascent, descent)
    for g in glyphs:
        assert g["w"] < 256 and g["h"] < 256 and g["adv"] < 256 and -128 <= g["dx"] < 128
        out += struct.pack(">7i", g["u"], g["h"], g["w"], g["adv"], g["dy"], g["dx"], 0)
    for g in glyphs:
        out += g["bmp"]
    nm = name.encode()
    out += bytes([len(nm)]) + nm + b"\0" + bytes([len(nm)]) + nm + b"\0" + b"\x01"
    return bytes(out)


def main(out_h, specimen_png=None):
    parts = [
        "// Auto-generated smooth fonts for TFT_eSPI (loadFont from PROGMEM).\n",
        "// Space Grotesk + JetBrains Mono, SIL Open Font License 1.1.\n",
        "#pragma once\n#include <pgmspace.h>\n\n",
    ]
    total = 0
    rendered = []
    for name, path, wt, px, chars, tr in FONTS:
        glyphs, asc, desc = make_font(path, wt, px, chars, tr)
        data = vlw_bytes(glyphs, px, asc, desc, name)
        total += len(data)
        rendered.append((name, glyphs, asc, desc))
        parts.append(f"// {name}: {os.path.basename(path)} wght {wt}, {px}px, {len(glyphs)} glyphs, {len(data)} bytes\n")
        parts.append(f"const uint8_t {name}_DATA[] PROGMEM = {{\n")
        for i in range(0, len(data), 24):
            parts.append("  " + ",".join(f"0x{b:02X}" for b in data[i:i + 24]) + ",\n")
        parts.append("};\n\n")
    parts.insert(3, f"// Total font data: {total} bytes\n\n")
    with open(out_h, "w") as fh:
        fh.write("".join(parts))
    print(f"wrote {out_h}: {total} bytes of font data")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "fonts.h"))
