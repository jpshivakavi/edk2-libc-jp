# VS2022 Python 3.12.13 — MIN (Iteration 1) build

**Workspace:** `c:\Users\njayapra\github\edk2-libc-jp-vsfix`  
**Branch:** `feature/python-3.12.13-vs2022` (MIN is the default INF mode)

Same playbook as GCC AppPkg **Iteration 1**: core + in-tree extensions only; **no** vendored **zlib**, **ctypes/libffi**, or **OpenSSL** (`_hashlib` / `_ssl`) in the link.

**Runtime (hang, 368 entry, deepfreeze, packaging):** [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md)  
**FULL build host setup:** [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) — pass **`BUILD_PYTHON312_FULL=TRUE`** for Phase 8.

---

## What MIN excludes

| Phase 8 | Modules |
|---------|---------|
| 8.1 | `zlib`, `Modules/zlibmodule.c` |
| 8.5 | `_ctypes`, GCC libffi, MSFT `libffi_msvc`, `win64.asm` |
| 8.3–8.4 | OpenSSL libcrypto/libssl, `_hashopenssl.c`, `_ssl.c` |

**Still present:** builtin hashes (`_md5`, `_sha2`, …), `edk2console`, socket, expat, frozen/deepfreeze, etc.

Controlled by **`Python312_MIN.inf`** vs **`Python312.inf`** (selected in [`AppPkg.dsc`](../../AppPkg.dsc) from **`BUILD_PYTHON312_FULL`**) and **`#ifdef BUILD_PYTHON312_FULL`** in [`PyMod-3.12.13/Modules/config.c`](./Python-3.12.13/PyMod-3.12.13/Modules/config.c).

EDK does **not** allow `!if $(BUILD_PYTHON312_FULL)` inside INF `[BuildOptions]` / `[Sources]` — use the two-INF pattern (runtime notes §1).

---

## VS2022-only extras in MIN INF

| Item | Why |
|------|-----|
| **`PyMod-3.12.13/efi/src/msvc_chkstk.c`** \| MSFT | **`__chkstk`** for libmpdec; FULL gets it from **`libffi_msvc/ffi.c`** |
| **`/DPY_UEFI_MSVC_368_ENTRY=1`** | **`ShellCEntryLib`** on Shell stack (no **`edk2_switch_stack`** / IDT) — required for VS2022 runtime today |
| **`/DPY_UEFI_BOOT_TRACE=1`** | Optional firmware **`Print()`** ladder; remove when V6 smoke is done |

Details: [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md).

---

## Prerequisites

Same as the Windows build guide: patches (local) if needed, **`srcprep.py`**, then **`build`**. Frozen artifacts are **in git** under **`PyMod-3.12.13/Python/frozen_modules/`** and **`deepfreeze/`** — no regen on fresh clone. Run **`Tools\build\regen_frozen_windows.cmd`** only when changing frozen inputs (runtime notes §5; WSL guide §6).

**Run the pre-flight checks before either destructive step** — both are usually no-ops on this branch, and both fail confusingly when run blind:

- **Patches:** [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) **§4** — classifies each patch **`NEEDS APPLY`** / **`ALREADY APPLIED`** / **`CONFLICT`** instead of just failing.
- **Frozen regen:** same guide **§6** — host must be **Python 3.12.x** (**`magic 168627659`**); a different minor rewrites all 24 headers with a bad magic that only fails at **runtime**.

```cmd
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix
set PACKAGES_PATH=c:\Users\njayapra\github\edk2;%EDK2_LIBC_PATH%
set NASM_PREFIX=C:\NASM\
cd /d c:\Users\njayapra\github\edk2
call edksetup.bat
```

---

## Build MIN (VS2022)

```cmd
build -t VS2022 -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312
```

Do **not** pass `BUILD_PYTHON312_FULL` (defaults to **FALSE** in [`AppPkg.dsc`](../../AppPkg.dsc)).

After a prior FULL build, delete stale module output if needed:

```cmd
rd /s /q c:\Users\njayapra\github\edk2\Build\AppPkg\NOOPT_VS2022\X64\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\Python312
```

Artifact (typical):

```text
Build\AppPkg\NOOPT_VS2022\X64\edk2-libc-jp-vsfix\...\Python312_MIN\DEBUG\Python312.efi
```

---

## Build FULL (when re-enabling Phase 8)

```cmd
build -t VS2022 -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE
```

Add **`/DPY_UEFI_MSVC_368_ENTRY=1`** (and optional **`/DPY_UEFI_BOOT_TRACE=1`**) to **`Python312.inf`** MSFT flags — **same as MIN** (done in tree).

---

## Package

```cmd
set WORKSPACE=c:\Users\njayapra\github\edk2
set EDK2_LIBC_PATH=c:\Users\njayapra\github\edk2-libc-jp-vsfix
cd /d %EDK2_LIBC_PATH%\AppPkg\Applications\Python\Python-3.12.13
create_python_pkg.bat VS2022 NOOPT X64 c:\Users\njayapra\github\edk2-libc-jp-vsfix\myUEFIPy312_MIN
```

Copy **`<OutFolder>\EFI`** to **`fsN:\EFI`**. See runtime notes §8.

---

## UEFI smoke (MIN)

```text
Python312.efi -h
Python312.efi -S -c "import sys; print(sys.version)"
Python312.efi -S -c "import ssl"
```

Expect **`-h`** with return to prompt, **`import ssl` → ModuleNotFoundError** (or no `_ssl`).

When MIN is green, enable Phase 8 via **`BUILD_PYTHON312_FULL=TRUE`** or batches **8.1 → 8.2 → 8.5 → 8.3 → 8.4** per [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md).

---

## GCC regression

After INF/config changes, on WSL:

```bash
build -t GCC -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312
```

FULL on GCC:

```bash
build ... -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE
```

GCC keeps the **`UefiMain` + stack switch + IDT** path; **`PY_UEFI_MSVC_368_ENTRY`** is MSVC-only.
