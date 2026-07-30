#!/usr/bin/env python3
"""Screenshot a Home Assistant Lovelace view and save it as a 1-bit PNG.

Rendered at 2x resolution and downscaled with a sharp filter so text/icon
edges anti-alias cleanly, then reduced to pure black/white with a hard
luminance threshold (matching what ESPHome's online_image does at runtime).
Floyd-Steinberg dithering was tried here first but scatters the anti-aliased
edges of small text into black/white noise, which reads as fuzzy on a
text-heavy dashboard -- dithering suits photographic gradients, not UI chrome.

The dashboard itself should use the "epaper" theme (see epaper_theme.yaml)
so text/icons are already pure black on white going into this threshold --
that leaves only true anti-aliasing (a pixel or two at each edge) for the
threshold to resolve, rather than HA's default muted secondary-text gray.
"""
import argparse
import io
import os
import sys
import tempfile

from PIL import Image, ImageFilter
from playwright.sync_api import sync_playwright


SUPERSAMPLE = 2
THRESHOLD = 150


def render(url: str, width: int, height: int, output_path: str) -> None:
    with sync_playwright() as p:
        browser = p.chromium.launch(args=["--no-sandbox"])
        page = browser.new_page(
            viewport={"width": width, "height": height},
            device_scale_factor=SUPERSAMPLE,
        )
        page.goto(url, wait_until="networkidle", timeout=30000)
        page.wait_for_timeout(1500)  # let cards/charts finish animating in
        png_bytes = page.screenshot(type="png")
        browser.close()

    image = Image.open(io.BytesIO(png_bytes)).convert("L")
    # Thicken thin glyph/icon strokes before downscaling, or a single-pixel-wide
    # stroke at 2x can get averaged away to near-white by the LANCZOS resize below.
    image = image.filter(ImageFilter.MinFilter(3))
    if image.size != (width, height):
        image = image.resize((width, height), Image.LANCZOS)
    bw = image.point(lambda p: 255 if p >= THRESHOLD else 0, mode="L").convert(
        "1", dither=Image.NONE
    )

    directory = os.path.dirname(output_path) or "."
    fd, tmp_path = tempfile.mkstemp(dir=directory, suffix=".png")
    try:
        with os.fdopen(fd, "wb") as f:
            bw.save(f, format="PNG")
        os.replace(tmp_path, output_path)  # atomic so ESPHome never reads a partial file
    except Exception:
        os.unlink(tmp_path)
        raise


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--url", required=True)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    try:
        render(args.url, args.width, args.height, args.output)
    except Exception as exc:  # noqa: BLE001 - top-level render failure, log and let run.sh retry
        print(f"[render.py] render failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
