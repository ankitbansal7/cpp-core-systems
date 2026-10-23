#!/usr/bin/env bash
# Configure, build, and run the TCP benchmark in Release.
# Usage: bash scripts/run-benchmarks.sh [round-trips] [stream-MiB]
set -euo pipefail

usage() {
    printf 'Usage: bash scripts/run-benchmarks.sh [round-trips: 1..1000000] [stream-MiB: 1..4096]\n'
    printf 'Defaults: 10000 round trips, 128 MiB stream.\n'
}

if (( $# == 1 )) && [[ "$1" == "--help" ]]; then
    usage
    exit 0
fi
if (( $# > 2 )); then
    usage >&2
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    printf 'Required tool not found: cmake\n' >&2
    exit 1
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/out/build/linux-benchmarks"

cmake -S "$repo_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCPP_CORE_SYSTEMS_BUILD_EXAMPLES=OFF \
    -DCPP_CORE_SYSTEMS_BUILD_TESTS=OFF \
    -DCPP_CORE_SYSTEMS_BUILD_BENCHMARKS=ON

cmake --build "$build_dir" --config Release --target bench_tcp --parallel

# Single-config generators put the executable directly in benchmarks/;
# multi-config generators (such as Ninja Multi-Config) add Release/.
benchmark="$build_dir/benchmarks/bench_tcp"
if [[ -x "$build_dir/benchmarks/Release/bench_tcp" ]]; then
    benchmark="$build_dir/benchmarks/Release/bench_tcp"
fi
exec "$benchmark" "$@"
