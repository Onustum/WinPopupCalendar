#!/usr/bin/env python3
"""Рисует иконку календаря (стиль Windows 11) и сохраняет app.ico."""
from PIL import Image, ImageDraw

SZ = 256
img = Image.new("RGBA", (SZ, SZ), (0, 0, 0, 0))
d = ImageDraw.Draw(img)

# страница календаря
page = (24, 40, 232, 232)
d.rounded_rectangle(page, radius=28, fill=(245, 246, 248, 255))
# синяя шапка
d.rounded_rectangle((24, 40, 232, 108), radius=28, fill=(0, 120, 212, 255))
d.rectangle((24, 80, 232, 108), fill=(0, 120, 212, 255))
# кольца переплёта
for x in (76, 180):
    d.rounded_rectangle((x - 9, 18, x + 9, 62), radius=9, fill=(210, 214, 220, 255))
    d.rounded_rectangle((x - 5, 22, x + 5, 58), radius=5, fill=(120, 130, 140, 255))
# сетка дней
dot = (90, 96, 108, 255)
accent = (0, 120, 212, 255)
xs = (64, 112, 160, 208)
ys = (136, 172, 208)
for r, y in enumerate(ys):
    for c, x in enumerate(xs):
        col = accent if (r == 1 and c == 1) else dot
        rad = 11 if col == accent else 8
        d.ellipse((x - rad, y - rad, x + rad, y + rad), fill=col)

img.save("app.ico", sizes=[(16, 16), (20, 20), (24, 24), (32, 32),
                           (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)])
print("app.ico written")
