#!/usr/bin/env python3
"""Deterministic VALO design token generator (valo.tokens.json -> ValoTokens.hpp + ValoTheme.qml)."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any


def to_camel(name: str, prefix: str = "") -> str:
    parts = [p for p in name.replace("/", "-").split("-") if p]
    if not parts:
        return prefix
    if prefix:
        return prefix + "".join(p[:1].upper() + p[1:] for p in parts)
    return parts[0] + "".join(p[:1].upper() + p[1:] for p in parts[1:])


def fmt_num(val: Any) -> str:
    if isinstance(val, bool):
        return "true" if val else "false"
    if isinstance(val, int):
        return str(val)
    if isinstance(val, float):
        text = repr(val)
        return text
    return str(val)


def write_if_changed(path: Path, content: str) -> bool:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_text(encoding="utf-8") == content:
        return False
    path.write_text(content, encoding="utf-8")
    return True


def generate_hpp(data: dict[str, Any]) -> str:
    primitives: dict[str, str] = data["primitives"]
    colors: dict[str, dict[str, str]] = data["colors"]
    metrics: dict[str, Any] = data["metrics"]
    material: dict[str, Any] = data["material"]
    composite: dict[str, Any] = data["composite_styles"]

    lines: list[str] = [
        "// GENERATED FILE — DO NOT EDIT MANUALLY.",
        "// Source of truth: src/theme/tokens/valo.tokens.json (via tools/codegen/generate_valo_tokens.py)",
        "#pragma once",
        "#include <QColor>",
        "#include <QtGlobal>",
        "#include <array>",
        "",
        "namespace Systalia::ValoTokens {",
        "",
        "// =========================================================================",
        "// 1. PRIMITIVES",
        "// =========================================================================",
    ]
    for key, hex_val in primitives.items():
        var = to_camel(key, "primitive")
        lines.append(f'inline const QColor {var} = QColor("{hex_val}");')

    lines += [
        "",
        "// =========================================================================",
        "// 2. SEMANTIC COLORS",
        "// =========================================================================",
    ]
    for key, spec in colors.items():
        var = to_camel(key)
        alias_var = to_camel(spec["alias"], "primitive")
        # Keep direct QColor or alias depending on original contract; values are identical.
        if key.startswith("icon/sidebar/") or key.startswith("icon/file/") or key.startswith("icon/folder/"):
            lines.append(f"inline const QColor {var} = {alias_var};")
        else:
            lines.append(f'inline const QColor {var} = QColor("{spec["resolved"]}");')

    lines += [
        "",
        "// =========================================================================",
        "// 3. METRICS",
        "// =========================================================================",
    ]
    for key, val in metrics.items():
        var = to_camel(key)
        lines.append(f"inline constexpr qreal {var} = {fmt_num(val)};")
        if key == "font/heading":
            lines.append('inline constexpr auto fontFamily = "Inter";')

    lines += [
        "",
        "// =========================================================================",
        "// 4. MATERIAL & OPTICAL",
        "// =========================================================================",
    ]
    for key, val in material.items():
        var = to_camel(key)
        if var == "specularStrokeWidth" or key == "specular/stroke-width":
            var = "specularStrokeWidth"
        lines.append(f"inline constexpr qreal {var} = {fmt_num(val)};")

    fill_stops = composite["windowFillStops"]
    spectral_stops = composite["spectralOverlayStops"]
    shadows = composite["windowShadows"]
    blur_radius = float(composite["backdropBlurRadius"])

    lines += [
        "",
        "// =========================================================================",
        "// 5. COMPOSITE STYLES",
        "// =========================================================================",
        "struct GradientStop {",
        "    qreal position;",
        "    QColor color;",
        "    qreal opacity;",
        "};",
        "",
        "struct ShadowSpec {",
        "    enum class Type { Inner, Drop };",
        "    Type type;",
        "    qreal x;",
        "    qreal y;",
        "    qreal blur;",
        "    QColor color;",
        "    qreal opacity;",
        "};",
        "",
        f"inline const std::array<GradientStop, {len(fill_stops)}> windowFillStops = {{{{",
    ]
    for idx, stop in enumerate(fill_stops):
        comma = "," if idx + 1 < len(fill_stops) else ""
        lines.append(
            f'    {{ {float(stop["position"])}, QColor("{stop["color"]}"), {float(stop["opacity"])} }}{comma}'
        )
    lines += [
        "}};",
        "",
        f"inline const std::array<GradientStop, {len(spectral_stops)}> spectralOverlayStops = {{{{",
    ]
    for idx, stop in enumerate(spectral_stops):
        comma = "," if idx + 1 < len(spectral_stops) else ""
        lines.append(
            f'    {{ {float(stop["position"])}, QColor("{stop["color"]}"), {float(stop["opacity"])} }}{comma}'
        )
    lines += [
        "}};",
        "",
        f"inline constexpr qreal backdropBlurRadius = {blur_radius};",
        "",
        f"inline const std::array<ShadowSpec, {len(shadows)}> windowShadows = {{{{",
    ]
    for idx, sh in enumerate(shadows):
        comma = "," if idx + 1 < len(shadows) else ""
        stype = "ShadowSpec::Type::Inner" if sh["type"] == "inner" else "ShadowSpec::Type::Drop"
        lines.append(
            f'    {{ {stype}, {float(sh["x"])}, {float(sh["y"])}, {float(sh["blur"])}, QColor("{sh["color"]}"), {float(sh["opacity"])} }}{comma}'
        )
    lines += [
        "}};",
        "",
        "} // namespace Systalia::ValoTokens",
        "",
    ]
    return "\n".join(lines)


def generate_qml(data: dict[str, Any]) -> str:
    primitives: dict[str, str] = data["primitives"]
    colors: dict[str, dict[str, str]] = data["colors"]
    metrics: dict[str, Any] = data["metrics"]
    material: dict[str, Any] = data["material"]
    composite: dict[str, Any] = data["composite_styles"]

    lines: list[str] = [
        "// GENERATED FILE — DO NOT EDIT MANUALLY.",
        "// Source of truth: src/theme/tokens/valo.tokens.json (via tools/codegen/generate_valo_tokens.py)",
        "pragma Singleton",
        "",
        "import QtQuick",
        "",
        "QtObject {",
        "    id: root",
        "",
        "    // =========================================================================",
        "    // 1. PRIMITIVES",
        "    // =========================================================================",
    ]
    for key, hex_val in primitives.items():
        var = to_camel(key, "primitive")
        lines.append(f'    readonly property color {var}: "{hex_val}"')

    lines += [
        "",
        "    // =========================================================================",
        "    // 2. SEMANTIC COLORS",
        "    // =========================================================================",
    ]
    for key, spec in colors.items():
        var = to_camel(key)
        alias_var = to_camel(spec["alias"], "primitive")
        lines.append(f"    readonly property color {var}: {alias_var}")

    lines += [
        "",
        "    // =========================================================================",
        "    // 3. METRICS",
        "    // =========================================================================",
    ]
    for key, val in metrics.items():
        var = to_camel(key)
        prop_type = "int" if key.startswith("motion/") else "real"
        lines.append(f"    readonly property {prop_type} {var}: {fmt_num(val)}")
        if key == "font/heading":
            lines.append('    readonly property string fontFamily: "Inter"')

    lines += [
        "",
        "    // =========================================================================",
        "    // 4. MATERIAL & OPTICAL",
        "    // =========================================================================",
    ]
    for key, val in material.items():
        var = to_camel(key)
        if key == "glass/backdrop-filter-enabled":
            lines.append(f"    readonly property real {var}: {float(val)}")
            lines.append("    readonly property bool backdropFilterEnabled: true")
        else:
            lines.append(f"    readonly property real {var}: {fmt_num(val)}")

    fill_stops = composite["windowFillStops"]
    spectral_stops = composite["spectralOverlayStops"]
    shadows = composite["windowShadows"]
    blur_radius = fmt_num(composite["backdropBlurRadius"])

    lines += [
        "",
        "    // =========================================================================",
        "    // 5. COMPOSITE STYLES",
        "    // =========================================================================",
        "    readonly property var windowFillStops: [",
    ]
    for idx, stop in enumerate(fill_stops):
        comma = "," if idx + 1 < len(fill_stops) else ""
        lines.append(
            f'        {{ position: {float(stop["position"])}, color: "{stop["color"]}", opacity: {float(stop["opacity"])} }}{comma}'
        )
    lines += [
        "    ]",
        "",
        "    readonly property var spectralOverlayStops: [",
    ]
    for idx, stop in enumerate(spectral_stops):
        comma = "," if idx + 1 < len(spectral_stops) else ""
        lines.append(
            f'        {{ position: {float(stop["position"])}, color: "{stop["color"]}", opacity: {float(stop["opacity"])} }}{comma}'
        )
    lines += [
        "    ]",
        "",
        f"    readonly property real backdropBlurRadius: {blur_radius}",
        "",
        "    readonly property var windowShadows: [",
    ]
    for idx, sh in enumerate(shadows):
        comma = "," if idx + 1 < len(shadows) else ""
        lines.append(
            f'        {{ type: "{sh["type"]}", x: {sh["x"]}, y: {sh["y"]}, blur: {sh["blur"]}, color: "{sh["color"]}", opacity: {float(sh["opacity"])} }}{comma}'
        )
    lines += [
        "    ]",
        "",
        "    function withAlpha(baseColor, alpha) {",
        "        if (!baseColor) return Qt.rgba(0, 0, 0, alpha)",
        "        const c = Qt.color(baseColor)",
        "        return Qt.rgba(c.r, c.g, c.b, alpha)",
        "    }",
        "}",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--hpp", required=True, type=Path)
    parser.add_argument("--qml", required=True, type=Path)
    args = parser.parse_args()

    data = json.loads(args.input.read_text(encoding="utf-8"))
    write_if_changed(args.hpp, generate_hpp(data))
    write_if_changed(args.qml, generate_qml(data))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
