#!/usr/bin/env python3
"""Convert any PIL-readable image (BMP, PNG, JPEG, ...) to a C++ RGB565 header.

The output is a `#pragma once` header that defines, inside namespace `assets`:

    inline constexpr int      k<Name>Width;
    inline constexpr int      k<Name>Height;
    inline constexpr uint16_t k<Name>Pixels[Width * Height];

Pixels are stored in host (little-endian) RGB565, row-major, top-down.

For 16bpp BMPs the file is decoded directly (PIL's reader for 16bpp
BI_BITFIELDS is unreliable and produces garbled colors). Other formats
are routed through PIL.

When --size N is given, the source is scaled and *center-cropped* so the
NxN destination square is fully filled while preserving aspect ratio
(cover behavior — excess pixels along the longer axis are discarded).

Usage:
    python tools/image_to_header.py <input> <output.h> <name> [--size N]

Example (matches the eye1 conversion checked into the repo):
    python tools/image_to_header.py src/assets/eye1.bmp src/assets/eye1_data.h Eye1 --size 160
"""

import argparse
import os
import struct
from pathlib import Path
from typing import Iterable

from PIL import Image


def rgb_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def _mask_shift(mask: int) -> tuple[int, int]:
    """For a contiguous bitmask, return (lsb_shift, bit_count)."""
    if mask == 0:
        return 0, 0
    shift = 0
    while (mask >> shift) & 1 == 0:
        shift += 1
    bits = 0
    m = mask >> shift
    while m & 1:
        bits += 1
        m >>= 1
    return shift, bits


def _scale_to_8(value: int, bits: int) -> int:
    """Scale a `bits`-wide channel value up to 8 bits."""
    if bits == 0 or bits >= 8:
        return value & 0xFF
    # Replicate high bits into the low bits — standard BMP-style scaling.
    return ((value << (8 - bits)) | (value >> max(0, 2 * bits - 8))) & 0xFF


def _decode_bmp_native(path: Path) -> Image.Image:
    """Decode a BMP file directly when PIL's 16bpp path is unreliable.

    Returns a PIL RGB image. Falls back to PIL.Image.open() for anything
    other than the cases we hand-decode (currently: 16bpp BI_RGB and
    16bpp BI_BITFIELDS).
    """
    data = path.read_bytes()
    if data[:2] != b"BM":
        return Image.open(path).convert("RGB")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size     = struct.unpack_from("<I", data, 14)[0]
    width        = struct.unpack_from("<i", data, 18)[0]
    height_raw   = struct.unpack_from("<i", data, 22)[0]
    bpp          = struct.unpack_from("<H", data, 28)[0]
    compression  = struct.unpack_from("<I", data, 30)[0]

    if bpp != 16:
        return Image.open(path).convert("RGB")

    height    = abs(height_raw)
    top_down  = height_raw < 0
    row_bytes = ((width * bpp + 31) // 32) * 4

    if compression == 0:
        # BI_RGB: 16bpp is RGB555 (high bit unused).
        rmask, gmask, bmask = 0x7C00, 0x03E0, 0x001F
    elif compression == 3:
        # BI_BITFIELDS: masks live at the end of the DIB header.
        rmask = struct.unpack_from("<I", data, 14 + 40)[0]
        gmask = struct.unpack_from("<I", data, 14 + 44)[0]
        bmask = struct.unpack_from("<I", data, 14 + 48)[0]
    else:
        # Unsupported 16bpp variant — defer to PIL and hope for the best.
        return Image.open(path).convert("RGB")

    rsh, rbits = _mask_shift(rmask)
    gsh, gbits = _mask_shift(gmask)
    bsh, bbits = _mask_shift(bmask)

    img = Image.new("RGB", (width, height))
    out = img.load()
    for y in range(height):
        # BMP rows are stored bottom-up unless height is negative.
        src_y    = y if top_down else (height - 1 - y)
        row_off  = pixel_offset + src_y * row_bytes
        for x in range(width):
            v = struct.unpack_from("<H", data, row_off + x * 2)[0]
            r = _scale_to_8((v & rmask) >> rsh, rbits)
            g = _scale_to_8((v & gmask) >> gsh, gbits)
            b = _scale_to_8((v & bmask) >> bsh, bbits)
            out[x, y] = (r, g, b)
    return img


def load_image(src: Path) -> Image.Image:
    if src.suffix.lower() == ".bmp":
        return _decode_bmp_native(src)
    return Image.open(src).convert("RGB")


def cover_resize(img: Image.Image, size: int) -> Image.Image:
    """Scale + center-crop so the result is exactly size x size and fills it,
    preserving the source aspect ratio (CSS `object-fit: cover` semantics)."""
    src_w, src_h = img.size
    scale = size / min(src_w, src_h)
    new_w = max(size, round(src_w * scale))
    new_h = max(size, round(src_h * scale))
    img = img.resize((new_w, new_h), Image.LANCZOS)
    left = (new_w - size) // 2
    top  = (new_h - size) // 2
    return img.crop((left, top, left + size, top + size))


def convert(src: Path, dst: Path, name: str, size: int | None) -> None:
    img = load_image(src)
    src_w, src_h = img.size
    if size is not None:
        img = cover_resize(img, size)
    # Pre-rotate 90° CW so the on-panel orientation comes out upright with
    # the eyes_debug renderer's drawBitmap16Data path (PIL's ROTATE_270 is
    # a 90° clockwise rotation).
    img = img.transpose(Image.ROTATE_270)
    w, h = img.size

    px = img.load()
    values: Iterable[int] = (
        rgb_to_rgb565(*px[x, y]) for y in range(h) for x in range(w)
    )
    values = list(values)

    rel_src = Path(os.path.relpath(src, Path.cwd())).as_posix()
    lines = [
        f"// Auto-generated from {rel_src} by tools/image_to_header.py -- do not edit by hand.",
        f"// Source image was {src_w}x{src_h}; output is {w}x{h} RGB565 (host byte order).",
        "#pragma once",
        "",
        "#include <stdint.h>",
        "",
        "namespace assets {",
        f"inline constexpr int k{name}Width  = {w};",
        f"inline constexpr int k{name}Height = {h};",
        "",
        f"inline constexpr uint16_t k{name}Pixels[k{name}Width * k{name}Height] = {{",
    ]
    for i in range(0, len(values), 16):
        row = ", ".join(f"0x{v:04X}" for v in values[i : i + 16])
        lines.append(f"    {row},")
    lines += ["};", "}  // namespace assets", ""]

    dst.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {dst} ({w}x{h}, {len(values) * 2} bytes pixel data)")


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("input", type=Path, help="Source image (BMP, PNG, ...)")
    p.add_argument("output", type=Path, help="Destination .h file")
    p.add_argument("name", help="Symbol prefix; e.g. Eye1 -> kEye1Pixels / kEye1Width / kEye1Height")
    p.add_argument("--size", type=int, default=None, help="Resize to NxN (square). Omit to keep native size.")
    args = p.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    convert(args.input, args.output, args.name, args.size)


if __name__ == "__main__":
    main()
