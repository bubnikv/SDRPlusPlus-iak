#!/usr/bin/env python3
"""Build complete cross-platform icon sets for each 'signal' treatment of the water line."""
import io, json, math, os, struct
import cairosvg
from PIL import Image

OUT = "."  # output directory
S = json.load(open("subpaths.json"))  # vector trace of the source PNG
WAVE = S[1]["d"]
WX0, WY0, WX1, WY1 = 17, 104, 496, 319
BOT, X0, X1, YB = 410, 17, 496, 392

FOAM, SIGNAL = "#FFFFFF", "#8FE8F9"
GRAD = ('<linearGradient id="sea" x1=".1" y1="0" x2=".9" y2="1">'
        '<stop offset="0" stop-color="#28A0C8"/><stop offset=".45" stop-color="#1462AE"/>'
        '<stop offset="1" stop-color="#0B2F63"/></linearGradient>')

TIERS = {"micro":   dict(box=392, dil=24, rw=46, amp=44),
         "compact": dict(box=388, dil=16, rw=38, amp=40),
         "medium":  dict(box=386, dil=12, rw=34, amp=39),
         "master":  dict(box=384, dil=9,  rw=30, amp=38)}

def tier_for(px):
    return "micro" if px <= 24 else "compact" if px <= 48 else "medium" if px <= 96 else "master"

def poly(pts):
    return "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts)

# ------------------------------------------------------------ signal shapes
def r_sine(amp):
    step = (X1 - X0) / 4
    d, sign = f"M{X0},{YB}", -1
    for i in range(4):
        a = X0 + i * step
        d += f" C{a+step*.34:.0f},{YB+sign*amp} {a+step*.66:.0f},{YB+sign*amp} {a+step:.0f},{YB}"
        sign *= -1
    return d

AM_HEIGHT, AM_FLOOR, AM_DROP = 1.30, 0.20, 20.0   # height, envelope minimum, downward shift

def am_drop(t):
    """Shift scales with stroke weight so clearance stays even across size tiers."""
    return AM_DROP * t["rw"] / 30.0

def am_peak(amp):
    return amp * 1.30 * AM_HEIGHT

def r_am(amp, drop=AM_DROP):
    peak = am_peak(amp)
    base = YB + drop
    pts = []
    for i in range(241):
        t = i / 240
        env = AM_FLOOR + (1 - AM_FLOOR) * (0.5 - 0.5 * math.cos(2 * math.pi * t))
        pts.append((X0 + t * (X1 - X0), base - peak * env * math.sin(2 * math.pi * 3.0 * t)))
    return poly(pts)

def r_chirp(amp):
    pts = []
    for i in range(201):
        t = i / 200
        pts.append((X0 + t * (X1 - X0), YB - amp * 1.10 * math.sin(2 * math.pi * (0.9 * t + 1.35 * t * t))))
    return poly(pts)

def r_spectrum(amp):
    k = amp / 40.0
    pts = [(17, YB), (70, YB - 7 * k), (120, YB), (150, YB - 54 * k), (162, YB),
           (210, YB - 8 * k), (262, YB),
           (372, YB - 92 * k), (386, YB),
           (424, YB - 9 * k), (452, YB), (474, YB - 42 * k), (486, YB), (496, YB - 5 * k)]
    return poly(pts)

STROKES = {"sine": r_sine, "am": r_am, "chirp": r_chirp, "spectrum": r_spectrum}

def waterfall(t):
    rows = [([(0, 74), (96, 132), (268, 52), (348, 96), (462, 17)], 1.00),
            ([(20, 58), (120, 150), (300, 78), (404, 60)], 0.55)]
    h = t["rw"] * 1.15
    gap = h * 0.70
    out = ""
    for i, (segs, op) in enumerate(rows):
        y = YB - h - gap * 0.5 + i * (h + gap)
        for sx, sw in segs:
            out += (f'<rect x="{X0+sx}" y="{y:.1f}" width="{sw}" height="{h:.1f}" '
                    f'rx="{h/2:.1f}" fill="{SIGNAL}" opacity="{op}"/>')
    return out

STYLES = ["am"]

# ------------------------------------------------------------------ drawing
def artwork(tier, style, box=None, cx=256, cy=256, foam=FOAM, sig=SIGNAL):
    t = TIERS[tier]
    box = box or t["box"]
    bottom = BOT
    if style == "am":
        bottom = max(BOT, YB + am_drop(t) + am_peak(t["amp"]) + t["rw"] / 2)
    s = box / (WX1 - WX0)
    top = cy - ((bottom - WY0) * s) / 2
    tx, ty = cx - box / 2 - WX0 * s, top - WY0 * s
    g = (f'<g transform="translate({tx:.2f},{ty:.2f}) scale({s:.4f})" '
         'stroke-linejoin="round" stroke-linecap="round">'
         f'<path d="{WAVE}" fill="{foam}" stroke="{foam}" stroke-width="{t["dil"]/s:.2f}"/>')
    if style == "waterfall":
        g += waterfall(t).replace(SIGNAL, sig)
    else:
        d = (r_am(t["amp"], am_drop(t)) if style == "am" else STROKES[style](t["amp"]))
        g += (f'<path d="{d}" fill="none" stroke="{sig}" '
              f'stroke-width="{t["rw"]/s:.2f}"/>')
    return g + "</g>"

def tile_svg(tier, style, canvas=512, inset=0, radius=112):
    k = (canvas - 2 * inset) / 512
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{canvas}" height="{canvas}" '
            f'viewBox="0 0 {canvas} {canvas}">\n  <defs>{GRAD}</defs>\n'
            f'  <g transform="translate({inset},{inset}) scale({k:.6f})">\n'
            f'    <rect width="512" height="512" rx="{radius}" ry="{radius}" fill="url(#sea)"/>\n'
            f'    {artwork(tier, style)}\n  </g>\n</svg>')

def flat_svg(style, foam, sig):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" '
            f'viewBox="0 0 512 512">\n  {artwork("compact", style, box=470, foam=foam, sig=sig)}\n</svg>')

def render(svg, px):
    return Image.open(io.BytesIO(cairosvg.svg2png(
        bytestring=svg.encode(), output_width=px, output_height=px))).convert("RGBA")

def icon(style, px):
    return render(tile_svg(tier_for(px), style), px)

# ------------------------------------------------------------- ICO and ICNS
def write_ico(path, images):
    entries, blobs = [], []
    for px, im in sorted(images.items()):
        if px <= 64:
            p = im.load()
            xor = b"".join(b"".join(bytes((p[x, y][2], p[x, y][1], p[x, y][0], p[x, y][3]))
                                    for x in range(px)) for y in range(px - 1, -1, -1))
            stride = ((px + 31) // 32) * 4
            mask = bytearray()
            for y in range(px - 1, -1, -1):
                bits = bytearray(stride)
                for x in range(px):
                    if p[x, y][3] < 128:
                        bits[x // 8] |= 0x80 >> (x % 8)
                mask += bits
            blob = struct.pack("<IiiHHIIiiII", 40, px, px * 2, 1, 32, 0,
                               len(xor) + len(mask), 0, 0, 0, 0) + xor + bytes(mask)
        else:
            b = io.BytesIO(); im.save(b, "PNG", optimize=True); blob = b.getvalue()
        entries.append((px, len(blob))); blobs.append(blob)
    off = 6 + 16 * len(entries)
    out = struct.pack("<HHH", 0, 1, len(entries))
    for px, size in entries:
        out += struct.pack("<BBBBHHII", px if px < 256 else 0, px if px < 256 else 0,
                           0, 0, 1, 32, size, off)
        off += size
    open(path, "wb").write(out + b"".join(blobs))

ICNS_TYPES = [("icp4", 16), ("icp5", 32), ("ic11", 32), ("ic12", 64), ("ic07", 128),
              ("ic13", 256), ("ic08", 256), ("ic14", 512), ("ic09", 512), ("ic10", 1024)]

def write_icns(path, maker):
    chunks = b""
    for typ, px in ICNS_TYPES:
        b = io.BytesIO(); maker(px).save(b, "PNG", optimize=True); data = b.getvalue()
        chunks += typ.encode() + struct.pack(">I", len(data) + 8) + data
    open(path, "wb").write(b"icns" + struct.pack(">I", len(chunks) + 8) + chunks)

# ------------------------------------------------------------------- build
def build(style):
    root = f"{OUT}/{style}"
    for sub in ("svg", "png", "windows", "macos", "android", "linux"):
        os.makedirs(f"{root}/{sub}", exist_ok=True)

    open(f"{root}/wave-{style}.svg", "w").write(tile_svg("master", style))
    open(f"{root}/svg/wave-{style}-48.svg", "w").write(tile_svg("compact", style))
    open(f"{root}/svg/wave-{style}-24.svg", "w").write(tile_svg("micro", style))
    open(f"{root}/svg/wave-on-dark.svg", "w").write(flat_svg(style, FOAM, SIGNAL))
    open(f"{root}/svg/wave-on-light.svg", "w").write(flat_svg(style, "#0F4C97", "#2AA3CE"))

    sizes = [16, 20, 24, 32, 40, 48, 64, 72, 96, 128, 144, 192, 256, 512, 1024]
    pngs = {}
    for px in sizes:
        im = icon(style, px)
        im.save(f"{root}/png/icon-{px}.png", optimize=True)
        pngs[px] = im

    write_ico(f"{root}/windows/wave-{style}.ico",
              {p: pngs[p] for p in (16, 20, 24, 32, 40, 48, 64, 128, 256)})
    for px in (16, 20, 24, 32, 40, 48, 64, 256):
        pngs[px].save(f"{root}/windows/toolbar-{px}.png", optimize=True)

    def mac(px):
        return render(tile_svg(tier_for(round(px * 0.804)), style, canvas=512,
                               inset=round(512 * 0.098), radius=104), px)
    write_icns(f"{root}/macos/wave-{style}.icns", mac)
    mac(1024).save(f"{root}/macos/icon-1024.png", optimize=True)

    fg = ('<svg xmlns="http://www.w3.org/2000/svg" width="432" height="432" viewBox="0 0 432 432">'
          f'<g transform="translate(216,216) scale(0.6439) translate(-256,-256)">'
          f'{artwork("medium", style, box=400)}</g></svg>')
    bgs = ('<svg xmlns="http://www.w3.org/2000/svg" width="432" height="432" viewBox="0 0 432 432">'
           f'<defs>{GRAD}</defs><rect width="432" height="432" fill="url(#sea)"/></svg>')
    render(fg, 432).save(f"{root}/android/ic_launcher_foreground.png", optimize=True)
    render(bgs, 432).save(f"{root}/android/ic_launcher_background.png", optimize=True)
    open(f"{root}/android/ic_launcher.xml", "w").write(
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
        '    <background android:drawable="@mipmap/ic_launcher_background"/>\n'
        '    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>\n'
        '</adaptive-icon>\n')
    for dpi, px in (("mdpi", 48), ("hdpi", 72), ("xhdpi", 96), ("xxhdpi", 144), ("xxxhdpi", 192)):
        os.makedirs(f"{root}/android/mipmap-{dpi}", exist_ok=True)
        pngs[px].save(f"{root}/android/mipmap-{dpi}/ic_launcher.png", optimize=True)

    for px in (16, 24, 32, 48, 64, 128, 256, 512):
        d = f"{root}/linux/hicolor/{px}x{px}/apps"
        os.makedirs(d, exist_ok=True)
        pngs[px].save(f"{d}/wave-{style}.png", optimize=True)
    os.makedirs(f"{root}/linux/hicolor/scalable/apps", exist_ok=True)
    open(f"{root}/linux/hicolor/scalable/apps/wave-{style}.svg", "w").write(tile_svg("master", style))
    return pngs

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for s in STYLES:
        build(s)
        print("built", s)
