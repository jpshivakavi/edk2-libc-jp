#!/usr/bin/env bash
#
# Stage Python 3.12.13 UEFI runtime tree for FAT / QEMU rootfs.
#
# Layout (matches PREFIX "fs0:\\EFI" from pyconfig.h / edk2-py312 stage*):
#
#   <OutFolder>/
#     EFI/
#       bin/Python312.efi
#       lib/python3.12/          # from AppPkg .../Python-3.12.13/Lib
#       stdlib/etc/              # hosts, resolv.conf from StdLib
#
# Usage (from anywhere; needs WORKSPACE = edk2 build root):
#   export EDK2_LIBC_PATH=/path/to/edk2-libc
#   ./create_python_pkg.sh GCC NOOPT X64 /path/to/out
#
set -euo pipefail

TOOL_CHAIN_TAG=${1:-}
TARGET=${2:-}
ARCH=${3:-}
OUT_FOLDER=${4:-}

usage() {
  cat <<EOF

Create Python 3.12 EFI package (Iteration 1).

Usage: $0 <ToolChain> <Target> <Architecture> <OutFolder>
  ToolChain     e.g. GCC, GCC5
  Target        e.g. NOOPT, DEBUG, RELEASE
  Architecture  e.g. X64
  OutFolder     destination directory for the EFI/ tree

EOF
}

error_missing_efi() {
  echo "Failed to create Python EFI package."
  echo "Python312.efi not found under Build/AppPkg/${TARGET}_${TOOL_CHAIN_TAG}/${ARCH}/"
  echo "Build with: build -a ${ARCH} -b ${TARGET} -t ${TOOL_CHAIN_TAG} -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312"
}

if [[ -z "${TOOL_CHAIN_TAG}" || -z "${TARGET}" || -z "${ARCH}" || -z "${OUT_FOLDER}" ]]; then
  usage
  exit 1
fi

if [[ -z "${WORKSPACE:-}" ]]; then
  echo "WORKSPACE is not set (run edksetup.sh first)."
  exit 1
fi

if [[ -z "${EDK2_LIBC_PATH:-}" ]]; then
  echo "Warning: EDK2_LIBC_PATH not set; assuming WORKSPACE contains AppPkg/StdLib."
  EDK2_LIBC_PATH="${WORKSPACE}"
fi

PYTHON_SRC="${EDK2_LIBC_PATH}/AppPkg/Applications/Python/Python-3.12.13"
BUILD_DIR="${WORKSPACE}/Build/AppPkg/${TARGET}_${TOOL_CHAIN_TAG}/${ARCH}"
PYTHON_BIN="${BUILD_DIR}/Python312.efi"

# Fallback to module DEBUG/OUTPUT paths used by some toolchains
if [[ ! -f "${PYTHON_BIN}" ]]; then
  for candidate in \
    "${BUILD_DIR}/AppPkg/Applications/Python/Python-3.12.13/Python312/DEBUG/Python312.efi" \
    "${BUILD_DIR}/AppPkg/Applications/Python/Python-3.12.13/Python312/OUTPUT/Python312.efi"
  do
    if [[ -f "${candidate}" ]]; then
      PYTHON_BIN="${candidate}"
      break
    fi
  done
fi

if [[ ! -f "${PYTHON_BIN}" ]]; then
  error_missing_efi
  exit 1
fi

if [[ ! -d "${PYTHON_SRC}/Lib" ]]; then
  echo "Python Lib/ not found at ${PYTHON_SRC}/Lib"
  exit 1
fi

BIN_DIR="${OUT_FOLDER}/EFI/bin"
LIB_DIR="${OUT_FOLDER}/EFI/lib/python3.12"
ETC_DIR="${OUT_FOLDER}/EFI/stdlib/etc"

mkdir -p "${BIN_DIR}" "${LIB_DIR}" "${ETC_DIR}"

cp -f "${PYTHON_BIN}" "${BIN_DIR}/Python312.efi"

# Prefer rsync (excludes large/unused trees); fall back to cp
if command -v rsync >/dev/null 2>&1; then
  rsync -a \
    --exclude '__pycache__/' \
    --exclude 'test/' \
    --exclude 'tests/' \
    --exclude 'idlelib/' \
    --exclude 'tkinter/' \
    --exclude 'turtledemo/' \
    --exclude 'lib2to3/' \
    "${PYTHON_SRC}/Lib/" "${LIB_DIR}/"
else
  cp -a "${PYTHON_SRC}/Lib/." "${LIB_DIR}/"
  find "${LIB_DIR}" -type d \( -name __pycache__ -o -name test -o -name tests \
    -o -name idlelib -o -name tkinter -o -name turtledemo -o -name lib2to3 \) \
    -prune -exec rm -rf {} + 2>/dev/null || true
fi

# Overlay UEFI-touched Lib from PyMod when present
if [[ -d "${PYTHON_SRC}/PyMod-3.12.13/Lib" ]]; then
  if command -v rsync >/dev/null 2>&1; then
    rsync -a "${PYTHON_SRC}/PyMod-3.12.13/Lib/" "${LIB_DIR}/"
  else
    cp -a "${PYTHON_SRC}/PyMod-3.12.13/Lib/." "${LIB_DIR}/"
  fi
fi

if [[ -d "${EDK2_LIBC_PATH}/StdLib/Efi/StdLib/etc" ]]; then
  cp -a "${EDK2_LIBC_PATH}/StdLib/Efi/StdLib/etc/." "${ETC_DIR}/"
fi

ABS_OUT="${OUT_FOLDER}"
if [[ "${OUT_FOLDER}" != /* ]]; then
  ABS_OUT="$(pwd)/${OUT_FOLDER}"
fi

echo
echo "Python 3.12 EFI package ready:"
echo "  ${ABS_OUT}/EFI/bin/Python312.efi"
echo "  ${ABS_OUT}/EFI/lib/python3.12/"
echo "  ${ABS_OUT}/EFI/stdlib/etc/"
echo
echo "On UEFI Shell (fs0: = this EFI tree):"
echo "  fs0:"
echo "  cd EFI\\\\bin"
echo "  Python312.efi"
echo
echo "Smoke checks:"
echo "  import sys; print(sys.version, sys.platform); print(sys.path)"
echo "  import os; print(os.listdir('fs0:\\\\'))"
echo "  # expect ImportError for: ssl, ctypes, zlib, readline"
