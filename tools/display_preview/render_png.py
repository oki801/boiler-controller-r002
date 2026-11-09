#!/usr/bin/env python3
"""A PPM képekből előnézeti képet készít: nagyítva (4x, élsimítás nélkül, hogy a pixelek látszanak) és
valós méretben egymás mellett. Használat: render_png.py <mappa>"""
import glob, os, sys
from PIL import Image, ImageDraw
d = sys.argv[1]
files = sorted(glob.glob(os.path.join(d, "*.ppm")))
Z = 4
cols = 5
cell = 128 * Z + 8
rows = (len(files) + cols - 1) // cols
sheet = Image.new("RGB", (cols * cell, rows * (cell + 14)), (30, 30, 30))
dr = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(f).convert("RGB")
    big = im.resize((128 * Z, 128 * Z), Image.NEAREST)
    x, y = (i % cols) * cell + 4, (i // cols) * (cell + 14) + 12
    sheet.paste(big, (x, y))
    dr.text((x, y - 11), os.path.basename(f)[:-4], fill=(200, 200, 200))
    im.resize((128 * Z, 128 * Z), Image.NEAREST).save(f[:-4] + ".png")
sheet.save(os.path.join(d, "osszes.png"))
print("osszes.png:", sheet.size)
