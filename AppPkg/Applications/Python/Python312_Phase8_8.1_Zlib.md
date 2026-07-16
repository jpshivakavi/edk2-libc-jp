# Phase 8.1 — Enable `zlib` (edk2-zlib)

First external-package batch per [`Python312_AppPkg_Migration_Plan.md`](./Python312_AppPkg_Migration_Plan.md).

**AppPkg changes (this repo):** `Python312.inf` links `LibZlib` and `Modules/zlibmodule.c`;
`PyMod-3.12.13/Modules/config.c` registers `zlib`.

**You must add** [edk2-zlib](https://github.com/intel-sandbox/firmware.boot.uefi.python38.edk2-zlib)
on `PACKAGES_PATH` (same layout as edk2-py312).

---

## 1. Clone edk2-zlib (WSL)

```bash
mkdir -p ~/src
cd ~/src
git clone https://github.com/intel-sandbox/firmware.boot.uefi.python38.edk2-zlib.git edk2-zlib
```

Or reuse a copy from an existing edk2-py312 tree:

```bash
ls ~/src/edk2-py312/edk2-zlib/efi/LibZlib/LibZlib.dec
```

---

## 2. Environment (add zlib to PACKAGES_PATH)

Example (adjust edk2 / edk2-libc paths):

```bash
export PACKAGES_PATH=$HOME/src/edk2-py312/edk2:$HOME/src/edk2-libc:$HOME/src/edk2-zlib/efi
export EDK2_LIBC_PATH=$HOME/src/edk2-libc
export WORKSPACE=$HOME/src/edk2-py312/edk2
export PYTHON_COMMAND=python3
export EDK_TOOLS_PATH=$WORKSPACE/BaseTools

cd "$WORKSPACE"
. edksetup.sh
```

`LibZlib/LibZlib.dec` must resolve from the **`edk2-zlib/efi`** segment (mirrors
`edk2-py312` `Makefile` `python` target).

---

## 3. StdLib patches + srcprep (unchanged)

```bash
cd ~/src/edk2-libc
# patches already applied? verify:
ls StdLib/LibC/Uefi/upipe.c

cd AppPkg/Applications/Python/Python-3.12.13
python3 srcprep.py
```

Frozen / deepfreeze artifacts must still be present under `Python/`.

---

## 4. Build

```bash
cd "$WORKSPACE"
build -a X64 -b NOOPT -t GCC \
  -p "$EDK2_LIBC_PATH/AppPkg/AppPkg.dsc" \
  -D BUILD_PYTHON312
```

---

## 5. Package + smoke

```bash
cd "$EDK2_LIBC_PATH/AppPkg/Applications/Python/Python-3.12.13"
./create_python_pkg.sh GCC NOOPT X64 ~/py312_efi
```

On UEFI Shell:

```python
>>> import zlib
>>> zlib.crc32(b"uefi")
>>> import gzip   # uses zlib when available
```

---

## Reference

| Item | edk2-py312 |
|------|------------|
| PACKAGES_PATH zlib segment | `$(EDK2_ZLIB_DIR)/efi` |
| Extension source | `Modules/zlibmodule.c` in `PythonExtLib.inf` |
| Library | `LibZlib` / `LibZlib.dec` |

Python 3.6.8 AppPkg vendored zlib under `Modules/zlib/*.c`; **3.12 AppPkg uses
edk2-zlib** instead (no vendored zlib `.c` in `Python312.inf`).
