# Lab: VS2022 FULL — interactive `exit()` skips the console detach (2026-09-08)

**Branch:** `feature/python-3.12.13-vs2022` · **Code state:** **`4cf5698a`**
(*fix(python312): track rsp high-water mark to size the firmware stack budget*)
**Toolchain:** **VS2022 FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY`
**Result:** **Two candidate causes eliminated, one variable left.** The hang is **not** a stack
overflow (depths 256 bytes apart, same 4 KB page) and **not** the `Py_Exit()` exit route
(`-c "import sys; sys.exit(0)"` exits clean, and `StdLib` converges both routes). A leaked ConInEx
handle is also ruled out by code inspection. **What remains:** the interactive run is the only one
that **read the console** through `PyOS_Readline`. Tests to isolate it are at the end.

**Predecessors:** [`2026-09-07_VS2022_FULL_pyreadline_hang.md`](./2026-09-07_VS2022_FULL_pyreadline_hang.md)
(root cause of the *non-interactive* hang) ·
[`2026-09-08_VS2022_FULL_stackcheck_fix.md`](./2026-09-08_VS2022_FULL_stackcheck_fix.md)
(the `PyOS_CheckStack` fix, which cleared the non-interactive case).

---

## The measurement that redirected the investigation

`stack_min_rsp` / `stack_entry_rsp` instrumentation added at `4cf5698a` prints
`stack high-water min_rsp=… limit=… used=…` after `ShellCEntryLib` returns.
`entry_rsp = limit + budget = 0x6A951608 + 0x18000 = 0x6A969608`.

| Run | `min_rsp` | vs `limit` (`0x6A951608`) | `used` | Shell `exit` |
|-----|-----------|---------------------------|--------|--------------|
| `-S -c "import re"` | `0x6A952E68` | **`+0x1860`** = 6 240 B **above** — guard never fired | `0x167A0` = 91 808 B = **89.7 KB** | clean |
| `-S -c "import json"` | `0x6A94E9E8` | **`−0x2C20`** = 11 296 B below | `0x1AC20` = 109 600 B = **107.0 KB** | clean |
| interactive `>>> import json` | `0x6A94E8E8` | **`−0x2D20`** = 11 552 B below | `0x1AD20` = 109 856 B = **107.3 KB** | **HANG** |

**The clean run and the hanging run differ by `0x100` — 256 bytes.** Peak depth, overshoot past
`limit` (≈11 KB in both, i.e. the sampled guard's unchecked excursion plus the `MemoryError`
unwind), and `used` are all effectively identical. **Stack depth cannot be what distinguishes them**,
so the previous conclusion — "the 96 KB budget is too generous" — was **wrong**.

Both values also sit inside the **same 4 KB page** (`0x6A94E000`), so a page-aligned firmware stack
base cannot lie between them. That closes the last way depth could have been the variable.

### Corollary: do not lower `PY_UEFI_FIRMWARE_STACK_BUDGET`

`import re` runs clean but bottoms out only **6 240 bytes above `limit`**. The 96 KB budget is
**nearly too tight already**, not too generous. Lowering it — the change the previous note
proposed — would have started raising `MemoryError` on shallow, working imports and on Phase 8.
Measuring first is what caught this.

**Why the depths are so large at all:** these are `-b NOOPT` builds. No inlining and no frame
reuse means the import machinery's frames are far bigger than a release build's, so ~90 KB for
`import re` is consistent rather than alarming.

## Confirmed: `Py_Exit()` bypasses the `Py_RunMain()` tail

Diffing the boot trace of the two `import json` runs is decisive. The non-interactive run prints:

```text
Py_RunMain after pymain_run_python      <-- present
edk2_console_detach_readline enter      <-- present
stop_timer: already off                 <-- present
edk2_console_detach_readline leave      <-- present
Py_FinalizeEx enter ... leave
Py_RunMain after Py_FinalizeEx          <-- present
after Py_BytesMain                      <-- present
after main()                            <-- present
after ShellCEntryLib
```

The interactive run goes **straight from `exit()` to `Py_FinalizeEx enter`**, and every line above
is **absent**:

```text
>>> exit()
Py_FinalizeEx enter
Py_FinalizeEx after wait_for_thread_shutdown
Py_FinalizeEx after _PyAtExit_Call
Py_FinalizeEx after flush_std_files
Py_FinalizeEx skip PyGC_Collect (FULL)
Py_FinalizeEx leave
after ShellCEntryLib                    <-- note: no "after Py_BytesMain", no "after main()"
```

`exit()` raises `SystemExit`, which propagates out of `PyRun_InteractiveOne()` into
`PyErr_Print()` → `_Py_HandleSystemExit()` → **`Py_Exit()`**, and `Py_Exit()` calls
`Py_FinalizeEx()` and then `exit()`. It **never returns through `Py_RunMain()`**. But the only
detach call on the normal route lives there:

```730:731:AppPkg/Applications/Python/Python-3.12.13/PyMod-3.12.13/Modules/main.c
    py312_boot_print_ascii("Py_RunMain after pymain_run_python");
    edk2_console_detach_readline();
```

## Correction — the leaked-ConInEx theory is dead

The first reading of this was "the skipped detach leaks ConInEx, and BDS trips over the stale
interface". **Code inspection rules that out for the reported scenario.** There are only two places
`console_in` is ever set, and neither runs here:

| Site | Reached when |
|------|--------------|
| `edk2main.c:119` (`UefiMain`) | **Never** — the `OpenProtocol` is inside `#ifdef PY_UEFI_PYREADLINE`, and `PY_UEFI_PYREADLINE` is **not defined in any INF**; it appears only in `edk2main.c` and `Modules/main.c` |
| `edk2console.c:128` (`edk2_console_ensure_input`) | Only from `edk2console.getkeys()` (`:227`), i.e. the **pyreadline** input hook |

The reported session had `PY_UEFI_READLINE` **unset**, so `readline` was the stub, the REPL was
**stdio**, `getkeys()` was never called, and `g_edk2_globals.console_in` stayed **NULL**. With
`console_in == NULL`, `edk2_console_detach_readline()` skips `CloseProtocol` entirely and
`drain_input` returns immediately — so **there was nothing to leak and the detach would have been a
no-op.** It cannot be the cause.

The `pylifecycle.c` change is kept as **correct hardening** — it is genuinely needed for
`PY_UEFI_PYREADLINE` development builds and for the pyreadline phases, where `console_in` *is*
non-NULL and `Py_Exit()` really would leak it — but **it does not explain or fix this hang.**

## What is actually established, and what is not

**Established:** the hang tracks the **exit route**, not stack depth. `-S -c` returns normally
through `Py_RunMain()` → `main()` → `ShellCEntryLib`; the REPL's `exit()` longjmps out of
`Py_Exit()` straight to `ShellCEntryLib`, skipping the tails of `Py_RunMain()`, `Py_BytesMain()`
and `main()`. `main()`'s tail is only a trace print and `return rc` (`Programs/python.c:39-40`), so
the interesting difference is inside the C runtime's `exit()`, not in port code.

**Not established:** whether the trigger is the `exit()`/longjmp route itself, or simply the fact
that the session **read interactive stdin** (which the `-c` runs never do). Both changed at once in
the reported test, so they are still confounded.

## Decisive test — no rebuild, runs on the `4cf5698a` image

`sys.exit()` inside `-c` takes the **same `Py_Exit()` route as the REPL**:
`PyRun_SimpleStringFlags` (`Python/pythonrun.c:508`) calls `PyErr_Print()` on any exception,
`_PyErr_PrintEx` calls `handle_system_exit` (`:773`), and that calls **`Py_Exit(exitcode)`**
(`:777`). So the route can be exercised **shallow and non-interactively**, unconfounding it:

| | normal return | `Py_Exit()` route |
|---|---|---|
| **shallow** | `-c "print(1+1)"` — known clean | **`-c "import sys; sys.exit(0)"`** ← the test |
| **deep** | `-c "import json"` — known clean | interactive `import json` — known HANG |

- **Hangs** ⇒ the `exit()`/longjmp route alone is sufficient. Depth and interactive stdin are both
  irrelevant, and the fix belongs in what `exit()` skips versus a normal return.
- **Clean** ⇒ the route alone is not enough; the variable is **interactive stdin use**, and the
  next step is a shallow interactive session (`1+1`, then `exit()`) to confirm.

Absence of `after Py_BytesMain` / `after main()` in the trace is the marker that the `Py_Exit()`
route was actually taken, so the test is self-verifying.

## Result: the exit route is EXONERATED

Run on hardware at `abab8acc`:

| Test | Route | Depth | Shell `exit` |
|------|-------|-------|--------------|
| `-c "print(1+1)"` | normal return | shallow | clean |
| **`-c "import sys; sys.exit(0)"`** | **`Py_Exit()`** | shallow | **clean** |
| `-c "import json"` | normal return | deep | clean |
| interactive `import json` | `Py_Exit()` | deep | **HANG** |

**The `Py_Exit()`/longjmp route alone does not cause the hang.** That is corroborated by
`StdLib/LibC/Main/Main.c`, where both routes converge: `exit()` runs `exitCleanup()` (atexit
handlers plus `gMD->cleanup`) and then `_Exit()` longjmps to the `setjmp` at `:191`, and
everything after it — `ExitVal = gMD->ExitValue`, the `close(i)` loop over all `OPEN_MAX` fds, the
`FreePool` of `gMD` — is **outside** the `setjmp` block and therefore runs either way. The only
real difference is the longjmp itself and the skipped `after main()` trace print, neither of
which is functional.

**So neither variable alone is sufficient:** not depth (256 bytes apart, same page), not the exit
route (just tested clean). What is left is the remaining thing unique to the interactive run — it
**read the console**, via `PyOS_Readline`. Every `-c` run opens `stdin:` as a TTY at
`Main.c:171` but never reads it.

## Next tests — separate "console read" from "deep import"

All three are shallow-to-deep variations that keep the **normal return route**, so the route is
held constant and only the console read varies. `input()` is used deliberately because it goes
through **`PyOS_Readline`**, the same read path the REPL uses.

| # | Command | Console read | Depth |
|---|---------|--------------|-------|
| T1 | `Python312.efi -S -c "s=input('t: '); print('read', s)"` | yes | shallow |
| T2 | `Python312.efi -S -c "s=input('t: '); import json; print('ok')"` | yes | deep |
| T3 | `Python312.efi` → `1+1` → `exit()` | yes (full REPL) | shallow |

- **T2 hangs, T1 clean** ⇒ the trigger is **console read + deep import**, with the exit route
  irrelevant. That is a fully non-interactive repro and the tightest one available.
- **T1 hangs** ⇒ reading the console is sufficient on its own; depth is irrelevant too.
- **T1 and T2 clean but T3 hangs** ⇒ something specific to the REPL loop rather than to reading.

### T1/T2/T3 result: all three CLEAN

| # | Run | REPL | console read | deep import | exit route | Shell `exit` |
|---|-----|------|--------------|-------------|-----------|--------------|
| — | `-c "print(1+1)"` | no | no | no | normal | clean |
| — | `-c "import sys; sys.exit(0)"` | no | no | no | `Py_Exit` | clean |
| — | `-c "import json"` | no | no | **yes** | normal | clean |
| T1 | `-c "s=input(...)"` | no | **before** | no | normal | clean |
| T2 | `-c "s=input(...); import json"` | no | **before** | **yes** | normal | clean |
| T3 | REPL `1+1` → `exit()` | **yes** | **before** | no | `Py_Exit` | clean |
| — | REPL `import json` → `exit()` | **yes** | **before + after** | **yes** | `Py_Exit` | **HANG** |

**T2 is the surprise.** It has the deep import *and* a console read *and* still exits clean, which
eliminates "console read + deep import" as a sufficient pair. Comparing the last two rows, the
hanging run is the only one with a **console read *after* the deep import**. Every clean run either
read before the import or never read at all.

So the current hypothesis is an **ordering** effect: whatever the deep import does to the stack (or
to memory below it), the damage only becomes fatal once the console is driven **afterwards**.

**T2 confirmed: it printed `MemoryError`, not `ok`.** So the guard did fire, the deep case really
was exercised, and the row stands. Note what that means about the sequence — `input()` ran first,
then `import json` raised, so the `MemoryError` terminated the script and there was **no console
read after the overflow**. T2 is therefore a clean instance of *overflow with a prior console read*
and it exits fine, which is exactly what makes the ordering theory the surviving explanation.

### T4/T5 result: both CLEAN — the ordering theory is dead too

| # | Run | `-S`? | console read | deep import | Shell `exit` |
|---|-----|-------|--------------|-------------|--------------|
| T4 | `-c "import re; input()"` | yes | **after** big import | no (no overflow) | clean |
| T5 | `t.py`: caught `MemoryError`, then `input()` | yes | **after** overflow | yes (**caught**) | clean |

T5 puts a console read *after* the overflow and still exits clean, so **"read after the overflow"
is not the trigger either.** Every non-REPL shape now tested is clean, including this one.

**What is left is narrow: the hang needs the REPL *and* a deep import.** T3 (REPL, shallow) is
clean; `-c "import json"` (deep, no REPL) is clean; only the two together fail. Neither is
sufficient alone.

### Variables that differ between T5 and the hanging run

Three, and they have been changing together:

1. **`-S`.** Every `-c` test used **`-S`**; the interactive runs were launched as plain
   `Python312.efi`, so **`site` was imported** — extra modules at baseline, and `exit`/`quit`
   installed. This has been an uncontrolled variable in every comparison so far.
2. **The exception was caught.** T5 wrapped the import in `try`/`except MemoryError`, so no
   traceback was printed and `sys.last_value`/`sys.last_traceback` were never set. In the REPL the
   `MemoryError` was **uncaught**, printed, and left a live traceback holding the overflow's frames.
3. **`exit()` is not a bare `SystemExit`.** It is `site.Quitter.__call__`, which calls
   **`sys.stdin.close()`** before raising. T3 also went through that and stayed clean, so it is not
   sufficient on its own — but it has never been tested *in combination with* the deep import.

### T7 result: HANGS — and it is the minimal repro

```text
Python312.efi -S
>>> import json
>>> raise SystemExit
Shell> exit          -> HANG
```

**Three suspects eliminated at once:** `-S` was used, so **`site` never loaded**; therefore `exit`
did not exist and `site.Quitter.__call__` never ran, so **`sys.stdin.close()` never happened**; and
the extra `site` module baseline is gone. None of them are involved.

**This is now the shortest known reproduction** — two typed lines under `-S`. Everything earlier in
this note that used plain `Python312.efi` can be retired in favour of it.

**Consequence for the smoke doc:** the VS2022 §4 "REPL and teardown" sign-off only covers
**shallow** sessions. A REPL session that performs a deep import is not covered by it.

### Still confounded between T7 (hang) and `-c "import json"` (clean)

| Difference | T7 | `-c "import json"` |
|---|---|---|
| Console read by the **tokenizer** via `PyOS_Readline` | yes, twice | never |
| Execution **continues** after the `MemoryError` | yes | no — exits immediately |
| `sys.last_type`/`last_value`/`last_traceback` set | yes, and then **kept alive while more code runs** | set, then immediate exit |

Note T5 is not a control for the third row: it **caught** the `MemoryError`, so `PyErr_Print()`
never ran and `sys.last_*` were never set. "Uncaught overflow followed by more execution" remains
untested outside the REPL.

### Next tests — decompose the REPL scenario

| # | Steps | Isolates |
|---|-------|----------|
| **T6** | `Python312.efi` → `import re` → `exit()` | **Is the overflow required?** `re` is big but stays under the bound. Hang ⇒ overflow irrelevant, it is REPL + big import |
| ~~T7~~ | ~~`Python312.efi -S` → `import json` → `raise SystemExit`~~ | **Done — HANGS.** `site`/`exit()`/`sys.stdin.close()` all eliminated |
| **T8** | `Python312.efi -S` → `exec("try:\n import json\nexcept MemoryError:\n print('caught')")` → `raise SystemExit` | **Is the *uncaught* exception required?** A string literal carries the newlines, so this is one typed REPL line. Clean ⇒ traceback printing / retained `sys.last_*` frames matter, not the import |

### T6 result: CLEAN — the overflow *is* required

```text
Python312.efi
>>> import re
>>> exit()
Shell> exit          -> no hang
```

`re` is a big import (42 modules, ~90 KB of stack) but stays under the bound, so no `MemoryError`.
In a REPL that exits clean. **So "REPL + big import" is not the trigger — the guard actually has to
fire.**

## Truth table: three conditions, all necessary

| Overflow (`MemoryError`)? | **Uncaught** — printed and `sys.last_*` set? | Execution **continues** after? | Result | Evidence |
|---|---|---|---|---|
| no | — | yes | clean | T6 (REPL `import re`), T4 (`-c`) |
| **yes** | **yes** | **no** — exits immediately | clean | `-c "import json"` |
| **yes** | no — **caught** | **yes** | clean | T5 (staged `t.py`) |
| **yes** | no — **caught**, in the REPL | **yes** | clean | **T8** |
| **yes** | **yes** | **yes** | **HANG** | T7, and the original interactive report |

T8 is the controlled counterpart of T7 — identical REPL, `-S`, import and continuation, differing
**only** in whether the exception is caught — so the last row is isolated to a single variable.

Every single-condition and two-condition combination is clean; only all three together fail. So the
trigger is **an uncaught stack-overflow `MemoryError` whose traceback is printed and retained,
followed by continued execution.**

The mechanism this points at is the third row's difference from the fourth: `PyErr_Print()` sets
`sys.last_type`/`last_value`/`last_traceback`, which **pins the frame objects from the overflow**
instead of letting them be released — and then the interpreter keeps running with them held. In
`-c` the process exits before that matters; when caught, they are never pinned at all.

## Next tests — confirm the model and escape the REPL

**T8 — CONFIRMED CLEAN.** Same REPL, same `-S`, same deep import, same continued execution as T7 —
the *only* change is that the `MemoryError` is caught:

```text
Python312.efi -S
>>> exec("try:\n import json\nexcept MemoryError:\n print('caught')")
>>> raise SystemExit
Shell> exit          -> no hang
```

This is the tightest pair in the whole investigation: **T7 and T8 differ in exactly one thing —
whether the overflow exception is caught — and that flips hang to clean.** The truth table above is
now confirmed on all four rows, and the trigger is pinned to what `PyErr_Print()` does that a
`try`/`except` does not: display the traceback and store it in `sys.last_type`/`last_value`/
`last_traceback`, pinning the overflow's frame objects while the interpreter keeps running.

**T9 — reproduce without the REPL.** This reproduces the REPL's handling faithfully in a script:
print the traceback *and* pin it via `sys.last_*`, then keep going. Stage as `t9.py`:

```python
import sys
try:
    import json
except MemoryError:
    sys.last_type, sys.last_value, sys.last_traceback = sys.exc_info()
    sys.excepthook(sys.last_type, sys.last_value, sys.last_traceback)
s = input('t: ')
print('ok', s)
```

```text
Python312.efi -S t9.py
```

A hang here **removes the REPL from the repro entirely** and confirms the model. That matters a
lot: it turns this into a scripted, non-interactive test that can go in the smoke doc and be run
unattended.

If T9 hangs, split it to find which half is load-bearing — drop the `sys.excepthook(...)` line to
test **retention alone**, or keep the `excepthook` call and `del sys.last_type, sys.last_value,
sys.last_traceback` after it to test **printing alone**.

## Firmware-side evidence — worth collecting now

Proposed long ago and never run, because there was no short repro. T7 gives one, so this is now
cheap. The Shell prompt returns normally, so `memmap` can be run **before** typing `exit`:

```text
memmap                                   (fresh boot, baseline)
Python312.efi -S -c "import re; print('ok')"
memmap                                   (known-clean run)
Python312.efi -S
>>> import json
>>> raise SystemExit
memmap                                   (known-hanging run, BEFORE typing exit)
```

Diff free-page totals and descriptor counts across the three. This is the first direct look at
what the hanging run leaves behind for BDS, and unlike everything above it does not depend on
guessing the mechanism first. Requires a Debug shell profile for `memmap`.

### Superseded tests — the one uncovered cell

**T4 — console read after a large but non-overflowing import.** Quoting-free, and `import re`
(42 modules, ~90 KB) is known to stay under the bound:

```text
Python312.efi -S -c "import re; s=input('t: '); print('ok', s)"
```

A hang here means the overflow is not needed at all — merely reading the console after a big
import is enough, which would point at the firmware console path running on a stack already
mostly consumed.

**T5 — console read after the overflow.** This needs `try`/`except`, so use a staged script
rather than fighting `-c` quoting in the Shell:

```python
try:
    import json
except MemoryError:
    print('caught')
s = input('t: ')
print('ok', s)
```

```text
Python312.efi -S t.py
```

This is the exact cell the matrix is missing — overflow, then a console read, on the **normal**
exit route and outside the REPL. A hang isolates the trigger completely.

### Caveat: `min_rsp` cannot see firmware frames

"Depth is ruled out" is narrower than it sounds. `stack_min_rsp` is updated **only inside
`PyOS_CheckStack()`**, so it samples **Python's** rsp and nothing else. Firmware code called *from*
Python — file I/O down the FAT and block-driver chains, console I/O, timer notifies at raised TPL —
pushes frames **below** the sampled point and is invisible to the instrumentation. The two
`import json` runs measuring 256 bytes apart therefore means only that **Python** reached the same
depth in both, **not** that the hardware did.

This gives T2 a concrete mechanism if it hangs: a **firmware console call made while Python is
already ~107 KB deep** on a ~128 KB stack. That is stack pressure the measurement cannot observe,
and it is specific to VS2022 because that image shares the firmware stack with the Shell while GCC
runs on a private 64 MB `malloc`'d buffer.

## Why this family of bugs is VS2022-only

One divergence in `edk2main.c` explains all of it. VS2022 sets `PY_UEFI_MSVC_368_ENTRY`
(`Python312.inf:1073`, `Python312_MIN.inf:327`) and returns **before** `edk2_switch_stack()` and
`py_install_idt()`; GCC falls through to both. Two consequences follow:

| | VS2022 | GCC |
|---|---|---|
| Stack | UEFI **firmware** stack, ~128 KB | private `malloc`'d **64 MB** (`edk2stack.h:5`) |
| Headroom | `import re` lands within **6 KB** of the bound | never close |
| An overrun damages | **live Shell/BDS state** — surfaces only at Shell `exit` | a buffer that is freed anyway |
| `PyOS_CheckStack()` bound | was NULL until 2026-09-08 | always valid |

The `-b NOOPT` flavour compounds the first row: MSVC without optimisation allocates every local up
front with no inlining or frame reuse, so frames are much fatter than a release build's.

**Why the divergence exists:** not by choice. `edk2main.c:203-205` and `Python312.inf:1070-1072`
record that without the 368 path VS2022 **hung inside `ShellCEntryLib`** after the stack switch
(boot stopped at `before ShellCEntryLib`), so a boot hang was traded for an exit hang. Note
`edk2stack.nasm` and `edk2handler.nasm` are **not** toolchain-tagged (`Python312.inf:62,69`), so
`edk2_switch_stack` is already compiled into the VS2022 image — the assembly is not the blocker.

## Still open after this

The **capability** gap is untouched: VS2022 runs on the ~128 KB firmware stack, so `json`,
`logging` and pyreadline remain unusable there and will keep raising `MemoryError`. That needs the
stack switch to work under MSVC — cheapest experiment is to switch the stack but **skip
`py_install_idt()`**, since `edk2_switch_stack()` and `py_install_idt()` are independent calls that
the existing comments blame as a pair.
