#!/usr/bin/env python3
import subprocess
import tempfile
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "report_assets"
OUTPUT_WIDTH = 2400
FLOW_BG = (248, 250, 252)

SCREENSHOT_SPECS = [
    ("local_screenshots/01_ctest_all_passed.png", "screenshot_ctest.svg.png", 2.528),
    ("local_screenshots/03_dig_local_test1.png", "screenshot_local_a.svg.png", 5.291),
    ("local_screenshots/04_dig_blocked_nxdomain.png", "screenshot_blocked.svg.png", 1.667),
    ("local_screenshots/05_dig_relay_cache.png", "screenshot_relay_cache.svg.png", 3.116),
    ("local_screenshots/02_dnsrelay_server_log.png", "screenshot_server_log.svg.png", 2.420),
]

FLOW_SPECS = [
    ("flow_architecture.svg", "flow_architecture.svg.png", 2.741),
    ("flow_client.svg", "flow_client.svg.png", 2.741),
    ("flow_upstream.svg", "flow_upstream.svg.png", 3.033),
]


def target_size(ratio):
    return OUTPUT_WIDTH, max(1, round(OUTPUT_WIDTH / ratio))


def sample_background(image):
    rgb = image.convert("RGB")
    box = max(4, min(rgb.width, rgb.height) // 20)
    total_r = 0
    total_g = 0
    total_b = 0
    total_count = 0
    corners = [
        (0, 0),
        (rgb.width - box, 0),
        (0, rgb.height - box),
        (rgb.width - box, rgb.height - box),
    ]
    for x0, y0 in corners:
        crop = rgb.crop((x0, y0, x0 + box, y0 + box))
        pixels = crop.load()
        for x in range(crop.width):
            for y in range(crop.height):
                r, g, b = pixels[x, y]
                total_r += r
                total_g += g
                total_b += b
                total_count += 1
    return (
        total_r // total_count,
        total_g // total_count,
        total_b // total_count,
    )


def contain_on_canvas(source_path, dest_path, ratio, background):
    image = Image.open(source_path).convert("RGBA")
    target_w, target_h = target_size(ratio)
    scale = min(target_w / image.width, target_h / image.height)
    resized = image.resize(
        (max(1, round(image.width * scale)), max(1, round(image.height * scale))),
        Image.Resampling.LANCZOS,
    )
    canvas = Image.new("RGBA", (target_w, target_h), background + (255,))
    offset = ((target_w - resized.width) // 2, (target_h - resized.height) // 2)
    canvas.alpha_composite(resized, offset)
    canvas.convert("RGB").save(dest_path, optimize=True)


def render_svg_temp(svg_path):
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as handle:
        temp_png = Path(handle.name)
    try:
        subprocess.run(
            ["rsvg-convert", "-w", "2200", "-o", str(temp_png), str(svg_path)],
            cwd=str(ROOT),
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return temp_png
    except Exception:
        temp_png.unlink(missing_ok=True)
        raise


def render_screenshots():
    for source_name, output_name, ratio in SCREENSHOT_SPECS:
        source_path = ASSETS / source_name
        output_path = ASSETS / output_name
        background = sample_background(Image.open(source_path))
        contain_on_canvas(source_path, output_path, ratio, background)


def render_flows():
    for svg_name, output_name, ratio in FLOW_SPECS:
        svg_path = ASSETS / svg_name
        output_path = ASSETS / output_name
        temp_png = render_svg_temp(svg_path)
        try:
            contain_on_canvas(temp_png, output_path, ratio, FLOW_BG)
        finally:
            temp_png.unlink(missing_ok=True)


def main():
    render_screenshots()
    render_flows()


if __name__ == "__main__":
    main()
