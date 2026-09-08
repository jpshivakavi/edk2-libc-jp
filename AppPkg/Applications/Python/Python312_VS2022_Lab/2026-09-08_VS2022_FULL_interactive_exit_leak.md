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
