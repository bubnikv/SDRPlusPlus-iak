# OpenWebRX+ frequency catalog snapshot

`system-v1.full.json` is the readable audit representation generated from a
pinned OpenWebRX+ revision plus the reviewed SDR++ IARU regional supplements
under `../iaru-overlays/`. The updater derives the readable stripped desktop
JSON and canonical short-key Android CBOR runtime resources from the same
validated combined document, so normal builds and application startup never
access the upstream repositories.

Generated files are split by role:

- this directory contains the full catalog, source manifest, stable ID
  registry, validation/churn report, and the exact pinned upstream inputs under
  the stable `upstream/` path for repository review and offline regeneration;
- `root/res/frequency_catalog/system-v1.json` is the stripped desktop payload;
- `root/res/frequency_catalog/system-v1.cbor` is the equivalent Android
  payload;
- the runtime directory also contains only the small manifest, notice, and
  upstream license needed in binary distributions.

Regenerate from the checked-in pinned inputs:

```text
python scripts/update_openwebrx_catalog.py
```

Update it from a local OpenWebRX+ checkout:

```text
python scripts/update_openwebrx_catalog.py --source-dir PATH --revision COMMIT --advance-revision
```

Ordinary generation uses the checked-in upstream snapshot. Explicitly fetch
and verify the pinned revision again with:

```text
python scripts/update_openwebrx_catalog.py --fetch
```

The pinned commit is recorded in `source-manifest.json`, not encoded in the
directory name. Advancing the revision replaces the generator-owned
`upstream/` mirror in place, producing a normal Git diff at stable paths.

Use `--accept-new-ids` only after reviewing new semantic Bands. Prefer adding an
entry to `band_aliases` when an upstream rename refers to an existing Band.
Use `--check` to verify that every generated file is current without modifying
the tree or using the network. The check validates the full catalog, its
stripped projection, and a decode/key-expansion round trip of the
deterministic short-key CBOR. It also regenerates the legacy SDR++ coverage
audit in `../legacy-comparison/` and rejects new discrepancy fingerprints.

The coverage audit can also be run independently:

```text
python scripts/audit_legacy_band_coverage.py --check
```

The IARU overlay inputs contain only ranges absent from the matching
OpenWebRX+ regional profile. They target the existing regional `plan_id`
rather than creating a second active Plan, preserving one
`(band_id, plan_id)` band-stack identity. Every supplemental Segment retains
its official IARU document, revision, page locator, and checked date in the
full catalog. See `../iaru-overlays/review-report.md` for the English
translations and the reviewed national/stale-data exclusions.

When a deliberate source update changes coverage, review the JSON/Markdown
report and record dispositions in `review-decisions.json`. The explicit
`--accept-coverage-baseline` updater option acknowledges the current
fingerprints without pretending that they have been individually resolved.

The generated data is derived from OpenWebRX+ and is distributed under the
GNU Affero General Public License v3.0. The pinned upstream license text is in
`root/res/frequency_catalog/OPENWEBRX-LICENSE.txt`; exact inputs, hashes,
revision, and source URLs are in `source-manifest.json`.
