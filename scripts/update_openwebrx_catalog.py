#!/usr/bin/env python3
"""Generate SDR++ system Bands and Bookmarks from a pinned OpenWebRX+ tree.

The installed application never runs this tool. Developers invoke it on
demand, review the generated semantic diff, and commit the resulting snapshot.
Only Python's standard library is required.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import struct
import sys
import unicodedata
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any

# The audit helper is a source-tree tool, not an installed Python package.
# Keep ordinary regeneration from leaving __pycache__ beside checked-in tools.
sys.dont_write_bytecode = True
from audit_legacy_band_coverage import AuditError, build_audit


SCHEMA_VERSION = 1
CBOR_WIRE_SCHEMA_VERSION = 1
PROVIDER = "openwebrx"
BAND_FILES = {
    "default": "bands.json",
    "r1": "bands-r1.json",
    "r2": "bands-r2.json",
    "r3": "bands-r3.json",
}
PROFILE_NAMES = {
    "default": "OpenWebRX+ General",
    "r1": "OpenWebRX+ ITU Region 1",
    "r2": "OpenWebRX+ ITU Region 2",
    "r3": "OpenWebRX+ ITU Region 3",
}
PROFILE_REGION_MASKS = {"default": 0, "r1": 1, "r2": 2, "r3": 4}
SERVICE_BY_TAG = {
    "hamradio": "amateur",
    "broadcast": "broadcast",
    "public": "personal-radio",
    "service": "service",
}
SCANNABLE_MODES = {"lsb", "usb", "cw", "am", "sam", "nfm"}
SDRPP_MODES = {"NFM", "WFM", "AM", "DSB", "USB", "CW", "LSB", "RAW", "CWR"}
DIRECT_MODES = {
    "nfm": "NFM",
    "fm": "NFM",
    "wfm": "WFM",
    "am": "AM",
    "sam": "AM",
    "usb": "USB",
    "lsb": "LSB",
    "cw": "CW",
    "cwskimmer": "CW",
    "cwr": "CWR",
    "raw": "RAW",
}
AM_FALLBACK_MODES = {"acars", "adsb", "vdl2"}
NFM_FALLBACK_MODES = {
    "ais",
    "aprs",
    "dab",
    "dmr",
    "dstar",
    "freedv",
    "ism",
    "lora",
    "m17",
    "meteor",
    "meteor-lrpt",
    "nxdn",
    "packet",
    "page",
    "pocsag",
    "tetra",
    "ysf",
}
USB_FALLBACK_MODES = {
    "bpsk31",
    "bpsk63",
    "dsc",
    "fax",
    "fst4",
    "fst4w",
    "ft4",
    "ft8",
    "hfdl",
    "hell",
    "iscat",
    "js8",
    "jt4",
    "jt9",
    "jt65",
    "msk144",
    "navtex",
    "olivia",
    "psk",
    "q65",
    "rtty",
    "rtty170",
    "rtty450",
    "sitorb",
    "sstv",
    "wspr",
}
STABLE_ID_RE = re.compile(r"^[a-z0-9][a-z0-9._:-]{0,127}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
COUNTRY_RE = re.compile(r"^[a-z]{2}$")
MAX_CHURN_RATIO = 0.10


class UpdateError(RuntimeError):
    pass


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n").encode("utf-8")


def cbor_head(major: int, value: int) -> bytes:
    if value < 0:
        raise UpdateError("CBOR length/value cannot be negative")
    if value < 24:
        return bytes([(major << 5) | value])
    if value <= 0xFF:
        return bytes([(major << 5) | 24, value])
    if value <= 0xFFFF:
        return bytes([(major << 5) | 25]) + struct.pack(">H", value)
    if value <= 0xFFFFFFFF:
        return bytes([(major << 5) | 26]) + struct.pack(">I", value)
    if value <= 0xFFFFFFFFFFFFFFFF:
        return bytes([(major << 5) | 27]) + struct.pack(">Q", value)
    raise UpdateError("integer is too large for deterministic CBOR")


def cbor_bytes(value: Any) -> bytes:
    """Encode the JSON data model as deterministic RFC 8949 CBOR."""
    if value is None:
        return b"\xf6"
    if value is False:
        return b"\xf4"
    if value is True:
        return b"\xf5"
    if isinstance(value, int):
        return cbor_head(0, value) if value >= 0 else cbor_head(1, -1 - value)
    if isinstance(value, float):
        if not math.isfinite(value):
            raise UpdateError("non-finite number cannot be encoded as catalog CBOR")
        return b"\xfb" + struct.pack(">d", value)
    if isinstance(value, str):
        encoded = value.encode("utf-8")
        return cbor_head(3, len(encoded)) + encoded
    if isinstance(value, list):
        return cbor_head(4, len(value)) + b"".join(cbor_bytes(item) for item in value)
    if isinstance(value, dict):
        entries: list[tuple[bytes, bytes]] = []
        for key, item in value.items():
            if not isinstance(key, str):
                raise UpdateError("catalog CBOR object key is not a string")
            encoded_key = cbor_bytes(key)
            entries.append((encoded_key, cbor_bytes(item)))
        # RFC 8949 deterministic map order: shorter encoded keys first, then
        # bytewise lexical order.
        entries.sort(key=lambda entry: (len(entry[0]), entry[0]))
        return cbor_head(5, len(entries)) + b"".join(
            key + item for key, item in entries
        )
    raise UpdateError(f"cannot encode {type(value).__name__} as catalog CBOR")


def decode_cbor(data: bytes) -> Any:
    """Decode the deterministic CBOR subset emitted above for self-checking."""
    offset = 0

    def read(count: int) -> bytes:
        nonlocal offset
        end = offset + count
        if end > len(data):
            raise UpdateError("truncated generated CBOR")
        result = data[offset:end]
        offset = end
        return result

    def argument(additional: int) -> int:
        if additional < 24:
            return additional
        sizes = {24: 1, 25: 2, 26: 4, 27: 8}
        if additional not in sizes:
            raise UpdateError("generated CBOR uses an unsupported indefinite/reserved value")
        return int.from_bytes(read(sizes[additional]), "big")

    def item() -> Any:
        initial = read(1)[0]
        major = initial >> 5
        additional = initial & 0x1F
        if major == 0:
            return argument(additional)
        if major == 1:
            return -1 - argument(additional)
        if major == 3:
            try:
                return read(argument(additional)).decode("utf-8")
            except UnicodeDecodeError as exc:
                raise UpdateError("generated CBOR contains invalid UTF-8") from exc
        if major == 4:
            return [item() for _ in range(argument(additional))]
        if major == 5:
            result: dict[str, Any] = {}
            for _ in range(argument(additional)):
                key = item()
                if not isinstance(key, str) or key in result:
                    raise UpdateError("generated CBOR has a non-string or duplicate map key")
                result[key] = item()
            return result
        if major == 7 and additional == 20:
            return False
        if major == 7 and additional == 21:
            return True
        if major == 7 and additional == 22:
            return None
        if major == 7 and additional == 27:
            return struct.unpack(">d", read(8))[0]
        raise UpdateError("generated CBOR contains an unsupported value")

    result = item()
    if offset != len(data):
        raise UpdateError("generated CBOR has trailing data")
    return result


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def stable_hash(*parts: str, length: int = 24) -> str:
    digest = hashlib.sha256()
    for part in parts:
        encoded = part.encode("utf-8")
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
    return digest.hexdigest()[:length]


def normalize_text(value: Any) -> str:
    return " ".join(str(value).strip().split())


def semantic_name(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", normalize_text(value)).casefold()
    without_marks = "".join(c for c in normalized if not unicodedata.combining(c))
    return " ".join(without_marks.split())


def semantic_number(value: int | float) -> str:
    number = float(value)
    if number.is_integer():
        return str(int(number))
    return format(number, ".15g")


def slug(value: str) -> str:
    ascii_value = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode("ascii")
    result = re.sub(r"[^a-z0-9]+", "-", ascii_value.casefold()).strip("-")
    return result or "unnamed"


def checked_number(value: Any, path: str, *, positive: bool = False) -> int | float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise UpdateError(f"{path} must be a number")
    number = float(value)
    if not math.isfinite(number) or (positive and number <= 0) or (not positive and number < 0):
        qualifier = "positive" if positive else "non-negative"
        raise UpdateError(f"{path} must be finite and {qualifier}")
    return value


def load_json_bytes(data: bytes, path: str) -> Any:
    try:
        return json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise UpdateError(f"cannot parse {path}: {exc}") from exc


def normalize_source_bytes(data: bytes) -> bytes:
    # A local checkout may apply core.autocrlf while GitHub raw serves the blob
    # with LF. Canonicalize text inputs so both updater modes produce the same
    # hashes, manifest, and copied license.
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def source_parts(repository: str) -> tuple[str, str]:
    parsed = urllib.parse.urlparse(repository)
    parts = parsed.path.strip("/").removesuffix(".git").split("/")
    if parsed.hostname != "github.com" or len(parts) != 2 or not all(parts):
        raise UpdateError("source.repository must be a github.com owner/repository URL")
    return parts[0], parts[1]


@dataclass
class SourceTree:
    repository: str
    revision: str
    files: dict[str, bytes]

    def require(self, path: str) -> bytes:
        try:
            return self.files[path]
        except KeyError as exc:
            raise UpdateError(f"upstream source is missing {path}") from exc


def http_get(url: str) -> bytes:
    request = urllib.request.Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "User-Agent": "SDRPlusPlus-OpenWebRX-catalog-updater/1",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return response.read()
    except (urllib.error.URLError, TimeoutError) as exc:
        raise UpdateError(f"download failed for {url}: {exc}") from exc


def read_remote_tree(repository: str, revision: str) -> SourceTree:
    owner, repo = source_parts(repository)
    api_url = f"https://api.github.com/repos/{owner}/{repo}/git/trees/{revision}?recursive=1"
    tree = load_json_bytes(http_get(api_url), api_url)
    if not isinstance(tree, dict) or tree.get("truncated"):
        raise UpdateError("GitHub returned an invalid or truncated source tree")
    paths = {
        entry["path"]
        for entry in tree.get("tree", [])
        if isinstance(entry, dict)
        and entry.get("type") == "blob"
        and isinstance(entry.get("path"), str)
    }
    selected = set(BAND_FILES.values())
    selected.update(path for path in paths if path.startswith("bookmarks.d/") and path.endswith(".json"))
    selected.update({"LICENSE.txt", "owrx/bands.py", "owrx/bookmarks.py"})
    raw_root = f"https://raw.githubusercontent.com/{owner}/{repo}/{revision}"
    files = {
        path: normalize_source_bytes(http_get(f"{raw_root}/{path}"))
        for path in sorted(selected)
    }
    return SourceTree(repository, revision, files)


def read_local_tree(repository: str, revision: str, directory: Path) -> SourceTree:
    if not directory.is_dir():
        raise UpdateError(f"source directory does not exist: {directory}")
    selected = set(BAND_FILES.values())
    selected.update(
        path.relative_to(directory).as_posix()
        for path in (directory / "bookmarks.d").rglob("*.json")
    )
    selected.update({"LICENSE.txt", "owrx/bands.py", "owrx/bookmarks.py"})
    files: dict[str, bytes] = {}
    for path in sorted(selected):
        source_path = directory.joinpath(*PurePosixPath(path).parts)
        try:
            files[path] = normalize_source_bytes(source_path.read_bytes())
        except OSError as exc:
            raise UpdateError(f"cannot read {source_path}: {exc}") from exc
    return SourceTree(repository, revision, files)


def plan_scope(profile: str) -> dict[str, Any]:
    mask = PROFILE_REGION_MASKS[profile]
    return {"itu_region_mask": mask} if mask else {}


def bookmark_scope(path: str) -> dict[str, Any]:
    parts = PurePosixPath(path).parts
    if len(parts) >= 3 and parts[1] in {"r1", "r2", "r3"}:
        return {"itu_region_mask": PROFILE_REGION_MASKS[parts[1]]}
    if len(parts) >= 3 and COUNTRY_RE.fullmatch(parts[1]):
        return {"country_codes": [parts[1].upper()]}
    return {}


def resolve_mode(source_mode: str, underlying_mode: str, unknown: set[str]) -> str | None:
    for candidate in (underlying_mode, source_mode):
        mode = candidate.casefold()
        if not mode:
            continue
        if mode.upper() in SDRPP_MODES:
            return mode.upper()
        if mode in DIRECT_MODES:
            return DIRECT_MODES[mode]
        if mode in AM_FALLBACK_MODES:
            return "AM"
        if mode in NFM_FALLBACK_MODES:
            return "NFM"
        if mode in USB_FALLBACK_MODES:
            return "USB"
        unknown.add(mode)
    return None


def semantic_band_key(name: str, service: str) -> str:
    return f"{service}|{semantic_name(name)}"


def initial_band_id(name: str, service: str) -> str:
    normalized = semantic_name(name)
    # These IDs already exist in the legacy band-plan adapter. Reusing them is
    # what lets the native snapshot replace that adapter without changing the
    # semantic band-stack key.
    known = {
        ("amateur", "1.25m"): "band:amateur:125cm",
        ("broadcast", "am broadcast"): "band:broadcast:mediumwave",
        ("broadcast", "fm broadcast"): "band:broadcast:fm",
        ("personal-radio", "11m cb"): "band:personal-radio:cb",
        ("personal-radio", "pmr446"): "band:land-mobile:pmr446",
    }
    if (service, normalized) in known:
        return known[(service, normalized)]
    if service == "broadcast" and normalized.endswith(" broadcast"):
        return f"band:broadcast:{slug(normalized.removesuffix(' broadcast'))}"
    return f"band:{service}:{slug(name)}"


def canonical_band_key(key: str, aliases: dict[str, str]) -> str:
    seen: set[str] = set()
    current = key
    while current in aliases:
        if current in seen:
            raise UpdateError(f"band alias cycle includes {current!r}")
        seen.add(current)
        current = aliases[current]
    return current


def band_service(raw: dict[str, Any], path: str) -> str:
    tags = raw.get("tags", [])
    if not isinstance(tags, list) or not all(isinstance(tag, str) for tag in tags):
        raise UpdateError(f"{path}.tags must be an array of strings")
    services = {SERVICE_BY_TAG[tag] for tag in tags if tag in SERVICE_BY_TAG}
    if len(services) > 1:
        raise UpdateError(f"{path} maps to multiple services: {sorted(services)}")
    return next(iter(services), "other")


def allocate_band(
    name: str,
    service: str,
    registry: dict[str, Any],
    accept_new_ids: bool,
    new_keys: list[str],
) -> tuple[str, dict[str, Any]]:
    key = semantic_band_key(name, service)
    aliases = registry["band_aliases"]
    canonical = canonical_band_key(key, aliases)
    bands = registry["bands"]
    if canonical not in bands:
        if not accept_new_ids:
            raise UpdateError(
                f"new semantic Band {canonical!r}; review it, then use --accept-new-ids "
                "or add a band_aliases entry"
            )
        candidate = initial_band_id(name, service)
        used = {entry["band_id"] for entry in bands.values()}
        if candidate in used:
            candidate += "-" + stable_hash(canonical, length=8)
        bands[canonical] = {"band_id": candidate, "name": name, "service": service}
        new_keys.append(canonical)
    entry = bands[canonical]
    if entry.get("service") != service:
        raise UpdateError(f"registry service mismatch for {canonical!r}")
    return canonical, entry


def source_ref(tree: SourceTree, path: str, semantic_key: str) -> dict[str, Any]:
    owner, repo = source_parts(tree.repository)
    return {
        "provider": PROVIDER,
        "record_id": f"{PROVIDER}:{stable_hash(path, semantic_key, length=32)}",
        "upstream_id": f"{path}#{semantic_key}",
        "url": f"https://github.com/{owner}/{repo}/blob/{tree.revision}/{path}",
    }


def flatten_dial_value(value: Any, path: str) -> list[tuple[int | float, str]]:
    if isinstance(value, list):
        result: list[tuple[int | float, str]] = []
        for index, item in enumerate(value):
            result.extend(flatten_dial_value(item, f"{path}[{index}]"))
        return result
    if isinstance(value, dict):
        if "frequency" not in value:
            raise UpdateError(f"{path} object has no frequency")
        frequency = checked_number(value["frequency"], f"{path}.frequency", positive=True)
        underlying = normalize_text(value.get("underlying", ""))
        return [(frequency, underlying)]
    return [(checked_number(value, path, positive=True), "")]


def build_catalog(
    tree: SourceTree,
    registry: dict[str, Any],
    accept_new_ids: bool,
) -> tuple[dict[str, Any], dict[str, Any]]:
    unknown_modes: set[str] = set()
    skipped_dials: list[dict[str, Any]] = []
    new_band_keys: list[str] = []
    plans: list[dict[str, Any]] = []
    bands_by_id: dict[str, dict[str, Any]] = {}
    segments: list[dict[str, Any]] = []
    bookmarks: list[dict[str, Any]] = []
    bookmark_semantics: set[str] = set()
    segment_semantics: set[str] = set()

    for profile, path in BAND_FILES.items():
        plan_id = registry["plans"].get(profile)
        if not isinstance(plan_id, str):
            raise UpdateError(f"registry has no plan id for {profile}")
        plans.append(
            {
                "name": PROFILE_NAMES[profile],
                "plan_id": plan_id,
                "revision": tree.revision,
                "scope": plan_scope(profile),
                "source": "OpenWebRX+",
            }
        )
        raw_bands = load_json_bytes(tree.require(path), path)
        if not isinstance(raw_bands, list):
            raise UpdateError(f"{path} must contain an array")
        for index, raw in enumerate(raw_bands):
            item_path = f"{path}[{index}]"
            if not isinstance(raw, dict):
                raise UpdateError(f"{item_path} must be an object")
            name = normalize_text(raw.get("name", ""))
            if not name:
                raise UpdateError(f"{item_path}.name is empty")
            lower = checked_number(raw.get("lower_bound"), f"{item_path}.lower_bound")
            upper = checked_number(raw.get("upper_bound"), f"{item_path}.upper_bound")
            if float(upper) < float(lower):
                raise UpdateError(f"{item_path} has reversed bounds")
            service = band_service(raw, item_path)
            canonical, band_entry = allocate_band(
                name, service, registry, accept_new_ids, new_band_keys
            )
            band_id = band_entry["band_id"]
            bands_by_id[band_id] = {
                "band_id": band_id,
                "name": band_entry["name"],
                "service": band_entry["service"],
            }
            segment_semantic = f"{profile}|{canonical}"
            if segment_semantic in segment_semantics:
                raise UpdateError(f"duplicate semantic Segment {segment_semantic!r}")
            segment_semantics.add(segment_semantic)
            segments.append(
                {
                    "band_id": band_id,
                    "kind": "operating_plan",
                    "name": name,
                    "plan_id": plan_id,
                    "range": {"max_hz": upper, "min_hz": lower},
                    "segment_id": f"segment:openwebrx:{stable_hash(segment_semantic)}",
                    "service": service,
                    "source_ref": source_ref(tree, path, segment_semantic),
                    "status": "advisory",
                }
            )

            frequencies = raw.get("frequencies", {})
            if frequencies is None:
                frequencies = {}
            if not isinstance(frequencies, dict):
                raise UpdateError(f"{item_path}.frequencies must be an object")
            for raw_mode, raw_value in sorted(frequencies.items()):
                if not isinstance(raw_mode, str):
                    raise UpdateError(f"{item_path}.frequencies has a non-string mode")
                source_mode = normalize_text(raw_mode)
                for frequency, underlying in flatten_dial_value(
                    raw_value, f"{item_path}.frequencies.{raw_mode}"
                ):
                    if not (float(lower) <= float(frequency) <= float(upper)):
                        skipped_dials.append(
                            {
                                "band": name,
                                "frequency": frequency,
                                "mode": source_mode,
                                "profile": profile,
                                "reason": "outside band range",
                            }
                        )
                        continue
                    semantic = (
                        f"dial|{profile}|{canonical}|{source_mode.casefold()}|"
                        f"{semantic_number(frequency)}|{underlying.casefold()}"
                    )
                    if semantic in bookmark_semantics:
                        raise UpdateError(f"duplicate semantic Bookmark {semantic!r}")
                    bookmark_semantics.add(semantic)
                    bookmark = {
                        "band_id": band_id,
                        "bandwidth": 0,
                        "bookmark_id": f"bookmark:openwebrx:{stable_hash(semantic)}",
                        "frequency": frequency,
                        "layer": "system",
                        "name": f"{name} — {source_mode.upper()}",
                        "scope": plan_scope(profile),
                        "source_mode": source_mode,
                        "source_ref": source_ref(tree, path, semantic),
                    }
                    if underlying:
                        bookmark["underlying_mode"] = underlying
                    mode = resolve_mode(source_mode, underlying, unknown_modes)
                    if mode:
                        bookmark["mode"] = mode
                    if source_mode.casefold() not in SCANNABLE_MODES:
                        bookmark["scannable"] = False
                    bookmarks.append(bookmark)

    bookmark_paths = sorted(
        path for path in tree.files if path.startswith("bookmarks.d/") and path.endswith(".json")
    )
    for path in bookmark_paths:
        raw_bookmarks = load_json_bytes(tree.require(path), path)
        if not isinstance(raw_bookmarks, list):
            raise UpdateError(f"{path} must contain an array")
        scope = bookmark_scope(path)
        for index, raw in enumerate(raw_bookmarks):
            item_path = f"{path}[{index}]"
            if not isinstance(raw, dict):
                raise UpdateError(f"{item_path} must be an object")
            name = normalize_text(raw.get("name", ""))
            if not name:
                raise UpdateError(f"{item_path}.name is empty")
            frequency = checked_number(raw.get("frequency"), f"{item_path}.frequency", positive=True)
            source_mode = normalize_text(raw.get("modulation", ""))
            underlying = normalize_text(raw.get("underlying", ""))
            semantic = (
                f"file|{path}|{semantic_name(name)}|{semantic_number(frequency)}|"
                f"{source_mode.casefold()}|{underlying.casefold()}"
            )
            if semantic in bookmark_semantics:
                raise UpdateError(f"duplicate semantic Bookmark {semantic!r}")
            bookmark_semantics.add(semantic)
            bookmark = {
                "bandwidth": checked_number(
                    raw.get("bandwidth", 0), f"{item_path}.bandwidth"
                ),
                "bookmark_id": f"bookmark:openwebrx:{stable_hash(semantic)}",
                "frequency": frequency,
                "layer": "system",
                "name": name,
                "scope": scope,
                "source_mode": source_mode,
                "source_ref": source_ref(tree, path, semantic),
            }
            if underlying:
                bookmark["underlying_mode"] = underlying
            mode = resolve_mode(source_mode, underlying, unknown_modes)
            if mode:
                bookmark["mode"] = mode
            scannable = raw.get("scannable", source_mode.casefold() in SCANNABLE_MODES)
            if not isinstance(scannable, bool):
                raise UpdateError(f"{item_path}.scannable must be boolean")
            if not scannable:
                bookmark["scannable"] = False
            description = normalize_text(raw.get("description", ""))
            if description:
                bookmark["notes"] = description
            bookmarks.append(bookmark)

    catalog = {
        "bands": sorted(bands_by_id.values(), key=lambda item: item["band_id"]),
        "bookmarks": sorted(bookmarks, key=lambda item: item["bookmark_id"]),
        "plans": sorted(plans, key=lambda item: item["plan_id"]),
        "schema_version": SCHEMA_VERSION,
        "segments": sorted(segments, key=lambda item: item["segment_id"]),
    }
    diagnostics = {
        "new_band_keys": sorted(new_band_keys),
        "skipped_dial_entries": sorted(
            skipped_dials,
            key=lambda item: (
                item["profile"],
                item["band"],
                item["frequency"],
                item["mode"],
            ),
        ),
        "unknown_source_modes": sorted(unknown_modes),
    }
    validate_catalog(catalog)
    return catalog, diagnostics


def validate_stable_id(value: Any, path: str) -> None:
    if not isinstance(value, str) or not STABLE_ID_RE.fullmatch(value):
        raise UpdateError(f"{path} is not a valid stable ID")


def validate_catalog(catalog: dict[str, Any]) -> None:
    if catalog.get("schema_version") != SCHEMA_VERSION:
        raise UpdateError("generated catalog has the wrong schema version")
    ids: dict[str, set[str]] = {"plans": set(), "bands": set(), "segments": set(), "bookmarks": set()}
    id_fields = {
        "plans": "plan_id",
        "bands": "band_id",
        "segments": "segment_id",
        "bookmarks": "bookmark_id",
    }
    for collection, field in id_fields.items():
        for index, item in enumerate(catalog[collection]):
            value = item.get(field)
            validate_stable_id(value, f"{collection}[{index}].{field}")
            if value in ids[collection]:
                raise UpdateError(f"duplicate {field}: {value}")
            ids[collection].add(value)
    for index, segment in enumerate(catalog["segments"]):
        if segment["plan_id"] not in ids["plans"]:
            raise UpdateError(f"segments[{index}] has a dangling plan_id")
        if segment["band_id"] not in ids["bands"]:
            raise UpdateError(f"segments[{index}] has a dangling band_id")
        lower = float(segment["range"]["min_hz"])
        upper = float(segment["range"]["max_hz"])
        if not math.isfinite(lower) or not math.isfinite(upper) or lower < 0 or upper < lower:
            raise UpdateError(f"segments[{index}] has an invalid range")
    for index, bookmark in enumerate(catalog["bookmarks"]):
        frequency = float(bookmark["frequency"])
        if not math.isfinite(frequency) or frequency <= 0:
            raise UpdateError(f"bookmarks[{index}] has an invalid frequency")
        if "band_id" in bookmark and bookmark["band_id"] not in ids["bands"]:
            raise UpdateError(f"bookmarks[{index}] has a dangling band_id")
        if "mode" in bookmark and bookmark["mode"] not in SDRPP_MODES:
            raise UpdateError(f"bookmarks[{index}] has an unsupported mode")
        mask = bookmark.get("scope", {}).get("itu_region_mask", 0)
        if not isinstance(mask, int) or mask & ~0x07:
            raise UpdateError(f"bookmarks[{index}] has an invalid ITU region mask")


def catalog_ids(catalog: dict[str, Any] | None) -> dict[str, list[str]]:
    if not catalog:
        return {"bands": [], "bookmarks": [], "plans": [], "segments": []}
    fields = {
        "bands": "band_id",
        "bookmarks": "bookmark_id",
        "plans": "plan_id",
        "segments": "segment_id",
    }
    return {
        collection: sorted(item[field] for item in catalog.get(collection, []))
        for collection, field in fields.items()
    }


def build_churn(
    baseline: dict[str, list[str]],
    current: dict[str, list[str]],
) -> tuple[dict[str, Any], float]:
    result: dict[str, Any] = {}
    total_old = 0
    total_changed = 0
    for collection in ("plans", "bands", "segments", "bookmarks"):
        old = set(baseline.get(collection, []))
        new = set(current.get(collection, []))
        added = sorted(new - old)
        removed = sorted(old - new)
        total_old += len(old)
        total_changed += len(added) + len(removed)
        result[collection] = {
            "added": added,
            "added_count": len(added),
            "removed": removed,
            "removed_count": len(removed),
        }
    ratio = total_changed / total_old if total_old else 0.0
    return result, ratio


def load_optional_json(path: Path) -> dict[str, Any] | None:
    if not path.exists():
        return None
    value = load_json_bytes(path.read_bytes(), str(path))
    if not isinstance(value, dict):
        raise UpdateError(f"{path} must contain an object")
    return value


def validate_registry(registry: dict[str, Any]) -> None:
    if registry.get("schema_version") != 1 or registry.get("source") != PROVIDER:
        raise UpdateError("OpenWebRX ID registry has the wrong schema or source")
    if not isinstance(registry.get("plans"), dict):
        raise UpdateError("OpenWebRX ID registry has no plans object")
    if not isinstance(registry.get("bands"), dict):
        raise UpdateError("OpenWebRX ID registry has no bands object")
    if not isinstance(registry.get("band_aliases"), dict):
        raise UpdateError("OpenWebRX ID registry has no band_aliases object")
    all_ids: set[str] = set()
    for profile in BAND_FILES:
        validate_stable_id(registry["plans"].get(profile), f"registry.plans.{profile}")
    for key, entry in registry["bands"].items():
        if not isinstance(key, str) or not isinstance(entry, dict):
            raise UpdateError("registry.bands entries must be objects")
        validate_stable_id(entry.get("band_id"), f"registry.bands[{key!r}].band_id")
        if entry["band_id"] in all_ids:
            raise UpdateError(f"duplicate registry band_id: {entry['band_id']}")
        all_ids.add(entry["band_id"])
        if not normalize_text(entry.get("name", "")) or not normalize_text(entry.get("service", "")):
            raise UpdateError(f"registry.bands[{key!r}] has no name or service")
    for alias, target in registry["band_aliases"].items():
        if not isinstance(alias, str) or not isinstance(target, str):
            raise UpdateError("registry.band_aliases entries must be strings")
        canonical = canonical_band_key(alias, registry["band_aliases"])
        if canonical not in registry["bands"]:
            raise UpdateError(f"band alias {alias!r} resolves to missing {canonical!r}")


def apply_iaru_overlays(
    catalog: dict[str, Any],
    registry: dict[str, Any],
    directory: Path,
) -> dict[str, Any]:
    expected_profiles = {"r1", "r2", "r3"}
    paths = {path.stem: path for path in directory.glob("*.json")}
    if set(paths) != expected_profiles:
        raise UpdateError(
            "IARU overlay inputs must be exactly r1.json, r2.json, and r3.json"
        )

    bands_by_id = {band["band_id"]: band for band in catalog["bands"]}
    plans_by_id = {plan["plan_id"]: plan for plan in catalog["plans"]}
    segment_ids = {segment["segment_id"] for segment in catalog["segments"]}
    sources: list[dict[str, Any]] = []
    inputs: list[dict[str, Any]] = []
    profile_counts: dict[str, int] = {}

    for profile in sorted(paths):
        path = paths[profile]
        data = path.read_bytes()
        document = load_json_bytes(data, str(path))
        if not isinstance(document, dict):
            raise UpdateError(f"{path} must contain an object")
        if (
            document.get("schema_version") != 1
            or document.get("artifact_role") != "generator-input"
            or document.get("profile") != profile
        ):
            raise UpdateError(f"{path} has an invalid overlay header")
        source = document.get("source")
        if not isinstance(source, dict):
            raise UpdateError(f"{path}.source must be an object")
        required_source_fields = (
            "checked_on",
            "organization",
            "provider",
            "revision",
            "title",
            "url",
        )
        if any(
            not isinstance(source.get(field), str)
            or not normalize_text(source[field])
            for field in required_source_fields
        ):
            raise UpdateError(f"{path}.source is incomplete")
        provider = source["provider"]
        validate_stable_id(provider, f"{path}.source.provider")
        if provider != f"iaru-{profile}":
            raise UpdateError(f"{path}.source.provider does not match its profile")

        plan_id = registry["plans"][profile]
        plan = plans_by_id[plan_id]
        plan["source"] = "OpenWebRX+ + SDR++ IARU supplement"
        plan["revision"] = f"{plan['revision']}+{provider}:{source['revision']}"

        raw_segments = document.get("segments")
        if not isinstance(raw_segments, list) or not raw_segments:
            raise UpdateError(f"{path}.segments must be a non-empty array")
        seen_keys: set[str] = set()
        for index, raw in enumerate(raw_segments):
            item_path = f"{path}.segments[{index}]"
            if not isinstance(raw, dict):
                raise UpdateError(f"{item_path} must be an object")
            key = normalize_text(raw.get("key", ""))
            band_name = normalize_text(raw.get("band_name", ""))
            band_id = raw.get("band_id")
            source_locator = normalize_text(raw.get("source_locator", ""))
            if not key or not band_name or not source_locator:
                raise UpdateError(f"{item_path} has an empty key, name, or source locator")
            validate_stable_id(band_id, f"{item_path}.band_id")
            if key in seen_keys:
                raise UpdateError(f"{path} has duplicate Segment key {key!r}")
            seen_keys.add(key)
            canonical = semantic_band_key(band_name, "amateur")
            entry = registry["bands"].get(canonical)
            if not isinstance(entry, dict) or entry.get("band_id") != band_id:
                raise UpdateError(
                    f"{item_path} Band {canonical!r} is not pinned to {band_id!r} "
                    "in the ID registry"
                )
            band = {
                "band_id": band_id,
                "name": entry["name"],
                "service": "amateur",
            }
            existing_band = bands_by_id.get(band_id)
            if existing_band is not None and existing_band != band:
                raise UpdateError(f"{item_path} conflicts with existing Band {band_id}")
            if existing_band is None:
                catalog["bands"].append(band)
                bands_by_id[band_id] = band

            lower = checked_number(raw.get("min_hz"), f"{item_path}.min_hz", positive=True)
            upper = checked_number(raw.get("max_hz"), f"{item_path}.max_hz", positive=True)
            if float(upper) <= float(lower):
                raise UpdateError(f"{item_path} has an empty or reversed range")
            segment_id = f"segment:sdrpp-iaru:{profile}:{slug(key)}"
            if segment_id in segment_ids:
                raise UpdateError(f"duplicate supplemental Segment {segment_id}")
            segment_ids.add(segment_id)
            catalog["segments"].append(
                {
                    "band_id": band_id,
                    "kind": "operating_plan",
                    "name": band_name,
                    "plan_id": plan_id,
                    "range": {"max_hz": upper, "min_hz": lower},
                    "segment_id": segment_id,
                    "service": "amateur",
                    "source_ref": {
                        "provider": provider,
                        "record_id": f"{provider}:{profile}:{slug(key)}",
                        "upstream_id": f"{source['title']}#{source_locator}",
                        "url": source["url"],
                    },
                    "status": "advisory",
                }
            )

        source_summary = {
            "checked_on": source["checked_on"],
            "name": source["organization"],
            "provider": provider,
            "revision": source["revision"],
            "title": source["title"],
            "url": source["url"],
        }
        sources.append(source_summary)
        inputs.append(
            {
                "authority_url": source["url"],
                "local_path": f"../iaru-overlays/{path.name}",
                "path": f"iaru-overlays/{path.name}",
                "sha256": sha256(data),
                "size": len(data),
            }
        )
        profile_counts[profile] = len(raw_segments)

    catalog["bands"].sort(key=lambda item: item["band_id"])
    catalog["segments"].sort(key=lambda item: item["segment_id"])
    validate_catalog(catalog)
    return {
        "inputs": inputs,
        "profile_segment_counts": profile_counts,
        "sources": sources,
        "total_segments": sum(profile_counts.values()),
    }


def input_inventory(tree: SourceTree) -> list[dict[str, Any]]:
    owner, repo = source_parts(tree.repository)
    raw_root = f"https://raw.githubusercontent.com/{owner}/{repo}/{tree.revision}"
    return [
        {
            "path": path,
            "local_path": f"upstream/{path}",
            "sha256": sha256(data),
            "size": len(data),
            "url": f"{raw_root}/{path}",
        }
        for path, data in sorted(tree.files.items())
    ]


def runtime_catalog_from_full(catalog: dict[str, Any]) -> dict[str, Any]:
    runtime = copy.deepcopy(catalog)
    for plan in runtime["plans"]:
        if not plan.get("scope"):
            plan.pop("scope", None)
    for segment in runtime["segments"]:
        segment.pop("source_ref", None)
    for bookmark in runtime["bookmarks"]:
        if bookmark.get("bandwidth") == 0:
            bookmark.pop("bandwidth", None)
        if not bookmark.get("scope"):
            bookmark.pop("scope", None)
        source = bookmark.get("source_ref")
        if source:
            bookmark["source_ref"] = {
                "provider": source["provider"],
                "record_id": source["record_id"],
            }
    validate_catalog(runtime)
    return runtime


DOCUMENT_KEYS = {
    "schema_version": "v",
    "plans": "p",
    "bands": "b",
    "segments": "s",
    "bookmarks": "m",
}
PLAN_KEYS = {
    "plan_id": "i",
    "name": "n",
    "scope": "c",
    "source": "o",
    "revision": "r",
}
SCOPE_KEYS = {
    "itu_region_mask": "i",
    "country_codes": "c",
    "subdivisions": "s",
}
BAND_KEYS = {"band_id": "i", "name": "n", "service": "s"}
SEGMENT_KEYS = {
    "segment_id": "i",
    "plan_id": "p",
    "band_id": "b",
    "name": "n",
    "service": "s",
    "kind": "k",
    "status": "t",
    "range": "r",
    "default_frequency": "f",
    "default_mode": "m",
    "channel_spacing": "c",
}
RANGE_KEYS = {"min_hz": "l", "max_hz": "h"}
BOOKMARK_KEYS = {
    "bookmark_id": "i",
    "layer": "l",
    "band_id": "b",
    "scope": "s",
    "name": "n",
    "frequency": "f",
    "bandwidth": "w",
    "mode": "m",
    "source_mode": "d",
    "underlying_mode": "u",
    "scannable": "c",
    "schedule": "h",
    "notes": "o",
    "geo_info": "g",
    "source_ref": "r",
}
SOURCE_REF_KEYS = {
    "provider": "p",
    "record_id": "i",
    "upstream_id": "u",
    "url": "l",
}
SCHEDULE_KEYS = {
    "start_minute_utc": "s",
    "end_minute_utc": "e",
    "day_mask": "d",
    "valid_from_ymd": "f",
    "valid_until_ymd": "u",
    "days_text": "t",
}


def remap_object(
    value: Any,
    mapping: dict[str, str],
    path: str,
    *,
    compact: bool,
) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise UpdateError(f"{path} must be an object")
    key_map = mapping if compact else {short: long for long, short in mapping.items()}
    unknown = sorted(set(value) - set(key_map))
    if unknown:
        raise UpdateError(f"{path} contains unmapped keys: {', '.join(unknown)}")
    return {key_map[key]: item for key, item in value.items()}


def compact_scope(value: Any, path: str) -> dict[str, Any]:
    return remap_object(value, SCOPE_KEYS, path, compact=True)


def expand_scope(value: Any, path: str) -> dict[str, Any]:
    return remap_object(value, SCOPE_KEYS, path, compact=False)


def compact_cbor_catalog(catalog: dict[str, Any]) -> dict[str, Any]:
    compact = remap_object(catalog, DOCUMENT_KEYS, "catalog", compact=True)
    compact["w"] = CBOR_WIRE_SCHEMA_VERSION
    compact["p"] = []
    for index, plan in enumerate(catalog["plans"]):
        item = remap_object(plan, PLAN_KEYS, f"plans[{index}]", compact=True)
        if "scope" in plan:
            item["c"] = compact_scope(plan["scope"], f"plans[{index}].scope")
        compact["p"].append(item)
    compact["b"] = [
        remap_object(band, BAND_KEYS, f"bands[{index}]", compact=True)
        for index, band in enumerate(catalog["bands"])
    ]
    compact["s"] = []
    for index, segment in enumerate(catalog["segments"]):
        item = remap_object(segment, SEGMENT_KEYS, f"segments[{index}]", compact=True)
        item["r"] = remap_object(
            segment["range"],
            RANGE_KEYS,
            f"segments[{index}].range",
            compact=True,
        )
        compact["s"].append(item)
    compact["m"] = []
    for index, bookmark in enumerate(catalog["bookmarks"]):
        path = f"bookmarks[{index}]"
        item = remap_object(bookmark, BOOKMARK_KEYS, path, compact=True)
        if "scope" in bookmark:
            item["s"] = compact_scope(bookmark["scope"], path + ".scope")
        if "source_ref" in bookmark:
            item["r"] = remap_object(
                bookmark["source_ref"],
                SOURCE_REF_KEYS,
                path + ".source_ref",
                compact=True,
            )
        if "schedule" in bookmark:
            item["h"] = remap_object(
                bookmark["schedule"],
                SCHEDULE_KEYS,
                path + ".schedule",
                compact=True,
            )
        compact["m"].append(item)
    return compact


def expand_cbor_catalog(compact: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(compact, dict):
        raise UpdateError("decoded CBOR catalog must be an object")
    wire_version = compact.get("w")
    if wire_version != CBOR_WIRE_SCHEMA_VERSION:
        raise UpdateError(f"unsupported Android CBOR wire schema: {wire_version}")
    encoded = dict(compact)
    encoded.pop("w")
    catalog = remap_object(encoded, DOCUMENT_KEYS, "cbor", compact=False)
    catalog["plans"] = []
    for index, plan in enumerate(encoded["p"]):
        item = remap_object(plan, PLAN_KEYS, f"cbor.p[{index}]", compact=False)
        if "c" in plan:
            item["scope"] = expand_scope(plan["c"], f"cbor.p[{index}].c")
        catalog["plans"].append(item)
    catalog["bands"] = [
        remap_object(band, BAND_KEYS, f"cbor.b[{index}]", compact=False)
        for index, band in enumerate(encoded["b"])
    ]
    catalog["segments"] = []
    for index, segment in enumerate(encoded["s"]):
        item = remap_object(
            segment,
            SEGMENT_KEYS,
            f"cbor.s[{index}]",
            compact=False,
        )
        item["range"] = remap_object(
            segment["r"],
            RANGE_KEYS,
            f"cbor.s[{index}].r",
            compact=False,
        )
        catalog["segments"].append(item)
    catalog["bookmarks"] = []
    for index, bookmark in enumerate(encoded["m"]):
        path = f"cbor.m[{index}]"
        item = remap_object(bookmark, BOOKMARK_KEYS, path, compact=False)
        if "s" in bookmark:
            item["scope"] = expand_scope(bookmark["s"], path + ".s")
        if "r" in bookmark:
            item["source_ref"] = remap_object(
                bookmark["r"],
                SOURCE_REF_KEYS,
                path + ".r",
                compact=False,
            )
        if "h" in bookmark:
            item["schedule"] = remap_object(
                bookmark["h"],
                SCHEDULE_KEYS,
                path + ".h",
                compact=False,
            )
        catalog["bookmarks"].append(item)
    validate_catalog(catalog)
    return catalog


def validate_runtime_projection(
    full_catalog: dict[str, Any],
    runtime_catalog: dict[str, Any],
) -> None:
    expected = runtime_catalog_from_full(full_catalog)
    if runtime_catalog != expected:
        raise UpdateError("runtime catalog diverges from the stripped full catalog")
    if catalog_ids(runtime_catalog) != catalog_ids(full_catalog):
        raise UpdateError("runtime catalog typed IDs diverge from the full catalog")


def runtime_notice(
    repository: str,
    revision: str,
    license_id: str,
    supplemental_sources: list[dict[str, Any]],
) -> bytes:
    text = (
        "OpenWebRX+ frequency data\n"
        "\n"
        "This application contains a normalized band and bookmark dataset derived\n"
        "from OpenWebRX+ contributors.\n"
        f"Source: {repository}\n"
        f"Revision: {revision}\n"
        f"License: {license_id}\n"
        "The complete license text is distributed as OPENWEBRX-LICENSE.txt.\n"
        "\n"
        "SDR++ IARU regional supplements\n"
        "\n"
        "Missing amateur-band ranges are supplemented from official IARU regional\n"
        "band plans. These are advisory operating plans, not authorization to\n"
        "transmit; national regulations always prevail.\n"
    )
    for source in supplemental_sources:
        text += (
            f"{source['name']}: {source['title']}, {source['revision']}\n"
            f"Source: {source['url']}\n"
        )
    return text.encode("utf-8")


def files_below(directory: Path) -> set[Path]:
    if not directory.is_dir():
        return set()
    return {path for path in directory.rglob("*") if path.is_file()}


def remove_empty_directories(directory: Path) -> None:
    if not directory.is_dir():
        return
    children = sorted(
        (path for path in directory.rglob("*") if path.is_dir()),
        key=lambda path: len(path.parts),
        reverse=True,
    )
    for child in children:
        try:
            child.rmdir()
        except OSError:
            pass


def parse_args() -> argparse.Namespace:
    repo_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, help="read a local OpenWebRX+ checkout")
    parser.add_argument(
        "--fetch",
        action="store_true",
        help="download the selected revision even when a vendored snapshot exists",
    )
    parser.add_argument("--revision", help="40-character upstream commit to use")
    parser.add_argument(
        "--advance-revision",
        action="store_true",
        help="allow --revision to change the pinned manifest revision",
    )
    parser.add_argument(
        "--accept-new-ids",
        action="store_true",
        help="allocate IDs for reviewed new semantic Bands",
    )
    parser.add_argument(
        "--allow-id-churn",
        action="store_true",
        help=f"allow more than {MAX_CHURN_RATIO:.0%} entity ID churn",
    )
    parser.add_argument("--check", action="store_true", help="compare outputs without writing")
    parser.add_argument(
        "--data-dir",
        type=Path,
        default=repo_root / "data" / "frequency_catalog" / "openwebrx",
        help="full audit artifacts and generator inputs",
    )
    parser.add_argument(
        "--res-dir",
        type=Path,
        default=repo_root / "root" / "res" / "frequency_catalog",
        help="stripped runtime resources",
    )
    parser.add_argument(
        "--legacy-band-dir",
        type=Path,
        default=repo_root / "root" / "res" / "bandplans",
        help="legacy SDR++ band plans used by the coverage audit",
    )
    parser.add_argument(
        "--coverage-audit-dir",
        type=Path,
        default=repo_root / "data" / "frequency_catalog" / "legacy-comparison",
        help="legacy/OpenWebRX+ coverage inputs and generated reports",
    )
    parser.add_argument(
        "--iaru-overlay-dir",
        type=Path,
        default=repo_root / "data" / "frequency_catalog" / "iaru-overlays",
        help="reviewed SDR++ IARU regional supplement inputs",
    )
    parser.add_argument(
        "--accept-coverage-baseline",
        action="store_true",
        help="explicitly acknowledge the current discrepancy IDs as the audit baseline",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    data_dir = args.data_dir.resolve()
    res_dir = args.res_dir.resolve()
    legacy_band_dir = args.legacy_band_dir.resolve()
    coverage_audit_dir = args.coverage_audit_dir.resolve()
    iaru_overlay_dir = args.iaru_overlay_dir.resolve()
    if args.source_dir and args.fetch:
        raise UpdateError("--source-dir and --fetch are mutually exclusive")
    if args.check and args.accept_coverage_baseline:
        raise UpdateError("--check and --accept-coverage-baseline are mutually exclusive")
    source_manifest_path = data_dir / "source-manifest.json"
    registry_path = data_dir / "id-registry.json"
    full_catalog_path = data_dir / "system-v1.full.json"
    report_path = data_dir / "validation-report.json"
    runtime_json_path = res_dir / "system-v1.json"
    runtime_cbor_path = res_dir / "system-v1.cbor"
    runtime_manifest_path = res_dir / "manifest-v1.json"
    notice_path = res_dir / "NOTICE.txt"
    license_path = res_dir / "OPENWEBRX-LICENSE.txt"
    coverage_report_path = coverage_audit_dir / "coverage-report.json"
    coverage_markdown_path = coverage_audit_dir / "coverage-report.md"
    coverage_decisions_path = coverage_audit_dir / "review-decisions.json"
    coverage_scope_path = coverage_audit_dir / "scope-map.json"
    coverage_aliases_path = coverage_audit_dir / "band-aliases.json"

    manifest = load_optional_json(source_manifest_path)
    registry = load_optional_json(registry_path)
    if not manifest or not registry:
        raise UpdateError("the checked-in source manifest and ID registry are required")
    validate_registry(registry)
    source = manifest.get("source")
    if not isinstance(source, dict):
        raise UpdateError("manifest.source must be an object")
    repository = source.get("repository")
    pinned_revision = source.get("revision")
    if not isinstance(repository, str) or not COMMIT_RE.fullmatch(str(pinned_revision)):
        raise UpdateError("manifest has no valid repository or pinned commit")
    revision = args.revision or pinned_revision
    if not COMMIT_RE.fullmatch(revision):
        raise UpdateError("--revision must be a full lowercase 40-character commit")
    if revision != pinned_revision and not args.advance_revision:
        raise UpdateError("changing the pinned revision requires --advance-revision")
    if args.advance_revision and not args.revision:
        raise UpdateError("--advance-revision requires --revision")

    vendored_source_dir = data_dir / "upstream"
    if args.source_dir:
        tree = read_local_tree(repository, revision, args.source_dir.resolve())
    elif (
        not args.fetch
        and revision == pinned_revision
        and vendored_source_dir.is_dir()
    ):
        tree = read_local_tree(repository, revision, vendored_source_dir)
    else:
        tree = read_remote_tree(repository, revision)

    full_catalog, diagnostics = build_catalog(tree, registry, args.accept_new_ids)
    overlay_diagnostics = apply_iaru_overlays(
        full_catalog,
        registry,
        iaru_overlay_dir,
    )
    diagnostics["iaru_overlays"] = {
        "profile_segment_counts": overlay_diagnostics["profile_segment_counts"],
        "sources": overlay_diagnostics["sources"],
        "total_segments": overlay_diagnostics["total_segments"],
    }
    try:
        coverage_audit = build_audit(
            full_catalog,
            legacy_band_dir,
            coverage_audit_dir,
            args.accept_coverage_baseline,
        )
    except AuditError as exc:
        raise UpdateError(f"legacy coverage audit failed: {exc}") from exc
    if coverage_audit.new_discrepancy_ids:
        raise UpdateError(
            "legacy coverage audit found new unreviewed discrepancies; "
            "review coverage-report.json, then use --accept-coverage-baseline "
            "or record explicit decisions:\n  "
            + "\n  ".join(coverage_audit.new_discrepancy_ids)
        )
    full_catalog_data = json_bytes(full_catalog)
    runtime_catalog = runtime_catalog_from_full(full_catalog)
    validate_runtime_projection(full_catalog, runtime_catalog)
    runtime_json_data = json_bytes(runtime_catalog)
    compact_cbor = compact_cbor_catalog(runtime_catalog)
    runtime_cbor_data = cbor_bytes(compact_cbor)
    decoded_cbor = decode_cbor(runtime_cbor_data)
    if expand_cbor_catalog(decoded_cbor) != runtime_catalog:
        raise UpdateError("generated Android CBOR does not round-trip")
    registry_data = json_bytes(registry)

    previous_catalog = load_optional_json(full_catalog_path)
    previous_report = load_optional_json(report_path)
    same_revision = pinned_revision == revision
    if (
        same_revision
        and previous_report
        and previous_report.get("target_revision") == revision
        and isinstance(previous_report.get("baseline_ids"), dict)
    ):
        baseline_ids = previous_report["baseline_ids"]
        baseline_revision = previous_report.get("baseline_revision")
    else:
        baseline_ids = catalog_ids(previous_catalog)
        baseline_revision = pinned_revision if previous_catalog else None
    current_ids = catalog_ids(full_catalog)
    churn, churn_ratio = build_churn(baseline_ids, current_ids)
    if baseline_ids and churn_ratio > MAX_CHURN_RATIO and not args.allow_id_churn:
        raise UpdateError(
            f"entity ID churn is {churn_ratio:.1%}, above {MAX_CHURN_RATIO:.0%}; "
            "review the report and use --allow-id-churn if intentional"
        )

    persisted_diagnostics = {
        key: value for key, value in diagnostics.items() if key != "new_band_keys"
    }
    persisted_diagnostics["legacy_coverage"] = coverage_audit.summary
    report = {
        "artifact_role": "audit",
        "baseline_ids": baseline_ids,
        "baseline_revision": baseline_revision,
        "catalog_sha256": sha256(full_catalog_data),
        "diagnostics": persisted_diagnostics,
        "id_churn": churn,
        "id_churn_ratio": churn_ratio,
        "schema_version": 1,
        "target_revision": revision,
        "validation": {"errors": [], "status": "valid"},
    }
    report_data = json_bytes(report)
    counts = {
        "bands": len(full_catalog["bands"]),
        "bookmarks": len(full_catalog["bookmarks"]),
        "plans": len(full_catalog["plans"]),
        "segments": len(full_catalog["segments"]),
    }
    license_id = source.get("license", "AGPL-3.0-only")
    notice_data = runtime_notice(
        repository,
        revision,
        license_id,
        overlay_diagnostics["sources"],
    )
    runtime_manifest = {
        "catalog_schema_version": SCHEMA_VERSION,
        "catalogs": {
            "android": {
                "encoding": "cbor",
                "path": "system-v1.cbor",
                "sha256": sha256(runtime_cbor_data),
                "size": len(runtime_cbor_data),
                "wire_schema_version": CBOR_WIRE_SCHEMA_VERSION,
            },
            "desktop": {
                "encoding": "json",
                "path": "system-v1.json",
                "sha256": sha256(runtime_json_data),
                "size": len(runtime_json_data),
            },
        },
        "counts": counts,
        "license_path": "OPENWEBRX-LICENSE.txt",
        "notice_path": "NOTICE.txt",
        "schema_version": 1,
        "supplemental_sources": overlay_diagnostics["sources"],
        "source": {
            "attribution": "OpenWebRX+ contributors",
            "license": license_id,
            "name": "OpenWebRX+",
            "revision": revision,
        },
    }
    runtime_manifest_data = json_bytes(runtime_manifest)
    coverage_scope_data = coverage_scope_path.read_bytes()
    coverage_aliases_data = coverage_aliases_path.read_bytes()
    generated_manifest = {
        "artifact_role": "audit",
        "catalog_schema_version": SCHEMA_VERSION,
        "generated": {
            "counts": counts,
            "full_catalog": {
                "path": "system-v1.full.json",
                "sha256": sha256(full_catalog_data),
                "size": len(full_catalog_data),
            },
            "id_registry": {
                "path": "id-registry.json",
                "sha256": sha256(registry_data),
                "size": len(registry_data),
            },
            "runtime_cbor": {
                "path": "../../../root/res/frequency_catalog/system-v1.cbor",
                "sha256": sha256(runtime_cbor_data),
                "size": len(runtime_cbor_data),
            },
            "runtime_json": {
                "path": "../../../root/res/frequency_catalog/system-v1.json",
                "sha256": sha256(runtime_json_data),
                "size": len(runtime_json_data),
            },
            "runtime_manifest": {
                "path": "../../../root/res/frequency_catalog/manifest-v1.json",
                "sha256": sha256(runtime_manifest_data),
                "size": len(runtime_manifest_data),
            },
            "validation_report": {
                "path": "validation-report.json",
                "sha256": sha256(report_data),
                "size": len(report_data),
            },
            "legacy_coverage_audit": {
                "band_aliases": {
                    "path": "../legacy-comparison/band-aliases.json",
                    "sha256": sha256(coverage_aliases_data),
                    "size": len(coverage_aliases_data),
                },
                "coverage_markdown": {
                    "path": "../legacy-comparison/coverage-report.md",
                    "sha256": sha256(coverage_audit.markdown_data),
                    "size": len(coverage_audit.markdown_data),
                },
                "coverage_report": {
                    "path": "../legacy-comparison/coverage-report.json",
                    "sha256": sha256(coverage_audit.report_data),
                    "size": len(coverage_audit.report_data),
                },
                "review_decisions": {
                    "path": "../legacy-comparison/review-decisions.json",
                    "sha256": sha256(coverage_audit.decisions_data),
                    "size": len(coverage_audit.decisions_data),
                },
                "scope_map": {
                    "path": "../legacy-comparison/scope-map.json",
                    "sha256": sha256(coverage_scope_data),
                    "size": len(coverage_scope_data),
                },
            },
        },
        "inputs": input_inventory(tree) + overlay_diagnostics["inputs"],
        "schema_version": 1,
        "supplemental_sources": overlay_diagnostics["sources"],
        "source": {
            "attribution": "OpenWebRX+ contributors",
            "license": license_id,
            "license_path": "../../../root/res/frequency_catalog/OPENWEBRX-LICENSE.txt",
            "license_url": f"{repository}/blob/{revision}/LICENSE.txt",
            "repository": repository,
            "revision": revision,
        },
    }
    source_manifest_data = json_bytes(generated_manifest)
    outputs = {
        full_catalog_path: full_catalog_data,
        registry_path: registry_data,
        report_path: report_data,
        coverage_report_path: coverage_audit.report_data,
        coverage_markdown_path: coverage_audit.markdown_data,
        coverage_decisions_path: coverage_audit.decisions_data,
        source_manifest_path: source_manifest_data,
        runtime_json_path: runtime_json_data,
        runtime_cbor_path: runtime_cbor_data,
        runtime_manifest_path: runtime_manifest_data,
        notice_path: notice_data,
        license_path: tree.require("LICENSE.txt"),
    }
    for path, data in tree.files.items():
        outputs[vendored_source_dir.joinpath(*PurePosixPath(path).parts)] = data
    expected_vendored_files = {
        vendored_source_dir.joinpath(*PurePosixPath(path).parts)
        for path in tree.files
    }
    unexpected_vendored_files = files_below(vendored_source_dir) - expected_vendored_files
    previously_recorded_vendored_files: set[Path] = set()
    for item in manifest.get("inputs", []):
        if not isinstance(item, dict) or not isinstance(item.get("local_path"), str):
            continue
        candidate = (data_dir / item["local_path"]).resolve()
        try:
            candidate.relative_to(vendored_source_dir)
        except ValueError:
            continue
        previously_recorded_vendored_files.add(candidate)
    removable_vendored_files = (
        unexpected_vendored_files & previously_recorded_vendored_files
    )
    unknown_vendored_files = unexpected_vendored_files - removable_vendored_files

    if args.check:
        stale = []
        for path, expected in outputs.items():
            try:
                actual = path.read_bytes()
            except OSError:
                stale.append(str(path))
                continue
            if actual != expected:
                stale.append(str(path))
        stale.extend(str(path) for path in sorted(unexpected_vendored_files))
        if stale:
            raise UpdateError("generated files are missing or stale:\n  " + "\n  ".join(stale))
        print(
            f"OpenWebRX+ catalog is current at {revision}: "
            f"{counts['bands']} Bands, {counts['segments']} Segments, "
            f"{counts['bookmarks']} Bookmarks"
        )
        return 0

    data_dir.mkdir(parents=True, exist_ok=True)
    res_dir.mkdir(parents=True, exist_ok=True)
    if unknown_vendored_files:
        raise UpdateError(
            "refusing to remove untracked files from the vendored upstream mirror:\n  "
            + "\n  ".join(str(path) for path in sorted(unknown_vendored_files))
        )
    for path in removable_vendored_files:
        path.unlink()
    remove_empty_directories(vendored_source_dir)
    for path, data in outputs.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    print(
        f"Generated OpenWebRX+ catalog at {revision}: "
        f"{counts['bands']} Bands, {counts['segments']} Segments, "
        f"{counts['bookmarks']} Bookmarks; "
        f"desktop JSON {len(runtime_json_data)} bytes, "
        f"Android CBOR {len(runtime_cbor_data)} bytes"
    )
    if diagnostics["new_band_keys"]:
        print(f"Allocated {len(diagnostics['new_band_keys'])} new semantic Band IDs")
    if diagnostics["unknown_source_modes"]:
        print("Unmapped source modes: " + ", ".join(diagnostics["unknown_source_modes"]))
    if diagnostics["skipped_dial_entries"]:
        print(f"Skipped {len(diagnostics['skipped_dial_entries'])} out-of-band dial entries")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except UpdateError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
