# Lab: VS2022 FULL pyreadline — Shell `exit` hang reproduces (2026-09-07)

**Branch:** `feature/python-3.12.13-vs2022` · **Code state:** **`3afa03f5`** (same state pinned by
`python312-unified-full-lab-2026-09-04`; all commits since are docs-only)
**Toolchain:** **VS2022 FULL**, `-b NOOPT`, `BUILD_PYTHON312_FULL=TRUE`, `PY_UEFI_MSVC_368_ENTRY`
**Procedure:** [`../Python312_Smoke_Tests.md`](../Python312_Smoke_Tests.md) **§5.0** phases 1–5
**Result:** **pyreadline hang REPRODUCES on VS2022** — confirms the historical failure at the
current pinned code state. Manufacturing stdio policy stands.

---

## Observed

| Phase | Content | Result |
|------:|---------|--------|
| **1** | Stub default, env unset (§5.2) | **Pass** — teardown clean, Shell `exit` → firmware, **no hang** |
| **2** | Non-interactive opt-in (§5.3) | **Test passes**, then **Shell `exit` HANGS** |
| **3** | Interactive (§5.4) | `import readline` OK; **Up/Down arrow history works** after typing statements; then **Shell `exit` HANGS** returning to BIOS setup |
| **5** | Clear env, re-confirm stub (§5.6) | **Pass** — `exit` clean, **no hang** |

Phase 4 was not reported. Tab completion in phase 3 was not reported — history recall via
Up/Down was.

---

## What this narrows down

**1. The hang is pyreadline-specific, not a teardown regression.** Phases 1 and 5 both tear
down cleanly on the *same image*, differing only in whether the real readline path loaded. So
`edk2_console_detach_readline`, interpreter finalize and the MSVC 368 entry path are all fine in
the default configuration — nothing here contradicts the 2026-09-04 sign-off.

**2. Interactive use is NOT required to trigger it — this is the key new detail.** Phase 2 hung
with **no arrow keys pressed and no interactive REPL**, only a one-shot
`-S -c "import readline, …"`. The historical write-ups describe this as a *REPL* / line-editing
problem; it is not. **Merely importing the real `readline` module is sufficient.** At import,
`readline.py` runs:

```python
console.install_readline(rl.readline)   # -> edk2console.install_readline_hook
...
rl.read_history_file()
```

so hook install alone poisons the Shell `exit` path. That makes the repro far cheaper than an
interactive session and rules out keystroke handling as the cause.

**3. Python exits cleanly; the firmware handoff does not.** In both failing phases the
interpreter returned to **`Shell>`** normally. Only the subsequent Shell **`exit`** to
firmware/setup hangs. So the defect is in what is left behind for the Shell, not in Python
shutdown.

**4. It does not survive a power-cycle.** Phase 5 was clean on the following boot, so whatever
state is corrupted is not persisted to NVRAM.

---

## Next diagnostic — instrumentation is already in the image

**`/DPY_UEFI_BOOT_TRACE=1` is already on `MSFT:*_*_*_CC_FLAGS` in `Python312.inf`**, so
`PY312_CONSOLE_TRACE` (`edk2console.c`) and `py312_boot_print_ascii` are **live in the image
already tested** — no rebuild needed.

### There are TWO `detach_readline` calls — check the second one

This is the trap that makes a naive reading useless. `edk2_console_detach_readline()` runs at
**startup** and again **after** user code:

| # | Call site | When |
|---|-----------|------|
| **1** | `python.c` `main()`, before `Py_BytesMain` | Always, at startup — **harmless, always prints `enter`/`leave`** |
| **2** | `main.c` `Py_RunMain()`, after `pymain_run_python()` | **The one that matters** — the only detach that runs *after* `import readline` installed the hook |

So "`enter` and `leave` appeared" proves nothing on its own. Anchor on the line
**`Py_RunMain after pymain_run_python`** and read only what follows it.

### Expected ladder (phase 2, `-S -c "import readline …"`)

```text
Python312 boot: main enter
Python312 boot: after _PyMem_SetupAllocators
Python312 boot: edk2_console_detach_readline enter        <-- call #1 (startup, ignore)
Python312 boot: stop_timer: already off
Python312 boot: edk2_console_detach_readline leave
Python312: enter main
Python312 boot: before Py_BytesMain
Readline True                                             <-- script output
Python312 boot: Py_RunMain after pymain_run_python         <-- ANCHOR: read from here down
Python312 boot: edk2_console_detach_readline enter        <-- call #2 (post-import)
Python312 boot: stop_timer: ...
Python312 boot: edk2_console_detach_readline leave
Python312 boot: Py_RunMain after Py_FinalizeEx
Python312 boot: after Py_BytesMain
Shell>
```

### Decision table — applies to call #2 only

| After the anchor line | Interpretation | Where to look |
|-----------------------|----------------|---------------|
| `enter` **and** `leave` present | Detach completed. The leak is **after** it — handle/ConIn state left for the Shell, or an event never closed. **Most likely**, since Python reached `Shell>` | `CloseProtocol` semantics; whether the Shell's own ConInEx consumer is disturbed; `edk2_console_handoff_to_shell` is **not** on this path |
| `enter` but **no** `leave` | Hang is **inside** detach — `edk2_console_drain_input()`'s `while (ReadKeyStrokeEx…)` loop, or `CloseProtocol`. Contradicts reaching `Shell>`, so would be a surprise worth reporting | `edk2console.c` lines 100–160 |
| **Anchor line present, no `enter` at all** | Detach did **not** run post-import — the `#ifdef UEFI_C_SOURCE` block was compiled out or skipped | `main.c` ~730 |
| **Anchor line absent** | `Py_RunMain` never reached that point | `pymain_run_python` |
| `stop_timer: already off` at call #2 | The `getkeys` **timer was never created** — so the timer is **not** the culprit for a non-interactive hang | — |
| `stop_timer: TimerCancel` / `CloseEvent` / `done` at call #2 | A periodic timer **was** live and got cancelled — compare with phase 3, where interactive use is more likely to have started it | `edk2_console_start_timer` |

**Comparing phase 1 with phase 2 is the highest-value signal.** Phase 1 exits cleanly, so any
line present at call #2 in phase 2 but absent in phase 1 is the delta that breaks Shell `exit`.

**Known dead end — do not retry blind:** `edk2console.c` documents that resetting ConIn does
not help, and both directions were already tried:

> *"Do not Reset ConIn here; `Reset(TRUE)` hangs after REPL exit and `Reset(FALSE)` has hung the
> next Shell launch on VS2022. Drain only."*

So the fix is not a ConIn reset. Let the trace decide the direction before changing code.

### Capture

The trace lines are printed by `Print()` to **ConOut**, so shell `>` redirection is not
dependable for them. They are, however, the **last lines before the `Shell>` prompt**, so they
are on screen when the prompt returns — read them before typing `exit`. Serial / BMC console
capture is best if available; photographing the screen is an acceptable fallback.

---

## Status

**VS2022 pyreadline: NOT signed off — hang confirmed at `3afa03f5`.** Manufacturing default
remains **stdio**, and this run is positive evidence for that policy rather than a new defect.

**GCC is unaffected** — phases 1–5 all pass at the same code state
([`2026-09-07_GCC_FULL_pyreadline_phases.md`](./2026-09-07_GCC_FULL_pyreadline_phases.md)),
which keeps this firmly in the **GCC-vs-VS2022 divergence** category (deviations §11.3): the two
toolchains use different firmware entry paths, and this is a runtime difference from the **same
commit**.

**Operator note:** use **`set -v`** so the variable clears on the power-cycle each hang forces;
plain `set` is non-volatile and would persist (§5.6).
