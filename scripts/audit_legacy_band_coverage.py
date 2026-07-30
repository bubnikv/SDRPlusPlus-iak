#!/usr/bin/env python3
"""Compare legacy SDR++ band plans with the normalized system catalog.

The audit is deliberately stricter than the runtime query model. It compares
semantic Band candidates, Segment interval coverage, tuning metadata, and
scoped Bookmark evidence independently. Frequency overlap helps find audit
candidates but never assigns a runtime identity. The system catalog may layer
reviewed supplements over the pinned OpenWebRX+ base.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


SCHEMA_VERSION = 1
PROFILE_MASKS = {"default": 0, "r1": 1, "r2": 2, "r3": 4}
SERVICE_ALIASES = {
    "aircraft": "aviation",
    "amateur1": "amateur",
    "ham": "amateur",
    "mobile": "land-mobile",
}
AMATEUR_METRE_IDS = {
    "2200": "2200m", "2190": "2190m", "630": "630m", "160": "160m",
    "80": "80m", "60": "60m", "40": "40m", "30": "30m", "20": "20m",
    "17": "17m", "15": "15m", "12": "12m", "10": "10m", "8": "8m",
    "6": "6m", "4": "4m", "2": "2m",
}
AMATEUR_CM_IDS = {
    "125": "125cm", "70": "70cm", "33": "33cm", "23": "23cm",
    "13": "13cm", "9": "9cm", "6": "6cm", "5": "5cm", "3": "3cm",
    "1.2": "12mm",
}
AMATEUR_MM_IDS = {
    "12": "12mm", "6": "6mm", "4": "4mm", "2.5": "25mm",
    "2": "2mm", "1": "1mm",
}


class AuditError(RuntimeError):
    pass


@dataclass
class AuditArtifacts:
    report_data: bytes
    markdown_data: bytes
    decisions_data: bytes
    discrepancy_ids: list[str]
    new_discrepancy_ids: list[str]
    summary: dict[str, int]


def json_bytes(value: Any) -> bytes:
    return (json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True) + "\n").encode("utf-8")


def load_json(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise AuditError(f"cannot read {path}: {exc}") from exc


def normalized_name(value: str) -> str:
    value = value.casefold().replace("–", "-").replace("—", "-")
    value = re.sub(r"(?<=\d),(?=\d)", ".", value)
    value = re.sub(r"radio\s*amateur|radioamateur|amateur radio|ham band|ham", "amateur", value)
    value = re.sub(r"\bband\b", " ", value)
    value = re.sub(r"[^a-z0-9.]+", " ", value)
    return " ".join(value.split())


def normalized_service(value: str) -> str:
    service = re.sub(r"[^a-z0-9-]+", "-", value.casefold()).strip("-")
    return SERVICE_ALIASES.get(service, service or "other")


def stable_hash(*parts: str, length: int = 24) -> str:
    digest = hashlib.sha256()
    for part in parts:
        data = part.encode("utf-8")
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()[:length]


def checked_number(value: Any, path: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise AuditError(f"{path} must be a number")
    result = float(value)
    if result < 0:
        raise AuditError(f"{path} must be non-negative")
    return result


def frequency_scale(value: float) -> tuple[float, str]:
    magnitude = abs(value)
    if magnitude >= 1_000_000_000:
        return 1_000_000_000.0, "GHz"
    if magnitude >= 1_000_000:
        return 1_000_000.0, "MHz"
    if magnitude >= 1_000:
        return 1_000.0, "kHz"
    return 1.0, "Hz"


def format_scaled_frequency(value: float, scale: float) -> str:
    return f"{value / scale:.9f}".rstrip("0").rstrip(".")


def format_frequency_hz(value: float) -> str:
    scale, unit = frequency_scale(value)
    return f"{format_scaled_frequency(value, scale)} {unit}"


def format_frequency_range_hz(lower: float, upper: float) -> str:
    scale, unit = frequency_scale(max(abs(lower), abs(upper)))
    return (
        f"{format_scaled_frequency(lower, scale)}–"
        f"{format_scaled_frequency(upper, scale)} {unit}"
    )


def interval_overlap(a: tuple[float, float], b: tuple[float, float]) -> float:
    return max(0.0, min(a[1], b[1]) - max(a[0], b[0]))


def merge_intervals(values: Iterable[tuple[float, float]]) -> list[tuple[float, float]]:
    result: list[list[float]] = []
    for lower, upper in sorted(values):
        if result and lower <= result[-1][1]:
            result[-1][1] = max(result[-1][1], upper)
        else:
            result.append([lower, upper])
    return [(lower, upper) for lower, upper in result]


def covered_length(target: tuple[float, float], values: Iterable[tuple[float, float]]) -> float:
    return sum(interval_overlap(target, value) for value in merge_intervals(values))


def automatic_band_id(name: str, service: str) -> str | None:
    normalized = normalized_name(name)
    if service == "amateur":
        match = re.search(
            r"(?<![0-9.])([0-9]+(?:\.[0-9]+)?)\s*(mm|cm|m)(?![a-z])",
            normalized,
        )
        if match:
            number, unit = match.groups()
            if number == "1.25" and unit == "m":
                return "band:amateur:125cm"
            table = {
                "m": AMATEUR_METRE_IDS,
                "cm": AMATEUR_CM_IDS,
                "mm": AMATEUR_MM_IDS,
            }[unit]
            key = number.rstrip("0").rstrip(".") if "." in number else number
            if key in table:
                return "band:amateur:" + table[key]
    if "pmr446" in normalized.replace(" ", ""):
        return "band:land-mobile:pmr446"
    if service in {"personal-radio", "land-mobile"} and re.search(r"\b(cb|citizen)", normalized):
        return "band:personal-radio:cb"
    return None


def profile_plan_ids(catalog: dict[str, Any]) -> dict[str, list[str]]:
    result = {profile: [] for profile in PROFILE_MASKS}
    for plan in catalog.get("plans", []):
        mask = plan.get("scope", {}).get("itu_region_mask", 0)
        for profile, expected in PROFILE_MASKS.items():
            if mask == expected:
                result[profile].append(plan["plan_id"])
    return result


def scope_applies(scope: dict[str, Any], country: str, region_mask: int) -> bool:
    countries = scope.get("country_codes", [])
    subdivisions = scope.get("subdivisions", [])
    mask = scope.get("itu_region_mask", 0)
    if countries and country not in countries:
        return False
    if subdivisions:
        return False
    if mask and not (mask & region_mask):
        return False
    return True


def candidate_dict(segment: dict[str, Any], overlap: float) -> dict[str, Any]:
    bounds = segment["range"]
    return {
        "band_id": segment.get("band_id"),
        "max_hz": bounds["max_hz"],
        "min_hz": bounds["min_hz"],
        "name": segment["name"],
        "overlap_hz": overlap,
        "plan_id": segment["plan_id"],
        "segment_id": segment["segment_id"],
        "service": segment.get("service", ""),
    }


def build_audit(
    catalog: dict[str, Any],
    legacy_directory: Path,
    audit_directory: Path,
    accept_baseline: bool = False,
) -> AuditArtifacts:
    scope_map = load_json(audit_directory / "scope-map.json")
    alias_document = load_json(audit_directory / "band-aliases.json")
    decisions = load_json(audit_directory / "review-decisions.json")
    for name, document in (
        ("scope map", scope_map),
        ("band aliases", alias_document),
        ("review decisions", decisions),
    ):
        if document.get("schema_version") != SCHEMA_VERSION:
            raise AuditError(f"{name} has an unsupported schema")

    configured_plans = scope_map.get("plans", {})
    legacy_paths = sorted(legacy_directory.glob("*.json"), key=lambda path: path.name)
    actual_names = {path.name for path in legacy_paths}
    configured_names = set(configured_plans)
    if actual_names != configured_names:
        missing = sorted(actual_names - configured_names)
        stale = sorted(configured_names - actual_names)
        raise AuditError(f"scope map mismatch; missing={missing}, stale={stale}")

    aliases = alias_document.get("aliases", {})
    plan_ids_by_profile = profile_plan_ids(catalog)
    segments = catalog.get("segments", [])
    bookmarks = catalog.get("bookmarks", [])
    revision_values = sorted({
        plan.get("revision", "") for plan in catalog.get("plans", []) if plan.get("revision")
    })
    report_plans: list[dict[str, Any]] = []
    all_discrepancies: list[dict[str, Any]] = []
    status_totals: dict[str, int] = {}

    for legacy_path in legacy_paths:
        document = load_json(legacy_path)
        config = configured_plans[legacy_path.name]
        profiles = config.get("profiles", [])
        if not profiles or any(profile not in PROFILE_MASKS for profile in profiles):
            raise AuditError(f"{legacy_path.name} has invalid profile candidates")
        country = config.get("country_code", "")
        declared_country = document.get("country_code", "")
        accepted_legacy_countries = {
            "--",
            country,
            *config.get("legacy_country_codes", []),
        }
        if declared_country not in accepted_legacy_countries:
            raise AuditError(
                f"{legacy_path.name} country mismatch: {declared_country!r} != {country!r}"
            )
        relevant_plan_ids = {
            plan_id for profile in profiles for plan_id in plan_ids_by_profile[profile]
        }
        region_mask = 0
        for profile in profiles:
            region_mask |= PROFILE_MASKS[profile]
        relevant_segments = [
            segment for segment in segments if segment["plan_id"] in relevant_plan_ids
        ]
        applicable_bookmarks = [
            bookmark for bookmark in bookmarks
            if scope_applies(bookmark.get("scope", {}), country, region_mask)
        ]
        plan_entries: list[dict[str, Any]] = []
        plan_statuses: dict[str, int] = {}

        raw_bands = document.get("bands")
        if not isinstance(raw_bands, list):
            raise AuditError(f"{legacy_path.name}.bands must be an array")
        for index, raw in enumerate(raw_bands):
            if not isinstance(raw, dict):
                raise AuditError(f"{legacy_path.name}.bands[{index}] must be an object")
            name = str(raw.get("name", "")).strip()
            service = normalized_service(str(raw.get("type", "")))
            lower = checked_number(raw.get("start"), f"{legacy_path.name}.bands[{index}].start")
            upper = checked_number(raw.get("end"), f"{legacy_path.name}.bands[{index}].end")
            if upper < lower:
                legacy_identity = {
                    "end_hz": upper,
                    "file": legacy_path.name,
                    "name": name,
                    "service": service,
                    "start_hz": lower,
                }
                issues = ["invalid_reversed_range"]
                discrepancy_id = "legacy-band:" + stable_hash(
                    json.dumps(
                        {"issues": issues, "legacy": legacy_identity},
                        sort_keys=True,
                        separators=(",", ":"),
                    )
                )
                entry = {
                    "band_match": {
                        "band_id": None,
                        "key": f"{service}|{normalized_name(name)}",
                        "method": "none",
                    },
                    "bookmark_evidence": {
                        "default_frequency_matches": 0,
                        "in_range": 0,
                    },
                    "candidates": [],
                    "comparison": {
                        "covered_hz": 0,
                        "legacy_length_hz": 0,
                        "status": "invalid",
                        "uncovered_hz": 0,
                    },
                    "discrepancy_id": discrepancy_id,
                    "issues": issues,
                    "legacy": {
                        **legacy_identity,
                        "channel_spacing_hz": raw.get("chan", 0),
                        "default_frequency_hz": raw.get("def_freq", 0),
                        "default_mode": str(raw.get("def_mode", "")),
                        "index": index,
                    },
                }
                plan_entries.append(entry)
                plan_statuses["invalid"] = plan_statuses.get("invalid", 0) + 1
                status_totals["invalid"] = status_totals.get("invalid", 0) + 1
                all_discrepancies.append(entry)
                continue
            interval = (lower, upper)
            alias_key = f"{service}|{normalized_name(name)}"
            band_id = aliases.get(alias_key)
            match_method = "reviewed_alias" if band_id else ""
            if not band_id:
                band_id = automatic_band_id(name, service)
                if band_id:
                    match_method = "automatic_semantic"

            if band_id:
                identity_candidates = [
                    segment for segment in relevant_segments
                    if segment.get("band_id") == band_id
                ]
                identity_has_overlap = any(
                    interval_overlap(
                        interval,
                        (segment["range"]["min_hz"], segment["range"]["max_hz"]),
                    ) > 0
                    for segment in identity_candidates
                )
                semantic_mismatch = False
                if not identity_has_overlap:
                    geometry_candidates = [
                        segment for segment in relevant_segments
                        if normalized_service(segment.get("service", "")) == service
                        and interval_overlap(
                            interval,
                            (segment["range"]["min_hz"], segment["range"]["max_hz"]),
                        ) > 0
                    ]
                    if geometry_candidates:
                        identity_candidates = geometry_candidates
                        semantic_mismatch = True
            else:
                identity_candidates = [
                    segment for segment in relevant_segments
                    if normalized_service(segment.get("service", "")) == service
                    and interval_overlap(
                        interval,
                        (segment["range"]["min_hz"], segment["range"]["max_hz"]),
                    ) > 0
                ]
                semantic_mismatch = False
            overlaps = [
                (
                    segment,
                    interval_overlap(
                        interval,
                        (segment["range"]["min_hz"], segment["range"]["max_hz"]),
                    ),
                )
                for segment in identity_candidates
            ]
            overlapping = [(segment, overlap) for segment, overlap in overlaps if overlap > 0]
            candidate_intervals = [
                (segment["range"]["min_hz"], segment["range"]["max_hz"])
                for segment, _ in overlapping
            ]
            legacy_length = upper - lower
            covered = covered_length(interval, candidate_intervals)
            tolerance = 1.0
            exact = any(
                abs(segment["range"]["min_hz"] - lower) <= tolerance
                and abs(segment["range"]["max_hz"] - upper) <= tolerance
                for segment, _ in overlapping
            )
            same_service_conflicts = [
                segment for segment in relevant_segments
                if normalized_service(segment.get("service", "")) != service
                and interval_overlap(
                    interval,
                    (segment["range"]["min_hz"], segment["range"]["max_hz"]),
                ) > 0
            ]
            if exact:
                status = "exact"
            elif not overlapping:
                status = "service_conflict" if same_service_conflicts else "missing"
            elif legacy_length == 0 or covered >= legacy_length - tolerance:
                status = "openwebrx_contains_legacy"
            elif any(
                lower <= segment["range"]["min_hz"] + tolerance
                and upper >= segment["range"]["max_hz"] - tolerance
                for segment, _ in overlapping
            ):
                status = "legacy_contains_openwebrx"
            else:
                status = "partial_overlap"

            issues: list[str] = []
            if not band_id:
                issues.append("semantic_band_unmapped")
            if semantic_mismatch:
                issues.append("semantic_identity_mismatch")
            if status != "exact":
                issues.append("range_" + status)
            channel_spacing = checked_number(
                raw.get("chan", 0), f"{legacy_path.name}.bands[{index}].chan"
            )
            if channel_spacing > 0 and not any(
                abs(float(segment.get("channel_spacing", 0)) - channel_spacing) <= tolerance
                for segment, _ in overlapping
            ):
                issues.append("channel_spacing_missing_or_different")
            default_frequency = checked_number(
                raw.get("def_freq", 0), f"{legacy_path.name}.bands[{index}].def_freq"
            )
            in_range_bookmarks = [
                bookmark for bookmark in applicable_bookmarks
                if lower <= float(bookmark["frequency"]) <= upper
            ]
            default_evidence = [
                bookmark for bookmark in in_range_bookmarks
                if default_frequency > 0
                and abs(float(bookmark["frequency"]) - default_frequency) <= tolerance
            ]
            if default_frequency > 0 and not default_evidence and not any(
                abs(float(segment.get("default_frequency", 0)) - default_frequency) <= tolerance
                for segment, _ in overlapping
            ):
                issues.append("default_frequency_missing_or_different")

            candidates = [
                candidate_dict(segment, overlap)
                for segment, overlap in sorted(
                    overlapping,
                    key=lambda item: (
                        -item[1],
                        item[0]["plan_id"],
                        item[0]["segment_id"],
                    ),
                )
            ]
            conflict_candidates = [
                candidate_dict(
                    segment,
                    interval_overlap(
                        interval,
                        (segment["range"]["min_hz"], segment["range"]["max_hz"]),
                    ),
                )
                for segment in same_service_conflicts
            ] if status == "service_conflict" else []
            legacy_identity = {
                "end_hz": upper,
                "file": legacy_path.name,
                "name": name,
                "service": service,
                "start_hz": lower,
            }
            discrepancy_id = ""
            if issues:
                fingerprint = {
                    "band_id": band_id,
                    "candidates": candidates,
                    "conflicts": conflict_candidates,
                    "issues": issues,
                    "legacy": legacy_identity,
                    "status": status,
                }
                discrepancy_id = "legacy-band:" + stable_hash(
                    json.dumps(fingerprint, sort_keys=True, separators=(",", ":"))
                )
            entry = {
                "band_match": {
                    "band_id": band_id,
                    "key": alias_key,
                    "method": match_method or "none",
                },
                "bookmark_evidence": {
                    "default_frequency_matches": len(default_evidence),
                    "in_range": len(in_range_bookmarks),
                },
                "candidates": candidates,
                "comparison": {
                    "covered_hz": covered,
                    "legacy_length_hz": legacy_length,
                    "status": status,
                    "uncovered_hz": max(0.0, legacy_length - covered),
                },
                "discrepancy_id": discrepancy_id,
                "issues": issues,
                "legacy": {
                    **legacy_identity,
                    "channel_spacing_hz": channel_spacing,
                    "default_frequency_hz": default_frequency,
                    "default_mode": str(raw.get("def_mode", "")),
                    "index": index,
                },
            }
            if conflict_candidates:
                entry["service_conflicts"] = conflict_candidates
            plan_entries.append(entry)
            plan_statuses[status] = plan_statuses.get(status, 0) + 1
            status_totals[status] = status_totals.get(status, 0) + 1
            if discrepancy_id:
                all_discrepancies.append(entry)

        report_plans.append({
            "country_code": country,
            "legacy_file": legacy_path.name,
            "name": document.get("name", legacy_path.stem),
            "profiles": profiles,
            "segments": plan_entries,
            "specialized": bool(config.get("specialized", False)),
            "summary": dict(sorted(plan_statuses.items())),
        })

    discrepancy_ids = sorted(entry["discrepancy_id"] for entry in all_discrepancies)
    explicit_decisions = decisions.get("decisions", {})
    if not isinstance(explicit_decisions, dict):
        raise AuditError("review-decisions.json decisions must be an object")
    if accept_baseline:
        decisions["baseline_discrepancy_ids"] = [
            value for value in discrepancy_ids if value not in explicit_decisions
        ]
    baseline = set(decisions.get("baseline_discrepancy_ids", []))
    accepted = baseline | set(explicit_decisions)
    new_ids = sorted(set(discrepancy_ids) - accepted)
    stale_ids = sorted(accepted - set(discrepancy_ids))
    for entry in all_discrepancies:
        discrepancy_id = entry["discrepancy_id"]
        if discrepancy_id in explicit_decisions:
            entry["review"] = explicit_decisions[discrepancy_id]
        elif discrepancy_id in baseline:
            entry["review"] = {"disposition": "baseline_needs_review"}
        else:
            entry["review"] = {"disposition": "unreviewed"}

    issue_totals: dict[str, int] = {}
    for entry in all_discrepancies:
        for issue in entry["issues"]:
            issue_totals[issue] = issue_totals.get(issue, 0) + 1
    summary = {
        "catalog_segments": len(segments),
        "discrepancies": len(discrepancy_ids),
        "iaru_overlay_segments": sum(
            1 for segment in segments
            if segment.get("segment_id", "").startswith("segment:sdrpp-iaru:")
        ),
        "legacy_plans": len(report_plans),
        "legacy_segments": sum(len(plan["segments"]) for plan in report_plans),
        "metadata_discrepancies": sum(
            count for issue, count in issue_totals.items()
            if issue.startswith("channel_") or issue.startswith("default_")
        ),
        "new_unreviewed": len(new_ids),
        "range_discrepancies": sum(
            count for status, count in status_totals.items()
            if status != "exact"
        ),
        "semantic_discrepancies": sum(
            count for issue, count in issue_totals.items()
            if issue.startswith("semantic_")
        ),
        **{f"status_{key}": value for key, value in sorted(status_totals.items())},
    }
    report = {
        "artifact_role": "audit",
        "catalog_revisions": revision_values,
        "new_unreviewed_discrepancy_ids": new_ids,
        "plans": report_plans,
        "schema_version": SCHEMA_VERSION,
        "stale_review_ids": stale_ids,
        "summary": summary,
    }
    markdown = render_markdown(report)
    return AuditArtifacts(
        report_data=json_bytes(report),
        markdown_data=markdown.encode("utf-8"),
        decisions_data=json_bytes(decisions),
        discrepancy_ids=discrepancy_ids,
        new_discrepancy_ids=new_ids,
        summary=summary,
    )


def render_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    lines = [
        "# Legacy SDR++ / system catalog band coverage audit",
        "",
        "Generated deterministically from the committed legacy band plans and "
        "the normalized OpenWebRX+ catalog with reviewed supplemental overlays.",
        "Bookmark counts are channel evidence only; they never establish Segment coverage.",
        "",
        "## Summary",
        "",
        f"- System catalog Segments: {summary['catalog_segments']:,}",
        f"- IARU overlay Segments: {summary['iaru_overlay_segments']:,}",
        f"- Legacy plans: {summary['legacy_plans']:,}",
        f"- Legacy Segments: {summary['legacy_segments']:,}",
        f"- Discrepancies: {summary['discrepancies']:,}",
        f"- Range discrepancies: {summary['range_discrepancies']:,}",
        f"- Semantic discrepancies: {summary['semantic_discrepancies']:,}",
        f"- Tuning-metadata discrepancies: {summary['metadata_discrepancies']:,}",
        f"- New unreviewed discrepancies: {summary['new_unreviewed']:,}",
        "",
        "| Legacy plan | Profiles | Exact | Covered | Broader | Partial | Missing | Conflict | Invalid |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for plan in report["plans"]:
        counts = plan["summary"]
        lines.append(
            f"| {plan['name']} (`{plan['legacy_file']}`) | {', '.join(plan['profiles'])} "
            f"| {counts.get('exact', 0)} "
            f"| {counts.get('openwebrx_contains_legacy', 0)} "
            f"| {counts.get('legacy_contains_openwebrx', 0)} "
            f"| {counts.get('partial_overlap', 0)} "
            f"| {counts.get('missing', 0)} "
            f"| {counts.get('service_conflict', 0)} "
            f"| {counts.get('invalid', 0)} |"
        )
    lines.extend(["", "## Missing and partial coverage", ""])
    for plan in report["plans"]:
        notable = [
            entry for entry in plan["segments"]
            if entry["comparison"]["status"] in {
                "missing", "service_conflict", "partial_overlap",
                "legacy_contains_openwebrx", "invalid"
            }
        ]
        if not notable:
            continue
        lines.extend([f"### {plan['name']}", ""])
        for entry in notable:
            legacy = entry["legacy"]
            comparison = entry["comparison"]
            lines.append(
                f"- `{comparison['status']}`: {legacy['name']} "
                f"({format_frequency_range_hz(legacy['start_hz'], legacy['end_hz'])}, "
                f"{legacy['service']}), uncovered "
                f"{format_frequency_hz(comparison['uncovered_hz'])}, "
                f"`{entry['discrepancy_id']}`"
            )
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--catalog",
        type=Path,
        default=root / "data" / "frequency_catalog" / "openwebrx" / "system-v1.full.json",
    )
    parser.add_argument(
        "--legacy-dir",
        type=Path,
        default=root / "root" / "res" / "bandplans",
    )
    parser.add_argument(
        "--audit-dir",
        type=Path,
        default=root / "data" / "frequency_catalog" / "legacy-comparison",
    )
    parser.add_argument("--check", action="store_true")
    parser.add_argument(
        "--accept-baseline",
        action="store_true",
        help="acknowledge the current discrepancy IDs as the review baseline",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    audit_dir = args.audit_dir.resolve()
    artifacts = build_audit(
        load_json(args.catalog.resolve()),
        args.legacy_dir.resolve(),
        audit_dir,
        args.accept_baseline,
    )
    outputs = {
        audit_dir / "coverage-report.json": artifacts.report_data,
        audit_dir / "coverage-report.md": artifacts.markdown_data,
        audit_dir / "review-decisions.json": artifacts.decisions_data,
    }
    if args.check:
        stale = [
            str(path) for path, expected in outputs.items()
            if not path.is_file() or path.read_bytes() != expected
        ]
        if stale:
            raise AuditError("generated audit files are stale:\n  " + "\n  ".join(stale))
        if artifacts.new_discrepancy_ids:
            raise AuditError(
                "new unreviewed discrepancies:\n  "
                + "\n  ".join(artifacts.new_discrepancy_ids)
            )
        print(
            "Legacy coverage audit is current: "
            f"{artifacts.summary['legacy_segments']} Segments, "
            f"{artifacts.summary['discrepancies']} discrepancies"
        )
        return 0
    audit_dir.mkdir(parents=True, exist_ok=True)
    for path, data in outputs.items():
        path.write_bytes(data)
    print(
        "Generated legacy coverage audit: "
        f"{artifacts.summary['legacy_segments']} Segments, "
        f"{artifacts.summary['discrepancies']} discrepancies, "
        f"{artifacts.summary['new_unreviewed']} new unreviewed"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AuditError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
