# E/H electromagnetic-wave logo generator

This logo represents a transverse plane electromagnetic wave. In 3-D, the
propagation vector is **k = x**, the electric field is **E = y**, and the
magnetic field is **H = z**, so the three directions are mutually orthogonal.
An isometric projection maps the three basis directions to equal-length axes
separated by 120 degrees in the 2-D icon plane.

Both sinusoidal field surfaces are calculated from their amplitudes and phase,
closed back along the propagation axis, filled translucently, and outlined.
The SVG and raster outputs share the same mathematical scene definition.

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

The Windows ICO contains a PNG-compressed 256 px entry plus conventional 32-bit
DIB entries at 128, 64, 48, 32, 24, and 16 px. This hybrid layout follows the
Windows icon guidance while preserving compatibility with older icon readers.

Android 8 and newer use the generated adaptive icon in
`mipmap-anydpi-v26`, with separate vector foreground and solid background
drawables. Android 6 and 7 fall back to density-specific legacy PNGs from
`mipmap-mdpi` through `mipmap-xxxhdpi`.

The macOS source remains a premasked 1024 px PNG because the current bundle
script converts it into a legacy `.icns` file with `iconutil`. A full-bleed
source should be introduced together with a migration to an asset-catalog icon
pipeline, where macOS applies the platform mask itself.

Use `--check` (with optional `--install`) to verify that committed outputs are
byte-for-byte current without modifying them.
