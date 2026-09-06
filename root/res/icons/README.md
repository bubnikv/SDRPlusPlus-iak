# SDRIAK application icons

The application icon assets are the pre-rendered **Wave icon — AM envelope**
pack. The pack's `wave-am` files are installed under SDRIAK's existing
`sdriak` resource name so desktop integration, bundle metadata, and runtime
lookups remain stable.

The icon uses a blue gradient tile (`#28A0C8` through `#0B2F63`), a white
crest, and a cyan AM-envelope water line (`#8FE8F9`). Platform-specific files
are authored exports rather than resizes made during the build:

- `windows/sdriak.ico` is the multi-resolution Windows icon.
- `macos/sdriak.icns` is the complete macOS icon family.
- `linux/sdriak-*.png` and `linux/sdriak.svg` are the hicolor assets.
- `sdriak.png` is the single 512 px in-app loading/about/button logo.

Android launcher layers and density-specific fallbacks live in
`android/app/src/main/res`.

The source pack and its development-only generator live in `scripts/logo`.

The crest was vector-traced from the source artwork
https://www.flaticon.com/free-icon/sea_16077411 by Ricardo Ruiz.
The icon is free for personal and commercial purpose with attribution.
Thanks, Ricardo!
