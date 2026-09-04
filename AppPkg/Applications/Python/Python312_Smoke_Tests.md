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

## 5. pyreadline / readline

**Signed off on GCC only** (2026-09-01). VS2022 manufacturing stays stdio; VS2022 pyreadline
historically hung Shell `exit` and the next launch, and has **not** been re-labbed.

### 5.1 How the opt-in actually works — two independent switches

Getting this wrong is the main source of confusing results, because the wrong combination
fails **silently** into stub mode rather than erroring.

| Switch | Kind | Gates |
|--------|------|-------|
| **`PY_UEFI_READLINE`** | **runtime** shell env | Whether `readline.py` loads the **real** pyreadline path at all. Without it, `import readline` returns a **stub** |
| **`PY_UEFI_PYREADLINE`** | **compile-time** `CC_FLAGS` | Whether `pymain_import_readline` **auto-imports** readline at REPL start, and whether `UefiMain` opens **ConInEx** eagerly |

Two consequences worth internalising:

- **The env var alone never wires the REPL.** `main.c` skips auto-import under `UEFI_C_SOURCE`,
  and `site.py` skips `enablerlcompleter` on `os.name == 'uefi'`. You must type
  **`import readline`** yourself.
- **The compile flag alone is also not enough.** It auto-imports `readline`, but `readline.py`
  still checks the env var — so without it you auto-import the **stub**. The old "always on"
  behaviour needs **both**.

**A rebuild is not required** for the runtime opt-in: `edk2_console_ensure_input()` opens
ConInEx lazily, so the env + manual `import` path works on a stock FULL image.

### 5.2 Default is stub — verify this on both toolchains

This is a **manufacturing safety** check: scripts that `import readline` must not wire up the
console. Run with **`PY_UEFI_READLINE` unset**.

```text
Python312.efi -S -c "import readline; print(type(readline.rl).__name__)"
Python312.efi -S -c "import readline, sys; print('pyreadline' in sys.modules, 'edk2console' in sys.modules)"
Python312.efi -S -c "import readline; print(readline.rl.disable_readline, readline.get_line_buffer())"
```

| Check | Expected (stub) |
|-------|-----------------|
| `type(readline.rl).__name__` | **`_ReadlineStub`** |
| modules loaded | **`False False`** — neither `pyreadline` nor `edk2console` imported |
| `disable_readline`, `get_line_buffer()` | **`True None`** — every API is a no-op `dummy` |
| `hasattr(readline, 'GetOutputFile')` | **`False`** (only defined on the real path) |

**`_ReadlineStub` is the pass condition.** Seeing `Readline` here means the env var leaked in
(see §5.6) and your "default" runs are not testing the default.

### 5.3 Non-interactive opt-in check — cheapest VS2022 canary

This exercises the real pyreadline path, hook install and teardown **without** needing arrow
keys, so it is the fastest way to test the risky path:

```text
set PY_UEFI_READLINE 1
Python312.efi -S -c "import readline, sys; print(type(readline.rl).__name__, 'edk2console' in sys.modules)"
```

| Check | Expected |
|-------|----------|
| Output | **`Readline True`** |
| Shell **`exit`** afterwards | **No hang** |
| Relaunch | Banner normal |

Importing real `readline` runs `console.install_readline(rl.readline)` and
`rl.read_history_file()`, so this single command covers hook install plus the
`edk2_console_detach_readline` teardown. **On VS2022 this is exactly the path that used to
hang** — run it before the interactive test, not after.

### 5.4 Interactive opt-in — GCC signed off

```text
set PY_UEFI_READLINE 1
Python312.efi -S
```

| Step | Expected |
|------|----------|
| **`import readline`** at **`>>>`** — **required, before any arrow key** | Installs `PyOS_ReadlineFunctionPointer` via `edk2console` |
| Type a line, Enter | Accepted |
| Up-arrow | History recall |
| Tab | Completion (`rlcompleter`, bound via `parse_and_bind("tab: complete")`) |
| **`exit()`** → **`Shell>`** → **`exit`** | Reaches firmware, **no hang** |
| Relaunch | Banner normal |

### 5.5 Expected non-bugs

| Symptom | Cause |
|---------|-------|
| `SyntaxError: invalid non-printable character U+001B` on arrow keys | Env set but **`import readline`** not run — REPL is still stdio and `U+001B` is the ESC of the escape sequence |
| `SystemError: EDK2 input console unavailable (EFI status …)` | `edk2_console_ensure_input()` could not open ConInEx |
| `SystemError: EDK2 input console is closed. You are running on EFIv1 …` | No `SimpleTextInputEx` on this firmware |
| No line editing despite env set | Value not recognised — see §5.6 |

### 5.6 Two traps

**Value parsing is exact-match and case-sensitive.** `readline.py` accepts only:

```text
1   yes   true   YES   TRUE
```

**`set PY_UEFI_READLINE True`** (mixed case), `on`, or `enabled` all fall through to **stub
mode silently** — no warning, no error, just no line editing. Verify with §5.2 rather than
assuming the variable took effect.

**Shell env persists across launches.** Leaving the variable set silently invalidates every
later stdio-default test, including §2–§4. Clear it when done:

```text
set -d PY_UEFI_READLINE
```

Then re-run §5.2 and confirm you get **`_ReadlineStub`** again before trusting default-mode
results.

### 5.7 Compile-time variant (development only)

Add **`-DPY_UEFI_PYREADLINE=1`** to `GCC:` or `MSFT:` `CC_FLAGS` in `Python312.inf`. This
auto-imports readline at startup and opens ConInEx in `UefiMain`. **Not** manufacturing
default, and it still needs `PY_UEFI_READLINE` set to get the real module. Re-run §5.3, §5.4
and the teardown/relaunch checks after any such rebuild.

Note `edk2console` is compiled into **both** `Python312.inf` and `Python312_MIN.inf` and
registered as a builtin in `config.c`, so `import edk2console` succeeds regardless of these
flags — its presence proves nothing about whether readline is wired.

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
| Arrow keys → `U+001B` | `import readline` not run | §5.5 |
| `readline.rl` is `Readline` when testing defaults | `PY_UEFI_READLINE` left set from an earlier run | §5.6 — `set -d PY_UEFI_READLINE` |
| Env set but still no line editing | Value not exact-match (`True` ≠ `true`) | §5.6 |
| Shell `exit` hangs only after a readline run | `edk2_console_detach_readline` teardown | §5.3 · runtime notes §10 |

---

## 7. Sign-off state (2026-09-01)

| Scenario | GCC FULL | VS2022 FULL | VS2022 MIN |
|----------|----------|-------------|------------|
| Phase 8 **`-S -c`** + Shell **`exit`** | **Pass** | **Pass** | n/a |
| Stdio **`-S`** REPL + teardown | **Pass** | **Pass** | **Pass** |
| `import readline` stays stub, no env (§5.2) | Observed safe | Observed safe | **Pass** (Session 10) |
| Interactive pyreadline opt-in (§5.4) | **Pass** | **Not re-smoked** | n/a |

The **explicit `_ReadlineStub` / `sys.modules` assertions in §5.2 and the non-interactive
opt-in check in §5.3 are new** — prior runs confirmed stub `import readline` did not break
teardown, but never asserted which code path had loaded. Treat them as unrun.

Reference commits: GCC **`dbc8416c`**, VS2022 **`4dec4edf`** / **`3568d02d`**.
Pin: tag **`python312-unified-full-lab-2026-09-01`**.

**Latest regression — VS2022 + GCC FULL @ `3afa03f5` (2026-09-04):** Phase 8 spot-check
(**`sys.version`**; **`zlib, ctypes, hashlib`**; **`ssl.create_default_context()`**) plus Shell
**`exit`** and relaunch — **pass on both toolchains**, from the **same code state** (later
commits are docs-only). First hardware run on either toolchain **after the PyMod-3.12.13
consolidation**, so it clears the relocated frozen/deepfreeze artifacts on **both** entry paths.
Open gap: **`ctypes.sizeof(c_void_p)`** was not asserted, so the LLP64 canary is unexercised —
[`Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md`](./Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md).

**Build parity does not imply runtime parity.** VS2022 enters via
**`PY_UEFI_MSVC_368_ENTRY`** on the Shell stack while GCC uses **`edk2_switch_stack`** plus a
custom IDT, so stack limits, deep recursion and fault behaviour can differ between the two
images built from the **same commit**. Re-run this document on **both** toolchains after any
shared PyMod or INF change.
