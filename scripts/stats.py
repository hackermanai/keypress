#!/usr/bin/env python3

# Single file:
#   python3 stats.py --file ~/path/to/koi_sqlite.csv
#
# Directory:
#   python3 stats.py --dir ~/path/to/results
#
# Markdown output:
#   python3 stats.py --dir ~/path/to/results --markdown
#
# Change trimming:
#   python3 stats.py --dir ~/path/to/results --trim 1


import argparse
import csv
import math
from pathlib import Path
from statistics import mean, median

DISPLAY_NAMES = {
    "koi": "Koi",
    "vim": "Vim",
    "neovim": "Neovim",
    "nvim": "Neovim",
    "textmate": "TextMate",
    "vscode": "VS Code",
    "vs_code": "VS Code",
    "zed": "Zed",
    "sublime": "Sublime Text",
    "sublime_text": "Sublime Text",
    "emacs": "Emacs",
    "bbedit": "BBEdit",
}

def percentile(values, pct):
    """
    Nearest-rank percentile.
    """
    if not values:
        return None

    ordered = sorted(values)
    index = math.ceil((pct / 100.0) * len(ordered)) - 1
    index = max(0, min(index, len(ordered) - 1))

    return ordered[index]

def trim_extremes(values, count):
    """
    Remove 'count'-values from each end of the sorted sample.
    """
    ordered = sorted(values)

    if count <= 0:
        return ordered

    if len(ordered) <= count * 2:
        return ordered

    return ordered[count:-count]


def load_latencies(path, phase="all"):
    """
    Load latency_ms from rows whose status is "ok".

    phase="all" includes every valid phase.
    Other values include only rows matching that phase.
    """
    values = []

    with path.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)

        required = {"status", "latency_ms"}
        missing = required.difference(reader.fieldnames or [])

        if missing:
            missing_text = ", ".join(sorted(missing))
            raise ValueError(f"missing CSV columns: {missing_text}")

        for row in reader:
            if row.get("status") != "ok":
                continue

            if phase != "all" and row.get("phase", "") != phase:
                continue

            try:
                latency = float(row["latency_ms"])
            except (TypeError, ValueError):
                continue

            if math.isfinite(latency):
                values.append(latency)

    return values

def editor_name(path):
    """
    Convert names such as koi_sqlite.csv into Koi.
    """
    stem = path.stem.lower()

    for suffix in (
        "_sqlite",
        "-sqlite",
        "_latency",
        "-latency",
        "_benchmark",
        "-benchmark",
    ):
        if stem.endswith(suffix):
            stem = stem[: -len(suffix)]
            break

    if stem in DISPLAY_NAMES:
        return DISPLAY_NAMES[stem]

    return stem.replace("_", " ").replace("-", " ").title()


def calculate_stats(path, trim_count, phase):
    raw_values = load_latencies(path, phase=phase)
    values = trim_extremes(raw_values, trim_count)

    if not values:
        return None

    med = median(values)
    p95 = percentile(values, 95)
    p99 = percentile(values, 99)
    p999 = percentile(values, 99.9)

    return {
        "editor": editor_name(path),
        "path": path,
        "raw_n": len(raw_values),
        "n": len(values),
        "trimmed": len(raw_values) - len(values),
        "mean": mean(values),
        "median": med,
        "p95": p95,
        "p99": p99,
        "p99.9": p999,
        "max": max(values),
        "tail_spread": p95 - med,
    }


def find_csv_files(file_path=None, directory=None, pattern="*.csv"):
    if file_path is not None:
        path = file_path.expanduser().resolve()

        if not path.is_file():
            raise FileNotFoundError(f"file not found: {path}")

        return [path]

    directory = directory.expanduser().resolve()

    if not directory.is_dir():
        raise NotADirectoryError(f"directory not found: {directory}")

    return sorted(
        path
        for path in directory.glob(pattern)
        if path.is_file()
    )


def add_relative_metrics(results):
    """
    Add p95 ratio and percentage difference relative to the fastest p95.
    """
    if not results:
        return

    fastest_p95 = min(result["p95"] for result in results)

    for result in results:
        result["p95_ratio"] = result["p95"] / fastest_p95
        result["p95_slower_pct"] = (
            (result["p95"] - fastest_p95) / fastest_p95
        ) * 100.0


def print_text_table(results):
    headers = (
        "Editor",
        "n",
        "Mean",
        "Median",
        "p95",
        "p99",
        "p99.9",
        "Max",
        "Tail spread",
        "vs fastest",
    )

    rows = []

    for result in results:
        rows.append(
            (
                result["editor"],
                str(result["n"]),
                f'{result["mean"]:.1f}',
                f'{result["median"]:.1f}',
                f'{result["p95"]:.1f}',
                f'{result["p99"]:.1f}',
                f'{result["p99.9"]:.1f}',
                f'{result["max"]:.1f}',
                f'{result["tail_spread"]:.1f}',
                f'{result["p95_ratio"]:.2f}×',
            )
        )

    widths = [
        max(len(headers[index]), *(len(row[index]) for row in rows))
        for index in range(len(headers))
    ]

    numeric_columns = set(range(1, len(headers)))

    def format_row(row):
        cells = []

        for index, value in enumerate(row):
            if index in numeric_columns:
                cells.append(value.rjust(widths[index]))
            else:
                cells.append(value.ljust(widths[index]))

        return "  ".join(cells)

    print(format_row(headers))
    print(
        "  ".join(
            "-" * width
            for width in widths
        )
    )

    for row in rows:
        print(format_row(row))


def print_markdown_table(results):
    print(
        "| Editor | n | Mean (ms) | Median (ms) | p95 (ms) | "
        "p99 (ms) | p99.9 (ms) | Max (ms) | "
        "Tail Spread (ms) | vs Fastest p95 |"
    )
    print(
        "|:-------|--:|----------:|------------:|---------:|"
        "---------:|-----------:|---------:|-----------------:|"
        "---------------:|"
    )

    for result in results:
        print(
            f'| {result["editor"]} '
            f'| {result["n"]} '
            f'| {result["mean"]:.1f} '
            f'| {result["median"]:.1f} '
            f'| {result["p95"]:.1f} '
            f'| {result["p99"]:.1f} '
            f'| {result["p99.9"]:.1f} '
            f'| {result["max"]:.1f} '
            f'| {result["tail_spread"]:.1f} '
            f'| {result["p95_ratio"]:.2f}× |'
        )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Calculate latency statistics from one CSV file or "
            "all matching CSV files in a directory."
        )
    )

    source = parser.add_mutually_exclusive_group(required=True)

    source.add_argument(
        "--file",
        type=Path,
        help="Single CSV file to process.",
    )
    source.add_argument(
        "--dir",
        type=Path,
        help="Directory containing benchmark CSV files.",
    )

    parser.add_argument(
        "--pattern",
        default="*.csv",
        help='File pattern used with --dir. Default: "*.csv"',
    )
    parser.add_argument(
        "--phase",
        default="all",
        help='Phase to include, such as "appear" or "disappear". Default: all',
    )
    parser.add_argument(
        "--trim",
        type=int,
        default=2,
        help=(
            "Number of lowest and highest samples to remove. "
            "Default: 2."
        ),
    )
    parser.add_argument(
        "--markdown",
        action="store_true",
        help="Print the result as a Markdown table.",
    )

    args = parser.parse_args()

    if args.trim < 0:
        parser.error("--trim cannot be negative")

    try:
        paths = find_csv_files(
            file_path=args.file,
            directory=args.dir,
            pattern=args.pattern,
        )
    except (FileNotFoundError, NotADirectoryError) as error:
        parser.error(str(error))

    if not paths:
        parser.error(f'no files matched pattern "{args.pattern}"')

    results = []

    for path in paths:
        try:
            result = calculate_stats(
                path=path,
                trim_count=args.trim,
                phase=args.phase,
            )
        except (OSError, ValueError) as error:
            print(f"Skipping {path.name}: {error}")
            continue

        if result is None:
            print(f"Skipping {path.name}: no matching ok samples")
            continue

        results.append(result)

    if not results:
        parser.error("no usable benchmark samples found")

    # Rank by the metric used for the relative comparison.
    results.sort(key=lambda result: result["p95"])

    # This requires knowing the fastest p95 across every loaded file.
    add_relative_metrics(results)

    if args.markdown:
        print_markdown_table(results)
    else:
        print(
            f"Phase: {args.phase}; "
            f"trimmed up to {args.trim} low/high samples per file.\n"
        )
        print_text_table(results)


if __name__ == "__main__":
    main()


