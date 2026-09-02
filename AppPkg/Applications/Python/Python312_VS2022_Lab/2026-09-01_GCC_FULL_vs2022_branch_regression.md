# Lab sign-off: GCC FULL on `feature/python-3.12.13-vs2022` (2026-09-01)

**Branch:** `feature/python-3.12.13-vs2022` · **Commit:** **`dbc8416c`** (includes **`edk2main.c`** GCC **`py312_boot_print_ascii`** fix)  
**Build:** WSL Ubuntu 20.04, **`build -t GCC -b NOOPT`**, **`BUILD_PYTHON312` + `BUILD_PYTHON312_FULL=TRUE`**  
**Layout:** `PACKAGES_PATH=$HOME/src/edk2-py312/edk2:$EDK2_LIBC_PATH` (same as historical GCC AppPkg work)  
**Package:** `create_python_pkg.sh GCC NOOPT X64`  
**Image:** FULL `Python312.efi` — GCC entry (**`edk2_switch_stack` + `py_install_idt`**), not MSVC 368 path

---

## Build notes (first GCC FULL on vs2022 tip)

| Issue | Fix |
|-------|-----|
| **`py312_boot_print_ascii` redefinition** | **`dbc8416c`**: define in **`edk2main.c`** only when **`PY_UEFI_BOOT_TRACE`** (MSFT-only in INF); GCC uses **`py312boot.h`** inline stub |
| **Missing `frozen_modules/*.h`** | Copy/regen per [`Python312_WSL_GCC_Build_Guide.md`](../Python312_WSL_GCC_Build_Guide.md) §6 (not committed in git) |

---

## Console output vs VS2022

GCC builds **do not** pass **`-DPY_UEFI_BOOT_TRACE=1`** in **`Python312.inf`** (MSFT only). Expect **`Python312: UefiMain`**, **`Python312: enter main`**, then script output — **not** the long MSVC boot ladder. Normal; see deviations **§11** / migration status **§ Boot trace**.

---

## Manufacturing smoke (hardware — passed 2026-09-01)

Each: **`-S -c "…"`** → **`Shell>`** → **`exit`** → BIOS/setup, **no hang**.

| Command | Result |
|---------|--------|
| `import sys; print('ok')` | OK |
| `import zlib; print(zlib.__name__)` | OK |
| `import hashlib; print(hashlib.sha256(b'x').hexdigest()[:8])` | OK |
| `import ctypes; print(ctypes.sizeof(ctypes.c_void_p))` | **`8`** (X64) |
| `import ssl; print(ssl.__file__)` | **`.../ssl/__init__.py`** (UEFI package) |
| `import ssl; ssl.create_default_context(); print('ok')` | OK |
| `import zlib, ssl, ctypes, hashlib; print('phase8 ok')` | OK |

---

## Default stdio REPL (GCC + VS2022 FULL — passed 2026-09-01)

Manufacturing default: **no** **`PY_UEFI_READLINE`**.

```text
Python312.efi -S
```

| Step | Result |
|------|--------|
| Trivial lines at **`>>>`** | OK |
| **`exit(0)`** → **`Shell>`** → **`exit`** | **No hang** — **GCC** and **VS2022** FULL images |

---

## Optional pyreadline (GCC only — passed 2026-09-01)

**Canonical write-up:** migration status **§ UEFI REPL / pyreadline** (build/smoke tables in **§ GCC FULL regression — build and smoke**).

```text
set PY_UEFI_READLINE 1
Python312.efi -S
```

| Step | Result |
|------|--------|
| **`import readline`** at **`>>>`** (before arrow keys) | Required — installs **`PyOS_ReadlineFunctionPointer`** via **edk2console** |
| Up-arrow after typing a line | History recall **OK** |
| Tab | Completion **OK** |
| **`exit()`** → **`Shell>`** → **`exit`** | **No hang** |

**Failure mode (documented):** with env set but **without** **`import readline`**, up-arrow on stdio REPL → **`SyntaxError: invalid non-printable character U+001B`** (ESC from escape sequence).

**VS2022:** pyreadline opt-in **not** re-tested on this branch after Session 10 policy.

---

## Meaning

**VS2022-track PyMod** (ssl package, OpenSSL RNG, teardown) does **not** break **GCC FULL** manufacturing on the **same branch**. Host GCC toolchain was **unchanged** from prior AppPkg work (toolchain upgrade deferred).

**V6 manufacturing runtime** (Phase 8 **`-S -c`** + stdio **`-S`** on **GCC** and **VS2022** FULL) — **closed** 2026-09-01. **Next:** pre-upstream-push cleanup, optional unified git tag, **V7** CI.
