#!/usr/bin/env python3
"""Parse benchmark output, emit machine-readable reports, and enforce thresholds."""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any, Dict, List


BENCH_LINE_RE = re.compile(
    r"^(push-only|pop-only|mixed \(50/50\))\s+(\d+)\s+"
    r"([0-9]+(?:\.[0-9]+)?)\s+([0-9]+(?:\.[0-9]+)?)\s+"
    r"([0-9]+(?:\.[0-9]+)?)\s+([0-9]+(?:\.[0-9]+)?)\s+"
    r"([0-9]+(?:\.[0-9]+)?)x\s+([0-9]+(?:\.[0-9]+)?)x$"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, help="Path to benchmark stdout text file.")
    parser.add_argument("--thresholds", required=True, help="Path to thresholds JSON file.")
    parser.add_argument("--json", required=True, help="Path to output parsed JSON file.")
    parser.add_argument("--markdown", required=True, help="Path to output markdown summary file.")
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero when any threshold is violated.",
    )
    return parser.parse_args()


def load_thresholds(path: Path) -> Dict[str, Any]:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if "default" not in raw:
        raise ValueError("thresholds JSON must contain a 'default' object")
    default = raw["default"]
    if "min_speedup_p50" not in default or "min_speedup_p95" not in default:
        raise ValueError(
            "thresholds.default must include 'min_speedup_p50' and 'min_speedup_p95'"
        )
    if "overrides" not in raw:
        raw["overrides"] = {}
    return raw


def parse_bench_lines(text: str) -> List[Dict[str, Any]]:
    cases: List[Dict[str, Any]] = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        match = BENCH_LINE_RE.match(line)
        if not match:
            continue

        test_name = match.group(1)
        n = int(match.group(2))
        dheap_p50 = float(match.group(3))
        dheap_p95 = float(match.group(4))
        stl_p50 = float(match.group(5))
        stl_p95 = float(match.group(6))
        speedup_p50 = float(match.group(7))
        speedup_p95 = float(match.group(8))
        cases.append(
            {
                "test": test_name,
                "n": n,
                "dheap_p50_ms": dheap_p50,
                "dheap_p95_ms": dheap_p95,
                "stl_p50_ms": stl_p50,
                "stl_p95_ms": stl_p95,
                "speedup_p50": speedup_p50,
                "speedup_p95": speedup_p95,
            }
        )
    return cases


def evaluate_cases(cases: List[Dict[str, Any]], thresholds: Dict[str, Any]) -> List[Dict[str, Any]]:
    regressions: List[Dict[str, Any]] = []
    default_cfg = thresholds["default"]
    overrides = thresholds.get("overrides", {})

    for case in cases:
        key = f"{case['test']}@{case['n']}"
        cfg = dict(default_cfg)
        cfg.update(overrides.get(key, {}))
        min_p50 = float(cfg["min_speedup_p50"])
        min_p95 = float(cfg["min_speedup_p95"])

        p50_ok = case["speedup_p50"] >= min_p50
        p95_ok = case["speedup_p95"] >= min_p95
        case["threshold"] = {"min_speedup_p50": min_p50, "min_speedup_p95": min_p95}
        case["status"] = "PASS" if p50_ok and p95_ok else "FAIL"

        if p50_ok and p95_ok:
            continue

        regression = {
            "key": key,
            "test": case["test"],
            "n": case["n"],
            "speedup_p50": case["speedup_p50"],
            "speedup_p95": case["speedup_p95"],
            "min_speedup_p50": min_p50,
            "min_speedup_p95": min_p95,
        }
        regressions.append(regression)

    return regressions


def build_markdown(
    cases: List[Dict[str, Any]], regressions: List[Dict[str, Any]], thresholds: Dict[str, Any]
) -> str:
    lines: List[str] = []
    lines.append("## Benchmark Summary")
    lines.append("")
    lines.append(
        "Threshold defaults: "
        f"`Spd(p50) >= {thresholds['default']['min_speedup_p50']}`, "
        f"`Spd(p95) >= {thresholds['default']['min_speedup_p95']}`"
    )
    lines.append("")
    lines.append("| Test | N | DHeap p50 (ms) | DHeap p95 (ms) | STL p50 (ms) | STL p95 (ms) | Spd(p50) | Spd(p95) | Status |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---|")
    for case in cases:
        lines.append(
            "| {test} | {n} | {dheap_p50_ms:.3f} | {dheap_p95_ms:.3f} | "
            "{stl_p50_ms:.3f} | {stl_p95_ms:.3f} | {speedup_p50:.3f}x | {speedup_p95:.3f}x | {status} |".format(
                **case
            )
        )

    lines.append("")
    if regressions:
        lines.append("### Regression Alerts")
        for r in regressions:
            lines.append(
                "- `{key}` threshold violation: "
                "Spd(p50)={speedup_p50:.3f}x < {min_speedup_p50:.3f}x "
                "or Spd(p95)={speedup_p95:.3f}x < {min_speedup_p95:.3f}x".format(**r)
            )
    else:
        lines.append("### Regression Alerts")
        lines.append("- None")

    return "\n".join(lines) + "\n"


def main() -> int:
    args = parse_args()
    input_path = Path(args.input)
    threshold_path = Path(args.thresholds)
    json_path = Path(args.json)
    markdown_path = Path(args.markdown)

    bench_text = input_path.read_text(encoding="utf-8")
    thresholds = load_thresholds(threshold_path)
    cases = parse_bench_lines(bench_text)

    if not cases:
        print("No benchmark result rows parsed from input.", file=sys.stderr)
        return 2

    regressions = evaluate_cases(cases, thresholds)

    result = {
        "passed": len(regressions) == 0,
        "thresholds": thresholds,
        "cases": cases,
        "regressions": regressions,
    }
    json_path.write_text(json.dumps(result, indent=2), encoding="utf-8")
    markdown_path.write_text(build_markdown(cases, regressions, thresholds), encoding="utf-8")

    for r in regressions:
        print(
            "::warning::Benchmark regression in {key}: "
            "Spd(p50)={speedup_p50:.3f}x (min {min_speedup_p50:.3f}x), "
            "Spd(p95)={speedup_p95:.3f}x (min {min_speedup_p95:.3f}x)".format(**r)
        )

    if regressions and args.strict:
        print(f"Threshold check failed for {len(regressions)} case(s).", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
