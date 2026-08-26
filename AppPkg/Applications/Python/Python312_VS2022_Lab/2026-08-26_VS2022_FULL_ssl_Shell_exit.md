# Lab sign-off: VS2022 FULL — `import ssl` and Shell `exit` (2026-08-26)

**Branch:** `feature/python-3.12.13-vs2022`  
**Image:** FULL `Python312.efi` (`BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY=1`)  
**Environment:** UEFI Shell, same session, `Shell>` → **`exit`** → BIOS/setup after each one-liner.

---

## Symptom (before fix)

| Test | Python run | Shell `exit` → BIOS |
|------|------------|---------------------|
| `import sys` | OK | Usually OK |
| `import _ssl`, `socket` alone | OK | OK |
| **`import ssl`** (even once) | OK (`ok` printed, back to `Shell>`) | **Hang** |

Boot trace often reached **`before return from UefiMain`**; hang was **after** Python returned to Shell, not during `import ssl`.

**Misleading paths tried:** MSVC skip-leak of `_ssl`/`ssl` module clear, partial monolithic `ssl.py` UEFI branches, post-`UefiMain` console/OpenSSL disarm (regressed `import sys` in some builds).

**Reference:** GCC FULL with `edk2_switch_stack` + normal teardown did not show this pattern.

---

## Root cause (working theory, validated in lab)

1. **Pure-Python `Lib/ssl.py`** at import: `_IntEnum._convert_`, large class graph, optional `Lib/socket.py` — leaves state that breaks **Shell** teardown on VS2022 even when **`Py_FinalizeEx`** completes.
2. **MSVC-only “skip teardown”** for `_ssl`/`ssl`/GC/atexit could leave extension module state inconsistent with a “clean” interpreter shutdown.
3. **368-style entry** (`ShellCEntryLib` on Shell stack) is still required on VS2022 hardware; do not remove it to fix ssl.

---

## Fix summary

| Layer | Change |
|-------|--------|
| **Stdlib** | `Lib/ssl/` package: on `os.name == 'uefi'`, load **`_uefi_min.py`** only (`_ssl` + minimal `create_default_context`); full stdlib in **`_stdlib.py`** for non-UEFI |
| **Teardown** | Align MSVC with GCC: normal `_PyModule_Clear`, `_ssl`/`socket` `m_clear`, GC, `_PyAtExit_Call` (no MSVC skip-leak) |
| **Post-finalize** | `py312_uefi_phase8_after_finalize()` when `_ssl` loaded: `ERR_clear_error()` + `edk2_console_handoff_to_shell()` |
| **Source layout** | UEFI edits in **`PyMod-3.12.13/`**; **`Python312.inf`** builds `PyMod-.../Modules/main.c`, `Python/pylifecycle.c`, `Modules/_ssl.c`; run **`srcprep.py`** before package (also in `create_python_pkg.*`) |

Verify on stick:

```text
Python312.efi -S -c "import ssl, sys; print(ssl.__file__); print(type(ssl.Purpose.SERVER_AUTH).__name__)"
```

Expect path under **`...\ssl\__init__.py`** and **`str`** for Purpose (not `<enum 'Purpose'>` from old monolithic `ssl.py`).

---

## Manufacturing smoke (same session — passed 2026-08-26)

Each: `-S -c "…"` → **`Shell>`** → **`exit`** → BIOS, **no hang**.

| Command | Notes |
|---------|--------|
| `import sys; print('ok')` | Baseline |
| `import ssl; print('ok')` | Former failure |
| `import ssl; print(ssl.create_default_context())` | SSLContext + Purpose |
| `import hashlib; print(hashlib.sha256(b'x').hexdigest()[:8])` | **`2d711642`** |
| `import ctypes; print(ctypes.sizeof(ctypes.c_void_p))` | **`8`** (X64) |

---

## Deploy checklist

1. Rebuild FULL: `build -t VS2022 -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE`
2. **`python srcprep.py`** in `Python-3.12.13/` (or use `create_python_pkg.bat` / `.sh`, which runs it)
3. Stage **`EFI/bin/Python312.efi`** + **`EFI/lib/python3.12/`** — whole **`ssl/`** directory, not `.efi` alone
4. On device: `map -r`, run smokes above

---

## Debug playbook (Shell exit hang)

1. **Bisect imports:** `sys` → `_ssl` → `socket` → **`ssl`** — only `ssl` implicated pure-Python layer.
2. **Boot trace** (`PY_UEFI_BOOT_TRACE=1`): covers Python + `UefiMain`, **not** Shell `exit`.
3. If **`import ssl` works** but Shell **`exit` hangs**: check stdlib deploy (`ssl/` package vs old `ssl.py`); confirm PyMod overlay in package script.
4. If hang **before** `Shell>`: suspect **`Py_FinalizeEx`** / GC / `_ssl` clear — compare boot trace ladder in [runtime notes §7](../Python312_VS2022_UEFI_Runtime_Notes.md).

---

## Do not regress

- **`PY_UEFI_MSVC_368_ENTRY=1`** on VS2022 FULL/MIN for current hardware.
- **`OPENSSL_atexit` / `OPENSSL_cleanup`** no-op on **`OPENSSL_SYS_UEFI`** (OpenSSL init in `PyMod-.../Modules/openssl/crypto/init.c`).
- Edit UEFI deltas in **`PyMod-3.12.13/`**, then **`srcprep.py`** for `.py` / `.h` overlays.

## UEFI `ssl` — not full stdlib

The minimal package is documented in **[Migration Status § UEFI ssl scope](../Python312_VS2022_Migration_Status.md#uefi-ssl-module-full--scope-and-limitations)** (`SSLSocket`/`SSLObject` absent at import; use **`_ssl._SSLContext`** and explicit CA paths).
