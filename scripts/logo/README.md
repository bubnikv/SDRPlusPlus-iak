# Penrose E/H electromagnetic-wave icon

This trio represents a transverse plane electromagnetic wave. In 3-D, the
propagation vector is **k = x**, the electric field is **E = y**, and the
magnetic field is **H = z**, so the three directions are mutually orthogonal.
The H field is projected into the 2-D icon plane using an oblique orthographic
projection so that the two field planes remain visually distinguishable.

Files:
- `emwave.domain` — minimal Penrose domain
- `emwave.substance` — one EM-wave instance
- `emwave-icon.style` — icon-style version, no text
- `emwave-scientific.style` — same geometry with E/H/k labels
- `*.trio.json` — ready for `roger trio`
- `preview-*.svg` — deterministic previews using the same point geometry

Render with current Penrose/Roger:

```sh
cd penrose_emwave
npx @penrose/roger trio emwave-icon.trio.json
npx @penrose/roger trio emwave-scientific.trio.json
```

The preview SVG files are included because this execution environment could not
fetch `@penrose/roger` from npm; they are not represented as Roger-generated output.
