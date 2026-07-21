#!/usr/bin/env python3
"""Screenshot a Home Assistant Lovelace view and save it as a dithered 1-bit PNG.

Pixels are converted to pure black/white via Floyd-Steinberg dithering here,
because ESPHome's online_image component only does a hard luminance threshold
at runtime (no dithering) -- doing it here avoids visible banding on the
1-bit e-paper panel.
"""
import argparse
import io
import os
import sys
import tempfile

from PIL import Image
from playwright.sync_api import sync_playwright


def render(url: str, width: int, height: int, output_path: str) -> None:
    with sync_playwright() as p:
        browser = p.chromium.launch(args=["--no-sandbox"])
        page = browser.new_page(viewport={"width": width, "height": height})
        page.goto(url, wait_until="networkidle", timeout=30000)
        page.wait_for_timeout(1500)  # let cards/charts finish animating in
        png_bytes = page.screenshot(type="png")
        browser.close()

    image = Image.open(io.BytesIO(png_bytes)).convert("L")
    if image.size != (width, height):
        image = image.resize((width, height))
    dithered = image.convert("1")  # Pillow dithers L->1 with Floyd-Steinberg by default

    directory = os.path.dirname(output_path) or "."
    fd, tmp_path = tempfile.mkstemp(dir=directory, suffix=".png")
    try:
        with os.fdopen(fd, "wb") as f:
            dithered.save(f, format="PNG")
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
