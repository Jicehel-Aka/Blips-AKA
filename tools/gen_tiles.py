#!/usr/bin/env python3
"""Generates main/assets/tiles_data.cpp (the 16 px sprite atlas of Blips for Gamebuino AKA).

Sources
  assets/original/graphics/*.png   sprites of Blips (MIT, Willems Davy): wall 1001.com (CC BY-SA 3.0),
                                   floor / coin / player Kenney (CC0), boxes SpriteAttack (CC0).
  drawn here                       dynamite, bomb box, explosion, eraser: ORIGINAL artwork of this port
                                   (the upstream dynamite is a paid asset that may not be redistributed).

Atlas = one column of 16x16 tiles in the framebuffer pixel order (R | G<<5 | B<<11), 0xF81F = transparent.
Tile order must match main/assets/tiles.h.
"""
import math, os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "original", "graphics")
OUT = os.path.join(ROOT, "main", "assets", "tiles_data.cpp")
KEY = 0xF81F
S = 16


def load(name):
    return Image.open(os.path.join(SRC, name)).convert("RGBA")


def shrink(im, size=S):
    """32 -> 16 with premultiplied alpha (no dark halo on the sprite border)."""
    im = im.convert("RGBA")
    px = im.load()
    pm = Image.new("RGBA", im.size)
    pp = pm.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b, a = px[x, y]
            pp[x, y] = (r * a // 255, g * a // 255, b * a // 255, a)
    out = pm.resize((size, size), Image.BOX)
    po = out.load()
    for y in range(size):
        for x in range(size):
            r, g, b, a = po[x, y]
            if a:
                po[x, y] = (min(255, r * 255 // a), min(255, g * 255 // a), min(255, b * 255 // a), a)
    return out


# ---------------------------------------------------------------------------- own artwork (32x32 canvas)
def dynamite():
    """A red stick with bands, a fuse and a spark, lying diagonally. Drawn at 4x then reduced."""
    k = 4
    im = Image.new("RGBA", (32 * k, 32 * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy, ang = 16 * k, 17 * k, math.radians(-40)
    ca, sa = math.cos(ang), math.sin(ang)

    def pt(u, v):                       # u along the stick, v across it
        return (cx + (u * ca - v * sa) * k, cy + (u * sa + v * ca) * k)

    L, Wd = 11.0, 4.6
    d.polygon([pt(-L, -Wd), pt(L, -Wd), pt(L, Wd), pt(-L, Wd)], fill=(176, 30, 28, 255), outline=(70, 8, 8, 255))
    d.polygon([pt(-L, -Wd), pt(L, -Wd), pt(L, -Wd + 2.0), pt(-L, -Wd + 2.0)], fill=(226, 84, 70, 255))
    for u in (-5.0, 5.0):
        d.polygon([pt(u - 1.4, -Wd), pt(u + 1.4, -Wd), pt(u + 1.4, Wd), pt(u - 1.4, Wd)], fill=(240, 220, 150, 255))
    d.polygon([pt(L, -2.2), pt(L + 1.8, -2.2), pt(L + 1.8, 2.2), pt(L, 2.2)], fill=(90, 60, 40, 255))
    fx, fy = pt(L + 1.8, 0)
    ex, ey = pt(L + 5.5, -3.5)
    d.line([(fx, fy), (ex, ey)], fill=(70, 50, 30, 255), width=int(1.6 * k))
    for r, c in ((3.6, (255, 150, 30, 255)), (2.4, (255, 220, 60, 255)), (1.1, (255, 255, 230, 255))):
        d.ellipse([ex - r * k, ey - r * k, ex + r * k, ey + r * k], fill=c)
    return im.resize((32, 32), Image.LANCZOS)


def explosion(phase, n=8):
    k = 4
    im = Image.new("RGBA", (32 * k, 32 * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    t = phase / (n - 1)
    r = (5 + 11 * math.sin(min(1.0, t * 1.15) * math.pi / 2)) * k
    alpha = int(255 * (1.0 - max(0.0, t - 0.55) / 0.45))
    cx = cy = 16 * k
    # spiky outline
    pts = []
    for i in range(24):
        a = i * math.tau / 24
        rr = r * (1.0 if i % 2 == 0 else 0.72) * (0.92 + 0.08 * math.sin(i * 2.3 + phase))
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    cols = [((255, 120, 20), 1.0), ((255, 190, 40), 0.78), ((255, 240, 150), 0.52), ((255, 255, 255), 0.26)]
    if t > 0.6:                                   # smoke at the end
        cols = [((150, 140, 135), 1.0), ((190, 180, 170), 0.7)]
    d.polygon(pts, fill=cols[0][0] + (alpha,))
    for c, f in cols[1:]:
        d.ellipse([cx - r * f, cy - r * f, cx + r * f, cy + r * f], fill=c + (alpha,))
    return im.resize((32, 32), Image.LANCZOS)


def eraser():
    k = 4
    im = Image.new("RGBA", (32 * k, 32 * k), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.line([(7 * k, 7 * k), (25 * k, 25 * k)], fill=(235, 60, 50, 255), width=5 * k)
    d.line([(25 * k, 7 * k), (7 * k, 25 * k)], fill=(235, 60, 50, 255), width=5 * k)
    return im.resize((32, 32), Image.LANCZOS)


def paste(base, over):
    out = base.copy()
    out.alpha_composite(over)
    return out


def tiles():
    floor = load("floor.png")
    wall = load("wall.png")
    box = load("box.png")
    pl1, pl2 = load("player.png"), load("player2.png")
    dyn = dynamite()
    t = []
    t.append(floor)                                          # T_FLOOR
    t.append(wall.crop((0, 0, 32, 32)))                      # T_WALL
    t.append(wall.crop((32, 0, 64, 32)))                     # T_WALLB
    t.append(paste(floor, load("diamond.png")))              # T_COIN (on its floor, as in the game the floor is drawn too: kept transparent below)
    t.append(dyn)                                            # T_BOMB
    t.append(box.crop((0, 0, 32, 32)))                       # T_BOX
    t.append(box.crop((64, 0, 96, 32)))                      # T_BOX1
    t.append(box.crop((96, 0, 128, 32)))                     # T_BOX2
    t.append(paste(box.crop((0, 0, 32, 32)), dyn))           # T_BOXBOMB (own dynamite on the plain box)
    t.append(box.crop((32, 0, 64, 32)))                      # T_BOXWALL
    t.append(eraser())                                       # T_ERASE
    t += [explosion(i) for i in range(8)]                    # T_EXPL
    t += [pl1.crop((i * 32, 0, i * 32 + 32, 32)) for i in range(16)]   # T_PLAYER
    t += [pl2.crop((i * 32, 0, i * 32 + 32, 32)) for i in range(16)]   # T_PLAYER2
    return t


def rgb565(r, g, b):
    v = (r >> 3) | ((g >> 2) << 5) | ((b >> 3) << 11)
    return v if v != KEY else KEY ^ 1


def main():
    ts = tiles()
    # the coin is drawn over its floor by the game: keep the coin alone
    ts[3] = load("diamond.png")
    data = []
    for im in ts:
        r = shrink(im)
        p = r.load()
        for y in range(S):
            for x in range(S):
                rr, gg, bb, aa = p[x, y]
                data.append(KEY if aa < 128 else rgb565(rr, gg, bb))
    lines = ["// GENERATED by tools/gen_tiles.py - do not edit.",
             "// Sprites of Blips (MIT, Willems Davy): wall 1001.com CC BY-SA 3.0; floor, coin, player Kenney CC0; boxes SpriteAttack CC0.",
             "// Dynamite, bomb box, explosion and eraser: original artwork of the AKA port.",
             '#include "tiles.h"', "",
             "const uint16_t TILES_16[T_COUNT * 16 * 16] = {"]
    for i in range(0, len(data), 16):
        lines.append("  " + ",".join("0x%04X" % v for v in data[i:i + 16]) + ",")
    lines.append("};")
    open(OUT, "w").write("\n".join(lines) + "\n")
    print("wrote", OUT, len(ts), "tiles")
    # contact sheet for a visual check
    sheet = Image.new("RGBA", (len(ts) * 34, 34), (60, 70, 100, 255))
    for i, im in enumerate(ts):
        r = shrink(im)
        sheet.paste(r.resize((32, 32), Image.NEAREST), (i * 34 + 1, 1), r.resize((32, 32), Image.NEAREST))
    sheet.save("/tmp/tiles_sheet.png")
    assert len(ts) == 51, len(ts)


if __name__ == "__main__":
    main()
