#!/usr/bin/env bash
# Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
cd "$root"
mkdir -p build/logs

if [[ $(uname -s) != Linux ]]; then
    echo "This script validates native Linux builds. See docs/LINUX.md." >&2
    exit 1
fi

premake=${PREMAKE5:-premake5}
export CC=${CC:-gcc-14}
export CXX=${CXX:-g++-14}
jobs=${HTN_BUILD_JOBS:-4}
for tool in "$premake" "$CC" "$CXX" make pkg-config python3 timeout; do
    command -v "$tool" >/dev/null || { echo "Missing tool: $tool (see docs/LINUX.md)" >&2; exit 1; }
done
pkg-config --exists sdl2 gtest || { echo "Install libsdl2-dev and libgtest-dev." >&2; exit 1; }

configurations=()
premake_options=()
for argument in "$@"; do
    case "$argument" in
        Debug|Release|Profile|ProfileDetailed) configurations+=("$argument") ;;
        --atom-diagnostics|--generated-execution-profiling|--runtime-backtracking-support=enabled)
            premake_options+=("$argument") ;;
        *) echo "Unsupported configuration or option: $argument" >&2; exit 1 ;;
    esac
done
if [[ ${#configurations[@]} == 0 ]]; then configurations=(Debug Release); fi

run_logged() {
    local log="$root/build/logs/$1.log"
    shift
    if "$@" > "$log" 2>&1; then
        echo "PASS: $log"
    else
        local status=$?
        tail -80 "$log"
        echo "FAILED ($status): $log" >&2
        return "$status"
    fi
}

run_tests() {
    # Existing tests resolve domain paths relative to the HTNTest project directory.
    (cd "$root/HTNTest" && timeout 300s "$1" --gtest_color=no)
}

toolset=gcc
if [[ $(basename "$CXX") == *clang* ]]; then toolset=clang; fi
# A separate source/build tree is required for each compiler/options combination.
# Make does not otherwise notice a changed compiler command for ordinary objects.
signature=$(printf '%s\n' "$(command -v "$CC")" "$("$CC" --version)" "$(command -v "$CXX")" "$("$CXX" --version)" "${premake_options[@]}")
signature_file="$root/build/linux-toolchain.txt"
if [[ -f "$signature_file" && $(cat "$signature_file") != "$signature" ]]; then
    echo "Compiler/options changed. Use a separate source/build tree to avoid stale objects." >&2
    exit 1
fi
printf '%s\n' "$signature" > "$signature_file"
run_logged "linux-full-$toolset-premake" "$premake" --cc="$toolset" "${premake_options[@]}" gmake
for configuration in "${configurations[@]}"; do
    prefix="linux-full-$toolset-$configuration"
    binaries="$root/bin/$configuration-linux-x86_64"
    run_logged "$prefix-build" make -j"$jobs" "config=${configuration,,}" "CC=$CC" "CXX=$CXX"
    run_logged "$prefix-tests" run_tests "$binaries/HTNTest/HTNTest"
    tail -3 "$root/build/logs/$prefix-tests.log"
    run_logged "$prefix-lsp" python3 HTNLanguageServer/tests/smoke.py "$binaries/HTNLanguageServer/HTNLanguageServer"
    run_logged "$prefix-allocations" "$binaries/HTNBenchmark/HTNBenchmark" --allocation-self-test
    run_logged "$prefix-benchmark" "$binaries/HTNBenchmark/HTNBenchmark" 20 2
    run_logged "$prefix-lifecycle" "$binaries/HTNBenchmark/HTNBenchmark" 20 1 --lifecycle
    # Uses isolated source fixtures and restores the original domain module.
    # Close other demo instances before running this validation.
    run_logged "$prefix-hot-reload" "$binaries/HTNHotReloadDemo/HTNHotReloadDemo" --pipeline-self-test
done
echo "PASS: Linux solution (editor excluded), regression suite, language server, benchmarks and hot reload ($toolset; ${configurations[*]})."
