#!/usr/bin/env python3

# python3 stats.py --file ~/path/to/latency.csv

import argparse
import csv
import math
from statistics import mean

def percentile(values, pct):
    if not values:
        return None

    values = sorted(values)
    index = math.ceil((pct / 100) * len(values)) - 1
    index = max(0, min(index, len(values) - 1))
    
    return values[index]

def trimmed(values):
    values = sorted(values)
    if len(values) >= 5:
        return values[1:-1]
    
    return values

def load_latencies(path):
    groups = {
        "appear": [],
        "disappear": [],
        "all": [],
    }

    with open(path, newline="") as file:
        reader = csv.DictReader(file)

        for row in reader:
            if row.get("status") != "ok":
                continue

            phase = row.get("phase", "")
            latency = float(row["latency_ms"])

            if phase in groups:
                groups[phase].append(latency)

            groups["all"].append(latency)

    return groups

def print_stats(name, values):
    raw_n = len(values)
    values = trimmed(values)
    n = len(values)

    if n == 0:
        print(f"{name:<10} n=0")
        return

    avg = mean(values)
    p95 = percentile(values, 95)
    trimmed_n = raw_n - n

    extra = f" trimmed={trimmed_n}" if trimmed_n else ""
    print(f"{name:<10} n={n:<4} avg={avg:8.3f} ms  p95={p95:8.3f} ms{extra}")

def main():
    parser = argparse.ArgumentParser(
        description="Print latency stats from a keypress CSV file."
    )
    parser.add_argument(
        "--file",
        default="latency.csv",
        help="CSV file to read. Default: latency.csv",
    )
    args = parser.parse_args()
    
    groups = load_latencies(args.file)

    print(f"File: {args.file}")
    print("Stats from ok samples only, trimmed one low/high sample when n >= 5.\n")

    print_stats("appear", groups["appear"])
    print_stats("disappear", groups["disappear"])
    print_stats("all", groups["all"])

if __name__ == "__main__":
    main()

