# E/H electromagnetic-wave logo generator

This logo represents a transverse plane electromagnetic wave. In 3-D, the
propagation vector is **k = x**, the electric field is **E = y**, and the
magnetic field is **H = z**, so the three directions are mutually orthogonal.
A restrained perspective projection starts near an isometric view, with the
camera turned slightly so the propagation axis is more horizontal and less
foreshortened, then applies a small depth scale so the near end is subtly
larger without visibly bending the wave.

Both sinusoidal field surfaces are calculated from their amplitudes and phase,
closed back along the propagation axis, filled translucently, and outlined.
The SVG and raster outputs share the same mathematical scene definition.
The Windows/Linux artwork uses a compact black plate with transparent
surroundings. The legacy macOS iconset uses a premasked 824 px plate with
continuous corners and a subtle shadow on its 1024 px canvas, following the
legacy macOS production grid. Only the graph is scaled for each platform and
target size; the background plate and its border remain fixed.

Raster files intentionally omit embedded DPI metadata: Windows selects an ICO
entry by pixel dimensions, and Android selects density-qualified resources.
The renderer uses proportional stroke widths at normal sizes and minimum pixel
widths at small sizes so the wave outlines remain dominant while axes and field
vectors remain visible. Desktop icons use a 1.20 graph scale at 48 px and
larger, 1.24 at 32 px, and 1.28 at 24 px and smaller. The Windows 16, 24, and
32 px ICO entries and the equivalent Linux variants omit the E/H cross because
it is not independently resolvable at those sizes; 48 px and larger variants
retain it. The 16, 22, and 24 px Linux variants and the 16 and 24 px Windows
entries also omit field-vector hatching. The 32 px entries use four simplified
hatch positions, and 48 px and larger variants retain all six positions.

Generate the dependency-free SVG previews:

```sh
python scripts/logo/generate_logo.py
```

This writes `emwave-icon.svg` and `emwave-scientific.svg` next to the script.

To regenerate the desktop and Android application assets, install the pinned
raster dependency and request installation explicitly:

```sh
python -m pip install -r scripts/logo/requirements-logo.txt
python scripts/logo/generate_logo.py --install
```

Platform-specific desktop sources are grouped under `root/res/icons/windows`,
`root/res/icons/macos`, and `root/res/icons/linux`. The universal `sdriak.png`
remains directly under `root/res/icons` because the About/Credits dialog and
other runtime UI use it on every platform. Android launcher assets remain in
Android's density-qualified resource directories as required by its resource
system.

The Windows ICO contains a PNG-compressed 256 px entry plus conventional 32-bit
DIB entries at 128, 64, 48, 32, 24, and 16 px. This hybrid layout follows the
Windows icon guidance while preserving compatibility with older icon readers.
It is embedded under GLFW's reserved `GLFW_ICON` resource name, so GLFW uses
the multi-resolution ICO directly instead of loading separate PNGs at runtime.

Android 8 and newer use the generated adaptive icon in
`mipmap-anydpi-v26`, with separate vector foreground and full-bleed black
background drawables. Its foreground graph scale is 0.94 in the 108 dp layer.
Android 13 and newer select the `mipmap-anydpi-v33` variant, which adds a
monochrome layer for themed launcher icons. Android 6 and 7 fall back to
density-specific legacy PNGs from `mipmap-mdpi` through `mipmap-xxxhdpi`, with
a 1.18 graph scale.

The macOS source remains a premasked, size-specific iconset under
`root/res/icons/macos` because the current bundle script converts it into a
legacy `.icns` file with `iconutil`. Its graph scale is 1.12. The 16 px image
omits the E/H cross and hatching, the 32 px images omit the cross and simplify
the hatching, and images at 64 px and above retain everything. A full-bleed
source should be introduced together with a migration to an Icon Composer or
asset-catalog pipeline, where macOS applies the platform mask and visual
effects itself.

Linux packages install authored 16, 22, 24, 32, 48, 64, 96, 128, 192, 256,
and 512 px PNGs in the corresponding `hicolor` application-icon directories.
The universal `sdriak.png`, which is always packaged for the About/Credits,
loading-screen, and toolbar logo, also supplies the identical 512 px hicolor
and GLFW representation. The other Linux variants live under
`root/res/icons/linux`. The desktop entry uses the theme name `sdriak`; on X11,
GLFW loads the authored variants directly instead of reducing that 512 px
master. Wayland resolves the icon through the desktop entry, and macOS uses the
bundle's ICNS icon.

Use `--check` (with optional `--install`) to verify that committed outputs are
byte-for-byte current without modifying them.
