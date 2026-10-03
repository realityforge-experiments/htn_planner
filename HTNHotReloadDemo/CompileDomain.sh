#!/usr/bin/env bash
# Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com
set -euo pipefail

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
configuration=${1:-}
case "$configuration" in
    Debug) flags=(-O0 -g) ;;
    Profile|ProfileDetailed) flags=(-O3 -g) ;;
    Release) flags=(-O3) ;;
    *) echo "Unsupported configuration: $configuration" >&2; exit 1 ;;
esac

bin="$root/bin/$configuration-linux-x86_64/HTNHotReloadDemo"
output="$bin/candidate"
translator="$root/bin/$configuration-linux-x86_64/HTNTranslator/HTNTranslator"
source=${3:-$root/Domains/Wanderer.domain}
compiler=${CC:-gcc-14}
if [[ ! -x "$translator" || ! -f "$bin/libHTNRuntimeBridge.so" ]]; then
    echo "Matching translator/runtime bridge not found. Build HTNHotReloadDemo and its dependencies first." >&2
    exit 1
fi
command -v "$compiler" >/dev/null || { echo "C compiler not found: $compiler" >&2; exit 1; }

# The host passes its instrumentation macros as one argument. Split only into
# compiler arguments; never evaluate their contents as shell commands.
read -r -a defines <<< "${2:--DHTN_GENERATED_MODULE_EXPORTS}"
mkdir -p "$output"
echo "[1/2] Translating $source and linked domains..."
"$translator" "$source" CreateWandererHotReloadHTN "$output"
echo "[2/2] Compiling and linking generated C into candidate/libWandererHTN.so..."
"$compiler" -std=c11 -Wall -Wextra -Werror -fPIC -shared "${flags[@]}" "${defines[@]}" \
    -I"$root/HTNFramework/src" "$output/Wanderer.generated.c" -L"$bin" -lHTNRuntimeBridge \
    -Wl,-z,defs -Wl,-rpath,'$ORIGIN' -o "$output/libWandererHTN.so"
echo "Candidate build succeeded. Active module has not been changed."
