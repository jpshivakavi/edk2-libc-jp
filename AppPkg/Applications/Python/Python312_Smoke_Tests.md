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

**GCC only.** GCC passes phases 1–5 at `3afa03f5` (2026-09-07). **VS2022 hangs Shell `exit` at
phases 2 and 3 — confirmed 2026-09-07 at the same code state**, so VS2022 manufacturing stays
stdio. Importing the real `readline` module is enough to trigger it; no interactive REPL is
needed. Run §5 on VS2022 only when a power-cycle is acceptable.

### 5.0 Execution order — run this way

**Do GCC first, VS2022 second.** GCC is the signed-off path, so it validates the procedure
itself; if a step misbehaves on GCC, the procedure or the stick is wrong, not the port. VS2022
is the unproven path — **be ready to power-cycle**, and run it when a hard reset is acceptable.

Within each toolchain, go in this order. Each phase is a prerequisite for trusting the next.

| Phase | What | Section | Why this order |
|------:|------|---------|----------------|
| **0** | Pre-flight: FULL image, readline staged, env **unset** | §5.0.1 | A missing `readline.py` gives `ModuleNotFoundError`, not a stub — different failure |
| **1** | Confirm **stub** is the default | §5.2 | Proves the env is genuinely clear before you enable anything |
| **2** | Non-interactive opt-in | §5.3 | Covers hook install **and** teardown with no keyboard interaction — cheapest canary |
| **3** | Interactive: history + Tab | §5.4 | Only meaningful once phase 2 tears down cleanly |
| **4** | Optional: confirm the documented non-bugs | §5.5, §5.6 | Turns "it didn't work" into a known cause |
| **5** | **Clear the env**, re-confirm stub | §5.6 | Mandatory — plain `set` is **non-volatile** and survives reboot |

**If VS2022 hangs at phase 2 or 3:** that is the historical failure, not a new defect. Power-
cycle, record the phase and the last line printed, and stop — do not carry on to phase 3 after
a phase 2 hang. Leave VS2022 pyreadline marked unsigned-off, and check `set` on the next boot
so the env does not linger (use **`set -v`** when enabling, so a power-cycle clears it — §5.6).

#### 5.0.1 Pre-flight, per stick

```text
ls EFI\lib\python3.12\readline.py
ls EFI\lib\python3.12\pyreadline
set
```

| Check | Expected |
|-------|----------|
| `readline.py` | present — **without it `import readline` raises `ModuleNotFoundError`** |
| `pyreadline\` | present (~38 files, incl. `console\edk2.py`) |
| `set` output | **no `PY_UEFI_READLINE`** line |
| Image | **FULL** — §0.1 |

Both `create_python_pkg.bat` and `.sh` stage these from
`PyMod-3.12.13/Modules/readline/`. If the directory was missing at package time the script
prints **`Warning: … missing; import readline will fail`** — re-package rather than testing on
a stick that only had the `.efi` refreshed (§0.2).

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
set -v PY_UEFI_READLINE 1
Python312.efi -S -c "import readline, sys; print(type(readline.rl).__name__, 'edk2console' in sys.modules)"
```

**`-v` makes the variable volatile** so a forced power-cycle clears it — see §5.6.

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
set -v PY_UEFI_READLINE 1
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
set
Python312.efi -S -c "import readline; print(type(readline.rl).__name__)"
```

Confirm `set` no longer lists it **and** that you get **`_ReadlineStub`** again before trusting
default-mode results.

**`set` without `-v` creates a NON-volatile variable that survives reboot.** In `ShellPkg`,
`Set.c` passes the `-v` flag straight through as the `Volatile` argument of
`ShellSetEnvironmentVariable`, whose contract is *"non-volatile (FALSE) or volatile (TRUE)"*:

| Command | Volatile | Survives reboot |
|---------|----------|-----------------|
| `set PY_UEFI_READLINE 1` | FALSE | **Yes** — written to NVRAM |
| **`set -v PY_UEFI_READLINE 1`** | TRUE | **No** — cleared on reset |

**Prefer `set -v` for these tests.** It self-clears on reset, which matters because a VS2022
pyreadline hang forces a power-cycle — with `-v` the variable is gone on the next boot instead
of silently persisting into later default-mode runs. Delete either kind with:

```text
set -d PY_UEFI_READLINE
```

If you used plain `set`, also re-check `set` after a reboot to confirm it really went.

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
| Shell `exit` hangs after a readline run (VS2022) | **Not a readline bug.** `import logging` **alone** hangs Shell `exit` — no readline, no `edk2console`, no console I/O. pyreadline only reaches it via `pyreadline/logger.py` | lab `2026-09-07_VS2022_FULL_pyreadline_hang` |
| Shell `exit` hangs after importing pure-Python stdlib (VS2022) | Under investigation. Threshold measured at **43–48 modules** (`len(sys.modules)`: 23 and 42 clean; 48 and 65 hang). `re`, raw heap footprint, read-only file cycles, teardown, `edk2console` and `_thread` are **all ruled out**. **ROOT CAUSE:** VS2022 sets `PY_UEFI_MSVC_368_ENTRY`, so `edk2main.c` returns before the stack switch and Python runs on the **~128 KB firmware stack**; GCC gets a **64 MB** stack. Deep import chains overflow it and corrupt memory outside the image, so teardown looks clean and only BDS hangs | same lab note, "ROOT CAUSE" |
| `MemoryError: stack overflow` on a deep import (VS2022) | **Expected and correct** since the 2026-09-08 `PyOS_CheckStack` fix (`c3819602`) — the guard has a real bound and trips before the firmware stack is breached, so Shell `exit` stays clean on `-S -c` runs. Message is lowercase, from `Objects/object.c` | lab `2026-09-08_VS2022_FULL_stackcheck_fix` |
| Deep import in the **interactive REPL**, then Shell `exit` hangs (VS2022) | **Open, and NOT a stack overflow** — measured depths of the clean and hanging runs differ by 256 bytes. It tracks the **exit route**: REPL `exit()` raises `SystemExit`, which routes `PyErr_Print()` → `handle_system_exit()` → `Py_Exit()` and **never returns through `Py_RunMain()`/`main()`**. Whether the trigger is that route or merely having read interactive stdin is still confounded — run `-c "import sys; sys.exit(0)"` to separate them | lab `2026-09-08_VS2022_FULL_interactive_exit_leak` |
| Interactive session's teardown trace is missing `after Py_BytesMain` / `after main()` | Normal and expected on the `Py_Exit()` route — it longjmps straight to `ShellCEntryLib`. Use their absence as the **marker that the `Py_Exit()` route was taken** | same lab note |

---

## 7. Sign-off state (2026-09-01)

| Scenario | GCC FULL | VS2022 FULL | VS2022 MIN |
|----------|----------|-------------|------------|
| Phase 8 **`-S -c`** + Shell **`exit`** | **Pass** | **Pass** | n/a |
| **`ctypes.sizeof(c_void_p)`** == **`8`** (§3) | **Pass** (09-04) | **Pass** (09-04) | n/a |
| Four modules, one process (§3 `phase8 ok`) | **Pass** (09-04) | **Pass** (09-04) | n/a |
| Stdio **`-S`** REPL + teardown | **Pass** | **Pass** | **Pass** |
| Stub default **asserted** (§5.2) | **Pass** (09-07) | **Pass** (09-07) | Observed safe (Session 10) |
| Non-interactive opt-in (§5.3) | **Pass** (09-07) | **HANG** on Shell `exit` (09-07) | n/a |
| Interactive pyreadline opt-in (§5.4) | **Pass** (09-07) | **HANG** on Shell `exit` (09-07) | n/a |
| Env cleanup restores stub (§5.6) | **Pass** (09-07) | **Pass** (09-07) | n/a |

**GCC ran §5.0 phases 1–5 in full on 2026-09-07 at `3afa03f5`** — the same code state as the
`python312-unified-full-lab-2026-09-04` pin, so GCC pyreadline no longer rests on the pre-PyMod
2026-09-01 run. That was also the **first hardware run of the §5.2/§5.3 assertions on any
toolchain**; earlier sign-offs showed nothing broke but never asserted which path had loaded.
[`Python312_VS2022_Lab/2026-09-07_GCC_FULL_pyreadline_phases.md`](./Python312_VS2022_Lab/2026-09-07_GCC_FULL_pyreadline_phases.md).

**VS2022 pyreadline was run on 2026-09-07 and the hang reproduces at `3afa03f5`.** Phases 1 and
5 (stub) are clean on the same image; phases 2 and 3 both leave Shell `exit` hanging. Critically,
**phase 2 hung with no interactive REPL and no arrow keys** — importing the real `readline`
module is by itself sufficient, so this is not a line-editing problem as previously described.
Manufacturing stdio policy stands.

**The boot trace was captured the same day and moves the fault out of Python entirely.** The
ladder runs to completion — `Py_FinalizeEx`, `Py_BytesMain`, `main()`, `edk2_free_environ`,
`before return from UefiMain` — and the Shell prompt returns; only the Shell's own `exit` to
firmware hangs afterwards. `stop_timer: already off` on both detach calls proves the 1 ms
periodic timer was **never created**, so that long-suspected cause is already mitigated and this
is a different one. Do not spend further effort on Python-side teardown —
[`Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md`](./Python312_VS2022_Lab/2026-09-07_VS2022_FULL_pyreadline_hang.md)
carries the transcribed ladder and the ruled-out list.

**The bisect ran the same day and this turned out not to be a readline bug at all.**
`import edk2console` alone exits cleanly; `import pyreadline.rlmain` hangs — yet that import
constructs no `Console` and installs no hook, since `rl = Readline()` and
`console.install_readline(...)` both live in `readline.py`, not in the package `__init__`.
Then the decisive control:

```text
Python312.efi -S -c "import logging; print('ok')"     ->  ok, then Shell exit HANGS
```

**No readline, no `edk2console`, no console I/O.** Both hanging cases share exactly one
heavyweight import — `logging`, which pyreadline reaches via `pyreadline/logger.py` — while every
clean case (phase 8 C extensions, the phase 1 stub, `edk2console` alone) avoids it. Threading is
ruled out by inspection too: the build's pthread layer is `dummy_pthread.c`, pure static-array
bookkeeping with no `gBS` calls. **So §5's pyreadline rows below are not evidence against
pyreadline** — they were the first symptom of a general "pure-Python import hangs Shell `exit`"
defect on VS2022.

**`import json` then hung as well, which clears `logging` too.** `json` pulls neither `logging`
nor `threading`; what it shares is **`re`** (`json/decoder.py`, `encoder.py` and `scanner.py` all
`import re`, as does `logging/__init__.py:26`). `re` is now perfectly correlated with the hang.
This also explains why the phase 8 tests always looked clean — this port's `ssl/__init__.py` is
the UEFI-minimal variant that imports only `os`, so **§3 barely exercises the pure-Python stdlib**.
**All three mechanism runs then came back clean** — `import re`, a bare 16 MB `bytearray`, and 50
`open`/`close` cycles. So `re` itself is not sufficient, **raw heap footprint is not the metric**
(16 MB is far more than ~20 modules use), the file layer does not leak per open, and `_thread` is
cleared empirically too, since `re` pulls `functools`, whose line 21 is `from _thread import
RLock`. The margin is now razor thin: `import re` loads 15+ modules cleanly, while `import json`
adds only about five and hangs. Remaining candidates are allocation count / pool fragmentation, a
count-based limit, or C-stack depth in the import machinery.

**The threshold was then measured on 2026-09-08 at 43–48 modules** — `len(sys.modules)` is 23 at
the `-S` baseline and 42 after `import re` (both clean), versus 48 after `import json` and 65
after `import logging` (both hanging). Not a descriptor limit, since `OPEN_MAX` is 255. The
leading suspect is now **`.pyc` writing**: `create_python_pkg.sh` excludes `__pycache__`, so the
volume ships with no bytecode and **every stdlib import does a `__pycache__` mkdir, a temp-file
write, and a rename on FAT**. The earlier 50-cycle file test used `'rb'` and so never exercised
that path.

**`-B` then still hung, so bytecode writing is ruled out too** — along with `__pycache__` mkdir,
`_write_atomic` and `_os.replace`.

**ROOT CAUSE, found 2026-09-08 from the observation that GCC never hangs.** The toolchains take
different entry paths in `PyMod-3.12.13/efi/src/edk2main.c`. VS2022 images set
`PY_UEFI_MSVC_368_ENTRY` (on the MSFT `CC_FLAGS` of both `Python312.inf` and `Python312_MIN.inf`),
which makes the function **return at `:212`, before the stack switch at `:228`** — so the whole
interpreter runs on the **UEFI firmware stack**, on the order of 128 KB. GCC falls through and
gets `PY_UEFI_DEFAULT_STACK_SIZE`, **64 MB** (`edk2stack.h:5`). Deep import chains overflow the
firmware stack and corrupt memory *below* it, outside the image — which is exactly why Python's
teardown ladder is flawless and only the Shell's `exit` into BDS hangs. `PyOS_CheckStack()`
(`edk2main.c:249`) cannot catch it, because `g_edk2_globals.stack` is assigned at `:216` — after
the early return — so it is NULL and the check always answers "stack is fine" despite
`USE_STACKCHECK 1` being set. **Peak C-stack depth, not module count, is the real variable**;
`json` hangs where `re` does not because a package nests about two levels deeper. Fixes and a
depth-versus-count confirmation test are in the lab note.

The 2026-09-08 fix gives `PyOS_CheckStack()` a real bound and **clears the non-interactive case**.
The interactive REPL kept hanging for a **second, unrelated reason — still open.** It is not a
stack overflow: the clean and hanging runs bottom out 256 bytes apart, inside the same 4 KB page.
What it tracks is the **exit route** — REPL `exit()` goes through `Py_Exit()` and never returns
through `Py_RunMain()`/`main()`. The detach now also runs from `Py_FinalizeEx()`, which is correct
hardening for `PY_UEFI_PYREADLINE` builds, but it is a **no-op** for a stdio REPL because
`console_in` is NULL there, so it does not fix this.

**Do not lower `PY_UEFI_FIRMWARE_STACK_BUDGET`.** The high-water measurement shows `import re`
clears `limit` by only 6 240 bytes, so 96 KB is nearly too tight, not too generous. The `used`
figures are large (~90 KB for `import re`) because these are `-b NOOPT` builds.

Reference commits: GCC **`dbc8416c`**, VS2022 **`4dec4edf`** / **`3568d02d`**.
Pin: tag **`python312-unified-full-lab-2026-09-01`**.

**Latest regression — VS2022 + GCC FULL @ `3afa03f5` (2026-09-04):** §3 Phase 8 including
**`ctypes.sizeof(c_void_p)` → `8`** and **`phase8 ok`**, plus Shell **`exit`** and relaunch —
**pass on both toolchains**, from the **same code state** (later commits are docs-only). First
hardware run on either toolchain **after the PyMod-3.12.13 consolidation**, so it clears the
relocated frozen/deepfreeze artifacts on **both** entry paths.

**Every check guarding a known historical failure is green at this code state** — OpenSSL RNG
hang, `socket.py`/`selectors` teardown, LLP64 pointer width, deepfreeze static strings, and
finalize/re-entry. Still open: §2 baseline rows, itemised §4 REPL rows, and all of §5 —
[`Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md`](./Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md).

**Build parity does not imply runtime parity.** VS2022 enters via
**`PY_UEFI_MSVC_368_ENTRY`** on the Shell stack while GCC uses **`edk2_switch_stack`** plus a
custom IDT, so stack limits, deep recursion and fault behaviour can differ between the two
images built from the **same commit**. Re-run this document on **both** toolchains after any
shared PyMod or INF change.
