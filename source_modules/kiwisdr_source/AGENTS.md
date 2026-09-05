# KiwiSDR Source Notes

## KiwiSDR Receiver List / Map Service

The KiwiSDR map selector does not download map tiles or map images. The base
map is drawn locally by `geomap` from the SDRIAK resources file
`cty/map.json`. The web service is used only to fetch the public KiwiSDR
receiver list that is plotted on top of that local map.

The implementation is split across three units. Update this file together
with whichever one you change:

- `core/src/gui/brown/kiwisdr_directory.{h,cpp}` — networking layer:
  `KiwiSDRDirectoryClient` (HTTP fetch + cache + JS-to-JSON cleanup +
  `parseServerEntry`) and `KiwiSDRTester` (server probe). Owns the
  background threads. No ImGui dependency.
- `core/src/gui/brown/kiwisdr_map.{h,cpp}` — UI layer: `KiwiSDRMapSelector`
  (modal popup, marker drawing, hit-testing, EXT-API filter, geomap config).
  Holds a `KiwiSDRDirectoryClient` and a `KiwiSDRTester` and consumes their
  snapshots once per frame; no sockets or threads of its own.
- `core/src/utils/url.{h,cpp}` — `parseHttpHostPort` (URL → host+port,
  defaulting to `:80`) and `splitHostPort` (split a `host:port` string).
  Used by the tester, by `KiwiSDRClient`, and by the source module.

## Endpoint and cache

Receiver list endpoint:

```text
GET http://rx.linkfanel.net/kiwisdr_com.js
Host: rx.linkfanel.net
```

The response is JavaScript, not strict JSON. It has this general shape:

```js
var kiwisdr_com = [
  { ... },
  { ... },
]
;
```

`cleanJsResponse` in `kiwisdr_directory.cpp` extracts the array by
literal-token search:

1. Find the prefix `var kiwisdr_com =`.
2. Find the **last** occurrence of the exact end token `},\n]\n;` (LF only —
   CRLF would break the match).
3. Take the substring between them and append `}]` to rebuild a closeable
   array.

This is brittle by design: if the upstream service changes its trailing
whitespace, line endings, or stops emitting a trailing comma, the parser
will fail. The PowerShell helper script in the repository uses a more
forgiving regex (`,\s*\]\s*$` and `;\s*$`) — keep that as the reference for
"what a tolerant parser would do" if this code ever needs to be relaxed.

The HTTP body is capped at `MAX_RESPONSE_SIZE = 4 MiB`; oversize responses
throw.

The cleaned JSON is cached as:

```text
<root>/kiwisdr_source.receiverlist.json
```

`<root>` is whatever path the constructor receives from the source module
(module-resolved root directory, not a hardcoded location). The cache is
reused while it is less than one hour old; when missing or stale, the
client fetches the service again. The fetch happens at most once per
`KiwiSDRDirectoryClient` lifetime — there is no auto-refresh on cache
expiry; the user has to restart the app (or the directory client) to pull
fresh data.

As a sanity check, the live response contained 892 entries on 2026-04-25;
expect the count to stay on the order of several hundred to ~1000.

## Per-entry fields

Each array element is a JSON object processed by `parseServerEntry` in
`kiwisdr_directory.cpp`. Required fields are guard-checked and the entry is
silently dropped (returns `std::nullopt`) if any are missing. Fields marked
**effectively required** are not in that guard but are consumed
unconditionally and will throw if absent (latent crash — see *Known issues*
below).

| Field       | Status               | How it is used                                                                 |
|-------------|----------------------|--------------------------------------------------------------------------------|
| `offline`   | required             | Entry is dropped at parse time if not equal to `"no"`.                         |
| `gps`       | required             | String `"(latitude, longitude)"`, parsed with `std::locale::classic()` so `.` is forced as decimal separator. |
| `name`      | required             | Display name shown in the selection panel.                                     |
| `url`       | required             | Public KiwiSDR URL, usually `http://host[:port][/...]`.                        |
| `snr`       | required             | Comma-separated string parsed via `sscanf("%f,%f", &maxSnr, &secondSnr)`. Only `maxSnr` (the first value) is used today; `secondSnr` is parsed but unused. |
| `users`     | required             | Current user count (`atoi`).                                                   |
| `users_max` | required             | Maximum user count (`atoi`). Drives the "full" filter — see below.             |
| `loc`       | effectively required | Free-text location. Not in the guard but read unconditionally.                 |
| `antenna`   | optional             | Antenna description, shown in the selection panel when present.                |
| `ext_api`   | optional             | Parsed via `atoi` (so non-numeric strings collapse to `0`). Drives the "Show EXT API only" filter. |

## Display and filtering rules

Filtering is applied during draw and hit-testing, not during parse (except
the `offline` filter, which is applied at parse time):

- `offline != "no"` → entry dropped at parse time, never reaches the map.
- `users >= users_max` → entry not drawn. This also silently hides any
  station that reports `users_max == 0`.
- "Show EXT API only" toggle, when **on**, hides entries where `ext_api <= 0`.
  When **off**, all otherwise-visible entries are drawn, including those
  with no `ext_api` field at all.

`KiwiSDRDirectoryClient` sorts servers by `maxSnr` ascending before
delivering them to the selector, so the strongest stations are drawn last
and therefore appear on top of weaker ones in overlapping clusters.

## Marker rendering

Markers are filled rounded squares sized to `style::baseFont->FontSize`,
drawn at the projected GPS coordinates by `KiwiSDRMapSelector::drawMarkers`.

Fill color is driven solely by the first SNR value (`maxSnr`); see
`markerFillForSnr` in `kiwisdr_map.h`:

| Condition                  | Fill color | RGB                |
|----------------------------|------------|--------------------|
| `maxSnr > 22`              | green      | `(0.0, 1.0, 0.0)`  |
| `12 < maxSnr <= 22`        | mid-gray   | `(0.6, 0.6, 0.6)`  |
| otherwise (incl. missing)  | dark gray  | `(0.3, 0.3, 0.3)`  |

Border:

- Unselected: black 1-px border.
- Selected: yellow border drawn twice (1-px outer, 1-px inner) for emphasis.

## Hit-testing and selection

`KiwiSDRMapSelector::handleHitTest` runs in two passes:

1. First pass uses a radius of `FontSize / 2`.
2. If nothing was hit, the radius is multiplied by 5 and the pass is
   repeated, making it easier to grab clustered markers on a coarse map.

The vector is iterated from end to beginning so the visually top-most
marker is picked first. On selection the chosen entry is removed from its
current position and re-appended at the end of the vector so it stays on
top for subsequent draws even if its SNR would otherwise place it below
others.

## Server test flow

`KiwiSDRTester::start(url, loc)` runs on a dedicated background thread and
proceeds as follows:

1. The URL is parsed via `url::parseHttpHostPort`: scheme is stripped, any
   path/query/fragment is stripped, and the port defaults to `80` when not
   explicit. URLs that do not start with `http://` are rejected with status
   "Unsupported URL scheme (only http:// is supported): …".
2. `KiwiSDRClient` connects to that endpoint and tunes 14.074 MHz IQ.
3. The test waits up to 5 s for the first IQ packet to arrive.
4. Result is reported via `statusText()`: "Got some data. Server OK: …" on
   success, "Disconnect, no data. Server NOT OK: …" or
   "Could not connect to server: …" on failure.
5. On success, `lastOk()` returns `{hostPort, loc}`. The selector renders a
   **Use tested server** button next to **Cancel** that commits via the
   `onSelected` callback.

The tester is cancellable via `cancel()`; the flag is checked in the wait
loop and on shutdown (the destructor sets it and joins).

## Error and loading states

- `KiwiSDRDirectoryClient::errorMessage()` returns the HTTP/parse error
  text if the fetch failed; the modal shows it in place of the marker
  overlay. The selector treats the very first successful `takeIfReady()`
  return as the trigger to start drawing markers, so even a legitimately
  empty list switches the UI out of the "Loading…" state.
- All shared state inside `KiwiSDRDirectoryClient` and `KiwiSDRTester` is
  protected by their own internal mutexes; the selector is single-threaded
  and reads snapshots once per frame.

## Known issues to be aware of when editing

- **`loc` is not guard-checked.** Missing `loc` will throw inside
  `parseServerEntry`; either add it to the required-field guard or stop
  reading it unconditionally.
- **`users_max == 0` makes a station invisible.** The `users < users_max`
  draw guard treats this as "full". If upstream ever starts emitting `0`
  for unknown limits, those stations will silently disappear.
- **`secondSnr` is parsed but unused.** Either consume it (e.g. show it in
  the selection panel) or drop the second `%f` from the `sscanf`.
