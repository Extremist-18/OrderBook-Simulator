#!/usr/bin/env python3
"""
bench/plot_results.py

Reads bench/results.csv (produced by bench/benchmark) and renders a
latency chart -- this is the image you screenshot/embed in your README.

Usage:
    python3 bench/plot_results.py
    # writes bench/latency_chart.png

Requires: matplotlib  (pip install matplotlib)
"""
import csv
import matplotlib.pyplot as plt

ROWS = []
with open("bench/results.csv") as f:
    reader = csv.DictReader(f)
    for row in reader:
        if row["operation"] == "throughput_orders_per_sec":
            THROUGHPUT = float(row["max_ns"])  # reused column, see benchmark.cpp
            continue
        ROWS.append(row)

ops = [r["operation"] for r in ROWS]
p50 = [float(r["p50_ns"]) for r in ROWS]
p95 = [float(r["p95_ns"]) for r in ROWS]
p99 = [float(r["p99_ns"]) for r in ROWS]
p999 = [float(r["p999_ns"]) for r in ROWS]

x = range(len(ops))
width = 0.2

fig, ax = plt.subplots(figsize=(10, 6))
ax.bar([i - 1.5 * width for i in x], p50, width, label="p50")
ax.bar([i - 0.5 * width for i in x], p95, width, label="p95")
ax.bar([i + 0.5 * width for i in x], p99, width, label="p99")
ax.bar([i + 1.5 * width for i in x], p999, width, label="p99.9")

ax.set_yscale("log")  # tail latencies dwarf p50 -- log scale keeps both readable
ax.set_ylabel("Latency (ns, log scale)")
ax.set_title(f"OrderBook Latency by Operation  |  Throughput: {THROUGHPUT:,.0f} orders/sec")
ax.set_xticks(list(x))
ax.set_xticklabels(ops, rotation=20, ha="right")
ax.legend()
ax.grid(axis="y", linestyle="--", alpha=0.4)

fig.tight_layout()
fig.savefig("bench/latency_chart.png", dpi=150)
print("Saved bench/latency_chart.png")
