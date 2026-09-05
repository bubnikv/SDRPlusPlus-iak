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
DESKTOP_BACKGROUND_SIZE = 880
MACOS_BACKGROUND_SIZE = 920
BACKGROUND_RADIUS_RATIO = 190.0 / 920.0
WAVE_HALF_SPAN = 340.0 * 2.0 / 3.0
WAVE_START = -WAVE_HALF_SPAN
WAVE_END = WAVE_HALF_SPAN
FIELD_AXES_X = WAVE_START
K_AXIS_START = FIELD_AXES_X - 35.0
K_AXIS_END = WAVE_END + 45.0
WAVE_SEGMENTS = 64
E_AMPLITUDE = 217.5
H_AMPLITUDE = 180.0
CAMERA_AZIMUTH = math.radians(57.0)
CAMERA_ELEVATION = math.asin(1.0 / math.sqrt(3.0))
CAMERA_SCALE = 1.0 / math.cos(CAMERA_ELEVATION)
PERSPECTIVE_DISTANCE = 5200.0
RASTER_SCALE = 4
WAVE_STROKE_WIDTH = 24.0
K_AXIS_WIDTH = 10.0
FIELD_AXIS_WIDTH = 10.0
FIELD_VECTOR_WIDTH = 6.0
WAVE_MIN_PIXELS = 1.5
AXIS_MIN_PIXELS = 1.0
VECTOR_MIN_PIXELS = 0.75
BORDER_MIN_PIXELS = 0.75
FIELD_AXES_MIN_SIZE = 48
FIELD_VECTORS_MIN_SIZE = 32
FULL_FIELD_VECTORS_MIN_SIZE = 48
FIELD_VECTOR_FRACTIONS = (1 / 8, 2 / 8, 3 / 8, 5 / 8, 6 / 8, 7 / 8)
SIMPLIFIED_FIELD_VECTOR_FRACTIONS = (1 / 6, 2 / 6, 4 / 6, 5 / 6)
ICO_SIZES = (256, 128, 64, 48, 32, 24, 16)
ANDROID_LEGACY_SIZES = {
    "mdpi": 48,
    "hdpi": 72,
    "xhdpi": 96,
    "xxhdpi": 144,
    "xxxhdpi": 192,
}

BG_START: Color = (0, 0, 0, 255)
BG_END: Color = (0, 0, 0, 255)
BG: Color = BG_END
BORDER: Color = (64, 72, 80, 200)
K_AXIS: Color = (166, 177, 189, 255)
K_LABEL: Color = (218, 225, 232, 255)
E_AXIS: Color = (0, 125, 145, 255)
E_FILL: Color = (0, 200, 230, 128)
E_STROKE: Color = (0, 200, 230, 255)
H_AXIS: Color = (155, 96, 0, 255)
H_FILL: Color = (255, 160, 0, 128)
H_STROKE: Color = (255, 160, 0, 255)

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
    """Project 3-D E/H/k coordinates with a restrained near-isometric camera."""
    sin_azimuth = math.sin(CAMERA_AZIMUTH)
    cos_azimuth = math.cos(CAMERA_AZIMUTH)
    sin_elevation = math.sin(CAMERA_ELEVATION)
    cos_elevation = math.cos(CAMERA_ELEVATION)
    screen_x = CAMERA_SCALE * (sin_azimuth * x - cos_azimuth * z)
    screen_y = CAMERA_SCALE * (
        -sin_elevation * cos_azimuth * x
        + cos_elevation * y
        - sin_elevation * sin_azimuth * z
    )
    depth = (
        cos_elevation * cos_azimuth * x
        + sin_elevation * y
        + cos_elevation * sin_azimuth * z
    )
    perspective = PERSPECTIVE_DISTANCE / (PERSPECTIVE_DISTANCE - depth)
    return (screen_x * perspective, screen_y * perspective)


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


def field_vector_distances(*, simplified: bool = False) -> list[float]:
    span = WAVE_END - WAVE_START
    fractions = (
        SIMPLIFIED_FIELD_VECTOR_FRACTIONS
        if simplified
        else FIELD_VECTOR_FRACTIONS
    )
    return [WAVE_START + span * fraction for fraction in fractions]


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
    definitions = ET.SubElement(root, svg_tag("defs"))
    gradient = ET.SubElement(
        definitions,
        svg_tag("linearGradient"),
        {"id": "background-gradient", "x1": "0", "y1": "0", "x2": "1", "y2": "1"},
    )
    ET.SubElement(
        gradient,
        svg_tag("stop"),
        {"offset": "0", "stop-color": css_color(BG_START)},
    )
    ET.SubElement(
        gradient,
        svg_tag("stop"),
        {"offset": "1", "stop-color": css_color(BG_END)},
    )
    background_size = DESKTOP_BACKGROUND_SIZE
    background_radius = background_size * BACKGROUND_RADIUS_RATIO
    ET.SubElement(
        root,
        svg_tag("rect"),
        {
            "x": fmt((CANVAS_SIZE - background_size) / 2.0),
            "y": fmt((CANVAS_SIZE - background_size) / 2.0),
            "width": str(background_size),
            "height": str(background_size),
            "rx": fmt(background_radius),
            "fill": "url(#background-gradient)",
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
    for index, distance in enumerate(field_vector_distances()):
        phase = math.sin(2.0 * math.pi * (distance - WAVE_START) / (WAVE_END - WAVE_START))
        origin = project(distance, 0.0, 0.0)
        add_line(
            root,
            origin,
            project(distance, E_AMPLITUDE * phase, 0.0),
            (*E_STROKE[:3], 72),
            FIELD_VECTOR_WIDTH,
            f"E-field vector {index}",
        )
        add_line(
            root,
            origin,
            project(distance, 0.0, H_AMPLITUDE * phase),
            (*H_STROKE[:3], 72),
            FIELD_VECTOR_WIDTH,
            f"H-field vector {index}",
        )

    add_line(root, project(K_AXIS_START, 0.0, 0.0), project(K_AXIS_END, 0.0, 0.0), K_AXIS, K_AXIS_WIDTH, "Propagation axis k")
    add_line(root, project(FIELD_AXES_X, -185.0, 0.0), project(FIELD_AXES_X, 185.0, 0.0), E_AXIS, FIELD_AXIS_WIDTH, "Electric-field axis E")
    add_line(root, project(FIELD_AXES_X, 0.0, -155.0), project(FIELD_AXES_X, 0.0, 155.0), H_AXIS, FIELD_AXIS_WIDTH, "Magnetic-field axis H")

    add_path(root, h_curve, fill=None, stroke=H_STROKE, width=WAVE_STROKE_WIDTH, title="H-field sine curve")
    add_path(root, e_curve, fill=None, stroke=E_STROKE, width=WAVE_STROKE_WIDTH, title="E-field sine curve")
    if scientific:
        add_svg_label(root, "E", (-270.0, 390.0), E_STROKE, 64)
        add_svg_label(root, "H", (-455.0, 55.0), H_STROKE, 64)
        add_svg_label(root, "k", (370.0, -225.0), K_LABEL, 58, italic=True)

    ET.indent(root, space="  ")
    xml = ET.tostring(root, encoding="utf-8", xml_declaration=True)
    return xml + b"\n"


def scaled_points(points: Iterable[Point], scale: float) -> list[tuple[float, float]]:
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


def build_raster_master(
    *, target_size: int = CANVAS_SIZE, background_size: int = DESKTOP_BACKGROUND_SIZE
) -> "PillowImage.Image":
    try:
        from PIL import Image, ImageChops, ImageDraw
    except ImportError as error:
        raise RuntimeError(
            "Pillow is required for --install; install requirements-logo.txt"
        ) from error

    size = target_size * RASTER_SCALE
    scale = size / CANVAS_SIZE

    def line_width(design_width: float, minimum_pixels: float) -> int:
        return max(
            1,
            round(max(design_width * scale, minimum_pixels * RASTER_SCALE)),
        )

    image = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    inset = (CANVAS_SIZE - background_size) / 2.0 * scale
    background_radius = background_size * BACKGROUND_RADIUS_RATIO * scale

    vertical = Image.linear_gradient("L").resize((size, size))
    horizontal = vertical.transpose(Image.Transpose.ROTATE_90)
    blend = ImageChops.add(
        vertical.point(lambda value: value // 2),
        horizontal.point(lambda value: value // 2),
    )
    gradient = Image.composite(
        Image.new("RGBA", image.size, BG_END),
        Image.new("RGBA", image.size, BG_START),
        blend,
    )
    background_mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(background_mask).rounded_rectangle(
        (inset, inset, size - inset, size - inset),
        radius=background_radius,
        fill=255,
    )
    image = Image.composite(gradient, image, background_mask)
    image = composite_draw(
        image,
        lambda layer: layer.rounded_rectangle(
            (inset, inset, size - inset, size - inset),
            radius=background_radius,
            outline=BORDER,
            width=line_width(5.0, BORDER_MIN_PIXELS),
        ),
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
    def draw_vectors(layer: "PillowImageDraw.ImageDraw") -> None:
        if target_size < FIELD_VECTORS_MIN_SIZE:
            return
        distances = field_vector_distances(
            simplified=target_size < FULL_FIELD_VECTORS_MIN_SIZE
        )
        for distance in distances:
            phase = math.sin(2.0 * math.pi * (distance - WAVE_START) / (WAVE_END - WAVE_START))
            origin = scaled_points([project(distance, 0.0, 0.0)], scale)[0]
            e_end = scaled_points([project(distance, E_AMPLITUDE * phase, 0.0)], scale)[0]
            h_end = scaled_points([project(distance, 0.0, H_AMPLITUDE * phase)], scale)[0]
            draw_polyline(layer, [origin, e_end], (*E_STROKE[:3], 72), line_width(FIELD_VECTOR_WIDTH, VECTOR_MIN_PIXELS), round_ends=False)
            draw_polyline(layer, [origin, h_end], (*H_STROKE[:3], 72), line_width(FIELD_VECTOR_WIDTH, VECTOR_MIN_PIXELS), round_ends=False)

    image = composite_draw(image, draw_vectors)

    def draw_axes(layer: "PillowImageDraw.ImageDraw") -> None:
        draw_polyline(layer, scaled_points([project(K_AXIS_START, 0.0, 0.0), project(K_AXIS_END, 0.0, 0.0)], scale), K_AXIS, line_width(K_AXIS_WIDTH, AXIS_MIN_PIXELS))
        if target_size >= FIELD_AXES_MIN_SIZE:
            draw_polyline(layer, scaled_points([project(FIELD_AXES_X, -185.0, 0.0), project(FIELD_AXES_X, 185.0, 0.0)], scale), E_AXIS, line_width(FIELD_AXIS_WIDTH, AXIS_MIN_PIXELS))
            draw_polyline(layer, scaled_points([project(FIELD_AXES_X, 0.0, -155.0), project(FIELD_AXES_X, 0.0, 155.0)], scale), H_AXIS, line_width(FIELD_AXIS_WIDTH, AXIS_MIN_PIXELS))

    image = composite_draw(image, draw_axes)
    image = composite_draw(
        image,
        lambda layer: draw_polyline(layer, scaled_points(h_curve, scale), H_STROKE, line_width(WAVE_STROKE_WIDTH, WAVE_MIN_PIXELS)),
    )
    image = composite_draw(
        image,
        lambda layer: draw_polyline(layer, scaled_points(e_curve, scale), E_STROKE, line_width(WAVE_STROKE_WIDTH, WAVE_MIN_PIXELS)),
    )

    return image


def png_bytes(master: "PillowImage.Image", size: int) -> bytes:
    from PIL import Image

    resized = master.resize((size, size), Image.Resampling.LANCZOS)
    output = io.BytesIO()
    resized.save(output, format="PNG", optimize=True)
    return output.getvalue()


def ico_bytes() -> bytes:
    from PIL import Image

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

    def rendered_entry(size: int, *, bitmap_format: str) -> tuple[bytes, bytes]:
        master = build_raster_master(target_size=size)
        image = master.resize((size, size), Image.Resampling.LANCZOS)
        output = io.BytesIO()
        image.save(
            output,
            format="ICO",
            sizes=[(size, size)],
            bitmap_format=bitmap_format,
        )
        return entries(output.getvalue())[size]

    # Microsoft recommends PNG compression only for the 256 px image. Smaller
    # entries remain conventional 32-bit DIBs for older Windows icon consumers.
    selected = [
        rendered_entry(size, bitmap_format="png" if size == 256 else "bmp")
        for size in ICO_SIZES
    ]

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
    ET.SubElement(
        root,
        "gradient",
        {
            android_attr("angle"): "315",
            android_attr("startColor"): css_color(BG_START),
            android_attr("endColor"): css_color(BG_END),
            android_attr("type"): "linear",
        },
    )
    return android_xml(root)


def android_adaptive_icon_xml(*, monochrome: bool = False) -> bytes:
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
    if monochrome:
        ET.SubElement(
            root,
            "monochrome",
            {android_attr("drawable"): "@drawable/ic_launcher_monochrome"},
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


def android_foreground_xml(*, monochrome: bool = False) -> bytes:
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
    if monochrome:
        e_fill = h_fill = (255, 255, 255, 128)
        e_stroke = h_stroke = (255, 255, 255, 255)
        e_axis = h_axis = k_axis = (255, 255, 255, 176)
        vector_color = (255, 255, 255, 88)
    else:
        e_fill, h_fill = E_FILL, H_FILL
        e_stroke, h_stroke = E_STROKE, H_STROKE
        e_axis, h_axis, k_axis = E_AXIS, H_AXIS, K_AXIS
        vector_color = None

    add_android_path(group, filled_surface(h_curve), fill=h_fill, close=True)
    add_android_path(group, filled_surface(e_curve), fill=e_fill, close=True)
    for distance in field_vector_distances():
        phase = math.sin(
            2.0 * math.pi * (distance - WAVE_START) / (WAVE_END - WAVE_START)
        )
        origin = project(distance, 0.0, 0.0)
        add_android_path(
            group,
            [origin, project(distance, E_AMPLITUDE * phase, 0.0)],
            stroke=vector_color or (*e_stroke[:3], 72),
            width=FIELD_VECTOR_WIDTH,
        )
        add_android_path(
            group,
            [origin, project(distance, 0.0, H_AMPLITUDE * phase)],
            stroke=vector_color or (*h_stroke[:3], 72),
            width=FIELD_VECTOR_WIDTH,
        )

    add_android_path(
        group,
        [project(K_AXIS_START, 0.0, 0.0), project(K_AXIS_END, 0.0, 0.0)],
        stroke=k_axis,
        width=K_AXIS_WIDTH,
    )
    add_android_path(
        group,
        [project(FIELD_AXES_X, -185.0, 0.0), project(FIELD_AXES_X, 185.0, 0.0)],
        stroke=e_axis,
        width=FIELD_AXIS_WIDTH,
    )
    add_android_path(
        group,
        [project(FIELD_AXES_X, 0.0, -155.0), project(FIELD_AXES_X, 0.0, 155.0)],
        stroke=h_axis,
        width=FIELD_AXIS_WIDTH,
    )
    add_android_path(group, h_curve, stroke=h_stroke, width=WAVE_STROKE_WIDTH)
    add_android_path(group, e_curve, stroke=e_stroke, width=WAVE_STROKE_WIDTH)
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
        master = build_raster_master(target_size=512)
        macos_master = build_raster_master(
            target_size=1024,
            background_size=MACOS_BACKGROUND_SIZE,
        )
        icon_png = png_bytes(master, 512)
        outputs.extend(
            [
                (icon_dir / "sdriak.png", icon_png),
                (icon_dir / "sdriak.macos.png", png_bytes(macos_master, 1024)),
                (icon_dir / "sdriak.ico", ico_bytes()),
                (
                    android_res / "drawable" / "ic_launcher_background.xml",
                    android_background_xml(),
                ),
                (
                    android_res / "drawable" / "ic_launcher_foreground.xml",
                    android_foreground_xml(),
                ),
                (
                    android_res / "drawable" / "ic_launcher_monochrome.xml",
                    android_foreground_xml(monochrome=True),
                ),
                (
                    android_res / "mipmap-anydpi-v26" / "ic_launcher.xml",
                    android_adaptive_icon_xml(),
                ),
                (
                    android_res / "mipmap-anydpi-v33" / "ic_launcher.xml",
                    android_adaptive_icon_xml(monochrome=True),
                ),
            ]
        )
        outputs.extend(
            (
                android_res / f"mipmap-{density}" / "ic_launcher.png",
                png_bytes(build_raster_master(target_size=size), size),
            )
            for density, size in ANDROID_LEGACY_SIZES.items()
        )

    return 0 if all(emit(path, content, check=args.check) for path, content in outputs) else 1


if __name__ == "__main__":
    raise SystemExit(main())
