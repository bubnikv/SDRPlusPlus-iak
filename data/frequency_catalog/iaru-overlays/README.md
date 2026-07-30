# Reviewed IARU regional overlays

These files are small, reviewed SDR++ supplements to the pinned OpenWebRX+
band data. They contain only amateur Bands or portions of Bands that are
absent from the corresponding OpenWebRX+ regional profile.

The IARU documents are the authority for inclusion. Legacy SDR++ country files
are evidence that a range should be investigated, not authority for widening
an entire region. A regional band plan is operating guidance and does not
grant permission to transmit; national regulations always prevail.

`r1.json`, `r2.json`, and `r3.json` are generator inputs. The OpenWebRX+
updater validates and layers their Segments into the matching stable regional
`plan_id`, preserving one `(band_id, plan_id)` key for band-stack memories.
The full catalog retains an IARU `source_ref` for every added Segment. The
desktop JSON and Android CBOR are regenerated from that combined catalog.

The accompanying `review-report.md` explains the translations, accepted
supplements, and exclusions found while reviewing the legacy discrepancy
report.
