#!/usr/bin/env bash
# Configure and build the library and examples on Linux.
# Usage: bash scripts/build.sh [Debug|Release|RelWithDebInfo|MinSizeRel]
set -euo pipefail

build_type="${1:-Debug}"
if (( $# > 1 )); then
    printf 'Usage: bash scripts/build.sh [Debug|Release|RelWithDebInfo|MinSizeRel]\n' >&2
    exit 1
fi
case "$build_type" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *)
        printf 'Unsupported build type: %s\n' "$build_type" >&2
        exit 1
        ;;
esac

if ! command -v cmake >/dev/null 2>&1; then
    printf 'Required tool not found: cmake\n' >&2
    exit 1
fi

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/out/build/linux-${build_type,,}"

cmake -S "$repo_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DCPP_CORE_SYSTEMS_BUILD_EXAMPLES=ON \
    -DCPP_CORE_SYSTEMS_BUILD_TESTS=OFF \
    -DCPP_CORE_SYSTEMS_BUILD_BENCHMARKS=OFF

cmake --build "$build_dir" --parallel
