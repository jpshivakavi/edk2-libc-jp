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

### Iteration 1 scope constraint (hard)

**Do not** include extension modules that depend on these out-of-tree packages:

| Excluded package | C modules / pieces to omit in Iteration 1 |
|------------------|-------------------------------------------|
| `edk2-libffi` | `_ctypes` (`Modules/_ctypes/*`), `_ctypes_test`, `PythonTestLib`, `LibFFI` library class / `.dec` |
| `edk2-openssl` | `_ssl`, `_hashlib` (via `Modules/_hashopenssl.c`), `OpensslLib` / `LibOpenSSL` |
| `edk2-zlib` | `zlib` (`Modules/zlibmodule.c` + in-tree zlib objs if linked via LibZlib), `LibZlib` |
| `edk2-pyreadline` | `readline` (`Modules/readline.c`) — already optional/commented in current Ext INF |

**Iteration 1 `PACKAGES_PATH` must not require:**

```text
edk2-libffi/EFI
edk2-openssl/efi
edk2-zlib/efi
edk2-pyreadline   # whatever path that package uses
```

Use only:

```text
PACKAGES_PATH=<edk2>:<edk2-libc>
```

Builtin hashes that do **not** need OpenSSL (e.g. `_md5`, `_sha1`, `_sha2`, `_sha3`, `_blake2`) may stay if they build from in-tree sources. Prefer those in Iteration 1 over `_hashopenssl`.

Re-introducing ffi/ssl/zlib/readline is a **later iteration** (see Phase 6 batch list and Phase 8).

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
| `edk2-zlib` / `edk2-openssl` / `edk2-libffi` (+ optional pyreadline) | Used by full Ext today — **out of scope for AppPkg Iteration 1** |
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
| External packages | **None** in Iteration 1 | **Exclude** `edk2-libffi`, `edk2-openssl`, `edk2-zlib`, `edk2-pyreadline` and all modules that need them |
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

Do **not** add `edk2-openssl`, `edk2-zlib`, `edk2-libffi`, or `edk2-pyreadline` to `PACKAGES_PATH` until a later iteration.

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

**Do not extract / do not list in Iteration 1 INF** (defer with PyMod copies optional but commented out of INF + `config.c`):

- `Modules/_ctypes/*`, `_ctypes_test` → `edk2-libffi`
- `Modules/_ssl.c`, `Modules/_hashopenssl.c` → `edk2-openssl`
- `Modules/zlibmodule.c` (+ LibZlib objs) → `edk2-zlib`
- `Modules/readline.c` → `edk2-pyreadline`

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
# Iteration 2+ (needs external packages — DO NOT ENABLE YET):
#   Modules/_ctypes/*          # edk2-libffi
#   Modules/_ctypes/_ctypes_test.c
#   Modules/_ssl.c             # edk2-openssl
#   Modules/_hashopenssl.c     # edk2-openssl
#   Modules/zlibmodule.c       # edk2-zlib
#   Modules/readline.c         # edk2-pyreadline
```

4.4. `[Packages]`:

```inf
MdePkg/MdePkg.dec
MdeModulePkg/MdeModulePkg.dec
ShellPkg/ShellPkg.dec
StdLib/StdLib.dec
AppPkg/AppPkg.dec
# Iteration 2+ only:
# LibFFI/LibFFI.dec
# LibZlib/LibZlib.dec
# LibOpenSSL/LibOpenSSL.dec
# (pyreadline package .dec when available)
```

4.5. `[LibraryClasses]` — start from 3.6.8 + 3.12 needs **without** external crypto/ffi/zlib:

- `LibC`, `LibString`, `LibStdio`, `LibMath`, `LibWchar`, `LibGen`, `LibNetUtil`, `LibGdtoa` (3.12 uses this)
- `DevMedia`, `UefiLib`, `DebugLib`
- `BsdSocketLib` / `EfiSocketLib` when `_socket` is enabled (StdLib — OK for Iteration 1)
- **Not in Iteration 1:** `LibFFI`, `LibZlib`, `OpensslLib` (or any pyreadline lib)

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
| **`MIN` = Iteration 1** | glue + core + builtins + Ext **without** ctypes/ssl/zlib/readline | First AppPkg GCC link / REPL — **no external packages** |
| `STD` | MIN + more in-tree Ext (socket, decimal, asyncio, …) still without ffi/ssl/zlib/readline | Broader stdlib |
| `FULL` | STD + `_ctypes` + `_ssl`/`_hashopenssl` + `zlib` + optional `readline` | Match edk2-py312; needs those four packages |

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

**Iteration 2+ (Phase 8)** — one external package at a time, **easy → complex** (see Phase 8):

1. `zlib` + `edk2-zlib`
2. `readline` + `edk2-pyreadline` (optional; REPL line editing)
3. `_hashlib` + `edk2-openssl` (OpenSSL-backed hashes)
4. `_ssl` + same `edk2-openssl` package
5. `_ctypes`, `_ctypes_test` + `edk2-libffi`

After each batch: rebuild + import test.

6.5. Gate C: Iteration 1 smoke checklist from Phase 0.4 (ssl/ctypes/zlib/readline absent). Full edk2-py312 parity is **not** Gate C for Iteration 1.

---

## Phase 7 — Docs, CI, cleanup

**Exit criteria:** Another engineer can build GCC Python312 from edk2-libc alone (plus documented libc patches / external packages).

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

## Phase 8 — External packages (Iteration 2+)

**Goal:** Re-enable C extension modules that Iteration 1 omitted because they need
out-of-tree EDK II packages. Add **one batch at a time**; after each batch: extend
`PACKAGES_PATH`, `Python312.inf`, `PyMod-3.12.13/Modules/config.c`, rebuild, and
run import smoke tests.

**Prerequisite:** Iteration 1 green (GCC / `BUILD_PYTHON312` / package / basic REPL).

Do **not** block Iteration 1 on Phase 8. VS2022, coexistence packaging, dynamic
`.so` loading, and full edk2-py312 `FULL` INF parity remain **after** these five
batches unless noted.

---

### Iteration 1 omissions (inventory)

These are commented out in `PyMod-3.12.13/Modules/config.c` and omitted from
`Python312.inf` `[Sources]` / `[Packages]` / `[LibraryClasses]`:

| Python module | C sources (typical) | External package | Notes |
|---------------|-------------------|------------------|--------|
| `zlib` | `Modules/zlibmodule.c` (+ link via `LibZlib`, not 3.6-style vendored `Modules/zlib/*.c` unless you choose that) | **edk2-zlib** | `import zlib`; used by `gzip`, some stdlib paths |
| `readline` | `Modules/readline.c` | **edk2-pyreadline** | GNU **readline** module — not `Parser/myreadline.c` (already in INF) |
| `hashlib` (OpenSSL path) | `Modules/_hashopenssl.c` | **edk2-openssl** | Registers as **`_hashlib`** in `config.c`; in-tree `_md5` / `_sha*` / `_blake2` / `_sha3` already work without this |
| `ssl` | `Modules/_ssl.c` | **edk2-openssl** (same tree as `_hashlib`) | `import ssl`; usually enable **after** `_hashlib` smoke passes |
| `_ctypes` | `Modules/_ctypes/_ctypes.c`, `callbacks.c`, `callproc.c`, `cfield.c`, `stgdict.c`, … | **edk2-libffi** | Callbacks, libffi, platform glue |
| `_ctypes_test` | `Modules/_ctypes/_ctypes_test.c` | **edk2-libffi** | Optional test module; enable with `_ctypes` |

**Iteration 1 `PACKAGES_PATH` (unchanged until Phase 8):**

```text
PACKAGES_PATH=<edk2>:<edk2-libc>
```

**Phase 8 adds one path segment per package**, e.g.:

```text
PACKAGES_PATH=<edk2>:<edk2-libc>:<edk2-zlib>/efi
```

(Exact subdirectory names match each package’s README — mirror **edk2-py312**
`PACKAGES_PATH` when in doubt.)

---

### Recommended order: easy → complex

Work **top to bottom**. Complexity is integration surface (INF, link, UEFI
constraints, stdlib fallout), not just line count.

| Step | Package | Enable in `config.c` | Why this order |
|------|---------|----------------------|----------------|
| Step | Package | Enable in `config.c` | Why this order |
|------|---------|----------------------|----------------|
| **8.1** | **edk2-zlib** | `zlib` | Single extension, small API, no callbacks/asm; good first external link — **see** [`Python312_Phase8_8.1_Zlib.md`](./Python312_Phase8_8.1_Zlib.md) |
| **8.2** | **edk2-pyreadline** | `readline` | One module; improves REPL only; no TLS/crypto |
| **8.3** | **edk2-openssl** | `_hashlib` | One `.c` file; validates OpenSSL link before full `ssl` |
| **8.4** | **edk2-openssl** (same) | `_ssl` | TLS, contexts, cert APIs; depends on OpenSSL + often on 8.3 |
| **8.5** | **edk2-libffi** | `_ctypes`, then `_ctypes_test` | Hardest: libffi, `callproc`/callbacks, CPU helpers (NASM/GAS), optional `Lib/ctypes/*` via `srcprep` |

---

### Per-step checklist (repeat for 8.1 … 8.5)

1. Clone/build the external package for **GCC / X64** (same as edk2-py312).
2. Append package path to **`PACKAGES_PATH`** (keep `edk2` + `edk2-libc`).
3. Copy UEFI deltas from **edk2-py31213/edk2-cpython** into **PyMod** if any
   (`_ssl.c`, `_ctypes/*`, `zlibmodule.c`, `readline.c`, …).
4. Uncomment **`extern PyInit_*`** and **`_PyImport_Inittab`** entries in
   `PyMod-3.12.13/Modules/config.c`.
5. Add **`[Sources]`**, **`[Packages]`**, **`[LibraryClasses]`** to
   `Python312.inf` (mirror edk2-py312 `Python312.inf` / ExtLib for that module).
6. Apply **StdLib patches** locally (unchanged policy); run **`srcprep.py`**.
7. **`build -D BUILD_PYTHON312`** → **`create_python_pkg.sh`**.
8. **Smoke** (minimum):

| After step | Smoke |
|------------|--------|
| 8.1 | `import zlib`; `zlib.crc32(b"uefi")` |
| 8.2 | `import readline` (if exposed); REPL line edit / history if supported |
| 8.3 | `import hashlib`; `hashlib.sha256(b"x").hexdigest()` (OpenSSL path) |
| 8.4 | `import ssl`; create default context (depth per firmware policy) |
| 8.5 | `import ctypes`; simple `ctypes.c_int`; optional `_ctypes_test` |

---

### Phase 8 — Other deferred work (not external-package batches)

Do after or in parallel with 8.1–8.5 only if needed:

1. **VS2022 / MSFT** — NASM vs MASM, `/DUEFI_MSVC_64`, ctypes `libffi_msvc`, warning disables
2. **Coexistence packaging** — Python 3.6.8 and 3.12.13 on one FAT volume
3. **Dynamic extension loading** — `lib-dynload` beyond empty directory
4. **Free-threaded / experimental** CPython builds
5. **Full CPython test suite** on UEFI
6. **edk2-py312 `FULL` INF parity** — all Ext batches above + any remaining diffs

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
| `PACKAGES_PATH=.../efi` (zlib/ssl/ffi) | **Iteration 2+ only** — Iteration 1 uses edk2 + edk2-libc only |
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
[ ] Phase 8  External packages 8.1 zlib → 8.2 readline → 8.3–8.4 openssl → 8.5 ctypes
```

---

## Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Accidental link of LibFFI/OpenSSL/Zlib | Review INF `[Packages]`/`[LibraryClasses]`/`[Sources]`; CI must not set those `PACKAGES_PATH` entries in Iteration 1 |
| `config.c` still registers omitted modules | Comment out matching `_PyImport_Inittab` entries or link will fail / import will crash |
| Monolithic INF too large / link order issues | Start with MIN INF; add in-tree Ext in batches |
| Forgotten in-tree delta not copied to PyMod | Diff `edk2-cpython` vs vanilla 3.12.13; require inventory sign-off |
| Frozen files stale | CI always runs freeze or commits generated outputs with regen script |
| Libc patches missing | README gate + CI applies patches before build |
| PREFIX / getpath mismatch | One packaging layout; test `sys.path` on first boot |
| Expecting ssl/zip/ctypes parity with edk2-py312 too early | Document Iteration 1 vs FULL explicitly in Py312ReadMe |
| `srcprep` only copies `.h`/`.py` | Never put unique `.c` only in tree root; INF must point at PyMod `.c` |

---

## Deliverables

### Iteration 1 (this milestone)

1. `AppPkg/Applications/Python/Python-3.12.13/` in edk2-libc AppPkg style.
2. `PyMod-3.12.13/` containing UEFI deltas needed for MIN (external-package modules may exist in PyMod but must stay out of INF/`config.c`).
3. `Python312.inf` + `BUILD_PYTHON312` DSC gate with **no** LibFFI / OpenSSL / LibZlib / pyreadline dependencies.
4. `srcprep.py`, frozen scripts, libc patches, `create_python_pkg.*`.
5. `Py312ReadMe.txt` with GCC build instructions and an explicit “not included yet” list for ctypes/ssl/zlib/readline.
6. CI workflow producing a package artifact using `PACKAGES_PATH=edk2:edk2-libc` only.
7. Verified GCC build: REPL smoke; confirm `import ssl` / `ctypes` / `zlib` / `readline` are unavailable.

### Later iterations (Phase 8)

8. Wire external packages **in plan order**: **edk2-zlib** → **edk2-pyreadline** → **edk2-openssl** (`_hashlib` then `_ssl`) → **edk2-libffi** (`_ctypes`, `_ctypes_test`), updating INF + `config.c` each step.
9. VS2022 support (Phase 8 “other deferred”).

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
