#!/usr/bin/env bash
# Analyze the library, examples, and their included project headers.
# Usage: bash scripts/run-clang-tidy.sh [clang-tidy arguments]
set -euo pipefail

for tool in cmake clang-tidy find; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'Required tool not found: %s\n' "$tool" >&2
        exit 1
    fi
done

repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$repo_root/out/build/linux-clang-tidy"

cmake -S "$repo_root" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCPP_CORE_SYSTEMS_BUILD_EXAMPLES=ON \
    -DCPP_CORE_SYSTEMS_BUILD_TESTS=OFF \
    -DCPP_CORE_SYSTEMS_BUILD_BENCHMARKS=OFF

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    printf 'No compile_commands.json generated; use a Makefiles or Ninja CMake generator.\n' >&2
    exit 1
fi

# Check every source even if an earlier file fails; return a failure afterward.
status=0
while IFS= read -r -d '' source; do
    clang-tidy -p "$build_dir" --config-file="$repo_root/.clang-tidy" "$source" "$@" || status=$?
done < <(find "$repo_root/src" "$repo_root/examples" -type f \
    \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) -print0)

exit "$status"
