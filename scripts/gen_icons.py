#!/usr/bin/env python3
"""Generate 26 weather icon BMP files (13 conditions x 2 sizes) in RGB565 format.

Output: src/icons/
Format: 16-bit BMP with BI_BITFIELDS (RGB565)
"""

import math
import os
import struct
from PIL import Image, ImageDraw

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                       "src", "icons")

# ---------------------------------------------------------------------------
# Colour palette
# ---------------------------------------------------------------------------
SUN_GOLD   = (0xFF, 0xD7, 0x00, 255)
SUN_ORANGE = (0xFF, 0xA5, 0x00, 255)
CLOUD_L    = (0xC8, 0xD0, 0xD8, 255)
CLOUD_M    = (0xA0, 0xA8, 0xB0, 255)
CLOUD_D    = (0x80, 0x88, 0x90, 255)
RAIN_BLUE  = (0x44, 0x88, 0xFF, 255)
LIGHTNING  = (0xFF, 0xE0, 0x00, 255)
SNOW_W     = (0xE0, 0xE8, 0xF0, 255)
BG         = (0x14, 0x19, 0x23, 255)

SIZES = [80, 36]

# ---------------------------------------------------------------------------
# Helper: scale coordinates by icon size / 80 (design at 80, shrink for 36)
# ---------------------------------------------------------------------------

def s(val, sz):
    """Scale a design-coordinate (designed at 80px) to *sz*."""
    return val * sz / 80.0


def si(val, sz):
    return int(round(s(val, sz)))


# ---------------------------------------------------------------------------
# Drawing helpers
# ---------------------------------------------------------------------------

def draw_sun(draw, cx, cy, r_body, r_ray, n_rays, sz):
    """Draw a sun: filled circle + rays."""
    # Rays
    for i in range(n_rays):
        angle = 2 * math.pi * i / n_rays
        x1 = cx + math.cos(angle) * (r_body + s(2, sz))
        y1 = cy + math.sin(angle) * (r_body + s(2, sz))
        x2 = cx + math.cos(angle) * r_ray
        y2 = cy + math.sin(angle) * r_ray
        draw.line([(x1, y1), (x2, y2)], fill=SUN_GOLD, width=max(1, si(3, sz)))
    # Body
    draw.ellipse([cx - r_body, cy - r_body, cx + r_body, cy + r_body],
                 fill=SUN_GOLD, outline=SUN_ORANGE, width=max(1, si(2, sz)))
    # Inner highlight
    inner = r_body * 0.45
    draw.ellipse([cx - inner, cy - inner, cx + inner, cy + inner], fill=SUN_ORANGE)


def draw_cloud(draw, cx, cy, scale, color, sz):
    """Draw a cloud made of overlapping ellipses.  *scale* ~1.0 at 80px design."""
    sc = scale * sz / 80.0
    # Three bumps
    def e(x, y, rx, ry):
        draw.ellipse([cx + x * sc - rx * sc, cy + y * sc - ry * sc,
                       cx + x * sc + rx * sc, cy + y * sc + ry * sc], fill=color)
    e(-12, 0, 16, 12)
    e(8, -4, 18, 14)
    e(22, 2, 13, 10)
    # Base rectangle to merge
    draw.rectangle([cx - 28 * sc, cy + 0 * sc, cx + 35 * sc, cy + 12 * sc], fill=color)


def draw_rain_lines(draw, cx, cy, count, length, sz):
    """Draw diagonal rain lines below a cloud."""
    spacing = s(12, sz)
    start_x = cx - spacing * (count - 1) / 2
    for i in range(count):
        x = start_x + i * spacing
        draw.line([(x, cy), (x - s(4, sz), cy + s(length, sz))],
                  fill=RAIN_BLUE, width=max(1, si(2, sz)))


def draw_lightning(draw, cx, cy, sz):
    """Draw a zigzag lightning bolt."""
    pts = [
        (cx + s(2, sz), cy),
        (cx - s(4, sz), cy + s(12, sz)),
        (cx + s(2, sz), cy + s(10, sz)),
        (cx - s(6, sz), cy + s(24, sz)),
    ]
    draw.line(pts, fill=LIGHTNING, width=max(1, si(3, sz)))


def draw_snowflake(draw, cx, cy, r, sz):
    """Draw a small asterisk snowflake."""
    for angle_deg in [0, 60, 120]:
        a = math.radians(angle_deg)
        dx = math.cos(a) * r
        dy = math.sin(a) * r
        draw.line([(cx - dx, cy - dy), (cx + dx, cy + dy)],
                  fill=SNOW_W, width=max(1, si(1.5, sz)))


# ---------------------------------------------------------------------------
# Icon-drawing functions  (each takes draw, sz)
# ---------------------------------------------------------------------------

def icon_clear(draw, sz):
    cx, cy = sz / 2, sz / 2
    draw_sun(draw, cx, cy, s(16, sz), s(28, sz), 12, sz)


def icon_few_clouds(draw, sz):
    # Sun upper-left
    sun_cx, sun_cy = s(28, sz), s(28, sz)
    draw_sun(draw, sun_cx, sun_cy, s(12, sz), s(22, sz), 10, sz)
    # Cloud lower-right, overlapping
    draw_cloud(draw, s(42, sz), s(46, sz), 1.0, CLOUD_L, sz)


def icon_scattered_clouds(draw, sz):
    draw_cloud(draw, s(28, sz), s(30, sz), 0.85, CLOUD_M, sz)
    draw_cloud(draw, s(42, sz), s(44, sz), 1.0, CLOUD_L, sz)


def icon_broken_clouds(draw, sz):
    draw_cloud(draw, s(38, sz), s(36, sz), 1.2, CLOUD_D, sz)
    # Darker patch
    draw_cloud(draw, s(32, sz), s(38, sz), 0.6, (0x60, 0x68, 0x70, 255), sz)


def icon_overcast(draw, sz):
    draw_cloud(draw, s(40, sz), s(38, sz), 1.5, CLOUD_D, sz)


def icon_shower_rain(draw, sz):
    cloud_cy = s(30, sz)
    draw_cloud(draw, s(40, sz), cloud_cy, 1.1, CLOUD_M, sz)
    draw_rain_lines(draw, s(40, sz), cloud_cy + s(14, sz), 3, 14, sz)


def icon_rain(draw, sz):
    cloud_cy = s(28, sz)
    draw_cloud(draw, s(40, sz), cloud_cy, 1.2, CLOUD_D, sz)
    draw_rain_lines(draw, s(40, sz), cloud_cy + s(14, sz), 5, 18, sz)


def icon_drizzle(draw, sz):
    cloud_cy = s(30, sz)
    draw_cloud(draw, s(40, sz), cloud_cy, 1.0, CLOUD_L, sz)
    # Small dots
    spacing = s(14, sz)
    start_x = s(40, sz) - spacing
    dot_y = cloud_cy + s(18, sz)
    r = max(1, s(2.5, sz))
    for i in range(3):
        x = start_x + i * spacing
        draw.ellipse([x - r, dot_y - r, x + r, dot_y + r], fill=RAIN_BLUE)


def icon_thunderstorm(draw, sz):
    cloud_cy = s(26, sz)
    draw_cloud(draw, s(40, sz), cloud_cy, 1.2, CLOUD_D, sz)
    draw_lightning(draw, s(40, sz), cloud_cy + s(14, sz), sz)


def icon_snow(draw, sz):
    cloud_cy = s(28, sz)
    draw_cloud(draw, s(40, sz), cloud_cy, 1.1, CLOUD_M, sz)
    spacing = s(14, sz)
    start_x = s(40, sz) - spacing * 1.5
    flake_y = cloud_cy + s(22, sz)
    flake_r = s(4, sz)
    for i in range(4):
        draw_snowflake(draw, start_x + i * spacing, flake_y, flake_r, sz)


def icon_atmosphere(draw, sz):
    """3-4 horizontal wavy lines."""
    for i, y_off in enumerate([20, 34, 48, 60]):
        y = s(y_off, sz)
        pts = []
        for x in range(0, sz + 1, max(1, si(2, sz))):
            pts.append((x, y + math.sin(x / s(10, sz) + i) * s(4, sz)))
        if len(pts) >= 2:
            draw.line(pts, fill=CLOUD_L, width=max(1, si(2, sz)))


def icon_mist(draw, sz):
    """Light semi-transparent horizontal bars."""
    bar_color = (0xC8, 0xD0, 0xD8, 120)
    for y_off in [16, 28, 40, 52, 64]:
        y = s(y_off, sz)
        h = max(1, s(4, sz))
        draw.rectangle([s(8, sz), y, s(72, sz), y + h], fill=bar_color)


def icon_fog(draw, sz):
    """Dense horizontal gray bars, more opaque."""
    bar_color = (0x90, 0x98, 0xA0, 200)
    for y_off in [12, 22, 32, 42, 52, 62]:
        y = s(y_off, sz)
        h = max(1, s(6, sz))
        draw.rectangle([s(6, sz), y, s(74, sz), y + h], fill=bar_color)


# ---------------------------------------------------------------------------
# Map names -> draw funcs
# ---------------------------------------------------------------------------
ICONS = {
    "clear":             icon_clear,
    "few_clouds":        icon_few_clouds,
    "scattered_clouds":  icon_scattered_clouds,
    "broken_clouds":     icon_broken_clouds,
    "overcast":          icon_overcast,
    "shower_rain":       icon_shower_rain,
    "rain":              icon_rain,
    "drizzle":           icon_drizzle,
    "thunderstorm":      icon_thunderstorm,
    "snow":              icon_snow,
    "atmosphere":        icon_atmosphere,
    "mist":              icon_mist,
    "fog":               icon_fog,
}

# ---------------------------------------------------------------------------
# RGB565 BMP writer
# ---------------------------------------------------------------------------

def rgb565(r, g, b):
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    return (r5 << 11) | (g6 << 5) | b5


def write_rgb565_bmp(img, path):
    """Write an RGBA PIL Image as a 16-bit RGB565 BMP with BI_BITFIELDS."""
    w, h = img.size
    # BMP rows are bottom-to-top; each row is 2*w bytes, padded to 4-byte boundary
    row_bytes = w * 2
    row_pad = (4 - (row_bytes % 4)) % 4
    padded_row = row_bytes + row_pad

    # Header sizes
    # BMP file header: 14 bytes
    # DIB header (BITMAPINFOHEADER): 40 bytes
    # Color masks: 12 bytes (3 x DWORD)
    headers_size = 14 + 40 + 12
    pixel_data_size = padded_row * h
    file_size = headers_size + pixel_data_size

    pixels = img.load()

    with open(path, "wb") as f:
        # --- BMP File Header (14 bytes) ---
        f.write(b'BM')
        f.write(struct.pack('<I', file_size))   # file size
        f.write(struct.pack('<HH', 0, 0))       # reserved
        f.write(struct.pack('<I', headers_size)) # offset to pixel data

        # --- DIB Header (BITMAPINFOHEADER, 40 bytes) ---
        f.write(struct.pack('<I', 40))           # header size
        f.write(struct.pack('<i', w))            # width
        f.write(struct.pack('<i', h))            # height (positive = bottom-up)
        f.write(struct.pack('<H', 1))            # planes
        f.write(struct.pack('<H', 16))           # bits per pixel
        f.write(struct.pack('<I', 3))            # compression = BI_BITFIELDS
        f.write(struct.pack('<I', pixel_data_size))  # image size
        f.write(struct.pack('<i', 2835))         # X ppm (~72 dpi)
        f.write(struct.pack('<i', 2835))         # Y ppm
        f.write(struct.pack('<I', 0))            # colors used
        f.write(struct.pack('<I', 0))            # important colors

        # --- Color masks (12 bytes) ---
        f.write(struct.pack('<I', 0xF800))       # red mask
        f.write(struct.pack('<I', 0x07E0))       # green mask
        f.write(struct.pack('<I', 0x001F))       # blue mask

        # --- Pixel data (bottom-up) ---
        pad_bytes = b'\x00' * row_pad
        for y in range(h - 1, -1, -1):
            row_data = bytearray(row_bytes)
            for x in range(w):
                r, g, b, a = pixels[x, y]
                # Alpha-blend onto background
                if a < 255:
                    af = a / 255.0
                    r = int(r * af + BG[0] * (1 - af))
                    g = int(g * af + BG[1] * (1 - af))
                    b = int(b * af + BG[2] * (1 - af))
                val = rgb565(r, g, b)
                struct.pack_into('<H', row_data, x * 2, val)
            f.write(row_data)
            if row_pad:
                f.write(pad_bytes)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    count = 0
    for name, draw_fn in ICONS.items():
        for sz in SIZES:
            img = Image.new("RGBA", (sz, sz), BG)
            draw = ImageDraw.Draw(img, "RGBA")
            draw_fn(draw, sz)

            fname = f"{name}_{sz}.bmp"
            fpath = os.path.join(OUT_DIR, fname)
            write_rgb565_bmp(img, fpath)
            count += 1
            file_size = os.path.getsize(fpath)
            print(f"  {fname:<30s}  {file_size:>6d} bytes")

    print(f"\nGenerated {count} icon files in {OUT_DIR}")


if __name__ == "__main__":
    main()
