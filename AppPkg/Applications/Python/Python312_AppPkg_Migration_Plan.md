# Python 3.12.13 → edk2-libc AppPkg Migration Plan

## Goal

Restructure the working **GCC UEFI Python 3.12.13** port from
[`edk2-py312`](file:///c:/Users/njayapra/github/edk2-py312) (out-of-tree
`PythonPkg` + in-tree CPython deltas) into the **edk2-libc AppPkg style** used
by Python 3.6.8, so that:

```text
build -a X64 -b RELEASE -t GCC \
  -p AppPkg/AppPkg.dsc \
  -D BUILD_PYTHON312
```

produces a UEFI Shell application (e.g. `Python.efi` / `Python312.efi`) under
the standard AppPkg layout.

**VS2022 is out of scope for this plan.** Complete and verify **GCC first**,
then add a follow-on MSVC porting plan.

### Upstream PR priority — vendored FULL parity (locked)

**Goal for tianocore/edk2-libc:** one reproducible workspace, same as official docs and CI:

```text
PACKAGES_PATH=<edk2>:<edk2-libc>
```

No **intel-sandbox** (or other) sibling EDK packages on `PACKAGES_PATH` for the contribution.
Match **edk2-py312 FULL** module surface (`zlib`, `readline`, OpenSSL-backed `hashlib`/`ssl`, GCC
`_ctypes`) by **vendoring third-party library sources inside this repository**, following the
**Python 3.6.8 AppPkg precedent** (e.g. `Modules/zlib/*.c` in `Python368.inf`, MSVC-only
vendored `libffi_msvc` for `_ctypes`).

| Module / feature | edk2-py312 (reference) | Upstream AppPkg approach (Phase 8) |
|------------------|------------------------|--------------------------------------|
| `zlib` | intel-sandbox **edk2-zlib** @ `8ae7f507` | **`PyMod-3.12.13/Modules/zlib/`** + stock `Modules/zlibmodule.c` |
| `readline` | `edk2-pyreadline` | **`PyMod-.../Modules/readline/`** (`readline.py` + `pyreadline/`); **edk2console** in INF; staged by `create_python_pkg` |
| `_hashlib` (OpenSSL) | `edk2-openssl` | **`PyMod-.../Modules/openssl/`** (or libcrypto tree) + `_hashopenssl.c` in PyMod when forked |
| `ssl` | same OpenSSL package | Same OpenSSL tree under PyMod + `_ssl.c` |
| `_ctypes` / `_ctypes_test` | `edk2-libffi` | **`PyMod-.../Modules/_ctypes/`** + vendored libffi (3.6.8: `libffi_msvc` under PyMod) |

**Reference only (do not require for upstream build):** intel-sandbox `edk2-zlib`, `edk2-openssl`,
`edk2-libffi`, `edk2-pyreadline` — use their INF/source lists when choosing what to vendor, then
delete any `LibZlib` / `LibOpenSSL` / `LibFFI` `.dec` dependencies from `Python312.inf`.

**Licenses:** add or extend third-party notices in root `Readme.md` (and per-tree `README.txt`
under each **`PyMod-3.12.13/Modules/<vendor>/`** tree) for zlib, OpenSSL, libffi, readline.

**3.6.8 GCC baseline vs FULL parity:** upstream 3.6.8 GCC shipped **zlib** only; `_ssl` and GCC
`_ctypes` were not in the green path. This plan’s **FULL** target is **edk2-py312 parity on GCC**,
not a literal copy of 3.6.8’s module set.

### Iteration 1 scope constraint (hard)

**Do not** link extension modules that depend on vendored third-party trees until Phase 8 adds
those sources to `Python312.inf` and enables `config.c` entries.

| Omitted in Iteration 1 | Depends on (Phase 8 vendoring) |
|------------------------|--------------------------------|
| `zlib` | `PyMod-3.12.13/Modules/zlib/*.c` (not `LibZlib`) |
| `readline` | `PyMod-.../Modules/readline/` + `readline.c` |
| `_hashlib` (OpenSSL path) | `PyMod-.../Modules/openssl/` + `_hashopenssl.c` |
| `ssl` | Same OpenSSL tree under PyMod + `_ssl.c` |
| `_ctypes`, `_ctypes_test` | `PyMod-.../Modules/_ctypes/` + libffi vendor subtree |

**Iteration 1 and upstream FULL `PACKAGES_PATH` (unchanged):**

```text
PACKAGES_PATH=<edk2>:<edk2-libc>
```

Builtin hashes that do **not** need OpenSSL (e.g. `_md5`, `_sha1`, `_sha2`, `_sha3`, `_blake2`)
stay enabled in Iteration 1. Enable `_hashopenssl` in Phase 8.3 after OpenSSL is vendored.

Re-enable omitted modules in **Phase 8 (vendored batches)** — one library at a time, easy → complex.

---

## Source and target layouts

### Source (working today): `edk2-py312`

| Path | Role |
|------|------|
| `c:\Users\njayapra\github\edk2-py312\` | Root orchestrator (`Makefile`, patches, submodules) |
| `edk2-cpython\` | CPython 3.12.13 with UEFI `#ifdef UEFI_C_SOURCE` edits **in-tree** |
| `edk2-cpython\efi\PythonPkg\` | Separate EDK II package (`PythonPkg.dsc`) |
| `Python312.inf` + `PythonCoreLib` / `Builtin` / `Ext` / `Test` | Split libraries → thin app |
| `efi\PythonPkg\src\` | `UefiMain`, console/fs/environ, dummies, `module_config.c` |
| `efi\PythonPkg\Include\pyconfig.h` | UEFI `pyconfig` (`PLATFORM "uefi"`, `PREFIX "fs0:\\EFI"`) |
| `edk2-zlib` / `edk2-openssl` / `edk2-libffi` (+ optional pyreadline) | **Reference** for what to vendor into edk2-libc — **not** required on `PACKAGES_PATH` for upstream |
| `patch\0001`–`0004` | Required `edk2-libc` fixes (`make patch_libc`) |

Build today:

```bash
make patch_libc
make frozen
make python   # build -p PythonPkg/PythonPkg.dsc -t GCC
```

### Target (3.6.8 pattern): `edk2-libc`

| Path | Role |
|------|------|
| `AppPkg\Applications\Python\Python-3.12.13\` | CPython tree + port artifacts |
| `PyMod-3.12.13\` | UEFI overlays (forked `.c`, `.h`, `.py`, package glue) |
| `srcprep.py` | Copy `.h` / `.py` from PyMod into the CPython tree |
| `Python312.inf` | **Single** `UEFI_APPLICATION` INF (merged source list) |
| `create_python_pkg.sh` / `.bat` | Stage EFI filesystem tree |
| `AppPkg\AppPkg.dsc` | Gate with `!if $(BUILD_PYTHON312)` |
| `AppPkg\AppPkg.dec` | Expose `Applications/Python/Python-3.12.13/Include` |

Reference implementation:

- `AppPkg\Applications\Python\Python-3.6.8\`
- `Py368ReadMe.txt`, `Python368.inf`, `PyMod-3.6.8\`, `srcprep.py`

---

## Design decisions (lock these early)

Record choices in a short `Py312ReadMe.txt` when scaffolding.

| Decision | Recommendation for GCC milestone 1 | Notes |
|----------|------------------------------------|-------|
| Entry point | Keep **`UefiMain`** → `ShellCEntryLib` initially | Matches working 3.12; later optional simplify to 3.6.8-style direct `ShellCEntryLib` |
| Library layout | **Monolithic** `Python312.inf` | Collapse Core/Builtin/Ext into one INF; **omit TestLib** in Iteration 1 |
| Overlay model | **`PyMod-3.12.13` + `srcprep.py`** | Extract deltas from `edk2-cpython`; stop editing stock tree in place |
| PREFIX / install layout | Keep **`fs0:\EFI`** for first GCC bring-up **or** align to 3.6.8 `\EFI\StdLib` | Pick **one** and update `pyconfig.h` + packaging together |
| Third-party libs | **None linked** in Iteration 1 | Phase 8 **vendors** zlib/openssl/libffi/readline **in-repo**; never `PACKAGES_PATH` sandbox packages for upstream |
| Frozen / deepfreeze | **Required** pre-build step | Port `efi\scripts\frozen_modules.mk` into AppPkg scripts |
| Libc patches | Document applying `edk2-py312\patch\*.patch` until upstreamed | Same constraint as current 3.12 root README |
| Binary name | `Python312.efi` vs `Python.efi` | Prefer `Python` (`BASE_NAME = Python`) for Shell familiarity, or keep `Python312` for coexistence with 3.6.8 |

---

## Phase 0 — Prerequisites and baseline capture

**Duration estimate:** short  
**Exit criteria:** Known-good 3.12 GCC build still works; inventory file exists.

### Steps

0.1. Confirm host can build current port:

```bash
cd /path/to/edk2-py312
git submodule update --init --recursive
make patch_libc
make local_python   # if not already present
make frozen
make python
```

0.2. Record artifact path, e.g.:

```text
edk2/Build/PythonPkg/NOOPT_GCC/X64/Python312.efi
```

0.3. Capture inventory (commit hash + lists):

- Commit SHA of `edk2-py312` and `edk2-cpython`
- List of files containing `UEFI_C_SOURCE` under `edk2-cpython`
- Source lists from:
  - `efi/PythonPkg/Python312.inf`
  - `PythonCoreLib.inf`
  - `PythonBuiltinLib.inf`
  - `PythonExtLib.inf`
  - `PythonTestLib.inf`
- Contents of `efi/PythonPkg/src/`
- Contents of `patch/`
- `efi/PythonPkg/Include/pyconfig.h` (and any `Include/efi/*.h`)
- Frozen script: `efi/scripts/frozen_modules.mk`

0.4. Capture a smoke checklist against current image (QEMU or hardware).

For **migration Iteration 1**, treat this as the *target* checklist (ssl/ctypes/zlib intentionally N/A):

```text
[ ] python starts / REPL banner shows "on uefi"
[ ] import os, sys, json
[ ] import math, datetime, _decimal, asyncio   # OK if in-tree; skip if not in MIN INF
[ ] open/read a .py file from FS
[ ] import ssl / ctypes / zlib / readline     # MUST fail or be absent in Iteration 1
```

Keep a separate note of the full edk2-py312 checklist (with ssl/ctypes/zlib) for a **later** iteration only.

0.5. Create a working branch on **edk2-libc** (do not push unless asked):

```bash
cd /path/to/edk2-libc
git checkout -b feature/python-3.12.13-apppkg
```

---

## Phase 1 — Scaffold AppPkg directory tree

**Exit criteria:** Empty tree + docs exist; DSC gate compiles when INF is a stub.

### Steps

1.1. Create target directory:

```text
AppPkg/Applications/Python/Python-3.12.13/
```

1.2. Populate from CPython 3.12.13 sources using **one** of:

- **Option A (preferred):** Copy from `edk2-py312/edk2-cpython` but **exclude** `efi/` temporarily; you will reintroduce UEFI pieces via PyMod.
- **Option B:** Extract vanilla `Python-3.12.13` tarball, then overlay UEFI deltas in Phase 2–3.

1.3. Create empty overlay and tooling placeholders:

```text
Python-3.12.13/
  PyMod-3.12.13/          # to be filled
  srcprep.py              # clone from Python-3.6.8/srcprep.py; set src = PyMod-3.12.13
  Python312.inf           # stub, then grow
  create_python_pkg.sh
  create_python_pkg.bat
  Py312ReadMe.txt
  frozen/                 # or Scripts/ — hold freeze recipes
```

1.4. Update `srcprep.py` from 3.6.8:

- Change `src = r'PyMod-3.12.13'`
- Keep copy of `.h` and `.py` only (same as today)
- Optionally fix missing `import stat` if present in 3.6.8 script
- Document that **`.c` files remain under PyMod and are referenced by INF path** (do not rely on srcprep for `.c`)

1.5. Update `AppPkg/AppPkg.dec`:

```dsc
[Includes]
  Applications/Python/Python-3.6.8/Include
  Applications/Python/Python-3.12.13/Include
```

1.6. Update `AppPkg/AppPkg.dsc` Components:

```dsc
!if $(BUILD_PYTHON312)
  AppPkg/Applications/Python/Python-3.12.13/Python312.inf
!endif
```

Keep `BUILD_PYTHON368` unchanged so both can coexist.

1.7. Ensure build environment variables (same as 3.6.8). **Iteration 1:**

```bash
export PACKAGES_PATH=/path/to/edk2:/path/to/edk2-libc
export EDK2_LIBC_PATH=/path/to/edk2-libc
```

Do **not** add intel-sandbox `edk2-openssl`, `edk2-zlib`, `edk2-libffi`, or `edk2-pyreadline` to `PACKAGES_PATH` for the upstream contribution (Phase 8 vendors into this repo instead).

---

## Phase 2 — Extract PyMod-3.12.13 overlays

**Exit criteria:** Every UEFI-specific delta lives under `PyMod-3.12.13/` mirroring CPython paths; stock tree under `Python-3.12.13/` can be vanilla or cleaned.

### Steps

2.1. Create mirrored directories under `PyMod-3.12.13` for each forked path.

2.2. **C / headers known to use `UEFI_C_SOURCE`** (copy from `edk2-cpython` into PyMod):

**Core / runtime**

- `Programs/python.c`
- `Python/fileutils.c`
- `Python/pystate.c`
- `Python/sysmodule.c`
- `Python/getargs.c`
- `Python/pytime.c`
- `Python/bootstrap_hash.c`
- `Objects/floatobject.c`
- `Objects/complexobject.c`
- `Parser/tokenizer.c`
- `Include/pymath.h`
- Internal headers as needed (`Include/internal/...` if patched)

**Modules (platform / OS)**

- `Modules/posixmodule.c` (+ `Modules/clinic/posixmodule.c.h` if required)
- `Modules/getpath.c`
- `Modules/timemodule.c`
- `Modules/signalmodule.c`
- `Modules/socketmodule.c`
- `Modules/_posixsubprocess.c`
- `Modules/faulthandler.c`
- `Modules/gcmodule.c`
- `Modules/mathmodule.c`
- `Modules/cmathmodule.c`
- `Modules/_datetimemodule.c`
- `Modules/mmapmodule.c`
- `Modules/termios.c`
- `Modules/_pickle.c`
- `Modules/_sre/sre_lib.h` (if patched)
- `Modules/expat/xmlparse.c` (if patched)
- `Modules/_decimal/...` (if patched)
- Other Ext files only if they contain UEFI deltas **and** are in Iteration 1 scope

**Do not list in Iteration 1 INF** (defer; Phase 8 adds vendored sources + `config.c`):

- `Modules/_ctypes/*`, `_ctypes_test` → vendored **libffi** (8.5)
- `Modules/_ssl.c`, `Modules/_hashopenssl.c` → vendored **OpenSSL** (8.3–8.4)
- `Modules/zlibmodule.c` + `PyMod-3.12.13/Modules/zlib/*.c` → vendored **zlib** (8.1)
- `Modules/readline.c` → vendored **readline** (8.2)

2.3. **Pure Python UEFI surface** (into PyMod `Lib/`):

- `Lib/os.py`
- `Lib/uefipath.py` (3.12; new vs 3.6.8 `ntpath`-centric approach)
- `Lib/pathlib.py` (if patched)
- `Lib/importlib/_bootstrap_external.py`
- `Lib/site.py`
- `Lib/asyncio/uefi_events.py` and related asyncio files (if present)
- Any other Lib files that differ from upstream 3.12.13
- Defer `Lib/ctypes/*` overlays until Iteration 2 (`edk2-libffi`) — optional to copy into PyMod now, but do not rely on them in Iteration 1

2.4. **Move EFI package glue into PyMod** (new vs 3.6.8 — keep under a clear prefix):

Suggested layout:

```text
PyMod-3.12.13/
  efi/
    Include/pyconfig.h
    Include/efi/*.h          # if any
    src/edk2main.c
    src/edk2stack.nasm
    src/edk2handler.nasm
    src/edk2console.c
    src/edk2excep.c
    src/environ.c
    src/fs.c
    src/time.c
    src/urandom.c
    src/cmath.c
    src/dummy_dlfcn.c
    src/dummy_unistd.c
    src/dummy_pthread.c
    src/dummy_mmap.c
    Modules/config.c         # rename from module_config.c to match 3.6.8 naming
```

Alternatively mirror 3.6.8 closely:

```text
PyMod-3.12.13/Modules/config.c     # was module_config.c
PyMod-3.12.13/Modules/edk2main.c   # or Keep Programs/ + separate UefiMain sources in INF
PyMod-3.12.13/Include/pyconfig.h
```

Pick one convention and list every INF path from it.

2.5. Revert in-tree copies under `Python-3.12.13/` of files that now live **only** in PyMod (so diffs stay reviewable). INF will compile PyMod paths for those `.c` files.

2.6. Run `python srcprep.py` and verify headers/Lib overlays land correctly.

2.7. Produce an inventory markdown or spreadsheet:

| Upstream path | PyMod path | Why (UEFI) |
|---------------|------------|------------|

---

## Phase 3 — Port frozen / deepfreeze pre-build

**Exit criteria:** Host can generate the frozen sources the INF needs before `build`.

### Steps

3.1. Copy `edk2-cpython/efi/scripts/frozen_modules.mk` (and helpers) into:

```text
AppPkg/Applications/Python/Python-3.12.13/frozen/
```

3.2. Adjust paths for AppPkg layout (no root `Makefile` `PYTHON_DIR`).

3.3. Document host bootstrap requirement:

- Need a host Python / `_freeze_module` build (3.12 today uses `make local_python` from a pinned cpython checkout).
- For AppPkg: either document “install CPython 3.12 on host” or keep a small `frozen/bootstrap.md`.

3.4. Add a thin wrapper:

- `frozen/run_freeze.sh` (Linux/GCC primary)
- Optional `frozen/run_freeze.bat` later for Windows

3.5. Ensure generated outputs are either:

- committed (if stable and small enough), or
- generated in CI/`srcprep` and listed in `.gitignore` with clear regen steps

3.6. Gate: running freeze produces the same files that `PythonCoreLib.inf` / deepfreeze expected.

---

## Phase 4 — Author monolithic `Python312.inf`

**Exit criteria:** INF parses; modules link or fail with actionable undefined refs (not missing files).

### Steps

4.1. Start from `Python368.inf` structure and `Python312.inf` from edk2-py312.

4.2. Set defines:

```inf
[Defines]
  BASE_NAME       = Python          # or Python312
  MODULE_TYPE     = UEFI_APPLICATION
  ENTRY_POINT     = UefiMain        # Phase 1 recommendation
  DEFINE PYTHON_VERSION = 3.12.13
```

4.3. Merge `[Sources]` in this order (comment sections clearly):

1. **EFI / UEFI glue** — `PyMod-$(PYTHON_VERSION)/efi/src/*` (or equivalent)
2. **Programs** — `Programs/python.c` or PyMod override
3. **Parser / Python / Objects** — prefer stock paths; use `PyMod-.../` only where forked
4. **`PyMod-.../Modules/config.c`** (inittab)
5. **Builtin modules** — from `PythonBuiltinLib.inf`
6. **Core-only modules** — from `PythonCoreLib.inf` (if not already covered)
7. **Safe extensions** — from `PythonExtLib.inf` **minus** ffi/ssl/zlib/readline (see below)
8. **Omitted in Iteration 1** — do not list; keep commented block for later:

```inf
# Iteration 2+ (Phase 8 — vendored libs; enable one batch at a time):
#   PyMod-$(PYTHON_VERSION)/Modules/zlib/*.c
#   PyMod-$(PYTHON_VERSION)/Modules/readline.c + Modules/readline/
#   PyMod-$(PYTHON_VERSION)/Modules/_hashopenssl.c + Modules/openssl/
#   PyMod-$(PYTHON_VERSION)/Modules/_ssl.c
#   PyMod-$(PYTHON_VERSION)/Modules/_ctypes/* + libffi under PyMod
```

4.4. `[Packages]`:

```inf
MdePkg/MdePkg.dec
MdeModulePkg/MdeModulePkg.dec
ShellPkg/ShellPkg.dec
StdLib/StdLib.dec
AppPkg/AppPkg.dec
# Phase 8: no extra .dec — third-party code is [Sources] in Python312.inf only
```

4.5. `[LibraryClasses]` — start from 3.6.8 + 3.12 needs **without** external crypto/ffi/zlib:

- `LibC`, `LibString`, `LibStdio`, `LibMath`, `LibWchar`, `LibGen`, `LibNetUtil`, `LibGdtoa` (3.12 uses this)
- `DevMedia`, `UefiLib`, `DebugLib`
- `BsdSocketLib` / `EfiSocketLib` when `_socket` is enabled (StdLib — OK for Iteration 1)
- **Not in Iteration 1:** vendored zlib/openssl/libffi/readline object lists (Phase 8)

4.6. `[BuildOptions]` for **GCC** (align with both ports):

```inf
[BuildOptions]
  GCC:*_*_*_CC_FLAGS = -Wno-unused-function -Wno-format -Wno-error \
    -fno-strict-aliasing \
    -I$(EDK2_LIBC_PATH)/AppPkg/Applications/Python/Python-3.12.13/Include \
    -DHAVE_MEMMOVE -DPy_BUILD_CORE \
    # plus Ext-specific -DPy_BUILD_CORE_BUILTIN where required
```

Notes:

- `UEFI_C_SOURCE` should already come from `StdLib/StdLib.inc` when AppPkg includes it — verify with `build -v` / preprocessor dump.
- Do **not** assume MSFT flags yet.
- NASM sources (`edk2stack.nasm`, `edk2handler.nasm`) need correct `| GCC` / tool rules; port from `Python312.inf` exactly.

4.7. Protocols: copy from current `Python312.inf` (`CpuArch`, `LoadedImage`, `SimpleTextInputEx`, `Rng`, `Shell`, …).

4.8. Milestone INF variants:

| Tag | Contents | Purpose |
|-----|----------|---------|
| **`MIN` = Iteration 1** | glue + core + builtins + Ext **without** ctypes/ssl/zlib/readline | First AppPkg GCC link / REPL |
| `STD` | MIN + more in-tree Ext (socket, decimal, asyncio, …) still without ffi/ssl/zlib/readline | Broader stdlib |
| `FULL` | STD + `_ctypes` + `_ssl`/`_hashopenssl` + `zlib` + optional `readline` | Match edk2-py312; **vendored** libs in-repo (Phase 8) |

**Iteration 1 = deliver and validate `MIN` (or `STD` with the same package exclusions) only.**

Also mirror exclusions in `PyMod-.../Modules/config.c` `_PyImport_Inittab[]`: comment out `{"_ctypes", ...}`, `{"_ssl", ...}`, `{"zlib", ...}`, `{"readline", ...}`, `{"_hashopenssl", ...}`, `{"_ctypes_test", ...}`.

---

## Phase 5 — Wire AppPkg DSC / StdLib / libc patches

**Exit criteria:** `build -D BUILD_PYTHON312` runs the INF under AppPkg + StdLib.

### Steps

5.1. Confirm `AppPkg.dsc` still `!include StdLib/StdLib.inc` (already true for AppPkg).

5.2. Apply `edk2-py312/patch/0001`–`0004` onto the **edk2-libc** tree used for build (until upstreamed):

| Patch | Topic (from current root flow) |
|-------|--------------------------------|
| 0001 | pipe / upipe |
| 0002 | ANSI escape console |
| 0003 | uninitialized static |
| 0004 | ioctl vararg |

**Policy (Iteration 1):** do **not** commit applied StdLib diffs on the
migration branch (zero StdLib divergence until an upstream tianocore PR).
Keep patches under AppPkg and require a local apply before build.

Document in `Py312ReadMe.txt` / WSL guide:

```text
Required before building Python 3.12 (from edk2-libc root):
  git apply --check --ignore-whitespace \
    AppPkg/Applications/Python/Python-3.12.13/patches/*.patch
  git apply --ignore-whitespace \
    AppPkg/Applications/Python/Python-3.12.13/patches/*.patch
  ls StdLib/LibC/Uefi/upipe.c
```

Patch files live in the AppPkg Python tree for self-containment:

```text
Python-3.12.13/patches/0001-*.patch
...
```

5.3. First GCC build command:

```bash
cd /path/to/edk2
. edksetup.sh
export PACKAGES_PATH=/path/to/edk2:/path/to/edk2-libc
export EDK2_LIBC_PATH=/path/to/edk2-libc

cd /path/to/edk2-libc/AppPkg/Applications/Python/Python-3.12.13
python3 srcprep.py
# run frozen wrappers if required

build -a X64 -b NOOPT -t GCC \
  -p "$EDK2_LIBC_PATH/AppPkg/AppPkg.dsc" \
  -D BUILD_PYTHON312
```

5.4. Fix compile errors systematically:

1. Missing includes / `pyconfig.h` not found → srcprep / `AppPkg.dec` / INF `-I`
2. Duplicate symbols → removed stock file while PyMod still listed wrongly
3. Missing frozen symbols → Phase 3
4. Undefined `UefiMain` / NASM → Sources/`BuildOptions` for GCC assembler
5. Macro clashes (`PRI*`, StackCheckLib) → port fixes from `PythonPkg.dsc` / `pyconfig.h`

5.5. Gate B: `Python*.efi` exists under:

```text
Build/AppPkg/<TARGET>_GCC/X64/
```

---

## Phase 6 — Minimum runtime, then grow feature set

**Exit criteria:** REPL works from Shell; then known modules import.

### Steps

6.1. Adapt packaging (`create_python_pkg.sh`):

- Input: `Build/AppPkg/.../Python.efi` (or `Python312.efi`)
- Output layout — choose and document:

**Align to 3.12 current (PREFIX `fs0:\EFI`):**

```text
EFI/
  bin/Python312.efi      # or Tools/
  lib/python3.12/        # from Lib/
```

**Align to 3.6.8:**

```text
EFI/
  Tools/Python.efi
  StdLib/lib/python3.12/
  StdLib/etc/            # from StdLib/Efi/StdLib/etc
```

If you switch layout, update `PyMod-.../Include/pyconfig.h` `PREFIX` / `EXEC_PREFIX` / getpath landmarks together.

6.2. Boot under QEMU (reuse edk2-py312 `efi/scripts/dist` ideas) or hardware FAT volume.

6.3. Smoke tests:

```text
[ ] Banner: Python 3.12.13 ... on uefi
[ ] import sys; print(sys.platform, sys.path)
[ ] import os; os.listdir('.')
[ ] run a .py file from the volume
```

6.4. Re-enable extensions in INF + `config.c` in batches:

**Iteration 1 (this plan’s first green build)** — in-tree / StdLib only:

1. Core + builtins required for REPL / `import site`
2. Common Ext without external packages: e.g. `_json`, `_pickle`, `_struct`, `array`, `math`/`cmath`, `_datetime`, `_decimal`, `_asyncio`, `_socket` + BsdSocketLib/EfiSocketLib, in-tree hashes (`_md5`, `_sha*`, `_blake2`), `pyexpat`, etc.
3. **Explicitly skip:** `_ctypes`, `_ssl`, `_hashopenssl`, `zlib`, `readline`, `_ctypes_test`

**Iteration 2+ (Phase 8 — vendored)** — one batch at a time, **easy → complex**:

1. **8.1** `zlib` — `PyMod-3.12.13/Modules/zlib/` (commit `8ae7f507`); **no** `LibZlib`
2. **8.2** `readline` — vendored readline/libedit + `Modules/readline.c`
3. **8.3** `_hashlib` — vendored OpenSSL + `_hashopenssl.c`
4. **8.4** `ssl` — same OpenSSL tree + `_ssl.c`
5. **8.5** `_ctypes`, `_ctypes_test` — vendored libffi (GCC X64) + `_ctypes` sources + CPU helpers

After each batch: rebuild + import smoke. **`PACKAGES_PATH` stays `edk2:edk2-libc` for all steps.**

6.5. Gate C: Iteration 1 smoke checklist from Phase 0.4 (ssl/ctypes/zlib/readline absent). **Upstream FULL parity** (all Phase 8 batches green, same `PACKAGES_PATH`) is the **post–Iteration 1** target for the contribution, not Gate C for the first green MIN build.

---

## Phase 7 — Docs, CI, cleanup

**Exit criteria:** Another engineer can build GCC Python312 from edk2-libc alone (plus documented libc patches). Phase 8 vendored libs live **in this repo**, not as extra clones.

### Steps

7.1. Write `Py312ReadMe.txt` modeled on `Py368ReadMe.txt`:

- Limitations (static extensions, env vars, etc. — update for 3.12 realities)
- `srcprep.py` + frozen steps
- `PACKAGES_PATH` / `EDK2_LIBC_PATH`
- `build ... -D BUILD_PYTHON312`
- Install layout
- Enabling sockets / ssl
- Supported C modules list (from final `config.c`)

7.2. Add `GCCCompilationBKMs.rst` for 3.12 (or section in Py312ReadMe).

7.3. Add GitHub Action (clone of `build-python-uefi-gcc.yaml`):

- `srcprep.py`
- frozen step
- apply patches
- `build -t GCC -D BUILD_PYTHON312`
- `create_python_pkg.sh`
- upload artifact

7.4. Update root `Readme.md` license / path pointers for Python 3.12.13.

7.5. Remove or archive temporary duplicated sources; ensure PyMod is the only place for UEFI deltas.

7.6. Optional: open/track upstream PR for libc patches so AppPkg build needs fewer local patches.

---

## Phase 8 — Vendored third-party libraries (Iteration 2+, upstream FULL)

**Goal:** Re-enable C extension modules omitted in Iteration 1 by **adding vendored library
sources and module `.c` files to `Python312.inf`**, with matching `PyMod-3.12.13/Modules/config.c`
entries — **without** extending `PACKAGES_PATH` beyond `edk2` + `edk2-libc`.

**Prerequisite:** Iteration 1 green (GCC / `BUILD_PYTHON312` / package / basic REPL).

**Parity target:** Module import surface aligned with **edk2-py312 FULL** on GCC/X64, delivered as a
single upstream-friendly tree (contrast: 3.6.8 GCC only vendored **zlib**).

**PyMod layout for third-party / UEFI-adopted modules (match 3.6.8):**

- **Stock** CPython extension entrypoints that are unmodified stay in `Python-3.12.13/Modules/`
  (e.g. `zlibmodule.c`). **Vendored** third-party C libraries and **UEFI-forked** module `.c`
  files live under **`PyMod-3.12.13/Modules/`**, mirroring relative paths (`zlib/`, `_ctypes/`, …).
- **`Python312.inf`** lists **`PyMod-$(PYTHON_VERSION)/Modules/...`** for those paths (see
  `Python368.inf`: `Modules/zlib/*.c` in 3.6 stock tree; `_ctypes` uses `PyMod-$(PYTHON_VERSION)/Modules/_ctypes/...`).
  For 3.12, **all** vendored Phase 8 trees use the **PyMod** prefix in the INF.
- Add **`-I.../PyMod-3.12.13/Modules/<vendor>`** in `[BuildOptions]` when the stock module
  includes vendor headers (zlib: `zlib.h` from `PyMod-.../Modules/zlib/`).
- Document pins in `PyMod-3.12.13/Modules/<vendor>/README.txt` and `PyMod-3.12.13/README.txt`.

**Implementation hygiene:**

- When importing a file list from intel-sandbox EDK packages, **copy sources into
  `PyMod-3.12.13/Modules/`** and drop `[Packages]` / `[LibraryClasses]` references to
  `LibZlib`, `LibOpenSSL`, `LibFFI`.
- Record **version pins** in `Py312ReadMe.txt` and each vendor `README.txt` under PyMod.
- **EFI size:** vendored OpenSSL + libffi will grow `Python312.efi`; document in
  `Python312_EFI_Size_Notes.md` after 8.3–8.5.

---

### Iteration 1 omissions (inventory)

Same as before — commented out in `config.c` and omitted or partial in `Python312.inf`:

| Python module | C sources (typical) | Vendored dependency | Notes |
|---------------|-------------------|---------------------|--------|
| `zlib` | `PyMod-$(PYTHON_VERSION)/Modules/zlib/*.c` | **zlib** 1.2.11 (edk2-zlib `8ae7f507`) + `Modules/zlibmodule.c` |
| `readline` | `Modules/readline.c` | **readline** or **libedit** | Not `Parser/myreadline.c` (already in INF) |
| `hashlib` (OpenSSL) | `Modules/_hashopenssl.c` | **OpenSSL** subset | Registers as **`_hashlib`**; in-tree `_sha*` still work without it |
| `ssl` | `Modules/_ssl.c` | Same **OpenSSL** tree | Enable after 8.3 smoke |
| `_ctypes` | `Modules/_ctypes/*.c` | **libffi** + arch helpers | `cpu_gcc.s` / NASM per arch (see 3.6.8 `Python368.inf` `[Sources.X64]`) |
| `_ctypes_test` | `Modules/_ctypes/_ctypes_test.c` | **libffi** | Optional; enable with `_ctypes` |

**`PACKAGES_PATH` for all Phase 8 steps:**

```text
PACKAGES_PATH=<edk2>:<edk2-libc>
```

---

### Recommended order: easy → complex

| Step | Vendored lib | Enable in `config.c` | Primary work |
|------|--------------|----------------------|--------------|
| **8.1** | **edk2-zlib (vendored)** | `zlib` | **`PyMod-3.12.13/Modules/zlib/`** from commit `8ae7f507` — see [`Python312_Phase8_8.1_Zlib.md`](./Python312_Phase8_8.1_Zlib.md) |
| **8.2** | **edk2-pyreadline** | `readline` (stdlib) | **`PyMod-3.12.13/Modules/readline/`** — see [`Python312_Phase8_8.2_Readline.md`](./Python312_Phase8_8.2_Readline.md) |
| **8.3** | **OpenSSL** | `_hashlib` | Vendor minimal libcrypto (+ headers); `_hashopenssl.c`; UEFI build flags |
| **8.4** | **OpenSSL** (same) | `_ssl` | Add libssl pieces + `_ssl.c`; smoke after 8.3 |
| **8.5** | **libffi** | `_ctypes`, `_ctypes_test` | Vendor libffi for GCC X64; `_ctypes/*`; hardest batch |

---

### Per-step checklist (repeat for 8.1 … 8.5)

1. Choose upstream **version** and copy sources into **`PyMod-3.12.13/Modules/<vendor>/`**.
2. Add **`[Sources]`** (and arch-specific `[Sources.X64]` if needed) to `Python312.inf`.
3. Add **`[BuildOptions]`** defines/includes for that lib (`-I...`, `OPENSSL_NO_*`, `HAVE_CONFIG_H`, etc.).
4. Copy UEFI deltas from **edk2-py31213/edk2-cpython** into **PyMod** where needed.
5. Uncomment **`extern PyInit_*`** and **`_PyImport_Inittab`** in `PyMod-3.12.13/Modules/config.c`.
6. Update **license / Readme** notices for new vendored trees.
7. Apply **StdLib patches** locally; run **`srcprep.py`**.
8. **`build -D BUILD_PYTHON312`** → **`create_python_pkg.sh`** (no extra `PACKAGES_PATH` segments).
9. **Smoke:**

| After step | Smoke |
|------------|--------|
| 8.1 | `import zlib`; `zlib.crc32(b"uefi")` |
| 8.2 | `import readline` (if exposed); REPL line edit / history if supported |
| 8.3 | `import hashlib`; `hashlib.sha256(b"x").hexdigest()` (OpenSSL path) |
| 8.4 | `import ssl`; create default context (depth per firmware policy) |
| 8.5 | `import ctypes`; simple `ctypes.c_int`; optional `_ctypes_test` |

---

### Phase 8 — Optional / parallel (not vendoring batches)

1. **VS2022 / MSFT** — extend vendored **libffi** with `libffi_msvc` path (3.6.8-style) + NASM/MASM
2. **Coexistence packaging** — Python 3.6.8 and 3.12.13 on one FAT volume
3. **Dynamic extension loading** — `lib-dynload` beyond empty directory
4. **Free-threaded / experimental** CPython builds
5. **Full CPython test suite** on UEFI
6. **Fork-only sandbox build** — optional doc appendix: reproduce edk2-py312 using intel-sandbox
   packages (not part of upstream PR)

---

### Upstream PR sequencing (recommended)

| PR slice | Contents | `PACKAGES_PATH` |
|----------|----------|-----------------|
| **PR 1 (MIN)** | Iteration 1 — core port, docs, CI | `edk2:edk2-libc` |
| **PR 2+ or same PR** | Phase 8.1–8.5 vendored FULL | still `edk2:edk2-libc` |

Maintainers may prefer **MIN merged first**, then vendored batches in follow-ups; either way,
**no sandbox packages** in the upstream story.

---

## Mapping cheat sheet: edk2-py312 → AppPkg

| edk2-py312 | AppPkg destination |
|------------|--------------------|
| `efi/PythonPkg/PythonPkg.dsc` | Gate in `AppPkg/AppPkg.dsc` (`BUILD_PYTHON312`) |
| `Python312.inf` + 4 libs | Single `Python312.inf` |
| `src/module_config.c` | `PyMod-3.12.13/Modules/config.c` |
| `Include/pyconfig.h` | `PyMod-3.12.13/Include/pyconfig.h` (+ srcprep) |
| `src/edk2*.c`, NASM, dummies | `PyMod-3.12.13/...` + INF `[Sources]` |
| In-tree `UEFI_C_SOURCE` edits | `PyMod-3.12.13/<same relative path>` |
| `make frozen` | `Python-3.12.13/frozen/run_freeze.sh` |
| `make patch_libc` | `Python-3.12.13/patches/*.patch` applied to edk2-libc |
| `PACKAGES_PATH=.../efi` (zlib/ssl/ffi sandbox) | **Not used for upstream** — Phase 8 **vendors** into edk2-libc |
| `stage0.sh` disk image | `create_python_pkg.sh` (+ optional later QEMU helpers) |
| `ENTRY_POINT = UefiMain` | Keep initially |
| Builtin name `uefi` / `uefipath` | Keep (do not force rename to 3.6.8 `edk2`) unless API compat required |
| `_ctypes` / `_ssl` / `zlib` / `readline` | **Omitted in Iteration 1** — Phase **8.1–8.5** (see plan) |

---

## Suggested execution order (checklist)

```text
[ ] Phase 0  Baseline capture of edk2-py312 GCC build
[ ] Phase 1  Scaffold Python-3.12.13 + DSC/DEC gates + srcprep.py
[ ] Phase 2  Extract PyMod-3.12.13 (skip ctypes/ssl/zlib/readline for INF)
[ ] Phase 3  Port frozen/deepfreeze pre-build
[ ] Phase 4  Author MIN monolithic Python312.inf (no ffi/ssl/zlib/readline)
[ ] Phase 5  Libc patches + first GCC AppPkg link (PACKAGES_PATH = edk2 + edk2-libc only)
[ ] Phase 6  Package, REPL smoke (no ssl/ctypes/zlib/readline)
[ ] Phase 7  Docs + CI for Iteration 1
[ ] Phase 8  Vendored libs 8.1 zlib → 8.2 readline → 8.3–8.4 OpenSSL → 8.5 libffi/ctypes (PACKAGES_PATH unchanged)
```

---

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Accidental link of sandbox Lib* packages | INF must not list `LibZlib`/`LibOpenSSL`/`LibFFI` `.dec`; only vendored `[Sources]` |
| `config.c` still registers omitted modules | Comment out matching `_PyImport_Inittab` entries or link will fail / import will crash |
| Monolithic INF too large / link order issues | Start with MIN INF; add vendored batches in Phase 8 order |
| Forgotten in-tree delta not copied to PyMod | Diff `edk2-cpython` vs vanilla 3.12.13; require inventory sign-off |
| Frozen files stale | CI always runs freeze or commits generated outputs with regen script |
| Libc patches missing | README gate + CI applies patches before build |
| PREFIX / getpath mismatch | One packaging layout; test `sys.path` on first boot |
| OpenSSL/libffi vendoring size or license review | Document versions + `PyMod-.../Modules/<vendor>/README.txt`; expect longer upstream review for 8.3–8.5 |
| `srcprep` only copies `.h`/`.py` | Never put unique `.c` only in tree root; INF must point at PyMod `.c` |

---

## Deliverables

### Iteration 1 (this milestone)

1. `AppPkg/Applications/Python/Python-3.12.13/` in edk2-libc AppPkg style.
2. `PyMod-3.12.13/` containing UEFI deltas needed for MIN (external-package modules may exist in PyMod but must stay out of INF/`config.c`).
3. `Python312.inf` + `BUILD_PYTHON312` DSC gate with **no** sandbox `LibZlib` / `LibOpenSSL` / `LibFFI` `.dec` dependencies.
4. `srcprep.py`, frozen scripts, libc patches, `create_python_pkg.*`.
5. `Py312ReadMe.txt` with GCC build instructions and an explicit “not included yet” list for ctypes/ssl/zlib/readline until Phase 8.
6. CI workflow producing a package artifact using `PACKAGES_PATH=edk2:edk2-libc` only.
7. Verified GCC build: REPL smoke; confirm `import ssl` / `ctypes` / `zlib` / `readline` are unavailable until Phase 8.

### Upstream FULL (Phase 8 — same `PACKAGES_PATH`)

8. Vendored batches **8.1–8.5** in plan order (zlib → readline → OpenSSL → ctypes), updating INF + `config.c` + license notices each step.
9. VS2022 support (Phase 8 optional / parallel).

---

## Reference paths (local)

| Item | Path |
|------|------|
| Working 3.12 port | `c:\Users\njayapra\github\edk2-py312` |
| CPython + EFI package | `c:\Users\njayapra\github\edk2-py312\edk2-cpython` |
| Prior upgrade notes | `c:\Users\njayapra\github\edk2-py312\PY31213_UEFI_MIGRATION_PLAN.md` |
| 3.6.8 AppPkg reference | `c:\Users\njayapra\github\edk2-libc\AppPkg\Applications\Python\Python-3.6.8` |
| This plan | `c:\Users\njayapra\github\edk2-libc\AppPkg\Applications\Python\Python312_AppPkg_Migration_Plan.md` |

---

## Next action after this plan

Begin **Phase 0 + Phase 1**: capture baseline hashes/inventory, then scaffold
`Python-3.12.13/` and DSC/DEC gates with a stub INF before moving any PyMod
content.
