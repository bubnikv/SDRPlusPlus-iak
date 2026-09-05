#!/usr/bin/env python3
"""Generate the SDRIAK electromagnetic-wave logo.

The geometry is defined in a right-handed 3-D basis and projected into the
image with an isometric projection.  SVG output uses only the Python standard
library.  Installing the desktop and Android application assets additionally
requires Pillow; see requirements-logo.txt.
"""

from __future__ import annotations

import argparse
import io
import math
import struct
import sys
from pathlib import Path
from typing import TYPE_CHECKING, Callable, Iterable, Sequence
from xml.etree import ElementTree as ET

if TYPE_CHECKING:
    from PIL import Image as PillowImage
    from PIL import ImageDraw as PillowImageDraw


Point = tuple[float, float]
Color = tuple[int, int, int, int]

CANVAS_SIZE = 1024
BACKGROUND_SIZE = 920
BACKGROUND_RADIUS = 190
WAVE_START = -340.0
WAVE_END = 340.0
WAVE_SEGMENTS = 64
E_AMPLITUDE = 217.5
H_AMPLITUDE = 180.0
ISOMETRIC_X = math.sqrt(3.0) / 2.0
RASTER_SCALE = 4
ICO_SIZES = (256, 128, 64, 48, 32, 24, 16)
ANDROID_LEGACY_SIZES = {
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}

BG: Color = (5, 12, 24, 255)
BORDER: Color = (26, 82, 133, 217)
K_AXIS: Color = (209, 230, 250, 148)
K_ARROW: Color = (209, 230, 250, 199)
E_AXIS: Color = (46, 212, 255, 122)
E_ARROW: Color = (46, 212, 255, 191)
E_FILL: Color = (31, 209, 255, 52)
E_GLOW: Color = (20, 166, 255, 46)
E_STROKE: Color = (31, 209, 255, 255)
H_AXIS: Color = (84, 245, 148, 122)
H_ARROW: Color = (84, 245, 148, 191)
H_FILL: Color = (77, 245, 140, 48)
H_GLOW: Color = (51, 242, 122, 38)
H_STROKE: Color = (77, 245, 140, 255)

SVG_NS = "http://www.w3.org/2000/svg"
ANDROID_NS = "http://schemas.android.com/apk/res/android"
ET.register_namespace("", SVG_NS)
ET.register_namespace("android", ANDROID_NS)


def svg_tag(name: str) -> str:
    return f"{{{SVG_NS}}}{name}"


def fmt(value: float) -> str:
    """Format coordinates deterministically while avoiding negative zero."""
    if abs(value) < 0.0005:
        value = 0.0
    text = f"{value:.3f}".rstrip("0").rstrip(".")
    return text if text else "0"


def css_color(color: Color) -> str:
    return f"#{color[0]:02x}{color[1]:02x}{color[2]:02x}"


def css_opacity(color: Color) -> str:
    return fmt(color[3] / 255.0)


def project(x: float, y: float, z: float) -> Point:
    """Project 3-D E/H/k coordinates onto equal 120-degree screen axes."""
    return (ISOMETRIC_X * (x - z), y - 0.5 * (x + z))


def to_screen(point: Point) -> Point:
    """Convert centered, y-up diagram coordinates to SVG/raster coordinates."""
    return (CANVAS_SIZE / 2.0 + point[0], CANVAS_SIZE / 2.0 - point[1])


def wave_samples() -> list[tuple[float, float]]:
    return [
        (
            WAVE_START + (WAVE_END - WAVE_START) * index / WAVE_SEGMENTS,
            math.sin(2.0 * math.pi * index / WAVE_SEGMENTS),
        )
        for index in range(WAVE_SEGMENTS + 1)
    ]


def baseline_points() -> list[Point]:
    return [project(distance, 0.0, 0.0) for distance, _ in wave_samples()]


def e_wave_points() -> list[Point]:
    return [
        project(distance, E_AMPLITUDE * phase, 0.0)
        for distance, phase in wave_samples()
    ]


def h_wave_points() -> list[Point]:
    return [
        project(distance, 0.0, H_AMPLITUDE * phase)
        for distance, phase in wave_samples()
    ]


def filled_surface(curve: Sequence[Point]) -> list[Point]:
    """Close a wave curve back along its propagation-axis baseline."""
    return [*curve, *reversed(baseline_points())]


def svg_points(points: Iterable[Point]) -> str:
    return " ".join(f"{fmt(x)},{fmt(y)}" for x, y in map(to_screen, points))


def svg_path(points: Iterable[Point], *, close: bool = False) -> str:
    screen_points = list(map(to_screen, points))
    if not screen_points:
        raise ValueError("a path must contain at least one point")
    commands = [f"M {fmt(screen_points[0][0])} {fmt(screen_points[0][1])}"]
    commands.extend(f"L {fmt(x)} {fmt(y)}" for x, y in screen_points[1:])
    if close:
        commands.append("Z")
    return " ".join(commands)


def add_title(element: ET.Element, title: str) -> None:
    ET.SubElement(element, svg_tag("title")).text = title


def add_line(
    parent: ET.Element,
    start: Point,
    end: Point,
    color: Color,
    width: float,
    title: str,
) -> None:
    x1, y1 = to_screen(start)
    x2, y2 = to_screen(end)
    element = ET.SubElement(
        parent,
        svg_tag("line"),
        {
            "x1": fmt(x1),
            "y1": fmt(y1),
            "x2": fmt(x2),
            "y2": fmt(y2),
            "stroke": css_color(color),
            "stroke-opacity": css_opacity(color),
            "stroke-width": fmt(width),
            "stroke-linecap": "round",
        },
    )
    add_title(element, title)


def add_polygon(
    parent: ET.Element,
    points: Sequence[Point],
    color: Color,
    title: str,
) -> None:
    element = ET.SubElement(
        parent,
        svg_tag("polygon"),
        {
            "points": svg_points(points),
            "fill": css_color(color),
            "fill-opacity": css_opacity(color),
        },
    )
    add_title(element, title)


def add_path(
    parent: ET.Element,
    points: Sequence[Point],
    *,
    fill: Color | None,
    stroke: Color | None,
    width: float = 0.0,
    close: bool = False,
    title: str,
) -> None:
    attributes = {
        "d": svg_path(points, close=close),
        "fill": "none" if fill is None else css_color(fill),
    }
    if fill is not None:
        attributes["fill-opacity"] = css_opacity(fill)
        attributes["fill-rule"] = "nonzero"
    if stroke is not None:
        attributes.update(
            {
                "stroke": css_color(stroke),
                "stroke-opacity": css_opacity(stroke),
                "stroke-width": fmt(width),
                "stroke-linecap": "round",
                "stroke-linejoin": "round",
            }
        )
    element = ET.SubElement(parent, svg_tag("path"), attributes)
    add_title(element, title)


def k_arrow_points() -> list[Point]:
    tip = project(390.0, 0.0, 0.0)
    base = project(352.0, 0.0, 0.0)
    perpendicular = (0.5, ISOMETRIC_X)
    return [
        tip,
        (base[0] + 19.0 * perpendicular[0], base[1] + 19.0 * perpendicular[1]),
        (base[0] - 19.0 * perpendicular[0], base[1] - 19.0 * perpendicular[1]),
    ]


def e_arrow_points() -> list[Point]:
    tip = project(WAVE_START, 185.0, 0.0)
    base = project(WAVE_START, 150.0, 0.0)
    return [tip, (base[0] - 16.0, base[1]), (base[0] + 16.0, base[1])]


def h_arrow_points() -> list[Point]:
    tip = project(WAVE_START, 0.0, 155.0)
    base = project(WAVE_START, 0.0, 120.0)
    perpendicular = (0.5, -ISOMETRIC_X)
    return [
        tip,
        (base[0] + 16.0 * perpendicular[0], base[1] + 16.0 * perpendicular[1]),
        (base[0] - 16.0 * perpendicular[0], base[1] - 16.0 * perpendicular[1]),
    ]


def add_svg_label(
    parent: ET.Element,
    text: str,
    center: Point,
    color: Color,
    size: int,
    *,
    italic: bool = False,
) -> None:
    x, y = to_screen(center)
    attributes = {
        "x": fmt(x),
        "y": fmt(y),
        "fill": css_color(color),
        "fill-opacity": css_opacity(color),
        "font-family": "sans-serif",
        "font-size": str(size),
        "font-weight": "700" if not italic else "400",
        "text-anchor": "middle",
        "dominant-baseline": "middle",
    }
    if italic:
        attributes["font-style"] = "italic"
    ET.SubElement(parent, svg_tag("text"), attributes).text = text


def build_svg(*, scientific: bool) -> bytes:
    root = ET.Element(
        svg_tag("svg"),
        {
            "version": "1.1",
            "viewBox": f"0 0 {CANVAS_SIZE} {CANVAS_SIZE}",
            "width": str(CANVAS_SIZE),
            "height": str(CANVAS_SIZE),
            "role": "img",
            "aria-label": "Isometric electromagnetic wave",
        },
    )
    ET.SubElement(
        root,
        svg_tag("rect"),
        {
            "x": fmt((CANVAS_SIZE - BACKGROUND_SIZE) / 2.0),
            "y": fmt((CANVAS_SIZE - BACKGROUND_SIZE) / 2.0),
            "width": str(BACKGROUND_SIZE),
            "height": str(BACKGROUND_SIZE),
            "rx": str(BACKGROUND_RADIUS),
            "fill": css_color(BG),
            "stroke": css_color(BORDER),
            "stroke-opacity": css_opacity(BORDER),
            "stroke-width": "5",
        },
    )

    e_curve = e_wave_points()
    h_curve = h_wave_points()

    # Filled field surfaces sit behind their outlines and all three axes.
    add_path(
        root,
        filled_surface(h_curve),
        fill=H_FILL,
        stroke=None,
        close=True,
        title="Filled H-field wave surface",
    )
    add_path(
        root,
        filled_surface(e_curve),
        fill=E_FILL,
        stroke=None,
        close=True,
        title="Filled E-field wave surface",
    )
    add_path(
        root,
        h_curve,
        fill=None,
        stroke=H_GLOW,
        width=28.0,
        title="H-field glow",
    )
    add_path(
        root,
        e_curve,
        fill=None,
        stroke=E_GLOW,
        width=28.0,
        title="E-field glow",
    )

    for index, distance in enumerate((-255.0, -170.0, -85.0, 85.0, 170.0, 255.0)):
        phase = math.sin(2.0 * math.pi * (distance - WAVE_START) / (WAVE_END - WAVE_START))
        origin = project(distance, 0.0, 0.0)
        add_line(
            root,
            origin,
            project(distance, E_AMPLITUDE * phase, 0.0),
            (E_STROKE[0], E_STROKE[1], E_STROKE[2], 56),
            3.0,
            f"E-field vector {index}",
        )
        add_line(
            root,
            origin,
            project(distance, 0.0, H_AMPLITUDE * phase),
            (H_STROKE[0], H_STROKE[1], H_STROKE[2], 56),
            3.0,
            f"H-field vector {index}",
        )

    add_line(root, project(-390.0, 0.0, 0.0), project(390.0, 0.0, 0.0), K_AXIS, 5.0, "Propagation axis k")
    add_line(root, project(WAVE_START, -185.0, 0.0), project(WAVE_START, 185.0, 0.0), E_AXIS, 4.0, "Electric-field axis E")
    add_line(root, project(WAVE_START, 0.0, -155.0), project(WAVE_START, 0.0, 155.0), H_AXIS, 4.0, "Magnetic-field axis H")

    add_path(root, h_curve, fill=None, stroke=H_STROKE, width=13.0, title="H-field sine curve")
    add_path(root, e_curve, fill=None, stroke=E_STROKE, width=13.0, title="E-field sine curve")
    add_polygon(root, k_arrow_points(), K_ARROW, "Propagation arrow")
    add_polygon(root, e_arrow_points(), E_ARROW, "Electric-field arrow")
    add_polygon(root, h_arrow_points(), H_ARROW, "Magnetic-field arrow")

    if scientific:
        add_svg_label(root, "E", (-270.0, 390.0), E_STROKE, 64)
        add_svg_label(root, "H", (-455.0, 55.0), H_STROKE, 64)
        add_svg_label(root, "k", (370.0, -225.0), (224, 237, 255, 230), 58, italic=True)

    ET.indent(root, space="  ")
    xml = ET.tostring(root, encoding="utf-8", xml_declaration=True)
    return xml + b"\n"


def scaled_points(points: Iterable[Point], scale: int) -> list[tuple[float, float]]:
    return [(x * scale, y * scale) for x, y in map(to_screen, points)]


def composite_draw(
    image: "PillowImage.Image",
    callback: Callable[["PillowImageDraw.ImageDraw"], None],
) -> "PillowImage.Image":
    from PIL import Image, ImageDraw

    layer = Image.new("RGBA", image.size, (0, 0, 0, 0))
    callback(ImageDraw.Draw(layer))
    return Image.alpha_composite(image, layer)


def draw_polyline(
    draw: "PillowImageDraw.ImageDraw",
    points: Sequence[tuple[float, float]],
    color: Color,
    width: int,
    *,
    round_ends: bool = True,
) -> None:
    draw.line(points, fill=color, width=width, joint="curve")
    if round_ends and points:
        radius = width / 2.0
        for x, y in (points[0], points[-1]):
            draw.ellipse((x - radius, y - radius, x + radius, y + radius), fill=color)


def build_raster_master() -> "PillowImage.Image":
    try:
        from PIL import Image, ImageDraw
    except ImportError as error:
        raise RuntimeError(
            "Pillow is required for --install; install requirements-logo.txt"
        ) from error

    scale = RASTER_SCALE
    size = CANVAS_SIZE * scale
    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    inset = (CANVAS_SIZE - BACKGROUND_SIZE) / 2.0 * scale
    draw.rounded_rectangle(
        (inset, inset, size - inset, size - inset),
        radius=BACKGROUND_RADIUS * scale,
        fill=BG,
        outline=BORDER,
        width=5 * scale,
    )

    e_curve = e_wave_points()
    h_curve = h_wave_points()

    image = composite_draw(
        image,
        lambda layer: layer.polygon(scaled_points(filled_surface(h_curve), scale), fill=H_FILL),
    )
    image = composite_draw(
        image,
        lambda layer: layer.polygon(scaled_points(filled_surface(e_curve), scale), fill=E_FILL),
    )
    image = composite_draw(
        image,
        lambda layer: draw_polyline(layer, scaled_points(h_curve, scale), H_GLOW, 28 * scale),
    )
    image = composite_draw(
        image,
        lambda layer: draw_polyline(layer, scaled_points(e_curve, scale), E_GLOW, 28 * scale),
    )

    def draw_vectors(layer: "PillowImageDraw.ImageDraw") -> None:
        for distance in (-255.0, -170.0, -85.0, 85.0, 170.0, 255.0):
            phase = math.sin(2.0 * math.pi * (distance - WAVE_START) / (WAVE_END - WAVE_START))
            origin = scaled_points([project(distance, 0.0, 0.0)], scale)[0]
            e_end = scaled_points([project(distance, E_AMPLITUDE * phase, 0.0)], scale)[0]
            h_end = scaled_points([project(distance, 0.0, H_AMPLITUDE * phase)], scale)[0]
            draw_polyline(layer, [origin, e_end], (*E_STROKE[:3], 56), 3 * scale, round_ends=False)
            draw_polyline(layer, [origin, h_end], (*H_STROKE[:3], 56), 3 * scale, round_ends=False)

    image = composite_draw(image, draw_vectors)

    def draw_axes(layer: "PillowImageDraw.ImageDraw") -> None:
        draw_polyline(layer, scaled_points([project(-390.0, 0.0, 0.0), project(390.0, 0.0, 0.0)], scale), K_AXIS, 5 * scale)
        draw_polyline(layer, scaled_points([project(WAVE_START, -185.0, 0.0), project(WAVE_START, 185.0, 0.0)], scale), E_AXIS, 4 * scale)
        draw_polyline(layer, scaled_points([project(WAVE_START, 0.0, -155.0), project(WAVE_START, 0.0, 155.0)], scale), H_AXIS, 4 * scale)

    image = composite_draw(image, draw_axes)
    image = composite_draw(
        image,
        lambda layer: draw_polyline(layer, scaled_points(h_curve, scale), H_STROKE, 13 * scale),
    )
    image = composite_draw(
        image,
        lambda layer: draw_polyline(layer, scaled_points(e_curve, scale), E_STROKE, 13 * scale),
    )

    def draw_arrows(layer: "PillowImageDraw.ImageDraw") -> None:
        layer.polygon(scaled_points(k_arrow_points(), scale), fill=K_ARROW)
        layer.polygon(scaled_points(e_arrow_points(), scale), fill=E_ARROW)
        layer.polygon(scaled_points(h_arrow_points(), scale), fill=H_ARROW)

    return composite_draw(image, draw_arrows)


def png_bytes(master: "PillowImage.Image", size: int) -> bytes:
    from PIL import Image

    resized = master.resize((size, size), Image.Resampling.LANCZOS)
    output = io.BytesIO()
    resized.save(output, format="PNG", optimize=True)
    return output.getvalue()


def ico_bytes(master: "PillowImage.Image") -> bytes:
    from PIL import Image

    base = master.resize((256, 256), Image.Resampling.LANCZOS)

    def encoded_ico(*, bitmap_format: str) -> bytes:
        output = io.BytesIO()
        base.save(
            output,
            format="ICO",
            sizes=[(size, size) for size in ICO_SIZES],
            bitmap_format=bitmap_format,
        )
        return output.getvalue()

    def entries(data: bytes) -> dict[int, tuple[bytes, bytes]]:
        reserved, image_type, count = struct.unpack_from("<HHH", data)
        if (reserved, image_type) != (0, 1):
            raise ValueError("Pillow produced an invalid ICO header")
        parsed: dict[int, tuple[bytes, bytes]] = {}
        for index in range(count):
            offset = 6 + 16 * index
            entry = data[offset : offset + 16]
            width, height, _, _, _, _, length, payload_offset = struct.unpack(
                "<BBBBHHII", entry
            )
            size = 256 if width == 0 else width
            actual_height = 256 if height == 0 else height
            if size != actual_height:
                raise ValueError("non-square image found in generated ICO")
            parsed[size] = (entry[:8], data[payload_offset : payload_offset + length])
        return parsed

    # Microsoft recommends PNG compression only for the 256 px image. Smaller
    # entries remain conventional 32-bit DIBs for older Windows icon consumers.
    png_entries = entries(encoded_ico(bitmap_format="png"))
    dib_entries = entries(encoded_ico(bitmap_format="bmp"))
    selected = [png_entries[256], *(dib_entries[size] for size in ICO_SIZES[1:])]

    header = struct.pack("<HHH", 0, 1, len(selected))
    directory = bytearray()
    payload = bytearray()
    payload_offset = len(header) + 16 * len(selected)
    for entry_prefix, image_data in selected:
        directory.extend(entry_prefix)
        directory.extend(struct.pack("<II", len(image_data), payload_offset))
        payload.extend(image_data)
        payload_offset += len(image_data)
    return header + bytes(directory) + bytes(payload)


def android_attr(name: str) -> str:
    return f"{{{ANDROID_NS}}}{name}"


def android_xml(root: ET.Element) -> bytes:
    ET.indent(root, space="  ")
    return ET.tostring(root, encoding="utf-8", xml_declaration=True) + b"\n"


def android_background_xml() -> bytes:
    root = ET.Element("shape", {android_attr("shape"): "rectangle"})
    ET.SubElement(root, "solid", {android_attr("color"): css_color(BG)})
    return android_xml(root)


def android_adaptive_icon_xml() -> bytes:
    root = ET.Element("adaptive-icon")
    ET.SubElement(
        root,
        "background",
        {android_attr("drawable"): "@drawable/ic_launcher_background"},
    )
    ET.SubElement(
        root,
        "foreground",
        {android_attr("drawable"): "@drawable/ic_launcher_foreground"},
    )
    return android_xml(root)


def add_android_path(
    parent: ET.Element,
    points: Sequence[Point],
    *,
    fill: Color | None = None,
    stroke: Color | None = None,
    width: float = 0.0,
    close: bool = False,
) -> None:
    attributes = {android_attr("pathData"): svg_path(points, close=close)}
    if fill is None:
        attributes[android_attr("fillColor")] = "#00000000"
    else:
        attributes[android_attr("fillColor")] = css_color(fill)
        attributes[android_attr("fillAlpha")] = css_opacity(fill)
    if stroke is not None:
        attributes.update(
            {
                android_attr("strokeColor"): css_color(stroke),
                android_attr("strokeAlpha"): css_opacity(stroke),
                android_attr("strokeWidth"): fmt(width),
                android_attr("strokeLineCap"): "round",
                android_attr("strokeLineJoin"): "round",
            }
        )
    ET.SubElement(parent, "path", attributes)


def android_foreground_xml() -> bytes:
    root = ET.Element(
        "vector",
        {
            android_attr("width"): "108dp",
            android_attr("height"): "108dp",
            android_attr("viewportWidth"): str(CANVAS_SIZE),
            android_attr("viewportHeight"): str(CANVAS_SIZE),
        },
    )
    group = ET.SubElement(
        root,
        "group",
        {
            android_attr("pivotX"): str(CANVAS_SIZE / 2),
            android_attr("pivotY"): str(CANVAS_SIZE / 2),
            android_attr("scaleX"): "0.8",
            android_attr("scaleY"): "0.8",
        },
    )
    e_curve = e_wave_points()
    h_curve = h_wave_points()
    add_android_path(group, filled_surface(h_curve), fill=H_FILL, close=True)
    add_android_path(group, filled_surface(e_curve), fill=E_FILL, close=True)
    add_android_path(group, h_curve, stroke=H_GLOW, width=28.0)
    add_android_path(group, e_curve, stroke=E_GLOW, width=28.0)

    for distance in (-255.0, -170.0, -85.0, 85.0, 170.0, 255.0):
        phase = math.sin(
            2.0 * math.pi * (distance - WAVE_START) / (WAVE_END - WAVE_START)
        )
        origin = project(distance, 0.0, 0.0)
        add_android_path(
            group,
            [origin, project(distance, E_AMPLITUDE * phase, 0.0)],
            stroke=(*E_STROKE[:3], 56),
            width=3.0,
        )
        add_android_path(
            group,
            [origin, project(distance, 0.0, H_AMPLITUDE * phase)],
            stroke=(*H_STROKE[:3], 56),
            width=3.0,
        )

    add_android_path(
        group,
        [project(-390.0, 0.0, 0.0), project(390.0, 0.0, 0.0)],
        stroke=K_AXIS,
        width=5.0,
    )
    add_android_path(
        group,
        [project(WAVE_START, -185.0, 0.0), project(WAVE_START, 185.0, 0.0)],
        stroke=E_AXIS,
        width=4.0,
    )
    add_android_path(
        group,
        [project(WAVE_START, 0.0, -155.0), project(WAVE_START, 0.0, 155.0)],
        stroke=H_AXIS,
        width=4.0,
    )
    add_android_path(group, h_curve, stroke=H_STROKE, width=13.0)
    add_android_path(group, e_curve, stroke=E_STROKE, width=13.0)
    add_android_path(group, k_arrow_points(), fill=K_ARROW, close=True)
    add_android_path(group, e_arrow_points(), fill=E_ARROW, close=True)
    add_android_path(group, h_arrow_points(), fill=H_ARROW, close=True)
    return android_xml(root)


def emit(path: Path, content: bytes, *, check: bool) -> bool:
    if check:
        if not path.is_file():
            print(f"missing: {path}", file=sys.stderr)
            return False
        if path.read_bytes() != content:
            print(f"out of date: {path}", file=sys.stderr)
            return False
        print(f"current: {path}")
        return True

    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_file() and path.read_bytes() == content:
        print(f"unchanged: {path}")
    else:
        path.write_bytes(content)
        print(f"wrote: {path}")
    return True


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path(__file__).resolve().parent,
        help="directory for emwave-icon.svg and emwave-scientific.svg",
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="also generate desktop PNG/ICO and Android launcher assets",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify outputs byte-for-byte instead of writing them",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    output_dir = args.output_dir.resolve()
    outputs: list[tuple[Path, bytes]] = [
        (output_dir / "emwave-icon.svg", build_svg(scientific=False)),
        (output_dir / "emwave-scientific.svg", build_svg(scientific=True)),
    ]

    if args.install:
        repo_root = Path(__file__).resolve().parents[2]
        icon_dir = repo_root / "root" / "res" / "icons"
        android_res = repo_root / "android" / "app" / "src" / "main" / "res"
        master = build_raster_master()
        icon_png = png_bytes(master, 512)
        outputs.extend(
            [
                (icon_dir / "sdriak.png", icon_png),
                (icon_dir / "sdriak.macos.png", png_bytes(master, 1024)),
                (icon_dir / "sdriak.ico", ico_bytes(master)),
                (
                    android_res / "drawable" / "ic_launcher_background.xml",
                    android_background_xml(),
                ),
                (
                    android_res / "drawable" / "ic_launcher_foreground.xml",
                    android_foreground_xml(),
                ),
                (
                    android_res / "mipmap-anydpi-v26" / "ic_launcher.xml",
                    android_adaptive_icon_xml(),
                ),
            ]
        )
        outputs.extend(
            (
                android_res / f"mipmap-{density}" / "ic_launcher.png",
                png_bytes(master, size),
            )
            for density, size in ANDROID_LEGACY_SIZES.items()
        )

    return 0 if all(emit(path, content, check=args.check) for path, content in outputs) else 1


if __name__ == "__main__":
    raise SystemExit(main())
