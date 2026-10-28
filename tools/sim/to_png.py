"""Turn the simulator's PPM frames into 3x PNGs plus a contact sheet."""
import glob, os, sys
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
frames = sorted(glob.glob(os.path.join(src, "*.ppm")), key=lambda f: int(os.path.basename(f).split("_")[0]))
main = [f for f in frames if int(os.path.basename(f).split("_")[0]) <= 8]  # 9+ are edge cases
edge = os.path.join(out, "edge-cases")
for f in frames:
    dest = out if f in main else edge
    os.makedirs(dest, exist_ok=True)
    Image.open(f).resize((960, 720), Image.NEAREST).save(os.path.join(dest, os.path.basename(f)[:-4] + ".png"))
sheet = Image.new("RGB", (640 * 2 + 30, 480 * 4 + 50), (30, 30, 30))
for i, f in enumerate(main):
    sheet.paste(Image.open(f).resize((640, 480), Image.NEAREST), (10 + (i % 2) * 650, 10 + (i // 2) * 490))
sheet.save(os.path.join(out, "screen-previews.png"))
print(f"{len(frames)} screens -> {out}")
