
#!/usr/bin/env python3

# pip install matplotlib

# Primary metric: p95 visible key-to-pixel latency.
# Lower is better.

# Expected filename format:
#   result_<editor>_<version>.csv
# 
# Examples:
#   python3 plotter.py --out latency.png
#   python3 plotter.py --dir .. --out latency.png
#   python3 plotter.py ../result_hackerman_alpha.csv ../result_vscode_latest.csv --out latency.png

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path
from statistics import median

import matplotlib.pyplot as plt

NAME_RE = re.compile(r"^result_(?P<editor>.+?)_(?P<version>.+?)\.csv$")

def series_name(path: Path) -> str:
    match = NAME_RE.match(path.name)
    if not match:
        return path.stem

    editor = match.group("editor").replace("_", " ")
    version = match.group("version").replace("_", " ")

    return f"{ editor }"

def percentile(values: list[float], p: float) -> float:
    if not values:
        return float("nan")

    values = sorted(values)

    if len(values) == 1:
        return values[0]

    k = (len(values) - 1) * (p / 100.0)
    lo = int(k)
    hi = min(lo + 1, len(values) - 1)
    frac = k - lo

    return values[lo] * (1.0 - frac) + values[hi] * frac

def trim_values(values: list[float], trim_percent: float) -> list[float]:
    if not values or trim_percent <= 0:
        return values

    values = sorted(values)

    trim_each = int(len(values) * (trim_percent / 100.0))

    if trim_each <= 0:
        return values

    if trim_each * 2 >= len(values):
        return values

    return values[trim_each:-trim_each]
    
def read_latencies(path: Path, phases: set[str] | None, skip_indices: int) -> list[float]:
    values: list[float] = []

    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)

        required = {"phase", "latency_ms", "status"}
        missing = required - set(reader.fieldnames or [])

        if missing:
            raise ValueError(f"{path}: missing columns: {', '.join(sorted(missing))}")

        for row in reader:
            if row.get("status") != "ok":
                continue

            phase = row.get("phase", "")

            if phases is not None and phase not in phases:
                continue
                
            try:
                index = int(row.get("index", "0"))
            except ValueError:
                index = 0
            
            if index < skip_indices:
                continue

            try:
                latency = float(row["latency_ms"])
            except ValueError:
                continue

            if latency >= 0:
                values.append(latency)

    return values

def metric_label(values: list[float]) -> str:
    return (
        f"n={len(values)}\n"
        f"med={median(values):.1f}\n"
        f"p95={percentile(values, 95):.1f}"
    )

def plot_series(
    paths: list[Path],
    phases: set[str] | None,
    out_path: Path,
    title: str,
    skip_indices: int,
    trim_percent: float,
) -> None:
    data: list[tuple[str, list[float]]] = []

    for path in paths:
        # values = read_latencies(path, phases)
        raw_values = read_latencies(path, phases, skip_indices)
        values = trim_values(raw_values, trim_percent)
        
        if raw_values and len(values) != len(raw_values):
            print(f"trimmed: {path} {len(raw_values)} -> {len(values)} samples")

        if not values:
            print(f"warning: { path } has no usable latency rows")
            continue

        data.append((series_name(path), values))
        
    data.sort(key=lambda item: percentile(item[1], 95))

    if not data:
        raise SystemExit("no usable data found")

    fig, ax = plt.subplots(figsize=(11, 6))

    labels = [name for name, _ in data]
    values = [vals for _, vals in data]

    ax.boxplot(values, tick_labels=labels, showmeans=True)

    all_values = [v for group in values for v in group]
    y_min = min(all_values)
    y_max = max(all_values)
    y_range = max(1.0, y_max - y_min)

    ax.set_ylim(y_min - y_range * 0.05, y_max + y_range * 0.35)

    for i, vals in enumerate(values, start=1):
        y = percentile(vals, 95) + y_range * 0.08
        box = { 
            "boxstyle": "round,pad=0.25",
            "facecolor": "white",
            "edgecolor": "0.75",
            "alpha": 0.9,
        }
        ax.text(i + 0.18, y, metric_label(vals), ha="left", va="bottom", fontsize=8, bbox=box)

    ax.set_title(title)
    ax.set_ylabel("Latency (ms)")
    ax.set_xlabel("Editor")
    ax.grid(axis="y", alpha=0.3)

    plt.xticks(rotation=35, ha="right")
    plt.tight_layout()
    fig.savefig(out_path, dpi=160)

    print(f"saved: {out_path}")
    print()

    for name, vals in data:
        print(
            f"{name}: "
            f"n={len(vals)}  "
            f"median={median(vals):.2f} ms  "
            f"p95={percentile(vals, 95):.2f} ms  "
            f"min={min(vals):.2f} ms  "
            f"max={max(vals):.2f} ms"
        )

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    # parser.add_argument("csv", nargs="+", type=Path)
    parser.add_argument("csv", nargs="*", type=Path)
    parser.add_argument("--dir", type=Path, default=Path("."), help="directory to search when no CSV files are given")
    parser.add_argument("--out", type=Path, default=Path("latency_plot.png"))
    parser.add_argument("--title", default="Visible key-to-pixel latency, sorted by p95")
    parser.add_argument("--phase", choices=["appear", "disappear", "both"], default="both", help="which phase to plot")
    parser.add_argument("--skip-indices", type=int, default=2, help="drop rows where index is below this value")
    parser.add_argument("--trim-percent", type=float, default=0.0, help="trim this percent from both tails after reading")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    phases: set[str] | None

    if args.phase == "both":
        phases = None
    else:
        phases = {args.phase}
        
    paths = args.csv
    if not paths:
        paths = sorted(args.dir.glob("result_*.csv"))
    
    if not paths:
        raise SystemExit(f"no result_*.csv files found in {args.dir}")

    plot_series(
        paths=paths,
        phases=phases,
        out_path=args.out,
        title=args.title,
        skip_indices=args.skip_indices,
        trim_percent=args.trim_percent,
    )

if __name__ == "__main__":
    main()
