#!/usr/bin/env bash
# Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec python3 "$root/ValidatePackage.py" "$@"
