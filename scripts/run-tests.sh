#!/usr/bin/env bash
# Configure, build, and run all tests. Extra arguments are passed to CTest.
# Usage: bash scripts/run-tests.sh [ctest arguments]
set -euo pipefail

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/out/build/linux-tests"

for tool in cmake ctest; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required tool not found: %s\n' "$tool" >&2
        exit 1
    fi
done

cmake -S "$repo_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCPP_CORE_SYSTEMS_BUILD_TESTS=ON \
    -DBUILD_TESTING=ON \
    -DCPP_CORE_SYSTEMS_BUILD_EXAMPLES=OFF

cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure "$@"
