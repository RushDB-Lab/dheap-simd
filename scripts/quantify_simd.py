#!/usr/bin/env python3
"""Quantify SIMD contribution by comparing SIMD-on and forced-scalar builds."""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Tuple


ROW_RE = re.compile(
    r"^(push-only|pop-only|mixed \(50/50\))\s+(\d+)\s+"
    r"([0-9]+(?:\.[0-9]+)?)\s+([0-9]+(?:\.[0-9]+)?)\s+"
    r"([0-9]+(?:\.[0-9]+)?)\s+([0-9]+(?:\.[0-9]+)?)\s+"
    r"([0-9]+(?:\.[0-9]+)?)x\s+([0-9]+(?:\.[0-9]+)?)x$"
)


def run(cmd: List[str], cwd: Path) -> str:
    proc = subprocess.run(
        cmd,
        cwd=str(cwd),
        check=False,
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout)
        sys.stderr.write(proc.stderr)
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}")
    return proc.stdout


def parse_rows(text: str) -> Dict[Tuple[str, int], Dict[str, float]]:
    parsed: Dict[Tuple[str, int], Dict[str, float]] = {}
    for raw in text.splitlines():
        line = raw.strip()
        m = ROW_RE.match(line)
        if not m:
            continue
        test = m.group(1)
        n = int(m.group(2))
        parsed[(test, n)] = {
            "dheap_p50": float(m.group(3)),
            "dheap_p95": float(m.group(4)),
            "stl_p50": float(m.group(5)),
            "stl_p95": float(m.group(6)),
            "spd_p50": float(m.group(7)),
            "spd_p95": float(m.group(8)),
        }
    if not parsed:
        raise RuntimeError("No benchmark rows parsed. Unexpected bench output format.")
    return parsed


def configure_build(
    repo: Path,
    build_dir: Path,
    force_scalar: bool,
    payload_bytes: int,
    arity: int,
    simd_policy: str,
) -> None:
    args = [
        "cmake",
        "-S",
        ".",
        "-B",
        str(build_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DDHEAP_FORCE_SCALAR={'ON' if force_scalar else 'OFF'}",
        f"-DDHEAP_NODE_PAYLOAD_BYTES={payload_bytes}",
        f"-DDHEAP_ARITY={arity}",
        f"-DDHEAP_SIMD_POLICY={simd_policy}",
    ]
    run(args, repo)
    run(["cmake", "--build", str(build_dir), "-j"], repo)


def bench_variant(build_dir: Path, warmup: int, iters: int, sizes: str, cwd: Path) -> str:
    return run(
        [
            str(build_dir / "bench_dheap4"),
            "--warmup",
            str(warmup),
            "--iters",
            str(iters),
            "--sizes",
            sizes,
        ],
        cwd,
    )


def print_table(
    simd: Dict[Tuple[str, int], Dict[str, float]],
    scalar: Dict[Tuple[str, int], Dict[str, float]],
) -> None:
    keys = sorted(simd.keys(), key=lambda x: (x[0], x[1]))
    header = (
        f"{'Test':<18} {'N':>10} "
        f"{'SIMD p50':>10} {'Scalar p50':>11} {'Gain p50':>10} "
        f"{'SIMD p95':>10} {'Scalar p95':>11} {'Gain p95':>10}"
    )
    print(header)
    print("-" * len(header))
    for key in keys:
        if key not in scalar:
            raise RuntimeError(f"Missing scalar row for {key}")
        s_on = simd[key]
        s_off = scalar[key]
        gain_p50 = s_off["dheap_p50"] / s_on["dheap_p50"]
        gain_p95 = s_off["dheap_p95"] / s_on["dheap_p95"]
        print(
            f"{key[0]:<18} {key[1]:>10d} "
            f"{s_on['dheap_p50']:>10.3f} {s_off['dheap_p50']:>11.3f} {gain_p50:>9.3f}x "
            f"{s_on['dheap_p95']:>10.3f} {s_off['dheap_p95']:>11.3f} {gain_p95:>9.3f}x"
        )


def geometric_mean(values: List[float]) -> float:
    if not values:
        return 0.0
    return math.exp(sum(math.log(v) for v in values) / len(values))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--warmup", type=int, default=1, help="Warmup iterations (default: 1)")
    parser.add_argument("--iters", type=int, default=7, help="Measured iterations (default: 7)")
    parser.add_argument(
        "--sizes",
        default="10000,100000,1000000",
        help="Comma-separated benchmark sizes (default: 10000,100000,1000000)",
    )
    parser.add_argument(
        "--build-root",
        default="build-simd-compare",
        help="Directory prefix for temporary build trees (default: build-simd-compare)",
    )
    parser.add_argument(
        "--payload-bytes",
        type=int,
        default=0,
        help="Extra payload bytes stored in each heap node (default: 0)",
    )
    parser.add_argument(
        "--arity",
        type=int,
        default=4,
        help="Heap arity d (default: 4)",
    )
    parser.add_argument(
        "--simd-policy",
        default="HYBRID",
        choices=["HYBRID", "ALWAYS", "NEVER"],
        help="SIMD policy for the non-scalar build (default: HYBRID)",
    )
    parser.add_argument(
        "--json",
        default="",
        help="Optional output path for JSON result payload.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.warmup < 0 or args.iters < 1 or args.payload_bytes < 0 or args.arity < 2:
        raise SystemExit("--warmup must be >= 0, --iters >= 1, --payload-bytes >= 0, --arity >= 2")

    repo = Path(__file__).resolve().parent.parent
    build_root = repo / args.build_root
    simd_dir = build_root / "simd-on"
    scalar_dir = build_root / "simd-off"
    simd_dir.mkdir(parents=True, exist_ok=True)
    scalar_dir.mkdir(parents=True, exist_ok=True)

    print("Configuring/building SIMD-enabled variant...")
    configure_build(
        repo,
        simd_dir,
        force_scalar=False,
        payload_bytes=args.payload_bytes,
        arity=args.arity,
        simd_policy=args.simd_policy,
    )
    print("Configuring/building forced-scalar variant...")
    configure_build(
        repo,
        scalar_dir,
        force_scalar=True,
        payload_bytes=args.payload_bytes,
        arity=args.arity,
        simd_policy=args.simd_policy,
    )

    print(
        f"Running bench (warmup={args.warmup}, iters={args.iters}, sizes={args.sizes}, "
        f"arity={args.arity}, policy={args.simd_policy}, payload={args.payload_bytes}) "
        "for SIMD-enabled variant..."
    )
    simd_out = bench_variant(simd_dir, args.warmup, args.iters, args.sizes, repo)
    print(
        f"Running bench (warmup={args.warmup}, iters={args.iters}, sizes={args.sizes}, "
        f"arity={args.arity}, policy={args.simd_policy}, payload={args.payload_bytes}) "
        "for forced-scalar variant..."
    )
    scalar_out = bench_variant(scalar_dir, args.warmup, args.iters, args.sizes, repo)

    simd = parse_rows(simd_out)
    scalar = parse_rows(scalar_out)

    print("\nSIMD contribution (DHeap only): gain = scalar / simd")
    print_table(simd, scalar)

    gains_p50: List[float] = []
    gains_p95: List[float] = []
    for key in simd:
        on = simd[key]
        off = scalar[key]
        gains_p50.append(off["dheap_p50"] / on["dheap_p50"])
        gains_p95.append(off["dheap_p95"] / on["dheap_p95"])
    print(
        "\nAggregate gain (geometric mean): "
        f"p50={geometric_mean(gains_p50):.3f}x, "
        f"p95={geometric_mean(gains_p95):.3f}x"
    )

    if args.json:
        # tuple keys are not JSON serializable; flatten keys.
        json_payload = {
            "warmup": args.warmup,
            "iters": args.iters,
            "sizes": args.sizes,
            "arity": args.arity,
            "simd_policy": args.simd_policy,
            "payload_bytes": args.payload_bytes,
            "cases": [],
        }
        for key in sorted(simd.keys(), key=lambda x: (x[0], x[1])):
            on = simd[key]
            off = scalar[key]
            json_payload["cases"].append(
                {
                    "test": key[0],
                    "n": key[1],
                    "simd_on": on,
                    "simd_off": off,
                    "gain_p50": off["dheap_p50"] / on["dheap_p50"],
                    "gain_p95": off["dheap_p95"] / on["dheap_p95"],
                }
            )
        Path(args.json).write_text(json.dumps(json_payload, indent=2), encoding="utf-8")
        print(f"\nWrote JSON report to: {args.json}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
