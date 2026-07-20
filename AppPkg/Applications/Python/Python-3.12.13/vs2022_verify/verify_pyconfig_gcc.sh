#!/usr/bin/env bash
# Phase V2 proof: GCC reference pyconfig (no UEFI_MSVC_*).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INC="$ROOT/Include"
OBJ="${TMPDIR:-/tmp}/verify_pyconfig_gcc_$$.o"

if [[ ! -f "$INC/pyconfig.h" ]]; then
  echo "ERROR: $INC/pyconfig.h missing. Run srcprep.py in Python-3.12.13 first." >&2
  exit 1
fi

echo "=== V2 GCC verify (no UEFI_MSVC_*) ==="
gcc -c -Wall -Wextra -I"$INC" -I"$ROOT/Include/internal" \
  "$(dirname "$0")/verify_pyconfig_sizes.c" -o "$OBJ"
rm -f "$OBJ"
echo "OK: V2 GCC pyconfig verify passed"
