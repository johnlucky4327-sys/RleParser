#!/usr/bin/env python3
"""
RedMoon RLE file parser.
Scans a folder for .rle files and dumps each surface as a BMP image.

Usage:
    python rle_dump.py <rle_folder> [--ext .rle]

Given folder X, creates X_dump/ alongside X with mirrored directory structure.
Each .rle file produces a subfolder named after the file (minus extension),
containing one BMP per surface index: 00000.bmp, 00001.bmp, ...
"""

import argparse
import os
import struct
import sys
import time

# ── RLE file constants ──────────────────────────────────────────────
HEADER_LEN = 14
RLEHDR_SIZE = 32  # 8 x int32: SrcX, SrcY, Wdh, Hgh, AdjX, AdjY, PixFmt, DataPtr

# ── pixel format constants (from DigiFX_old.h) ─────────────────────
PIXFMT_555 = 1
PIXFMT_565 = 2
PIXFMT_BGR = 128


def read_u4(data, off):
    return struct.unpack_from("<I", data, off)[0], off + 4


def read_i4(data, off):
    return struct.unpack_from("<i", data, off)[0], off + 4


def parse_rle_file(data):
    """Parse an RLE file and return a list of decoded RGBA images.

    Each element is a dict with keys:
        src_x, src_y, wdh, hgh, adj_x, adj_y, pix_fmt,
        tex_w, tex_h, pixels (bytes, RGBA top-to-bottom)
    or None if the surface is empty / invalid.
    """
    if len(data) < HEADER_LEN + 8:
        return []

    off = HEADER_LEN
    _version, off = read_u4(data, off)
    count, off = read_u4(data, off)

    # Read offset table
    offsets = []
    for i in range(count):
        if off + 4 > len(data):
            raise ValueError(f"offset table truncated at entry {i}/{count}, file size {len(data)}")
        pos, off = read_u4(data, off)
        offsets.append(pos)

    results = []
    for idx in range(count):
        try:
            surf = decode_surface(data, offsets[idx])
            results.append(surf)
        except Exception as e:
            print(f"  WARNING: surface {idx} decode failed: {e}")
            results.append(None)

    return results


def decode_surface(data, position):
    """Decode a single surface at the given file position."""
    if position == 0 or position + 4 + RLEHDR_SIZE > len(data):
        return None
    off = position
    length, off = read_u4(data, off)
    if length < RLEHDR_SIZE:
        return None

    end_offset = min(len(data), position + 4 + length)

    src_x, off = read_i4(data, off)
    src_y, off = read_i4(data, off)
    wdh, off = read_i4(data, off)
    hgh, off = read_i4(data, off)
    adj_x, off = read_i4(data, off)
    adj_y, off = read_i4(data, off)
    pix_fmt, off = read_i4(data, off)
    _data_ptr, off = read_i4(data, off)  # unused runtime pointer

    if wdh <= 0 or hgh <= 0 or wdh > 4096 or hgh > 4096:
        return None

    pad_x = max(0, src_x)
    pad_y = max(0, src_y)
    tex_w = wdh + pad_x
    tex_h = hgh + pad_y

    is_bgr = (pix_fmt & PIXFMT_BGR) != 0
    base_fmt = pix_fmt & 0x7F

    pixels, has_visible = decode_pixels(data, off, end_offset, wdh, hgh, pad_x, pad_y, tex_w, tex_h, base_fmt, is_bgr)

    if not has_visible:
        return None

    return {
        "src_x": src_x, "src_y": src_y,
        "wdh": wdh, "hgh": hgh,
        "adj_x": adj_x, "adj_y": adj_y,
        "pix_fmt": pix_fmt,
        "tex_w": tex_w, "tex_h": tex_h,
        "pixels": pixels,
    }


def decode_pixels(data, start_offset, end_offset, wdh, hgh, pad_x, pad_y, tex_w, tex_h, base_fmt, is_bgr):
    """Decode RLE-compressed pixel stream into RGBA bytes (top-to-bottom rows).
    Returns (pixels_bytes, has_visible).
    """
    buf = bytearray(tex_w * tex_h * 4)
    curx = 0
    cury = 0
    off = start_offset
    has_visible = False

    while off < end_offset:
        if off >= len(data):
            break
        flag = data[off]
        off += 1

        if flag == 1:  # COLOR RUN
            if off + 4 > end_offset:
                break
            color_count, off = read_u4(data, off)
            for _ in range(color_count):
                if cury >= hgh:
                    break
                if off + 2 > end_offset:
                    break
                tmp_color = data[off] | (data[off + 1] << 8)
                off += 2

                if tmp_color == 0:
                    curx += 1
                    continue

                has_visible = True
                if base_fmt == PIXFMT_555:
                    r = (tmp_color >> 10) & 0x1F
                    g = (tmp_color >> 5) & 0x1F
                    b = tmp_color & 0x1F
                    r = (r * 255) // 31
                    g = (g * 255) // 31
                    b = (b * 255) // 31
                else:  # RGB565 (default)
                    r = (tmp_color >> 11) & 0x1F
                    g = (tmp_color >> 5) & 0x3F
                    b = tmp_color & 0x1F
                    r = (r * 255) // 31
                    g = (g * 255) // 63
                    b = (b * 255) // 31

                if is_bgr:
                    r, b = b, r

                wx = curx + pad_x
                wy = cury + pad_y
                if 0 <= wx < tex_w and 0 <= wy < tex_h:
                    pi = (wy * tex_w + wx) * 4
                    buf[pi] = r
                    buf[pi + 1] = g
                    buf[pi + 2] = b
                    buf[pi + 3] = 255
                curx += 1

        elif flag == 2:  # SKIP
            if off + 4 > end_offset:
                break
            skip, off = read_i4(data, off)
            curx += skip // 2

        elif flag == 3:  # NEWLINE
            cury += 1
            if cury >= hgh:
                break

        else:  # END or unknown
            break

    return bytes(buf), has_visible


def write_bmp(path, width, height, rgba_pixels):
    """Write a 24-bit BMP. Transparent pixels become magenta (255,0,255)."""
    row_bytes = width * 3
    row_pad = (4 - row_bytes % 4) % 4
    row_size = row_bytes + row_pad
    pixel_data_size = row_size * height
    header_size = 14 + 40  # BMP header + BITMAPINFOHEADER
    file_size = header_size + pixel_data_size

    out = bytearray(file_size)
    # BMP file header
    out[0:2] = b"BM"
    struct.pack_into("<I", out, 2, file_size)
    struct.pack_into("<I", out, 10, header_size)
    # BITMAPINFOHEADER
    struct.pack_into("<I", out, 14, 40)
    struct.pack_into("<i", out, 18, width)
    struct.pack_into("<i", out, 22, height)
    struct.pack_into("<H", out, 26, 1)   # planes
    struct.pack_into("<H", out, 28, 24)  # bpp
    struct.pack_into("<I", out, 34, pixel_data_size)
    struct.pack_into("<i", out, 38, 2835)
    struct.pack_into("<i", out, 42, 2835)

    # Pixel data — BMP is bottom-up, buffer is top-to-bottom
    dst = header_size
    for y in range(height - 1, -1, -1):
        src_row = y * width * 4
        for x in range(width):
            pi = src_row + x * 4
            a = rgba_pixels[pi + 3]
            if a == 0:
                out[dst] = 0xFF; out[dst + 1] = 0x00; out[dst + 2] = 0xFF
            else:
                out[dst] = rgba_pixels[pi + 2]      # B
                out[dst + 1] = rgba_pixels[pi + 1]  # G
                out[dst + 2] = rgba_pixels[pi]       # R
            dst += 3
        dst += row_pad

    with open(path, "wb") as f:
        f.write(out)


def process_file(rle_path, out_dir, rel_path):
    """Parse one .rle file, skip all-transparent files, batch write."""
    with open(rle_path, "rb") as f:
        data = f.read()

    try:
        surfaces = parse_rle_file(data)
    except ValueError as e:
        print(f"  ERROR: {rel_path}: {e}")
        return 0
    if not surfaces:
        return 0

    # Collect visible surfaces: (index, width, height, pixels)
    visible = []
    for idx, surf in enumerate(surfaces):
        if surf is None:
            continue
        w, h = surf["tex_w"], surf["tex_h"]
        if w > 0 and h > 0:
            visible.append((idx, w, h, surf["pixels"]))

    # Skip if entire rle file has no visible surfaces
    if not visible:
        return 0

    os.makedirs(out_dir, exist_ok=True)
    for idx, w, h, pixels in visible:
        write_bmp(os.path.join(out_dir, f"{idx:05d}.bmp"), w, h, pixels)

    print(f"  {rel_path} -> {len(visible)} images")
    return len(visible)


def main():
    parser = argparse.ArgumentParser(description="RedMoon RLE → BMP dumper")
    parser.add_argument("folder", help="Input folder containing RLE files")
    parser.add_argument("--ext", default=".rle", help="File extension to scan (default: .rle)")
    args = parser.parse_args()

    src_dir = os.path.abspath(args.folder)
    if not os.path.isdir(src_dir):
        print(f"Error: {src_dir} is not a directory")
        sys.exit(1)

    ext = args.ext.lower()
    base_name = os.path.basename(src_dir)
    dump_dir = os.path.join(os.path.dirname(src_dir), base_name + "_dump")

    t0 = time.time()

    total_files = 0
    total_images = 0

    for root, _dirs, files in os.walk(src_dir):
        for fname in sorted(files):
            if not fname.lower().endswith(ext):
                continue
            if fname.startswith("."):
                continue

            rel_path = os.path.relpath(os.path.join(root, fname), src_dir)
            stem = os.path.splitext(rel_path)[0]
            out_dir = os.path.join(dump_dir, stem)
            rle_path = os.path.join(root, fname)

            n = process_file(rle_path, out_dir, rel_path)
            total_files += 1
            total_images += n

    elapsed = time.time() - t0
    print(f"\nDone: {total_files} files, {total_images} images → {dump_dir} ({elapsed:.1f}s)")


if __name__ == "__main__":
    main()
