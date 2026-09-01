# Build Python 3.12.13 (AppPkg FULL / Phase 8) with GCC on WSL Ubuntu

Guide for the **edk2-libc AppPkg** migration tree (not the old `edk2-py312`
`make python` flow). Follow these steps on **WSL2 Ubuntu** (20.04 / 22.04 / 24.04).

On branch **`feature/python-3.12.13-vs2022`** (sole manufacturing line for **GCC** and **VS2022**). **`feature/python-3.12.13-apppkg`** is reference-only. Status: [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md). **Phase 8** vendored libs are in **`Python312.inf`** — `zlib`, `readline`, `_ctypes`, OpenSSL `_hashlib` / `_ssl`.

**FULL build:** add **`-D BUILD_PYTHON312_FULL=TRUE`** to **`build -D BUILD_PYTHON312`**.

Related docs:

- Plan: [`Python312_VS2022_Port_Plan.md`](./Python312_VS2022_Port_Plan.md) · [`Python312_AppPkg_Migration_Plan.md`](./Python312_AppPkg_Migration_Plan.md)
- Status: [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md)
- BKMs: [`Python-3.12.13/GCCCompilationBKMs.rst`](./Python-3.12.13/GCCCompilationBKMs.rst) (GCC setup summary)
- ReadMe: [`Python-3.12.13/Py312ReadMe.txt`](./Python-3.12.13/Py312ReadMe.txt)
- Tree: `AppPkg/Applications/Python/Python-3.12.13/`

**PACKAGES_PATH:** `<edk2>:<edk2-libc>` only. Vendored zlib/openssl/libffi/readline
live under **`PyMod-3.12.13/Modules/`** — do **not** add sandbox `edk2-*` packages.
Phase guides: `Python312_Phase8_8.1_Zlib.md` … `8.5`, `8.3`, `8.4`.

**Deferred (plan):** Phase **7.3** CI and upstream PR until **Visual Studio** AppPkg
builds are working; this guide remains the GCC reference.

---

## 0. Layout you will use

### Interim (today — your working setup)

Python312 **build artifacts** live under the **edk2** tree inside **edk2-py312**,
not a standalone `~/src/edk2` clone (unless you have verified that tree builds AppPkg).

```text
~/src/edk2-py312/edk2     # WORKSPACE + BaseTools + Build/  (edksetup here)
~/src/edk2-libc           # jpshivakavi fork, branch feature/python-3.12.13-apppkg
~/src/edk2-py312          # optional: frozen/deepfreeze copy source only
```

```bash
export EDK2_LIBC_PATH=$HOME/src/edk2-libc
export PACKAGES_PATH=$HOME/src/edk2-py312/edk2:$EDK2_LIBC_PATH
# edksetup in ~/src/edk2-py312/edk2 — see §2 Option A, §7
```

Windows paths (example):

```text
/mnt/c/Users/njayapra/github/edk2-py312/edk2
/mnt/c/Users/njayapra/github/edk2-libc-jp   # or edk2-libc if same remote
```

Using `/mnt/c/...` works but is slower. Prefer `~/src/...` under WSL for builds.

### Target (upstream PR / CI — no edk2-py312 repo)

Same commands with **tianocore/edk2** only:

```text
~/src/edk2              # tianocore/edk2 — WORKSPACE + BaseTools + Build/
~/src/edk2-libc         # contribution tree
PACKAGES_PATH=$HOME/src/edk2:$HOME/src/edk2-libc
```

**Not there yet:** standalone `~/src/edk2` has failed or been untested for AppPkg
Python312 in this migration; use **edk2-py312/edk2** until §2 Option B is green.
**Phase 7.3 CI** should use the target layout. When switching, only change
**which edk2 tree** you `cd` into for `edksetup.sh`; keep **`EDK2_LIBC_PATH`**
and **`-p .../AppPkg/AppPkg.dsc -D BUILD_PYTHON312`** unchanged.

---

## 1. Install Ubuntu packages

```bash
sudo apt update
sudo apt install -y \
  build-essential uuid-dev iasl git nasm \
  python3 python3-pip python-is-python3 \
  libx11-dev libxext-dev

nasm -v          # ideally >= 2.15
gcc --version
python3 --version
```

On older Ubuntu, if `nasm` is too old, install a newer `.deb` (same approach as
`Python-3.6.8/GCCCompilationBKMs.rst`).

---

## 2. Prepare edk2 (BaseTools + `edksetup.sh`)

**Use the tree that already works for you:** `edk2-py312/edk2` (not a separate
`~/src/edk2` clone unless you have verified AppPkg builds there). Session 2 in
`Python312_AppPkg_Migration_Status.md` recorded **`Python312.efi`** under:

`~/src/edk2-py312/edk2/Build/AppPkg/NOOPT_GCC/X64/...`

### Option A — **Recommended (matches edk2-py312 / your green builds)**

```bash
mkdir -p ~/src
cd ~/src/edk2-py312/edk2

# One-time (or after BaseTools updates):
make -C BaseTools

export EDK_TOOLS_PATH=$HOME/src/edk2-py312/edk2/BaseTools
. edksetup.sh    # sets WORKSPACE to this edk2 tree
```

Windows path equivalent: `/mnt/c/Users/njayapra/github/edk2-py312/edk2`.

### Option B — Standalone tianocore `edk2` (**target** for upstream / CI)

Use this once Hello/AppPkg smoke and **`BUILD_PYTHON312`** both succeed from
`~/src/edk2` (same `PACKAGES_PATH` pattern as Option A, with `$HOME/src/edk2`
as the first segment).

```bash
mkdir -p ~/src
cd ~/src/edk2
git submodule update --init
make -C BaseTools
export EDK_TOOLS_PATH=$HOME/src/edk2/BaseTools
. edksetup.sh
```

If `build` fails immediately from Option B but works from Option A, keep using
**Option A** for AppPkg Python312; `create_python_pkg.sh` expects **`WORKSPACE`**
to be the same tree where **`Build/AppPkg/.../Python312.efi`** was produced.

Edit `Conf/target.txt` (under the edk2 tree) as needed:

```text
TOOL_CHAIN_TAG        = GCC
# or GCC5 on older BKMs — use whatever your edk2 Conf/tools_def supports
TARGET_ARCH           = X64
```

Quick smoke (optional; run after **Option A** `edksetup`):

```bash
export PACKAGES_PATH=$HOME/src/edk2-py312/edk2:$HOME/src/edk2-libc
export EDK2_LIBC_PATH=$HOME/src/edk2-libc
build -a X64 -b NOOPT -t GCC \
  -p $EDK2_LIBC_PATH/AppPkg/AppPkg.dsc \
  -m $EDK2_LIBC_PATH/AppPkg/Applications/Hello/Hello.inf
```

---

## 3. Use the AppPkg migration branch of edk2-libc

```bash
cd ~/src/edk2-libc   # jpshivakavi fork
git checkout feature/python-3.12.13-vs2022
git pull
```

Confirm these exist:

```bash
ls AppPkg/Applications/Python/Python-3.12.13/Python312.inf
ls AppPkg/Applications/Python/Python-3.12.13/PyMod-3.12.13/Modules/config.c
ls AppPkg/Applications/Python/Python-3.12.13/patches/*.patch
```

---

## 4. Apply libc patches (**required** setup — Phase 5.2)

These come from `edk2-py312` and are **not** in upstream edk2-libc yet.
**StdLib patch policy (FULL and MIN):** keep **zero StdLib divergence** on the Git branch;
apply patches locally as a build prerequisite (re-apply after a clean
checkout). **Patch 0001 (`upipe`) is required** to link; 0002–0004 are
strongly recommended for GCC runtime.

Use `--ignore-whitespace` (patch context can carry trailing spaces):

```bash
cd ~/src/edk2-libc   # same tree as EDK2_LIBC_PATH / PACKAGES_PATH

# Prefer a clean StdLib baseline (no half-applied local edits):
#   git checkout -- StdLib StdLibPrivateInternalFiles
#   git clean -fd StdLib StdLibPrivateInternalFiles

git apply --check --ignore-whitespace \
  AppPkg/Applications/Python/Python-3.12.13/patches/*.patch
git apply --ignore-whitespace \
  AppPkg/Applications/Python/Python-3.12.13/patches/*.patch

# Verify upipe landed:
ls StdLib/LibC/Uefi/upipe.c
```

If `git apply` still fails (line drift), try one patch at a time and record
failures in `Python312_AppPkg_Migration_Status.md`.

**Do not commit** StdLib / `StdLibPrivateInternalFiles` changes from these
patches into the Python migration branch. Upstream them separately (or keep
applying patches) until tianocore lands equivalents.

---

## 5. srcprep (overlay headers / Lib)

```bash
cd ~/src/edk2-libc/AppPkg/Applications/Python/Python-3.12.13
python3 srcprep.py
# Expect: Include/pyconfig.h with PLATFORM "uefi"
grep PLATFORM Include/pyconfig.h
```

---

## 6. Generate frozen / deepfreeze outputs (**required**)

These files are **gitignored** in CPython and were **not** copied into AppPkg:

- `Python/frozen_modules/*.h`
- `Python/deepfreeze/deepfreeze.c`

`Python312.inf` lists `Python/deepfreeze/deepfreeze.c`, so the build will fail without them.

**Windows:** [`Tools/build/regen_frozen_windows.cmd`](./Python-3.12.13/Tools/build/regen_frozen_windows.cmd) (Python **3.12.x** host; see [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) §6 Option C).

### Option A — Recommended: reuse edk2-py312 freeze (fastest)

If you already have `~/src/edk2-py312` (or `/mnt/c/Users/njayapra/github/edk2-py312`):

```bash
cd ~/src/edk2-py312   # adjust path
git submodule update --init --recursive   # if needed

# Builds host _freeze_module / bootstrap and generates frozen headers + deepfreeze.c
make local_python    # once; takes a while
make frozen

# Copy generated artifacts into the AppPkg tree
APP_PY=~/src/edk2-libc/AppPkg/Applications/Python/Python-3.12.13
SRC_PY=~/src/edk2-py312/edk2-cpython

mkdir -p "$APP_PY/Python/frozen_modules" "$APP_PY/Python/deepfreeze"
cp -a "$SRC_PY/Python/frozen_modules/"*.h "$APP_PY/Python/frozen_modules/"
cp -a "$SRC_PY/Python/deepfreeze/deepfreeze.c" "$APP_PY/Python/deepfreeze/"

ls "$APP_PY/Python/deepfreeze/deepfreeze.c"
ls "$APP_PY/Python/frozen_modules" | wc -l
```

**AppPkg fork (`edk2-libc-jp-vsfix`):** after copying artifacts, run this fork’s global + latin1 fix (stock edk2-py312 **`deepfreeze.c`** still has single-char **`&_Py_ID`**):

```bash
cd ~/src/edk2-libc-jp-vsfix/AppPkg/Applications/Python/Python-3.12.13   # adjust path
python3 Tools/build/generate_global_objects.py
python3 Tools/build/fix_deepfreeze_latin1.py
```

See [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §5.

### Option B — Freeze using AppPkg tree + edk2-py312 host tools

```bash
cd ~/src/edk2-py312
make local_python   # produces build/cpython/... _freeze_module

cd ~/src/edk2-libc/AppPkg/Applications/Python/Python-3.12.13
make -f frozen/frozen_modules.mk ROOT_DIR=~/src/edk2-py312
```

(You may need small path fixes in `frozen_modules.mk` if `ROOT_DIR` layout differs.)

### Verify before build

```bash
test -f Python/deepfreeze/deepfreeze.c && echo deepfreeze OK
test -f Python/frozen_modules/importlib._bootstrap.h && echo frozen OK
```

---

## 7. Build Python312 (AppPkg)

Run from the **same edk2 tree as §2 Option A** (`edk2-py312/edk2`). Source
Python + INF live in **`edk2-libc`** (fork); BaseTools + **`Build/`** live under
**`edk2-py312/edk2`**.

```bash
export EDK2_LIBC_PATH=$HOME/src/edk2-libc    # jpshivakavi fork on feature/python-3.12.13-apppkg
export PACKAGES_PATH=$HOME/src/edk2-py312/edk2:$EDK2_LIBC_PATH

cd $HOME/src/edk2-py312/edk2
export EDK_TOOLS_PATH=$PWD/BaseTools
. edksetup.sh

build -a X64 -b NOOPT -t GCC \
  -p "$EDK2_LIBC_PATH/AppPkg/AppPkg.dsc" \
  -D BUILD_PYTHON312 \
  -D BUILD_PYTHON312_FULL=TRUE
```

Expected artifact (path may vary slightly by toolchain):

```text
~/src/edk2-py312/edk2/Build/AppPkg/NOOPT_GCC/X64/Python312.efi
# or .../Python312/DEBUG/Python312.efi
```

**Packaging** (must use the same shell so **`WORKSPACE`** still points at
`edk2-py312/edk2`):

```bash
cd "$EDK2_LIBC_PATH/AppPkg/Applications/Python/Python-3.12.13"
./create_python_pkg.sh GCC NOOPT X64 ~/py312_efi
```

Notes:

- Use `GCC` or `GCC5` to match `Conf/target.txt` / `tools_def.txt`.
- `NOOPT` matches the current edk2-py312 default; try `RELEASE` later.
- **Do not** add openssl/zlib/libffi **packages** to `PACKAGES_PATH` — they are vendored in-repo.
- First OpenSSL link is large (~600+ libcrypto + libssl `.c`); allow long compile times.

Expected output (when successful):

```text
Build/AppPkg/NOOPT_GCC/X64/Python312.efi
```

(Exact `NOOPT_GCC` vs `NOOPT_GCC5` depends on toolchain tag.)

---

## 8. What to do when the build fails

Work the errors in this order and append each batch to
`Python312_AppPkg_Migration_Status.md`.

| Symptom | Likely fix |
|---------|------------|
| `upipe` undefined | Re-apply patch 0001; confirm `upipe.c` in `StdLib/LibC/Uefi/Uefi.inf` |
| `pyconfig.h` / `efi/*.h` not found | Re-run `srcprep.py`; check INF `-I.../Include` and `-I.../PyMod-3.12.13/efi/Include` |
| NASM errors on `edk2stack.nasm` / `edk2handler.nasm` | Install newer nasm; compare flags with edk2-py312 `Python312.inf` |
| GAS / `asm_trampoline.S` | May need `| GCC` path flags or temp stub for first link — copy approach from edk2-py312 CoreLib |
| `PyInit__ctypes` / `_ssl` / `zlib` undefined | Enable in `PyMod-3.12.13/Modules/config.c` and matching `[Sources]` in `Python312.inf` (FULL); see Phase 8 guides |
| OpenSSL `e_os.h` missing | Vendor repo-root `PyMod-.../Modules/openssl/e_os.h` (see Phase 8.3 Status) |
| OpenSSL `OPENSSL_ia32_rdseed_bytes` | Add `PyMod-.../Modules/openssl/efi/src/rand_rdrand.nasm` to INF |
| Missing frozen `*.h` | Restore from edk2-cpython or run freeze |
| Stack protector / `StackCheckLib` | edk2-py312 sets `StackCheckLibNull` in its DSC; AppPkg may need the same if GCC complains |

Save the log:

```bash
build ... -D BUILD_PYTHON312 2>&1 | tee ~/python312-apppkg-build.log
```

Paste the **first real error** (not the summary) when asking for help.

---

## 9. After a successful link (package + smoke)

Use the packaging script (creates `lib-dynload` and stdlib etc.):

```bash
cd "$EDK2_LIBC_PATH/AppPkg/Applications/Python/Python-3.12.13"
./create_python_pkg.sh GCC NOOPT X64 ~/py312_efi
```

`pyconfig.h` uses `PREFIX "fs0:\\EFI"`. On the Shell:

```text
fs0:
cd EFI\bin
Python312.efi
```

Smoke (FULL — matches `create_python_pkg.sh` hints):

```text
>>> import sys; print(sys.version)   # expect 3.12.13
>>> import os, json, math
>>> import zlib; import readline; import ctypes; import hashlib; import ssl
>>> hashlib.sha256(b"x").hexdigest()
>>> ssl.create_default_context()
```

`ssl.OPENSSL_VERSION_INFO` on UEFI reflects vendored **OpenSSL 1.1.1f**, not desktop 3.12.x **3.0.x**.

---

## 10. Checklist (copy into status doc when done)

```text
[ ] apt packages + nasm/gcc/python3
[ ] edk2 BaseTools + edksetup.sh
[ ] PACKAGES_PATH / EDK2_LIBC_PATH set (edk2 + edk2-libc only)
[ ] branch feature/python-3.12.13-apppkg (Phase 8 FULL)
[ ] git apply --ignore-whitespace patches/*.patch (upipe.c present)
[ ] python3 srcprep.py
[ ] frozen/deepfreeze artifacts present
[ ] build -D BUILD_PYTHON312 -t GCC
[ ] Python312.efi produced
[ ] create_python_pkg.sh + UEFI REPL smoke (zlib, readline, ctypes, hashlib, ssl)
```

---

## Common path mistakes

1. Building with only `edk2` on `PACKAGES_PATH` (AppPkg/StdLib not found).
2. Forgetting `EDK2_LIBC_PATH` (INF `-I$(EDK2_LIBC_PATH)/...` breaks).
3. Extra **`PACKAGES_PATH`** segments for `LibFFI` / `LibZlib` / OpenSSL — AppPkg **vendors** those under **`PyMod-3.12.13/Modules/`**; use **`~/src/edk2-py312`** submodules only as the **copy/reference** source (INF lists, include paths, commits), not as live EDK packages.
4. Running **`edksetup.sh`** from **`~/src/edk2`** while **`Build/`** and past green builds used **`~/src/edk2-py312/edk2`** — `create_python_pkg.sh` will not find `Python312.efi`.
5. Applying patches to the wrong libc checkout (must be the same tree as `EDK2_LIBC_PATH`).
