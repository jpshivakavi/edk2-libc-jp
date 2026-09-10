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

### 1.1 Read the stack boot trace once per build

> **`PY_UEFI_BOOT_TRACE` is OFF as of 2026-09-09, so a stock image of either toolchain prints
> nothing here.** The trace code is still in the tree, `#ifdef`-guarded. Re-enabling takes **two**
> edits, and either alone does nothing: add `/DPY_UEFI_BOOT_TRACE=1` to `MSFT:*_*_*_CC_FLAGS` (or
> `-DPY_UEFI_BOOT_TRACE=1` to `GCC:*_*_*_CC_FLAGS`) in `Python312.inf` / `Python312_MIN.inf`, **and**
> uncomment the `[BuildOptions]` block in `AppPkg/AppPkg.dsc` — that one is DSC-level, so EDK2
> appends it to every module including `Python312` itself. Rebuild, then revert both before
> committing. **§1.1 is now a diagnostic procedure, not a per-build step** — do it when
> investigating stack depth or an entry-path stop, not on every sweep. What remains unconditional on
> a stock image is the error paths and the CPU-fault report (§5.8).

**Confirming the reverse — that a stock image is silent —** is worth one command, because two boot
lines were historically *not* behind the macro and each had to be gated by hand (migration status
item 35). They live in the binary as **different encodings**, and checking only one of them is how
the second survived a round of testing: `Python312: UefiMain` is a wide literal from `Print(L"…")`,
`Python312: enter main` is narrow, from `fputs()` to stdout. Both must be absent:

```powershell
$b = [IO.File]::ReadAllBytes($p)      # $p = path to Python312.efi
"UefiMain (wide)   : " + [Text.Encoding]::Unicode.GetString($b).Contains("Python312: UefiMain")
"enter main (ascii): " + [Text.Encoding]::ASCII.GetString($b).Contains("Python312: enter main")
```

On hardware, `Python312.efi -S -c "print(1+1)"` should emit `2` and nothing else.

Any run of a `PY_UEFI_BOOT_TRACE` image prints this on the way out of `UefiMain`:

```text
Python312 boot: switched stack min_rsp=<hex> limit=<hex> size=<hex>
```

| Field | Expected | Meaning if wrong |
|-------|----------|------------------|
| `size` | **`4000000`** (64 MB) | Not on the dedicated stack |
| `min_rsp` | **well above** `limit` | Close to `limit` means the run nearly overflowed and passing was luck |
| `limit` | `stack` base + 8 KB margin | — |

`min_rsp` is the deepest `rsp` **sampled by `PyOS_CheckStack()`**, so it under-reports the true
peak — treat it as a floor on headroom, not a measurement. Depth used is roughly
`(base + size - 0x200) - min_rsp`.

**Boot stopping at `before ShellCEntryLib` on VS2022** means
**`MSFT:*_*_*_NASM_FLAGS = -DPY_UEFI_MS_ABI`** is missing from the INF: `edk2_switch_stack` then
sets `rsp` from whatever is in `rdi`/`rsi`. See deviations §11.1.

**Reference measurement — VS2022 FULL `-b NOOPT`, `32c63ba1`, 2026-09-08:**

```text
Python312.efi -S -c "import sys; print(sys.version)"
Python312 boot: switched stack min_rsp=6486B0A8 limit=60877038 size=4000000
```

| Derived | Value |
|---------|-------|
| Stack base / top | `0x60875038` / `0x64875038` |
| `rsp` after switch (`top - 0x200`) | `0x64874E38` |
| **Depth used** | **`0x9D90` = 40 336 B ≈ 39.4 KB** |
| Headroom above `limit` | 63.95 MB |
| Fraction of 64 MB used | **0.06 %** |

**`size` is confirmed independently**, not just read off the line: `min_rsp` lies 63.95 MB above
`limit`, and `min_rsp` is a real observed `rsp` on the switched stack, so the allocation must be
~64 MB regardless of what the (screen-truncated) `size` field showed.

**This is also the quantitative proof of the old root cause.** `import sys` is the *shallowest*
useful run and it still needs **39.4 KB — 41 % of the retired 96 KB firmware budget**. Earlier
high-water work measured `import re` at ~90 KB, clearing `limit` by only 6 240 B. So on the
firmware stack a deeper import had no chance, and the old "do not lower
`PY_UEFI_FIRMWARE_STACK_BUDGET`, 96 KB is nearly too tight" note was right.

Depths are large because these are `-b NOOPT` builds; expect less under `RELEASE`.

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
| **`import ssl`**, **`import ctypes`** | **Must fail** — MIN has no Phase 8. Either signature counts, see below |

**Which name the failure mentions depends on the `lib` tree, not the binary.** On a volume carrying
a **MIN**-packaged tree you get `No module named 'ssl'` / `'ctypes'`. On one still carrying a
**FULL** tree — common when reusing a stick between builds — the Python-level `ssl/` and `ctypes/`
packages import and then fail reaching for their extensions, giving **`No module named '_ssl'` /
`'_ctypes'`** (observed on MIN VS2022, 2026-09-09).

**Both are a pass, and the `_`-prefixed one is the stronger signal**: it proves the *binary* lacks
the extension. A failure on the bare `ssl` / `ctypes` name cannot distinguish "extension not linked"
from "lib tree incomplete". What would be a **failure** is either import *succeeding*.

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

> **VS2022: deep imports at the prompt are now in scope.** They used to hang Shell `exit`, so this
> section was restricted to trivial lines. Fixed 2026-09-08 by putting VS2022 on the same 64 MB
> stack as GCC — **`import json` at `>>>` followed by `exit()` and Shell `exit` is signed off on
> hardware**, with no `MemoryError`. Include a deep import when signing off §4; a hang here is now
> a regression, not a known defect.
> Lab: [`Python312_VS2022_Lab/2026-09-08_VS2022_nasm_abi_mismatch.md`](./Python312_VS2022_Lab/2026-09-08_VS2022_nasm_abi_mismatch.md).

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
Python312.efi -S -c "import readline, sys; print([m for m in sys.modules if 'edk2' in m or 'pyreadline' in m])"
Python312.efi -S -c "import readline; print(readline.rl.disable_readline, readline.get_line_buffer())"
```

| Check | Expected (stub) |
|-------|-----------------|
| `type(readline.rl).__name__` | **`_ReadlineStub`** |
| modules loaded | **`[]`** — neither `pyreadline` nor `edk2console` imported |
| `disable_readline`, `get_line_buffer()` | **`True None`** — every API is a no-op `dummy` |
| `hasattr(readline, 'GetOutputFile')` | **`False`** (only defined on the real path) |

**`_ReadlineStub` is the pass condition.** Seeing `Readline` here means the env var leaked in
(see §5.6) and your "default" runs are not testing the default.

**The module check deliberately uses the list form, not `'x' in sys.modules`.** Here the expected
answer *is* negative, so a boolean that can wrongly report `False` would turn a real failure into
a silent pass — the worst direction for a safety check. See the warning in §5.3.

### 5.3 Non-interactive opt-in check — cheapest VS2022 canary

This exercises the real pyreadline path, hook install and teardown **without** needing arrow
keys, so it is the fastest way to test the risky path:

```text
set -v PY_UEFI_READLINE 1
Python312.efi -S -c "import readline; print(type(readline.rl).__name__, readline.rl.disable_readline)"
```

**`-v` makes the variable volatile** so a forced power-cycle clears it — see §5.6.

| Check | Expected |
|-------|----------|
| Output | **`Readline False`** |
| Shell **`exit`** afterwards | **No hang** |
| Relaunch | Banner normal |

**`disable_readline` is the value that matters, and `False` is the pass.** `readline.py` only
reaches `import pyreadline.console.edk2` and `console.install_readline(rl.readline)` on the
`disable_readline == False` branch, so this single boolean covers the whole real path. A
`Readline` object with `disable_readline == True` means `Readline.__init__` fell back to
`MockConsole` and every API is a no-op — the failure this check exists to catch.

> **Do not use `'edk2console' in sys.modules` here.** It reads naturally but proved unreliable
> from the UEFI Shell on 2026-09-08: the boolean form returned **`False`** while
> `print([m for m in sys.modules if 'edk2' in m])` in the *same* interpreter state listed both
> **`edk2console`** and **`pyreadline.console.edk2`**, and `disable_readline` was `False`. Root
> cause of the disagreement is unconfirmed — likely the nested single quotes inside the `-c`
> string. If you need the module list, use the comprehension form, which has no inner quotes.

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
| `SyntaxError: invalid non-printable character U+001B` on arrow keys | **Either switch missing** leaves the REPL on stdio, so the arrow key arrives as a raw escape sequence and `U+001B` is its ESC byte. Two ways in: **(a)** `PY_UEFI_READLINE` set but **`import readline`** not typed; **(b)** `import readline` typed but the **env var not set**, which gives the `_ReadlineStub` — no hook, by design. Case (b) is the **§5.2 pass condition**, i.e. correct behaviour, not a fault. Confirm which with `readline.rl.disable_readline` (§5.3) |
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

## 5.8 CPU fault reporting — deliberate fault injection

**Destructive: the handler never returns. Run this last and expect to power-cycle.** Verified on
both toolchains 2026-09-08.

```text
Python312.efi -S -c "import ctypes; ctypes.cast(0x800000000000, ctypes.POINTER(ctypes.c_int))[0]"
```

Expected on the console:

```text
Python312 boot: unhandled CPU exception 13 rip=<addr> cr2=<addr>
```

then the machine sits there. **The print is the entire deliverable**; the stop after it is
`py_handle_exception()`'s own `while (exc_trap)` loop, not a crash. Faults are **reported, not
survivable** — `edk2_seh_try()` / `edk2_seh_catch()` exist but have no callers, so there is nothing
to recover into. **As of 2026-09-09 they do have callers** — the guarded primitives in §5.9 — so a
fault raised *inside* one of those three calls is survivable while everything else still lands here.
Design: [`Python312_SEH_Fault_Recovery_Design.md`](./Python312_SEH_Fault_Recovery_Design.md).

**Why this address:** `0x800000000000` is non-canonical (bit 47 set, bits 63:48 clear), so it raises
a **#GP (13)** deterministically, independent of how firmware mapped memory. For a **page fault
(14)** instead, use a plainly unmapped canonical address such as `0xFFFF800000000000`.

**Read `rip`, not `cr2`, for a #GP.** `cr2` only holds the faulting address on a page fault; on
anything else it is whatever the last page fault left behind.

| Result | Meaning |
|--------|---------|
| The line above, then a stop | **Pass.** IDT installed, vector routed, handler reached |
| **Silent** stop, no output at all | Handler ran but could not report — the `Print` is gated again. This was the pre-2026-09-08 GCC behaviour (deviations §11.8 #4) |
| Firmware's own exception dump, or a reset | The fault **bypassed** our handler — IDT not installed. On MSVC check `PY_UEFI_MSVC_IDT`. Confirming it from the boot trace (`before py_install_idt` rather than `skipping py_install_idt (MSVC)`) now needs a `PY_UEFI_BOOT_TRACE` rebuild — see §1.1 |
| A Python `ctypes` exception instead of a fault | The address got mapped. Pick a different one — the test proved nothing |

To take the IDT back out on MSVC, drop `/DPY_UEFI_MSVC_IDT=1` from the `MSFT` `CC_FLAGS`; faults then
go to firmware as they did before. GCC has no equivalent switch — it always installs the IDT.

---

## 5.9 Guarded memory access — survivable faults (`uefi.mem_read` / `mem_write` / `mem_probe`)

**Status: implemented in code 2026-09-09, NOT YET RUN on any configuration.** Unlike §5.8 this is
**non-destructive by design** — a fault inside these three calls raises `uefi.FaultError` and the
interpreter keeps running. If a test here stops the machine instead of raising, that is a failure of
this feature, and the machine still needs a power cycle.

Run test 0 first on each toolchain, then the rest in order in **one interactive session** so that
tests 3, 4 and 6 see the state left by the earlier ones.

```text
Python312.efi -S -c "import uefi; print(hasattr(uefi,'mem_read'), uefi.FaultError)"
```

> **A `SyntaxError` on a long line is usually the console, not the test — retype it before believing
> it.** Observed 2026-09-09 on VS2022 MIN over the HTML KVM client:
> `import time; t = time.time(); time.sleep(2); print(round(time.time() - t, 1))` returned
> `SyntaxError: unmatched ')'`, and **the identical line retyped ran fine and printed `2.0`**. So
> characters are dropped intermittently in transit; it is not a deterministic wrap at 80 columns.
> The trap is that Python faithfully reports a problem with the *mangled* line, which reads exactly
> like a typo in the procedure. Same class as the earlier `non-utf-8 code` syntax error, and **not**
> specific to this section. Splitting a long line across several short ones reduces the exposure and
> is worth doing for anything you will run repeatedly.

Two conventions make every row a short line you can type at a UEFI prompt:

- **A known-good address without `ctypes`:** `id(x)` is the address of a Python object, so
  `uefi.mem_read(id(x), 8)` reads its `ob_refcnt` and must return a small positive integer. That
  works on MIN, which has no other way to name a readable address.
- **Reading the exception without `try`/`except`:** after an unhandled error the REPL leaves the
  instance in `sys.last_value`, so the attributes can be inspected on the next line. Multi-line
  `try:` blocks through `-c` are a quoting fight in the Shell and are not needed.

```text
Python312.efi -S
>>> import sys, uefi
>>> x = b'abcd'
```

| # | Command (at the `>>>` prompt unless shown otherwise) | Expected |
|--:|------|----------|
| 0 | the one-liner above | `True <class 'uefi.FaultError'>`. Verified on **both** toolchains 2026-09-09, GCC included — that run is what established `UEFI_C_SOURCE` is not `MSFT:`-only (§7 correction). A `False` would mean the gate excluded the methods; stop on that toolchain if so |
| 1 | `uefi.mem_read(0x800000000000, 8)` | `uefi.FaultError: CPU exception 13 (rip=0x… cr2=0x…)` and **the prompt returns**. This is the whole feature in one line |
| 2 | `e = sys.last_value; print(e.vector, hex(e.rip), hex(e.cr2), e.error_code)` | `13`, a plausible code address for `rip`, and integers. `cr2` is **meaningless for a #GP** — see §5.8 |
| 3 | `print(uefi.mem_read(id(x), 8))` | A small positive integer (the refcount), no exception. **This is the test that matters**: it proves the recovery in 1 left the interpreter usable rather than merely appearing to |
| 4 | `import time; t = time.time(); time.sleep(2); print(round(time.time() - t, 1))` — or as four short lines if it gets mangled | About `2.0` (observed exactly `2.0`). An instant return or a hang means `RFLAGS.IF` was not restored after the fault — the design doc §2.1 defect regressing, which is the reason that fix exists |
| 5 | `print(uefi.mem_probe(0x800000000000), uefi.mem_probe(id(x)))` | `False True`, **no exception either way** |
| 6 | `print(sum(uefi.mem_probe(0x800000000000) for i in range(200)))` then repeat test 3 | `0`, then test 3 still works. 200 faults must leave the guard stack balanced; if it leaks, `edk2_seh_try()` starts returning `NULL` and test 3 turns into `RuntimeError` |
| 7 | `uefi.mem_read(0, 3)` then `uefi.mem_write(0x1000, 1, 256)` | `ValueError` on the size, `OverflowError` on the value. Both are rejected **before** the guard is installed, so **neither one touches an address** |
| 8 | `exit()`, then Shell `exit` | Clean, no hang — the teardown route this port has spent the most time on |
| 9 | Re-run §2, §3 and §4 | Unchanged. This must disturb nothing that already worked |

**A successful `mem_write` is deliberately not in the list above, and needs `ctypes` (so FULL only).
Verified on VS2022 FULL 2026-09-09, output exactly as shown:**

```text
>>> import ctypes, uefi
>>> b = ctypes.create_string_buffer(8); a = ctypes.addressof(b)
>>> uefi.mem_write(a, 1, 65); print(b.raw[:1])
b'A'
```

Every other row proves the guard survives a **bad** address; this is the only one that proves a
**good** one still does the ordinary thing. It is also read back through a completely independent
path — `ctypes` reading its own buffer — so the value genuinely reached memory rather than the call
merely returning without error.

That is the only safe shape for it — a buffer this process owns. **Never pick a write address any
other way.** The guard makes an *invalid* address survivable; it does nothing whatsoever about a
valid address that mattered, and there is no undo. In particular do not compute one by reading a
pointer out of a Python object: if the offset is wrong the value read is still a plausible address,
so the store lands somewhere real.

**On MIN this section is the only fault-path test available at all.** §5.8 needs `ctypes` to
dereference an address and MIN has no `_ctypes`, so MIN could previously show only that
`py_install_idt()` *ran*, never that a fault actually routes to the handler. A `FaultError` carrying
`vector == 13` is direct proof the IDT entry is live, on a configuration where that was
unobservable — see migration status item 30, which recorded this as the one unclosable gap.

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
| Shell `exit` hangs after importing pure-Python stdlib (VS2022) | **FIXED 2026-09-08.** VS2022 ran on the **~128 KB firmware stack** because `PY_UEFI_MSVC_368_ENTRY` returned before the stack switch; deep imports overflowed it and corrupted memory outside the image, so Python's teardown looked clean and only BDS hung. That workaround existed because `edk2_switch_stack` hung under MSVC — a **NASM ABI mismatch** plus `rsp`-relative reads of `UefiMain`'s frame. Both fixed; VS2022 now gets the same **64 MB** stack as GCC. A hang here is a **regression** | lab `2026-09-08_VS2022_nasm_abi_mismatch` |
| `MemoryError: stack overflow` on a deep import (VS2022) | **Now a regression, not expected.** It *was* correct between the `PyOS_CheckStack` fix (`c3819602`) and the stack-switch fix: the guard traded silent corruption for a catchable error on the small stack. With 64 MB there is no overflow to catch, and `import json` / `import logging` raise nothing. If you see it, the switch is not in effect — check the `switched stack` boot trace | labs `2026-09-08_VS2022_FULL_stackcheck_fix`, `..._nasm_abi_mismatch` |
| Deep import in the **interactive REPL**, then Shell `exit` hangs (VS2022) | **FIXED 2026-09-08**, same root cause as the row above — it was never a second defect. The long bisection (uncaught `MemoryError`, printed traceback, continued execution, REPL required) described the **trigger**; each condition was just a route to a depth 128 KB could not hold, and the REPL merely kept the process alive to touch the corrupted memory again | lab `2026-09-08_VS2022_FULL_interactive_exit_leak` (closed) |
| Interactive session's teardown trace is missing `after Py_BytesMain` / `after main()` | Normal and expected on the `Py_Exit()` route — it longjmps straight to `ShellCEntryLib`. Use their absence as the **marker that the `Py_Exit()` route was taken** | same lab note |
| `SyntaxError: Non-UTF-8 code starting with '\xff'` running a staged `.py` | The file is **UTF-16 with a BOM** — `0xFF` is the first BOM byte. The UEFI Shell's `edit` saves that way, and `type` still displays it correctly so it looks fine. Re-stage the file as **ASCII/UTF-8 with no BOM**; a coding declaration cannot fix it | — |

---

## 7. Sign-off state (2026-09-01)

| Scenario | GCC FULL | VS2022 FULL | VS2022 MIN |
|----------|----------|-------------|------------|
| Phase 8 **`-S -c`** + Shell **`exit`** | **Pass** | **Pass** | n/a |
| **`ctypes.sizeof(c_void_p)`** == **`8`** (§3) | **Pass** (09-04) | **Pass** (09-04) | n/a |
| Four modules, one process (§3 `phase8 ok`) | **Pass** (09-04) | **Pass** (09-04) | n/a |
| Stdio **`-S`** REPL + teardown | **Pass** | **Pass** | **Pass** (09-09, incl. `import json`) |
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

**RESOLVED 2026-09-08 — VS2022 now runs on the 64 MB stack, and both hangs were one defect.**
The `PY_UEFI_MSVC_368_ENTRY` workaround was itself the bug's enabler, and it existed only because
`edk2_switch_stack()` hung under MSVC. Two causes, both in the switch plumbing: the NASM helpers
read their arguments from **`rdi`/`rsi`** (System V) while MSVC passes them in **`rcx`/`rdx`**, so
`rsp` was set from garbage; and the switch moves `rsp` out from under a *running* `UefiMain`, whose
parameters MSVC addresses **`rsp`-relative with no frame pointer** — GCC survived that only because
`-O0` keeps `rbp`. Fixed via `PY_UEFI_MS_ABI` on `MSFT:*_*_*_NASM_FLAGS` and by reading the handles
from `g_edk2_globals`.

**Signed off on hardware, and re-swept on the rebuilt image** — tag
**`python312-vs2022-full-64mb-stack-2026-09-08`**, code state **`32c63ba1`**: `import json` and
`import logging` on `-c`, the REPL deep-import repro with both `raise SystemExit` and `exit()`, the
full §3 Phase 8 sweep, §4 REPL plus relaunch, and pyreadline §5.3 and §5.4 with working history and
Tab completion — all clean, **none raising `MemoryError`**. Measured depth is **39.4 KB of 64 MB**
(§1.1). The interactive case was never a second defect; the ten-round bisection in the exit-leak
lab note mapped the trigger accurately but the cause sat one layer below, in assembly never
exercised on this toolchain. `PY_UEFI_MSVC_368_ENTRY` and `PY_UEFI_FIRMWARE_STACK_BUDGET` are both
gone.

**MIN has since been swept too — 2026-09-09, see §7.1.** It carried `PY_UEFI_MSVC_368_ENTRY` as well
and had been on the switched path unvalidated until then.

Reference commits: GCC **`dbc8416c`**, VS2022 **`4dec4edf`** / **`3568d02d`**.
Pin: tag **`python312-unified-full-lab-2026-09-01`**.

**Latest regression — VS2022 + GCC FULL @ `3afa03f5` (2026-09-04):** §3 Phase 8 including
**`ctypes.sizeof(c_void_p)` → `8`** and **`phase8 ok`**, plus Shell **`exit`** and relaunch —
**pass on both toolchains**, from the **same code state** (later commits are docs-only). First
hardware run on either toolchain **after the PyMod-3.12.13 consolidation**, so it clears the
relocated frozen/deepfreeze artifacts on **both** entry paths.

**Every check guarding a known historical failure is green at this code state** — OpenSSL RNG
hang, `socket.py`/`selectors` teardown, LLP64 pointer width, deepfreeze static strings, and
finalize/re-entry. **§5 has since been swept on both toolchains** (2026-09-08 — §5.3 and §5.4 pass
with working history and Tab, §5.8 fault reporting confirmed on each). Still open: §2 baseline rows
and itemised §4 REPL rows, neither of which guards a known failure —
[`Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md`](./Python312_VS2022_Lab/2026-09-04_unified_FULL_post_pymod_smoke.md).

**Runtime parity as of 2026-09-08 — entry path *and* fault behaviour now match.** Both toolchains
take `edk2_switch_stack` onto 64 MB, both install the custom IDT (`PY_UEFI_MSVC_IDT` is set in both
INFs), and both report a CPU fault as `unhandled CPU exception N rip=… cr2=…` and then spin (§5.8).
Stack limits, recursion depth and fault reporting no longer differ.

**Diagnostics no longer differ either, as of 2026-09-09: `PY_UEFI_BOOT_TRACE` is off in both INFs**,
so a stock image of *either* toolchain prints no `Python312 boot:` ladder and no `switched stack`
measurement. The asymmetry that used to make VS2022 the chattier toolchain is gone by removal rather
than by adding the flag to GCC. Re-enable the trace per §1.1 when investigating.

**Correction, 2026-09-09: `UEFI_C_SOURCE` is *not* `MSFT:`-only, and this document said otherwise.**
It is set for **both** toolchains by a package-wide `[BuildOptions]` block in `StdLib/StdLib.inc`
(`GCC:*_*_*_CC_FLAGS = -nostdinc -nostdlib -DUEFI_C_SOURCE` at line 125, `MSFT:` equivalently at
124), which `AppPkg.dsc:159` `!include`s — so it reaches every module in the package, Python312
included, and the `/DUEFI_C_SOURCE` on the MSFT `CC_FLAGS` line of both INFs is **redundant
duplication**. The visible consequence: **the `Py_FinalizeEx()` detach is compiled into GCC images
too**, not VS2022 alone as previously stated here. Proved empirically by §5.9 test 0 returning
`True` on a GCC 5.3.1 image — `PyInit_uefi` and `mem_read` exist only behind that gate, so the
module could not have loaded otherwise. **This is the second time a DSC-level `[BuildOptions]` entry
has invalidated a claim based on reading the INFs alone** (the first was `PY_UEFI_BOOT_TRACE`,
§1.1): when asking whether a define reaches a module, `AppPkg.dsc` and everything it `!include`s
count as much as the INF.

**The last code difference, the GCC stack-alignment expression, is also gone** — deviations §11.8 #1
was unified on 2026-09-09 so both toolchains round the stack base up.

### 7.1 VS2022 MIN — swept and signed off 2026-09-09

MIN had quietly accumulated **four** deltas validated only on FULL: `PY_UEFI_MSVC_368_ENTRY` removed
(so MIN moved onto the 64 MB switched stack), `/DPY_UEFI_MSVC_IDT=1` added, the
`edk2_alloc_environ()` leak fix, and the two `edk2_seh_*` defect fixes
([`Python312_SEH_Fault_Recovery_Design.md`](./Python312_SEH_Fault_Recovery_Design.md) §2). This
sweep closes all four on hardware.

| Check | Result |
|-------|--------|
| §1.1 boot trace — `size=4000000` | **Pass.** 64 MB, so MIN really is on the switched stack, not the ~128 KB firmware stack |
| §1.1 — `before py_install_idt` | **Pass.** Not `skipping py_install_idt (MSVC)`, so `PY_UEFI_MSVC_IDT` took effect on MIN as it did on FULL |
| §2 — `-h`, `sys.version` → `3.12.13` | **Pass** |
| §2 — `import os, sys, json`, `hashlib` digest | **Pass.** Builtin hashes still present |
| §2 — `import ssl` / `import ctypes` | **Pass (must fail).** Failed on **`_ssl`** / **`_ctypes`** — FULL `lib` tree on the volume, binary lacks the extensions |
| §4 — `-S`, `import json`, `exit()`, Shell `exit` | **Pass.** No hang, **no `MemoryError`** — the sequence that used to hang FULL |
| §4 — relaunch | **Pass.** Covered by repeated launches across §2 and §4 |
| §5.8 fault injection | **n/a on MIN** — no `ctypes` to build the bad pointer with. The IDT is proven by the trace line instead |

**Measured depth: about 71 KB of 64 MB (0.11%).** From `min_rsp=648B71B8` against `limit=608CB038`,
which puts the base at `0x608C9038` and the top at `0x648C9038`. Higher than FULL's 39.4 KB only
because the sampled run was formatting an `ImportError` traceback; both are noise against 64 MB.

**`min_rsp=0` on a `-h` run is not a defect** — `PyOS_CheckStack()` is called only from
`PyObject_Repr`, `PyObject_Str` and `_Py_CheckRecursiveCall`, and `-h` prints usage from C without
evaluating Python, so it reaches none of them. `edk2main.h` documents zero as "never sampled".

The build also settled an open question from the `edk2_seh_*` fixes: `EnableInterrupts()` resolves
without adding `BaseLib` to `[LibraryClasses]`, since it is already in the module's link closure via
`UefiLib`/`DebugLib`.

### 7.2 GCC MIN — swept 2026-09-09

**First sweep of GCC MIN in this port.** It is a deliverable, and it had never been run — the GCC
line was always validated on FULL.

| Check | Result |
|-------|--------|
| §2 — `-h`, `sys.version`, `import os, sys, json`, `hashlib` | **Pass** |
| §2 — `import ssl` / `import ctypes` | **Pass (must fail)** |
| §4 — `-S`, `import json`, `exit()`, Shell `exit`, relaunch | **Pass** |
| §1.1 boot trace | **Unavailable on GCC** — `PY_UEFI_BOOT_TRACE` is on the `MSFT:` flags line only, so there is no ladder, no `size=`, no `min_rsp`. Absence is not a failure |
| §5.8 fault injection | **n/a** — no `_ctypes` to build the bad pointer with, and no other way to dereference an arbitrary address from pure Python |

**What this settles beyond MIN itself:** the two `edk2_seh_*` fixes touch shared code, and their only
real risk was that `EnableInterrupts()` might need an explicit `BaseLib` in `[LibraryClasses]`. The
GCC build linking cleanly confirms `BaseLib` is in the module's closure on **both** toolchains, not
just under MSVC.

**Consequence of §1.1 being unavailable and §5.8 being n/a:** on GCC MIN nothing directly proves the
IDT installed *or* that a fault routes to `py_handle_exception()`. That combination is unique to GCC
MIN — VS2022 MIN at least has the trace line — and it is unclosable until the guarded primitives
exist ([`Python312_SEH_Fault_Recovery_Design.md`](./Python312_SEH_Fault_Recovery_Design.md) §4). It
is not a regression risk, since the IDT code is shared and proven on GCC FULL.

> **GCC FULL has since been swept — see §7.3, including §5.8 green.** The gap noted here is closed.

### 7.3 GCC FULL — swept 2026-09-09, §5.8 green

| Check | Result |
|-------|--------|
| §2 baseline + `import os; print(len(os.environ))` | **Pass.** The `os.environ` count is the canary for the `edk2_alloc_environ()` fix |
| §3 Phase 8 — `zlib`, `hashlib`, `ssl.__file__`, `phase8 ok` | **Pass** |
| §3 — `ctypes.sizeof(c_void_p)` | **`8`.** LLP64 pointer width correct |
| §3 — `ssl.create_default_context()` | **Pass.** OpenSSL RNG canary |
| §4 — REPL `import json`, `exit()`, Shell `exit`, relaunch | **Pass.** No `MemoryError`, no hang |
| **§5.8 — fault injection** | **Pass. `unhandled CPU exception 13` with `rip` and `cr2`** |

**§5.8 here is the row that mattered for the `edk2_seh_*` fixes.** They edited
`py_handle_exception()`, and GCC is the toolchain where that function's `Print` was silently
compiled out until 2026-09-08 (deviations §11.8 #4). A green fault report on GCC FULL confirms the
reporting path survived the change on the toolchain where it was historically fragile.

### 7.4 State of the matrix at 2026-09-09

| Configuration | Status at this code state |
|---------------|---------------------------|
| **VS2022 MIN** | **Swept green** — §7.1, incl. boot trace and IDT trace line |
| **GCC MIN** | **Swept green** — §7.2, §2 + §4 (no trace, §5.8 n/a) |
| **GCC FULL** | **Swept green** — §7.3, incl. §5.8 fault reporting |
| **VS2022 FULL** | **Swept green** — rebuilt at this code state, §2/§3/§4 and §5.8 |

**All four configurations are green at one code state, on both toolchains — a first for this port.**
Tag `python312-seh-fix-all-configs-2026-09-09`. Previous sign-offs covered FULL on both toolchains
but never MIN on either, and the two MIN builds are shipped deliverables.

**What that means for the `edk2_seh_*` fixes specifically:** they were verified on every
configuration they compile into, and §5.8 confirmed on **both** FULL builds that the *unhandled*
fault-reporting branch still works with them in. That is the branch the fixes did not touch but sit
adjacent to, in the same function.

### 7.5 VS2022 MIN — §5.9 guarded primitives green 2026-09-09 (first hardware run)

**The `edk2_seh_*` recovery path executed successfully for the first time in this port.** All of
§5.9 tests 0–8 green on **VS2022 MIN**, as observed:

```text
>>> uefi.mem_read(0x800000000000, 8)
uefi.FaultError: CPU exception 13 (rip=0x6484bec6 cr2=0x0)
>>> e = sys.last_value; print(e.vector, hex(e.rip), hex(e.cr2), e.error_code)
13 0x6484bec6 0x0 0
>>> print(uefi.mem_read(id(x), 8))
1
>>> import time; t = time.time(); time.sleep(2); print(round(time.time() - t, 1))
2.0
>>> print(uefi.mem_probe(0x800000000000), uefi.mem_probe(id(x)))
False True
>>> print(sum(uefi.mem_probe(0x800000000000) for i in range(200)))
0                      <-- run twice, 400 recovered faults in total
>>> print(uefi.mem_read(id(x), 8))
1                      <-- unchanged after all of them
>>> uefi.mem_read(0, 3)
ValueError: size must be 1, 2, 4 or 8, not 3
>>> uefi.mem_write(0x1000, 1, 256)
OverflowError: value does not fit in 1 byte(s)
```

`exit()` → Shell `exit` then returned to firmware cleanly.

**The `0` from the loop is the strongest single result here.** Each iteration installed a guard, took
a real #GP, `longjmp`'d out of the fault handler and popped the guard; 400 of those leaving the
count at exactly `0` with the prompt still live means the guard stack balanced every time. A leak of
one slot per fault would have exhausted `EDK2_SEH_CONTEXT_SIZE` by the tenth iteration and every
call after it would have raised `RuntimeError` instead of returning `False`. The refcount read still
returning `1` afterwards, and a clean teardown after 400 non-unwinding `longjmp`s, is the evidence
that the abandoned C frames left nothing inconsistent behind.

Note `cr2=0x0` and `error_code=0`: correct for a #GP, where neither carries the address (§5.8).

**Three things this establishes that no earlier sweep could:**

1. **Recovery works, not merely reporting.** Until now `g_context_index` was permanently `-1` and
   the recovery branch of `py_handle_exception()` was dead code (design doc §1). A returned prompt
   after a #GP is that branch running.
2. **The §2.1 interrupt fix is confirmed by observation rather than by argument.** `time.sleep(2)`
   depends on the firmware timer, which stops if `RFLAGS.IF` is left clear by the `longjmp` that
   skips `iretq`. Before that fix this row would hang or return instantly.
3. **MIN's fault routing is now directly observable, closing the gap recorded in migration status
   item 30 as unclosable.** §5.8 needs `ctypes` to dereference an address and MIN has no `_ctypes`,
   so MIN could previously show only that `py_install_idt()` *ran*. A `FaultError` carrying
   `vector == 13` proves the IDT entry is live and the vector reaches our handler.

### 7.6 VS2022 FULL — §5.9 green plus the write path and the §2/§3/§4 regression, 2026-09-09

**§5.9 tests 0–8 green on FULL as well, the `ctypes` write path verified (`b'A'`), and §2/§3/§4
re-run clean** — `sys.version`, `phase8 ok`, `ctypes.sizeof(c_void_p)` → `8`, the `logging` deep
import, and a REPL `import json` → `exit()` → Shell `exit`.

**Why repeating §5.9 on FULL was not redundant.** The primitives are identical code, so what FULL
could change is not their behaviour but their *surroundings*: it links OpenSSL, zlib and `_ctypes`,
which moves where a fault lands relative to everything else in the image and adds a great deal of
state for a non-unwinding `longjmp` to disturb. Green here plus green on MIN means the mechanism does
not depend on how much else is loaded. FULL is also the shipping configuration, so the §2/§3/§4
regression is the check that the new module surface disturbed nothing that already worked.

### 7.7 GCC FULL — §5.9 green, and the `UEFI_C_SOURCE` question answered, 2026-09-09

**§5.9 tests 0–8 and the `ctypes` write row all green on GCC FULL**, banner
`Python 3.12.13 (main, Sep 9 2026, 15:58:52) [GCC 5.3.1 20160413] on uefi`:

```text
uefi.FaultError: CPU exception 13 (rip=0x64aee83a cr2=0x0)
13 0x64aee83a 0x0 0        <-- vector, rip, cr2, error_code
1                          <-- refcount read after the fault
2.0                        <-- timer still live
False True                 <-- probe bad / good
0                          <-- 200 recovered faults
1                          <-- refcount unchanged after them
ValueError: size must be 1, 2, 4 or 8, not 3
OverflowError: value does not fit in 1 byte(s)
b'A'                       <-- ctypes write path
```

`exit()` → Shell `exit` clean, no hang.

**Cross-toolchain reading of this:** the numbers are identical to VS2022 (§7.5, §7.6) apart from
`rip`, which differs only because the images differ. That matters more here than elsewhere, because
`setjmp`/`longjmp` on this platform is **EDK2's `SetJump`/`LongJump`**, not a libc implementation —
identical assembly on both toolchains, with no SEH-based unwinding on either. Matching behaviour is
what that predicts, and this is the run that confirms it rather than assuming it.

**Test 0 on this image is also what settled the `UEFI_C_SOURCE` question** — see the correction at
the top of §7. `mem_read` existing on a GCC image proved the define reaches GCC, and the mechanism
turned out to be `StdLib/StdLib.inc`, not the INFs.

### 7.8 GCC MIN — compile-verified 2026-09-09, runtime deliberately inferred

**Built clean; §5.9 was NOT run on this image, by decision rather than omission.** What the build
establishes is the part that could plausibly have failed: `PyInit_uefi` resolves and nothing in the
primitives needs a FULL-only symbol under the GCC linker.

**Why the sweep was judged to add almost nothing.** The matrix is toolchain × module set, and the
other three cells are observed:

| | MIN | FULL |
|---|---|---|
| **VS2022** | §7.5 — tests 0–8 | §7.6 — tests 0–8 + write + §2/§3/§4 |
| **GCC** | **this section — build only** | §7.7 — tests 0–8 + write |

GCC FULL against VS2022 FULL varied the **toolchain** and produced identical results; VS2022 MIN
against VS2022 FULL varied the **module set** and did the same. GCC MIN is the intersection of two
axes each already varied against the other. For it to fail alone, something in the primitives would
have to depend on a FULL-only define or symbol — nothing does, `edk2excep.c` is in MIN's
`[Sources]`, and `SetJump`/`LongJump` is the same EDK2 assembly on both toolchains.

**The gap, stated rather than glossed:** MIN is the configuration where §5.9 is the *only* fault-path
test that exists, since §5.8 needs `_ctypes` to dereference an address. So fault **routing on this
particular image** is inferred, not observed. That is the same gap migration status item 30
recorded for GCC MIN, now narrowed from "the whole configuration is unexercised" to "one cell of
four is compile-verified". Run §5.9 tests 0 and 1 if a GCC MIN image goes on hardware for any other
reason — two lines, and it closes this outright.

### 7.9 VS2022 FULL — re-verified after the guarded path moved translation units, 2026-09-09

**§5.9 tests 0–8, the `ctypes` write row and the §2/§3/§4 regression all green again**, on an image
where `uefi_guarded_access` no longer lives in `posixmodule.c`. It moved whole into
`efi/src/edk2excep.c` as `edk2_guarded_access()` so the new `edk2` module can share it instead of
carrying a second copy (CHIPSEC port phase 2 — see
[`Python312_Chipsec_Platform_API_Port.md`](./Python312_Chipsec_Platform_API_Port.md) §11).

**This re-run existed to detect nothing, which makes the passing rows the point rather than a
formality.** Four of them are the ones a bad move would have broken, and each fails in a different
and recognisable way:

| Row | What it would have caught |
|---|---|
| 1 — fault raises, prompt returns | the moved `setjmp` no longer owning the frame `longjmp` returns to |
| 3 — read after the fault | recovery that only *looks* successful, leaving the interpreter subtly unusable |
| 4 — `2.0` sleep | `RFLAGS.IF` not restored, i.e. the design doc §2.1 defect regressing |
| 6 — 200 faults then a read | guard stack leaking, which surfaces as `edk2_seh_try()` returning `NULL` |

**One new assertion, and it is the only positive claim in the phase:**

```text
Python312.efi -S -c "import edk2, uefi; print(edk2.FaultError is uefi.FaultError)"
True
```

`PyInit_edk2` borrows the type from `uefi` rather than creating one, so there is a single
`FaultError` and `except uefi.FaultError` keeps catching faults raised through either module. `False`
would have been the more dangerous result than an outright error: code catching `edk2.FaultError`
would still work while code catching `uefi.FaultError` would silently stop catching anything.

**Identity is necessary but not sufficient, so the borrow was also checked end to end** — a fault
raised through `uefi` must be catchable as `edk2.FaultError`, which is what any consumer wrapping
the memory APIs in `except edk2.FaultError` actually depends on. All three green:

```text
>>> import sys, uefi, edk2
>>> uefi.mem_read(0x800000000000, 8)
uefi.FaultError: CPU exception 13 (rip=0x... cr2=0x...)
>>> print(isinstance(sys.last_value, edk2.FaultError))
True
>>> print(issubclass(edk2.FaultError, OSError))
True
```

No `try`/`except` needed — `sys.last_value` holds the instance after an unhandled error, the same
convention §5.9 uses to keep every row a short line. `issubclass(..., OSError)` covers a different
audience from the identity check: tooling that catches `OSError` broadly and knows nothing about
this build still catches a fault, which is why `OSError` is the base. **The traceback correctly
still reads `uefi.FaultError`** — one type, keeping the name it was created with, exposed under two
module attributes. That is not a defect to fix.

**Surface inventory, confirmed `['FaultError']`:**

```text
Python312.efi -S -c "import edk2; print(sorted(n for n in dir(edk2) if not n.startswith('_')))"
```

Worth re-running at the end of every remaining port phase. It is the cheapest guard against a
function added to the method table but misspelled, or written and never added to the table at all
— both of which raise `AttributeError` on the name you expected and neither of which points at the
table as the cause.

**§5.8 was deliberately skipped.** It injects an *unhandled* fault, needs a power cycle, and
exercises the branch of `py_handle_exception` that phase 2 did not touch. Row 1 above covers the
handled branch, which is the part that moved.

**VS2022 MIN and GCC MIN both build and link clean** (2026-09-09), which is the whole of what was
asked of them — they compile the changed `posixmodule.c` and `edk2excep.c` but gain no
functionality, so "did not break" is the entire result.

### 7.10 GCC FULL — phase 2 re-verified, which closes it on all four configurations, 2026-09-09

**§5.9 tests 0–8, the `ctypes` write row and the three `edk2` module checks all green, matching
VS2022 FULL row for row.** With this, the guarded fault path has been exercised after the move on
both toolchains in FULL and compiled clean in both MINs, so **CHIPSEC port phase 2 is closed**.

This was the run in the phase that carried real information, and it is worth recording why rather
than filing it as one more green column. Every other check in phase 2 varied nothing that could
plausibly break: the two MIN builds are compile-only, and the VS2022 FULL re-run (§7.9) used the
same compiler that had already been proven on the code in its old location. Moving
`uefi_guarded_access` into `efi/src/edk2excep.c` changes which translation unit owns the frame that
`setjmp` captures and `longjmp` returns to. `SetJump`/`LongJump` is the same EDK2 assembly on both
sides, but **how each compiler lays out that frame is not** — MSVC and GCC differ on what lands in
registers versus the stack, and on what the optimiser is entitled to keep live across a call that
can return twice. A relocation problem was therefore more likely to appear on one toolchain than on
both, which is exactly why matching §7.7's values on GCC is the result that closes the phase rather
than merely agreeing with it.

The rows that would have shown it are the same four named in §7.9 — row 1 (fault raises *and* the
prompt returns), row 3 (a read after the fault), row 4 (the `2.0` sleep, i.e. `RFLAGS.IF`), row 6
(200 faults then a read). All four green here too.

### 7.11 Phase 7 UEFI variables (§16) — re-run needed after the CHIPSEC audit, 2026-09-10

**Code:** `6fe11059` or later (`PrintLib.h` in `c4672f8b`; `Py_BuildValue` fix required for
enumerate). Matrix: [`Python312_Chipsec_Platform_API_Port.md`](./Python312_Chipsec_Platform_API_Port.md) §16.

The rows below were green, but `GetNextVariableName` has since changed to CHIPSEC's actual
contract — **Name before NameSize**, and `bytes` accepted for name and GUID (§19). Re-run the
enumerate lines on both toolchains to re-close.

| Toolchain | Observed |
|---|---|
| **VS2022 FULL** | `PlatformLang` read (`st=0`); §16.5 validation errors; enumerate after `6fe11059` |
| **GCC FULL** | Same session green — banner `[GCC 5.3.1 ...] on uefi`, read + enumerate + validation |

Default run is **read-only** plus `SetVariable` argument validation — no firmware write.

```text
Python312.efi -S -c "import edk2; print('GetVariable' in dir(edk2), len([n for n in dir(edk2) if not n.startswith('_')]))"
```

Expect `True 16`.

Interactive (paste at `>>>`):

```text
import uuid
G = '8BE4DF61-93CA-11d2-AA0D-00E098032B8C'
st, attr, data, sz = edk2.GetVariable('PlatformLang', G, 128)
st, name, nsz, g = edk2.GetNextVariableName(512, '', '00000000-0000-0000-0000-000000000000')
st, name, nsz, g = edk2.GetNextVariableName(200, '\x00'.encode('utf-16-le'), uuid.uuid4().bytes_le)
edk2.GetVariable('PlatformLang', 'not-a-guid', 8)   # ValueError
edk2.SetVariable('X', 'not-a-guid', 0, b'', 0)       # ValueError
edk2.SetVariable('X', G, 0, b'abc', 10)              # ValueError DataSize
```

The second enumerate is CHIPSEC's exact spelling (UTF-16LE name bytes, `uuid.bytes_le` GUID);
both must return `st == 0` with a non-empty `name` and a `g` that `uuid.UUID()` parses.

CHIPSEC helper selection, worth one line while you are at the prompt:

```text
Python312.efi -S -c "import sys, platform; print(sys.platform, '|', platform.system())"
```

Both values must start with `uefi`, or `OsHelper.is_efi()` returns False and CHIPSEC falls back
to `NoneHelper`.

Tag: **`python312-chipsec-phase7-uefi-vars-2026-09-10`**.

### 7.12 Phase 8 `allocphysmem` / `freephysmem` (§17) — CLOSED VS2022 + GCC FULL, 2026-09-10

Matrix: [`Python312_Chipsec_Platform_API_Port.md`](./Python312_Chipsec_Platform_API_Port.md) §17.
Expect **`len([n for n in dir(edk2) if not n.startswith('_')]) == 18`**.

| Toolchain | Observed |
|---|---|
| **VS2022 FULL** | **Green** — §17.2–§17.4 at `dcf99d52` |
| **GCC FULL** | **Green** — same matrix, GCC banner |

```text
va, = edk2.allocphysmem(4096, 0xFFFFFFFF)
lo = va & 0xFFFFFFFF; hi = va >> 32
edk2.writemem_dword(lo, hi, 0xDEADBEEF)
edk2.readmem_dword(lo, hi) == 0xDEADBEEF
edk2.freephysmem(va)
edk2.allocphysmem(0, 0xFFFFFFFF)   # ValueError
```

Tag: **`python312-chipsec-phase8-allocphysmem-2026-09-10`**.

### 7.13 Phase 9 `_ex` + MP Services (§18) — CLOSED VS2022 + GCC FULL, 2026-09-10

Matrix: [`Python312_Chipsec_Platform_API_Port.md`](./Python312_Chipsec_Platform_API_Port.md) §18.
Expect **`len([n for n in dir(edk2) if not n.startswith('_')]) == 21`**.

| Toolchain | Observed |
|---|---|
| **VS2022 FULL** | **Green** — §18 at `6f70c9bf` |
| **GCC FULL** | **Green** — same matrix |

```text
import edk2
edk2.cpuid(0, 0) == edk2.cpuid_ex(0, 0, 0)
edk2.rdmsr_ex(99999, 0)   # ValueError
```

Tag: **`python312-chipsec-phase9-mp-ex-2026-09-10`** — **19/19 CHIPSEC APIs complete.**

### 7.14 `help()` works without a pager — after the `pydoc` fix, 2026-09-10

Stock `pydoc` picked `less` through `subprocess` and raised
**`PermissionError: [Errno 1] Operation not permitted`** on every `help()` call; the UEFI override
[`PyMod-3.12.13/Lib/pydoc.py`](./Python-3.12.13/PyMod-3.12.13/Lib/pydoc.py) returns `plainpager`
instead. Details and the in-session workaround: runtime notes §10.6.

**No rebuild needed** — `pydoc` is staged on the ESP, so repackage (or copy the single file) and:

```text
Python312.efi -S
>>> import edk2
>>> help(edk2.rdmsr)
>>> help(edk2)
```

Both must print the doc text and return to `>>>` with no traceback and no
`'(less)' is not recognized` line.

---

Re-run this document on **both** toolchains after any shared PyMod or INF change.
