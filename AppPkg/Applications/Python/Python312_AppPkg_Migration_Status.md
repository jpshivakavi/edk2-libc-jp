# Python 3.12.13 AppPkg Migration Status

**Plan:** [`Python312_AppPkg_Migration_Plan.md`](./Python312_AppPkg_Migration_Plan.md)  
**WSL GCC build guide:** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)  
**Started:** 2026-07-14  
**Updated:** 2026-07-14  
**Iteration:** 1 (no edk2-libffi / edk2-openssl / edk2-zlib / edk2-pyreadline)  
**Target repo:** `c:\Users\njayapra\github\edk2-libc`  
**Branch:** `feature/python-3.12.13-apppkg`  
**Source port:** `c:\Users\njayapra\github\edk2-py312` (use **`~/src/edk2-py31213/edk2-cpython`** for 3.12.13 sources; `~/src/edk2-py312/edk2-cpython` was 3.12.0 and must not be used for sync)

---

## Overall progress

| Phase | Name | Status |
|-------|------|--------|
| 0 | Prerequisites and baseline capture | **Done** (rebuild skipped on Windows) |
| 1 | Scaffold AppPkg directory tree | **Done** |
| 2 | Extract PyMod-3.12.13 overlays | **Done** (first pass; stock tree still holds UEFI deltas) |
| 3 | Port frozen / deepfreeze | **Done** (deepfreeze + global strings aligned for AppPkg) |
| 4 | Author monolithic Python312.inf (MIN) | **Done** (+ `Parser/myreadline.c`) |
| 5 | Wire DSC / libc patches / first GCC build | **Done** — `Python312.efi` built (GCC / NOOPT / X64) |
| 6 | Package + REPL smoke | **Done** — `create_python_pkg.*`, `py312_efi` layout, basic REPL on UEFI Shell (3.12.13, no exec_prefix warning) |
| 7 | Docs + CI | **In progress** — 7.1 Py312ReadMe, 7.2 GCCCompilationBKMs.rst |
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

### 2026-07-14 — Session 2 (WSL GCC)

1. Include path fixes (`Include/internal`, HACL, efi Include).
2. Aligned frozen/deepfreeze with AppPkg headers: `STRUCT_FOR_STR(dot)`, `_Py_LATIN1_CHR`, `_only_immortal`, single-char `_Py_ID`s needed by deepfreeze.
3. Enabled core `Parser/myreadline.c` (not GNU `readline`).
4. **First successful AppPkg build:** `Python312.efi`  
   Path: `~/src/edk2-py312/edk2/Build/AppPkg/NOOPT_GCC/X64/AppPkg/Applications/Python/Python-3.12.13/Python312/DEBUG/Python312.efi` (or sibling `OUTPUT`/FV path as produced by this build).

### 2026-07-14 — Session 3 (packaging)

1. Implemented `create_python_pkg.sh` / `.bat` for PREFIX `fs0:\EFI` layout (`EFI/bin`, `EFI/lib/python3.12`, `EFI/stdlib/etc`).
2. Staged sample package at `Python-3.12.13/pkg_out/` (local; do not commit).
3. Staged sample package at `Python-3.12.13/pkg_out/` (local; do not commit).

### 2026-07-14 — Session 4 (runtime)

1. Re-synced AppPkg core from **`~/src/edk2-py31213/edk2-cpython`** (3.12.13; not `edk2-py312` 3.12.0).
2. GCC rebuild + `create_python_pkg.sh` → `py312_efi/` (`EFI/bin/Python312.efi`, `lib/python3.12`, empty `lib-dynload`).
3. **Basic REPL smoke on UEFI Shell:** version **3.12.13**, no “platform dependent libraries” warning, Iteration‑1 expectations OK. Deeper stdlib/module testing deferred.

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
| 5.2 | Apply `patches/*.patch` to edk2-libc | **Required setup (not committed)** — `git apply --ignore-whitespace`; keep StdLib vanilla on branch |
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

## Phase 6 — Package + REPL smoke

### Checklist

| Step | Action | Result |
|------|--------|--------|
| 6.1 | `create_python_pkg.sh` / `.bat` (PREFIX `fs0:\EFI`) | **Done** (includes empty `lib-dynload`) |
| 6.2 | Stage `EFI/bin/Python312.efi` + `EFI/lib/python3.12/` | **Done** (`py312_efi/`) |
| 6.3 | Copy to FAT / hardware volume | **Done** (user) |
| 6.4 | Basic REPL: banner, `sys.version`, no getpath warning | **Done** (3.12.13 verified) |
| 6.5 | Extended import / stdlib matrix | **Deferred** (user follow-up) |

### Phase 6 result

**Done** for Iteration 1 basic smoke. See Session 4 work log.

---

## Phase 7 — Docs + CI

| Step | Action | Result |
|------|--------|--------|
| 7.1 | `Py312ReadMe.txt` (3.6.8-style) | **Done** |
| 7.2 | `GCCCompilationBKMs.rst` for 3.12.13 | **Done** |
| 7.3 | GitHub Action (GCC + BUILD_PYTHON312) | Not started |
| 7.4 | Root `Readme.md` pointers | Not started |
| 7.5 | PyMod-only UEFI delta cleanup | Not started |
| 7.6 | Upstream libc patches | Optional |

## Phase 8

External packages, VS2022 — deferred. See plan.

---

## Locked policy — StdLib patches

- **Do not commit** applied `StdLib/` / `StdLibPrivateInternalFiles/` diffs on this branch.
- **Required** before GCC build / after a clean checkout:
  `git apply --ignore-whitespace AppPkg/Applications/Python/Python-3.12.13/patches/*.patch`
- Patch 0001 (`upipe`) is mandatory to link; see `Py312ReadMe.txt` and WSL GCC guide §4.
- Optional later: upstream the four patches to tianocore/edk2-libc.

---

## Known issues / follow-ups

1. **Windows agent cannot run GCC AppPkg build** — continue on Linux/WSL (see WSL GCC guide).
2. **Frozen/deepfreeze artifacts missing** — gitignored; must generate/copy before AppPkg build.
3. **Stock CPython tree still has UEFI deltas** — same content as PyMod for forked files; cleaning to upstream vanilla is optional polish.
4. **`create_python_pkg.*` + basic REPL smoke** — **Done** (3.12.13 on UEFI Shell). Extended testing later.
5. **Local StdLib dirt** may exist from prior `git apply` / `make patch_libc` — discard or keep locally; never stage for Python commits.
6. **NASM sources** (`edk2stack.nasm`, `edk2handler.nasm`) and `asm_trampoline.S` need toolchain validation under EDK II GCC.
7. **`Modules/main.c`** still from CoreLib stock path — confirm vs 3.12 CLI/`PyConfig` expectations under UEFI.
8. Duplicate `efi/src/module_config.c` remains under PyMod; INF uses `Modules/config.c` only.

---

## Next actions (recommended)

**Follow:** [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md)

On WSL Ubuntu, in order:

1. Install packages + prepare edk2 BaseTools.
2. Check out `feature/python-3.12.13-apppkg`.
3. **Required:** `git apply --ignore-whitespace Python-3.12.13/patches/*.patch` (0001 = `upipe`).
4. `python3 srcprep.py` (if overlays changed).
5. Ensure frozen/deepfreeze artifacts are present (gitignored).
6. ~~`build -D BUILD_PYTHON312 -t GCC`; package; basic REPL smoke~~ — **Done** (Phase 6). Next: Phase 7 or deeper smoke / Iteration 2.
