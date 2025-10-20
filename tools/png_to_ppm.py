#!/usr/bin/env python3
"""
png_to_ppm.py - Convert a PNG image to an uncompressed PPM (P6) file.

Features:
- Outputs PPM P6 (binary) with maxval 255 and no compression.
- Optional scaling via --scale, or explicit --width/--height (aspect preserved
  when only one is given).
- Uses Pillow if available. Falls back to system tools if found (sips/magick/ffmpeg),
  but always writes a proper PPM P6 when using Pillow.

Usage:
  python tools/png_to_ppm.py in.png out.ppm [--scale 2.0]
  python tools/png_to_ppm.py in.png out.ppm --width 320
  python tools/png_to_ppm.py in.png out.ppm --height 200
  python tools/png_to_ppm.py in.png out.ppm --width 320 --height 200
"""

import argparse
import os
import sys
import subprocess
from shutil import which


def write_ppm_p6(path, width, height, rgb_bytes):
    with open(path, "wb") as f:
        header = f"P6\n{width} {height}\n255\n".encode("ascii")
        f.write(header)
        f.write(rgb_bytes)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def with_pillow(in_path, out_path, scale=None, width=None, height=None):
    try:
        from PIL import Image
    except Exception as e:
        return False, f"Pillow not available: {e}"

    img = Image.open(in_path)
    img = img.convert("RGB")  # drop alpha if present

    w, h = img.size
    if scale is not None:
        if scale <= 0:
            return False, "--scale must be > 0"
        w = max(1, int(round(w * scale)))
        h = max(1, int(round(h * scale)))
        img = img.resize((w, h), resample=Image.LANCZOS)
    else:
        if width is not None and height is not None:
            w = clamp(int(width), 1, 1 << 16)
            h = clamp(int(height), 1, 1 << 16)
            img = img.resize((w, h), resample=Image.LANCZOS)
        elif width is not None:
            w = clamp(int(width), 1, 1 << 16)
            h = max(1, int(round(img.size[1] * (w / img.size[0]))))
            img = img.resize((w, h), resample=Image.LANCZOS)
        elif height is not None:
            h = clamp(int(height), 1, 1 << 16)
            w = max(1, int(round(img.size[0] * (h / img.size[1]))))
            img = img.resize((w, h), resample=Image.LANCZOS)

    rgb_bytes = img.tobytes()  # RGB order
    write_ppm_p6(out_path, img.size[0], img.size[1], rgb_bytes)
    return True, None


def try_system_tool(in_path, out_path, scale=None, width=None, height=None):
    # As a fallback, try common tools. This path may produce a PPM via the tool
    # directly; it won’t enforce header format beyond what the tool outputs.
    # Prefer ImageMagick if available.
    cmd = None
    if which("magick") or which("convert"):
        prog = "magick" if which("magick") else "convert"
        size_arg = None
        if scale is not None:
            size_arg = f"{int(round(scale * 100))}%"
        elif width is not None and height is not None:
            size_arg = f"{int(width)}x{int(height)}!"
        elif width is not None:
            size_arg = f"{int(width)}"
        elif height is not None:
            size_arg = f"x{int(height)}"
        cmd = [prog, in_path]
        if size_arg:
            cmd += ["-resize", size_arg]
        cmd += ["ppm:" + out_path]
    elif sys.platform == "darwin" and which("sips"):
        # macOS sips can output PPM
        tmp = out_path
        cmd = ["sips", "-s", "format", "ppm"]
        if scale is not None:
            # sips scale; approximate via width
            from PIL import Image  # may fail; ignore then

            try:
                im = Image.open(in_path)
                nw = max(1, int(round(im.size[0] * scale)))
                cmd += ["-z", str(max(1, int(round(im.size[1] * scale)))), str(nw)]
            except Exception:
                pass
        elif width is not None and height is not None:
            cmd += ["-z", str(int(height)), str(int(width))]
        elif width is not None:
            cmd += ["--resampleWidth", str(int(width))]
        elif height is not None:
            cmd += ["--resampleHeight", str(int(height))]
        cmd += [in_path, "--out", tmp]
    elif which("ffmpeg"):
        size_arg = None
        if scale is not None:
            size_arg = f"scale=iw*{float(scale)}:ih*{float(scale)}:flags=lanczos"
        elif width is not None and height is not None:
            size_arg = f"scale={int(width)}:{int(height)}:flags=lanczos"
        elif width is not None:
            size_arg = f"scale={int(width)}:-2:flags=lanczos"
        elif height is not None:
            size_arg = f"scale=-2:{int(height)}:flags=lanczos"
        cmd = ["ffmpeg", "-y", "-i", in_path]
        if size_arg:
            cmd += ["-vf", size_arg]
        cmd += [out_path]

    if not cmd:
        return False, "No suitable system tool found (install Pillow or ImageMagick)"

    try:
        subprocess.check_call(cmd)
        return True, None
    except subprocess.CalledProcessError as e:
        return False, f"Tool failed: {e}"


def main(argv):
    ap = argparse.ArgumentParser(description="Convert PNG to PPM (P6, uncompressed).")
    ap.add_argument("input", help="Input PNG path")
    ap.add_argument("output", help="Output PPM path")
    ap.add_argument(
        "--scale", type=float, default=None, help="Uniform scale factor (e.g., 2.0)"
    )
    ap.add_argument(
        "--width",
        type=int,
        default=None,
        help="Target width (aspect preserved if height omitted)",
    )
    ap.add_argument(
        "--height",
        type=int,
        default=None,
        help="Target height (aspect preserved if width omitted)",
    )
    args = ap.parse_args(argv[1:])

    if not os.path.exists(args.input):
        print(f"Input not found: {args.input}", file=sys.stderr)
        return 2

    ok, err = with_pillow(args.input, args.output, args.scale, args.width, args.height)
    if ok:
        return 0
    # Fallback
    ok2, err2 = try_system_tool(
        args.input, args.output, args.scale, args.width, args.height
    )
    if ok2:
        return 0
    print(err or err2, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
