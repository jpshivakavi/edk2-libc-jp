# Python 3.12.13 UEFI smoke tests (VS2022 + GCC, MIN + FULL)

Single **runnable procedure** for validating a deployed **`Python312.efi`** on hardware.
Branch **`feature/python-3.12.13-vs2022`**.

This document is the **place to run tests from**. It does not restate *why* each behaviour
exists — that lives in the sources below, which remain authoritative for rationale and history:

| Topic | Document |
|-------|----------|
| Ordering, boot trace, teardown internals | [`Python312_VS2022_UEFI_Runtime_Notes.md`](./Python312_VS2022_UEFI_Runtime_Notes.md) §10–§11 |
| Phase V6 sign-off state | [`Python312_VS2022_Migration_Status.md`](./Python312_VS2022_Migration_Status.md) § Phase V6 |
| GCC vs VS2022 runtime divergence | [`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md) §11 |
| Per-run lab evidence | [`Python312_VS2022_Lab/`](./Python312_VS2022_Lab/) |
| Build + package | [`Python312_Windows_VS2022_Build_Guide.md`](./Python312_Windows_VS2022_Build_Guide.md) · [`Python312_WSL_GCC_Build_Guide.md`](./Python312_WSL_GCC_Build_Guide.md) |

---

## 0. Before you start

### 0.1 Know which image you deployed

MIN and FULL fail **differently**, so an unknown image makes results meaningless.

| Build | INF | Module dir in `Build\` | `import ssl` / `ctypes` |
|-------|-----|------------------------|--------------------------|
| **MIN** (default DSC) | `Python312_MIN.inf` | **`Python312_MIN\DEBUG\`** | **Must fail** |
| **FULL** (`-D BUILD_PYTHON312_FULL=TRUE`) | `Python312.inf` | **`Python312\DEBUG\`** | **Must succeed** |

`import ssl` failing on what you believed was FULL almost always means a MIN image was
staged, not an OpenSSL fault. Confirm the module directory before debugging anything.

### 0.2 Deploy completely

Copy the whole **`EFI\`** tree from `create_python_pkg.*` output to the FAT volume root.

**A `.efi`-only swap is valid for C-only changes.** After changing **`Lib/site.py`**,
**`readline.py`**, **`socket.py`**, **`ssl/`** or any staged stdlib, you **must** recopy
**`EFI\lib\python3.12\`** — those files are on disk, not embedded in the image.

### 0.3 Launch

```text
map -r
fs0:
cd EFI\bin
Python312.efi -h
```

---

## 1. Protocol — apply to every row below

**Each one-liner is a full cycle, not just a command:**

```text
Python312.efi -S -c "…"     → prints result
Shell>                       → returns to prompt
exit                         → must reach BIOS/setup with NO hang
```

The **Shell `exit`** step is the actual test. Historically the imports passed while
teardown hung, so a run that stops at `Shell>` has **not** been validated.

**Record:** toolchain, MIN/FULL, commit, and pass/fail per row. Log confirmed runs to
[`Python312_VS2022_Lab/`](./Python312_VS2022_Lab/) and update Phase V6 in the migration status.

---

## 2. MIN smoke

```text
Python312.efi -h
Python312.efi -S -c "import sys; print(sys.version)"
Python312.efi -S -c "print(1+1)"
Python312.efi -S -c "import os, sys, json; print('ok')"
```

| Check | Expected |
|-------|----------|
| **`-h`** | Help text, returns to Shell prompt |
| **`sys.version`** | **`3.12.13`** |
| **`import os`**, **`json`**, **`hashlib`** | **OK** |
| **`import ssl`**, **`import ctypes`** | **Must fail** — MIN has no Phase 8 |

---

## 3. FULL smoke (Phase 8)

Run **all of §2 first**, then:

```text
Python312.efi -S -c "import zlib; print(zlib.__name__)"
Python312.efi -S -c "import hashlib; print(hashlib.sha256(b'x').hexdigest()[:8])"
Python312.efi -S -c "import ctypes; print(ctypes.sizeof(ctypes.c_void_p))"
Python312.efi -S -c "import ssl; print(ssl.__file__)"
Python312.efi -S -c "import ssl; ssl.create_default_context(); print('ok')"
Python312.efi -S -c "import zlib, ssl, ctypes, hashlib; print('phase8 ok')"
```

| Check | Expected | Why it matters |
|-------|----------|----------------|
| **`ctypes.sizeof(c_void_p)`** | **`8`** on X64 | **`4`** means the LLP64 / **`UEFI_MSVC_64`** pointer-width bug — see deviations §2.2 |
| **`ssl.__file__`** | path under **`ssl/__init__.py`** | Confirms the UEFI **`Lib/ssl/`** package, not a monolithic `ssl.py` |
| **`ssl.create_default_context()`** | **`ok`** | **Primary VS2022 canary** — this hung before the OpenSSL RNG fix (deviations §11.7) |
| **`phase8 ok`** | prints | All four vendored modules coexist |

**`ssl.create_default_context()` is the row to run first when re-testing VS2022** after any
change to `rand_rdrand.nasm`, `rand_efi.c`, or OpenSSL glue.

---

## 4. REPL and teardown

Manufacturing default is the **stdio** REPL on both toolchains — **no** `PY_UEFI_READLINE`.

```text
Python312.efi
Python312.efi -S
Python312.efi -S -I
```

| Step | Expected |
|------|----------|
| Prompt | **`>>>`** via stdio |
| Trivial lines (`1+1`, `print('x')`) | OK |
| **`import readline`** with no env set | **Stub** — no error, no line editing |
| **`exit(0)`** | Returns to **`Shell>`** |
| Shell **`exit`** | Reaches firmware/setup, **no hang** |
| Relaunch **`Python312.efi`** | Banner appears again |

**Relaunch is a real test, not a formality.** A second launch that fails silently, or prints
**`py312_uefi_reentry_cleanup enter`**, means the previous interpreter never finalized —
record that separately from a Shell `exit` hang (runtime notes §7).

---

## 5. Optional pyreadline — **GCC only**

Not signed off on VS2022; VS2022 manufacturing stays stdio.

```text
set PY_UEFI_READLINE 1
Python312.efi -S
```

| Step | Expected |
|------|----------|
| **`import readline`** at **`>>>`** — **required first** | Installs `PyOS_ReadlineFunctionPointer` via edk2console |
| Up-arrow after a line | History recall |
| Tab | Completion |
| **`exit()`** → **`Shell>`** → **`exit`** | No hang |

**Setting the env var alone does nothing.** Without `import readline` the REPL is still stdio,
and arrow keys produce **`SyntaxError: invalid non-printable character U+001B`**. That is the
documented behaviour, not a regression.

---

## 6. Failure signatures

| Symptom | Likely cause | Where to look |
|---------|--------------|---------------|
| `import ssl` / `ctypes` fail on "FULL" | MIN image staged | §0.1 — check module dir |
| `ctypes.sizeof(c_void_p)` is **`4`** | `UEFI_MSVC_64` pointer width | Deviations §2.2 |
| `create_default_context()` hangs (VS2022) | OpenSSL RNG NASM ABI | Deviations §11.7 · lab `2026-08-27` |
| Imports OK, Shell **`exit`** hangs | Teardown / finalize | Runtime notes §10.5 · lab `2026-08-26` |
| Stale `site.py` / `readline.py` behaviour | `.efi`-only redeploy | §0.2 — recopy `EFI\lib\python3.12\` |
| `_PyUnicodeCheckConsistency` / `!_Py_IsImmortal` | deepfreeze missing `statically_allocated` | Runtime notes §5 |
| Arrow keys → `U+001B` | pyreadline not imported | §5 |

---

## 7. Sign-off state (2026-09-01)

| Scenario | GCC FULL | VS2022 FULL | VS2022 MIN |
|----------|----------|-------------|------------|
| Phase 8 **`-S -c`** + Shell **`exit`** | **Pass** | **Pass** | n/a |
| Stdio **`-S`** REPL + teardown | **Pass** | **Pass** | **Pass** |
| Optional pyreadline | **Pass** | **Not re-smoked** | n/a |

Reference commits: GCC **`dbc8416c`**, VS2022 **`4dec4edf`** / **`3568d02d`**.
Pin: tag **`python312-unified-full-lab-2026-09-01`**.

**Build parity does not imply runtime parity.** VS2022 enters via
**`PY_UEFI_MSVC_368_ENTRY`** on the Shell stack while GCC uses **`edk2_switch_stack`** plus a
custom IDT, so stack limits, deep recursion and fault behaviour can differ between the two
images built from the **same commit**. Re-run this document on **both** toolchains after any
shared PyMod or INF change.
