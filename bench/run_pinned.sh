#!/usr/bin/env bash
# bench/run_pinned.sh
#
# Runs the benchmark pinned to a single CPU core, which cuts down on
# noise from the OS scheduler migrating the process mid-run. Not
# essential, but it tightens the tail-latency numbers noticeably on a
# busy machine (laptop with background apps, shared CI runner, etc).
#
# Usage: bash bench/run_pinned.sh

set -e

if command -v taskset &> /dev/null; then
    echo "Pinning to core 0 with taskset..."
    taskset -c 0 ./bench/benchmark
elif command -v cset &> /dev/null; then
    cset shield -e ./bench/benchmark
else
    echo "taskset not found (this is normal on macOS) -- running unpinned."
    echo "On macOS you can try 'nice -n -20 ./bench/benchmark' for higher scheduling priority instead."
    ./bench/benchmark
fi
