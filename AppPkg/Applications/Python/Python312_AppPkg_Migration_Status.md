# Python 3.12.13 AppPkg Migration Status

**Plan:** [`Python312_AppPkg_Migration_Plan.md`](./Python312_AppPkg_Migration_Plan.md)  
**WSL GCC build guide:** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)  
**Started:** 2026-07-14  
**Updated:** 2026-07-14  
**Iteration:** 1 (no edk2-libffi / edk2-openssl / edk2-zlib / edk2-pyreadline)  
**Target repo:** `c:\Users\njayapra\github\edk2-libc`  
**Branch:** `feature/python-3.12.13-apppkg`  
**Source port:** `c:\Users\njayapra\github\edk2-py312`

---

## Overall progress

| Phase | Name | Status |
|-------|------|--------|
| 0 | Prerequisites and baseline capture | **Done** (rebuild skipped on Windows) |
| 1 | Scaffold AppPkg directory tree | **Done** |
| 2 | Extract PyMod-3.12.13 overlays | **Done** (first pass; stock tree still holds UEFI deltas) |
| 3 | Port frozen / deepfreeze | **Blocked until WSL** — artifacts gitignored; see WSL GCC guide §6 |
| 4 | Author monolithic Python312.inf (MIN) | **In progress** (INF generated; not yet GCC-built) |
| 5 | Wire DSC / libc patches / first GCC build | **Partial** (DSC/DEC gated; build on WSL — see guide) |
| 6 | Package + REPL smoke | Not started |
| 7 | Docs + CI | Not started |
| 8 | Deferred (external pkgs, VS2022) | Deferred |

**Legend:** Not started · In progress · Partial · Blocked · Done · Skipped

---

## Locked decisions (Iteration 1)

| Item | Choice |
|------|--------|
| External packages | None (`PACKAGES_PATH` = edk2 + edk2-libc only) |
| Omit modules | `_ctypes`, `_ctypes_test`, `_ssl`, `_hashopenssl` / `_hashlib` (OpenSSL), `zlib`, `readline` |
| Entry point | `UefiMain` |
| INF | Monolithic `Python312.inf` (MIN, ~241 sources) |
| Overlay | `PyMod-3.12.13` + `srcprep.py` |
| PREFIX | `fs0:\EFI` (from current 3.12 `pyconfig.h`) |
| Toolchain | GCC first |

---

## Work log

### 2026-07-14 — Session 1

1. Created status document.
2. **Phase 0:** Captured SHAs, INF inventories, patches, `UEFI_C_SOURCE` file list, pyconfig keys. Skipped full `make python` on Windows (Linux/GCC flow).
3. Created branch `feature/python-3.12.13-apppkg`.
4. **Phase 1:** Copied `edk2-cpython` → `AppPkg/Applications/Python/Python-3.12.13/` (excluded `.git` and `efi/`). Added `srcprep.py`, `Py312ReadMe.txt`, packaging stubs, `patches/`, `frozen/`. Updated `AppPkg.dec` / `AppPkg.dsc` for `BUILD_PYTHON312`.
5. **Phase 2:** Populated `PyMod-3.12.13` with EFI glue, UEFI-touched C/headers/Lib (Iteration 1), `Modules/config.c`. Ran `srcprep.py` → `Include/pyconfig.h` present (`PLATFORM "uefi"`).
6. **Phase 3 start:** Copied `frozen_modules.mk` into `frozen/`.
7. **Phase 4 start:** Generated MIN `Python312.inf` (241 sources, 9 omitted Ext refs). Commented excluded inittab entries in `PyMod-3.12.13/Modules/config.c`.

---

## Phase 0 — Prerequisites and baseline capture

### Checklist

| Step | Action | Result |
|------|--------|--------|
| 0.1 | Confirm host can rebuild current `edk2-py312` | **Skipped** on Windows agent — requires Linux/WSL `Makefile` + GCC. Re-verify in Phase 5. |
| 0.2 | Record `Python312.efi` artifact path | No local artifact found. Expected: `edk2/Build/PythonPkg/NOOPT_GCC/X64/Python312.efi` |
| 0.3 | Capture inventory | **Done** |
| 0.4 | Smoke checklist | Iteration 1 target recorded |
| 0.5 | Working branch | **Done** — `feature/python-3.12.13-apppkg` |

### Baseline SHAs

| Repo | Commit | Note |
|------|--------|------|
| `edk2-py312` | `d8ad35d0a82ed8e19626f6c255374ac55a248efb` | 2026-07-14 |
| `edk2-cpython` | `4d18b177996c6fa10c0309ae1533c2b869a224c0` | `py31213-working-good-20260714` |
| Version | `3.12.13` | `Include/patchlevel.h` |

### patch/ inventory

1. `0001-Implement-minimal-emulation-of-pipe-functionality.patch`
2. `0002-Introduce-support-for-ANSI-escape-codes-for-console.patch`
3. `0003-Fix-uninitialized-static-variable.patch`
4. `0004-Fix-ioctl-vararg-handling-for-Console-and-Shell-devi.patch`

Copied into: `Python-3.12.13/patches/`

### INF source counts (source port)

| INF | Count | Iteration 1 |
|-----|-------|-------------|
| Python312.inf | 15 | Keep glue |
| PythonCoreLib.inf | 126 | Keep |
| PythonBuiltinLib.inf | 27 | Keep |
| PythonExtLib.inf | 82 | Minus ctypes/ssl/zlib/hashopenssl/readline |
| PythonTestLib.inf | 1 | Omit |

### Phase 0 result

**Done** for inventory purposes. Live GCC rebuild of edk2-py312 deferred to Linux/WSL.

---

## Phase 1 — Scaffold AppPkg directory tree

### Checklist

| Step | Action | Result |
|------|--------|--------|
| 1.1 | Create `Python-3.12.13/` | **Done** |
| 1.2 | Populate from `edk2-cpython` (exclude `efi/`) | **Done** (robocopy exit 1 = files copied) |
| 1.3 | Create `PyMod-3.12.13/`, stubs | **Done** |
| 1.4 | Adapt `srcprep.py` | **Done** (`PyMod-3.12.13`, imports `stat`) |
| 1.5 | Update `AppPkg.dec` | **Done** — added `Applications/Python/Python-3.12.13/Include` |
| 1.6 | Update `AppPkg.dsc` | **Done** — `!if $(BUILD_PYTHON312)` → `Python312.inf` |
| 1.7 | Document PACKAGES_PATH | **Done** in `Py312ReadMe.txt` |

### Artifacts created

```text
AppPkg/Applications/Python/Python-3.12.13/
  (full CPython 3.12.13 tree without efi/)
  PyMod-3.12.13/
  patches/*.patch
  frozen/README.txt
  frozen/frozen_modules.mk   # from Phase 3 start
  srcprep.py
  Python312.inf
  Py312ReadMe.txt
  create_python_pkg.sh|.bat  # stubs returning error until Phase 6
```

### Phase 1 result

**Done.** DSC gate and packaging placeholders in place.

---

## Phase 2 — Extract PyMod-3.12.13 overlays

### Checklist

| Step | Action | Result |
|------|--------|--------|
| 2.1–2.2 | Copy EFI glue + Iteration 1 `UEFI_C_SOURCE` files into PyMod | **Done** |
| 2.3 | Lib overlays (`os`, `uefipath`, `site`, …) | **Done** |
| 2.4 | `module_config.c` → `PyMod/.../Modules/config.c` | **Done** |
| 2.5 | Revert stock tree to vanilla for forked files | **Not done** — tree still contains in-tree UEFI deltas (copied from edk2-cpython). Follow-up: optional clean to vanilla + INF-only PyMod paths. |
| 2.6 | Run `srcprep.py` | **Done** — `Include/pyconfig.h` with `PLATFORM "uefi"` |
| 2.7 | Inventory | Captured in this status + plan |

### PyMod layout (first pass)

```text
PyMod-3.12.13/
  efi/src/*          # UefiMain, dummies, console, fs, …
  efi/Include/*      # pyconfig + efi/*.h, dlfcn, pthread
  Include/pyconfig.h # also for srcprep into tree Include/
  Modules/config.c   # inittab (excluded modules commented)
  Modules/<UEFI forks>
  Programs/python.c
  Python/, Objects/, Parser/ forks
  Lib/os.py, uefipath.py, site.py, pathlib.py, …
```

### Explicitly not copied into PyMod (Iteration 1)

- `Modules/_ctypes/*`, `_ssl.c`, `_hashopenssl.c`, `zlibmodule.c`, `readline.c`

### Phase 2 result

**Done** for Iteration 1 overlay extraction. Stock-tree cleanup to vanilla is deferred.

---

## Phase 3 — Port frozen / deepfreeze

### Checklist

| Step | Action | Result |
|------|--------|--------|
| 3.1 | Copy `frozen_modules.mk` | **Done** → `Python-3.12.13/frozen/frozen_modules.mk` |
| 3.2 | Adjust paths for AppPkg | **Not started** |
| 3.3 | Document host bootstrap | **Not started** |
| 3.4 | `run_freeze.sh` wrapper | **Not started** |
| 3.5 | Commit vs generate policy | **Not started** |
| 3.6 | Gate: regenerate frozen outputs | **Not started** |

### Notes

- Source also has `Python/deepfreeze/deepfreeze.c` listed in CoreLib — may already be present in the copied tree.
- Next: adapt makefile paths and run on Linux/WSL with host Python freeze tools.

---

## Phase 4 — Monolithic Python312.inf (MIN)

### Checklist

| Step | Action | Result |
|------|--------|--------|
| 4.1–4.3 | Merge sources from 4 INFs, map PyMod paths | **Done** (generated) |
| 4.4–4.5 | Packages / LibraryClasses without external pkgs | **Done** (StdLib sockets kept) |
| 4.6 | GCC BuildOptions | **Done** (initial flags) |
| 4.7 | Protocols | **Done** |
| 4.8 | MIN = Iteration 1 | **Done** |
| — | Comment excluded inittab in `config.c` | **Done** |
| — | Successful GCC compile/link | **Not started** (Phase 5) |

### Generated INF stats

- Unique `[Sources]` entries: **241**
- Omitted Ext refs: **9** (ctypes / ssl / zlib / hashopenssl / test / readline)

### Phase 4 result

**In progress** — INF authored; awaiting GCC build to fix compile errors.

---

## Phase 5 — DSC / libc patches / first GCC build

### Checklist

| Step | Action | Result |
|------|--------|--------|
| 5.1 | AppPkg includes StdLib.inc | Already true |
| 5.2 | Apply `patches/*.patch` to edk2-libc | **Not started** |
| 5.3 | First GCC `build -D BUILD_PYTHON312` | **Not started** (needs Linux/WSL + edk2) |
| 5.4 | Fix compile/link errors | Pending |
| 5.5 | Gate: `Python312.efi` exists | Pending |

### DSC/DEC already updated in Phase 1

```text
build -a X64 -b NOOPT -t GCC \
  -p AppPkg/AppPkg.dsc \
  -D BUILD_PYTHON312
```

with `PACKAGES_PATH=<edk2>:<edk2-libc>` only.

---

## Phase 6+ / 7 / 8

Not started. See plan for steps.

---

## Known issues / follow-ups

1. **Windows agent cannot run GCC AppPkg build** — continue Phase 5 on Linux/WSL (see WSL GCC guide).
2. **Frozen/deepfreeze artifacts missing** — gitignored; must run `make frozen` (or equivalent) before AppPkg build.
3. **Stock CPython tree still has UEFI deltas** — same content as PyMod for forked files; cleaning to upstream vanilla is optional polish.
4. **`create_python_pkg.*` still stubs** (exit 1).
5. **NASM sources** (`edk2stack.nasm`, `edk2handler.nasm`) and `asm_trampoline.S` need toolchain validation under EDK II GCC.
6. **`Modules/main.c`** still from CoreLib stock path — confirm vs 3.12 CLI/`PyConfig` expectations under UEFI.
7. Duplicate `efi/src/module_config.c` remains under PyMod; INF uses `Modules/config.c` only.

---

## Next actions (recommended)

**Follow:** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)

On WSL Ubuntu, in order:

1. Install packages + prepare edk2 BaseTools.
2. Check out `feature/python-3.12.13-apppkg`.
3. Apply `Python-3.12.13/patches/*.patch` (0001 required for `upipe`).
4. `python3 srcprep.py`.
5. **Generate frozen headers** via edk2-py312 `make frozen` and copy into AppPkg (they are gitignored — not in the tree yet).
6. `build -D BUILD_PYTHON312 -t GCC` and paste the first error into this status log.
