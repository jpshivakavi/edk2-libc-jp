# Lab: the VS2022 stack switch never worked because the NASM used the wrong ABI (2026-09-08)

**Branch:** `feature/python-3.12.13-vs2022`
**Toolchain:** VS2022 X64, all flavours (also affects `Python312_MIN.inf`)
**Result:** **FIXED AND WORKING ON HARDWARE.** `Python312.efi -S -c "import json; print('ok')"`
prints `ok` and Shell `exit` is clean — the first time either has been possible on VS2022 in this
port. Took two defects, both found here: the NASM ABI mismatch below, and `rsp`-relative frame
access across the switch (*"Attempt 1"* / *"Attempt 2"*).

**Root enabler of the entire VS2022 bug family identified.** `edk2_switch_stack()`,
`edk2_get_idtr()` and `edk2_set_idtr()` are hand-written NASM that read their arguments from
**`rdi`/`rsi`** — the System V ABI. **MSVC passes them in `rcx`/`rdx`.** So on every VS2022 build
`edk2_switch_stack` computed `rsp` from whatever junk was in `rdi`/`rsi`.

That is precisely the failure the `PY_UEFI_MSVC_368_ENTRY` workaround was created to dodge, and it
was never a firmware incompatibility.

---

## How it was found

Not by looking for it. Ten Python-level tests (T1–T10) narrowed the Shell-`exit` hang to a
two-line REPL repro and eliminated every mechanism that could be tested from Python. With the
symptom exhausted, the remaining question was why the VS2022 image runs on the firmware stack at
all — which led to the code the workaround comment blames.

## The defect

```13:23:AppPkg/Applications/Python/Python-3.12.13/PyMod-3.12.13/efi/src/edk2stack.nasm
global edk2_switch_stack
        
edk2_switch_stack:
        add rdi, rsi
        mov rax, 0
        mov [rdi-0x8], rax        
        mov [rdi-0x10], rsp        
        mov rax, [rsp]        
        mov [rdi-0x208], rax
        lea rsp, [rdi-0x208]
        ret
```

The C declaration is a **plain** function, not `EFIAPI`:

```c
void edk2_switch_stack(uint64_t stack, uint64_t size);
```

so it inherits the compiler's default convention:

| Toolchain | Convention | Arg 1 | Arg 2 |
|-----------|-----------|-------|-------|
| GCC (EDK2 X64, non-`EFIAPI`) | System V AMD64 | `rdi` | `rsi` |
| **MSVC** | **Microsoft x64** | **`rcx`** | **`rdx`** |

Under MSVC, `add rdi, rsi` sums two unrelated live registers and `lea rsp, [rdi-0x208]` points
`rsp` at an arbitrary address. The very next `ret` reads a return address from that address. A hang
or immediate fault inside `ShellCEntryLib` is the only possible outcome — which is exactly what
`edk2main.c:203-205` and `Python312.inf:1070-1072` recorded:

> *VS2022 3.12 hang reproduces inside ShellCEntryLib only after stack switch — try 368-style path.*

**Same bug in the IDT helpers** (`edk2handler.nasm`): `edk2_get_idtr` does `sidt [rdi+6]` and
`edk2_set_idtr` does `lidt [rdi+6]`. Under MSVC the pointer is in `rcx`, so these would `sidt`/
`lidt` through a garbage pointer. That is why the stack switch and the IDT install appeared to fail
*as a pair* — they share one root cause, and the comments blaming them jointly were describing a
symptom.

**Not affected:** `edk2_revert_stack`, `edk2_read_rsp` and `edk2_pause` take **no arguments**, which
is why `edk2_read_rsp()` worked fine on VS2022 and the `stack_limit` / `stack_min_rsp`
instrumentation gave correct readings all along. `py_common_interrupt_entry` is also fine — it
already loads `rcx`/`rdx` per the EDK2 x64 convention before calling `py_handle_exception`.

## Why this explains everything

`PY_UEFI_MSVC_368_ENTRY` exists only to avoid the broken switch. It returns before
`edk2_switch_stack()`, so VS2022 runs the interpreter on the **UEFI firmware stack (~128 KB)** while
GCC gets **64 MB**. From that single fact:

- deep imports overflow on VS2022 and not on GCC
- the overflow lands in **live Shell/BDS memory**, so Python's teardown looks perfect and only
  Shell `exit` hangs
- `PyOS_CheckStack()` had no bound, because `g_edk2_globals.stack` is assigned after the early
  return
- `json`, `logging` and pyreadline are unusable on VS2022

## The fix

ABI-aware NASM, gated on a define supplied only to MSFT builds, so **GCC codegen is byte-for-byte
unchanged** and its sign-off stands.

| File | Change |
|------|--------|
| `efi/src/edk2stack.nasm` | `%ifdef PY_UEFI_MS_ABI` → `ARG1`/`ARG2` = `rcx`/`rdx`, else `rdi`/`rsi`; `edk2_switch_stack` uses `ARG1`/`ARG2` |
| `efi/src/edk2handler.nasm` | Same block; `edk2_get_idtr`/`edk2_set_idtr` use `ARG1` |
| `Python312.inf`, `Python312_MIN.inf` | `MSFT:*_*_*_NASM_FLAGS = -DPY_UEFI_MS_ABI` |
| `efi/src/edk2main.c` | New opt-in `PY_UEFI_MSVC_STACK_SWITCH` takes the 64 MB path on MSVC; `py_install_idt()`/`py_restore_idt()` skipped there unless `PY_UEFI_MSVC_IDT` is also defined; MSVC gets correct 512-byte **round-up** alignment for the stack base |

**The ABI fix alone is inert in the current default build** — with `PY_UEFI_MSVC_368_ENTRY` still
in force, none of the three fixed functions are called on the MSVC path. So it carries no risk on
its own, and the behavioural change is entirely behind the opt-in.

### Stack alignment, also MSVC-specific

`edk2_switch_stack()` leaves `rsp` at `base+size-0x200`, so `base+size` must be 16-byte aligned or
MSVC's `movaps` spills fault. The existing expression is:

```c
uint64_t aligned_stack = (uint64_t)stack + (((uint64_t)stack) % 512);
```

That **does not align anything** — it offsets the base by an arbitrary amount. The MSVC path now
rounds *up* to 512 instead (`(base + 511) & ~511`), which is safe because the `malloc` over-
allocates by 1024. The GCC expression is deliberately left untouched: it is wrong, but GCC is the
signed-off toolchain and correcting it needs its own re-test.

## How to try it

Add to the **MSFT** `CC_FLAGS` line in `Python312.inf` (`:1073`), then rebuild:

```
/DPY_UEFI_MSVC_STACK_SWITCH=1
```

Rebuild:

```bat
cd /d C:\Users\njayapra\github\edk2
call edksetup.bat
cd /d %EDK2_LIBC_PATH%
build -t VS2022 -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE
```

### Boot trace tells you immediately which path ran

Expected on the opt-in path — note `firmware stack rsp=...` is **absent**, replaced by:

```text
Python312 boot: before switch_stack
Python312 boot: skipping py_install_idt (MSVC stack-switch opt-in)
Python312 boot: before ShellCEntryLib
```

If boot stops at `before ShellCEntryLib`, the switch still fails and the ABI fix was not the whole
story — revert the flag, no harm done.

---

## Attempt 1 on hardware: the ABI fix works, but `ShellCEntryLib` hangs

```text
Python312: UefiMain
Python312 boot: UefiMain enter
Python312 boot: before switch_stack
Python312 boot: skipping py_install_idt (MSVC stack-switch opt-in)
Python312 boot: before ShellCEntryLib
<stuck>
```

**The ABI fix is confirmed good.** Two trace lines printed *after* `edk2_switch_stack()` returned,
so the switch executed, `rsp` moved to a valid address, and ordinary C code kept running on the new
stack. Before the fix this could not have happened — `rsp` would have been garbage and the `ret`
inside `edk2_switch_stack` would have died immediately.

**Second, independent defect: `edk2_switch_stack()` moves `rsp` out from under a *running*
function.** `UefiMain` has already executed its prologue, so its locals and spilled parameters live
in a frame on the *old* stack:

- **GCC at `-O0` keeps a frame pointer**, so those accesses go through `rbp`, which still points
  into the old stack. It keeps working, which is why this was never noticed.
- **MSVC x64 uses a fixed frame with `rsp`-relative addressing and no frame pointer.** After the
  switch, `image`, `systab` and `status` resolve to the *new* stack at the old offsets — garbage.

The `PY312_BOOT_PRINT` lines still worked because they pass only string literals. The very next
statement, `ShellCEntryLib(image, systab)`, reads two of those broken parameters and hands
`ShellCEntryLib` garbage handles. Hanging there is expected.

### Fix for attempt 2

Read the handles from `g_edk2_globals` — already populated well before the switch — and park the
return value in `g_edk2_globals.switch_status`. Globals are RIP-relative, so they are unaffected by
`rsp`. `UefiMain`'s own frame becomes addressable again after `edk2_revert_stack()`, where `status`
is recovered from the global.

Nothing else in `UefiMain` touches its frame between the switch and the revert — the only
statements there are `PY312_BOOT_PRINT` (literals), the skipped IDT calls, the `ShellCEntryLib`
call, and `edk2_revert_stack()` (no arguments). So this is sufficient, not just a patch over one
symptom.

**If attempt 2 gets past `before ShellCEntryLib`, the diagnosis is confirmed.** If it still hangs
there, the remaining suspect is `edk2_switch_stack`'s frame layout itself rather than the C frame,
and the right move is to stop hand-rolling this: EDK2 `BaseLib` provides ABI-correct
`SwitchStack()` plus `SetJump()`/`LongJump()`, which would let the interpreter run on the new stack
via a trampoline and return without ever moving `rsp` under a live frame.

---

## Attempt 2 on hardware: WORKS — VS2022 runs on the 64 MB stack

```text
Python312.efi -S -c "import json; print('ok')"      -> ok
Shell> exit                                          -> clean
```

**`import json` has never succeeded on VS2022 in this port.** It has raised `MemoryError: stack
overflow` (after the `PyOS_CheckStack` fix) or silently corrupted firmware memory and hung Shell
`exit` (before it). Both diagnoses are confirmed by this:

1. the **NASM ABI mismatch** — `rdi`/`rsi` vs `rcx`/`rdx`
2. **`rsp`-relative frame access across the switch** — MSVC has no frame pointer, so `UefiMain`'s
   parameters had to be read from globals

Together they were the entire reason `PY_UEFI_MSVC_368_ENTRY` existed, and therefore the reason
VS2022 ran on the ~128 KB firmware stack. **The root enabler of the whole VS2022 bug family is
fixed.**

### The interactive REPL — the actual defect — is also fixed

```text
Python312.efi
>>> import json          -> no MemoryError
>>> exit()               -> back to Shell
Shell> exit              -> clean
```

This is the sequence that hung on **every** prior attempt, and the one that survived ten rounds of
Python-level bisection in
[`2026-09-08_VS2022_FULL_interactive_exit_leak.md`](./2026-09-08_VS2022_FULL_interactive_exit_leak.md).
That note's final model — *"the hang needs an uncaught stack-overflow `MemoryError`, a printed
traceback retained in `sys.last_*`, continued execution, and structurally requires the REPL"* — was
an accurate description of the **trigger** and a wrong theory of the **cause**. Every one of those
conditions was just a way of reaching a stack depth the ~128 KB firmware stack could not hold. The
REPL "requirement" was never about the REPL: it kept the process alive past the corruption, whereas
`-c` and script routes exited before the damaged firmware memory was touched again.

Note that `import json` no longer raises `MemoryError` at all here, which is the distinction between
this fix and the earlier `PyOS_CheckStack` one. That fix made the overflow *visible and survivable*;
this one means there is no overflow to detect.

### Added: high-water report for the switched path

The `stack high-water` line only existed in the 368 branch. The switched path now prints, after
`edk2_revert_stack()` so the frame is valid again under MSVC:

```text
Python312 boot: switched stack min_rsp=... limit=... size=...
```

`used` should be a small fraction of `size` (`0x4000000` = 64 MB) and `min_rsp` should sit far above
`limit`. Anything near the limit would mean 64 MB is not actually in play.

## Acceptance sweep — still to run

| Test | Expected | Status |
|------|----------|--------|
| `-S -c "import json; print('ok')"` | `ok`, clean `exit` | **PASS** |
| **REPL `import json`**, exit console, Shell `exit` | no `MemoryError`, no hang — **the original defect** | **PASS** |
| `-S -c "import logging; print('ok')"` | `ok`, clean `exit` | **PASS** |
| Phase 8 sweep (smoke doc §3) | unchanged, all pass | **PASS** |
| pyreadline §5.3 | `Readline True`, clean `exit` | **PASS** |
| pyreadline §5.4 | up-arrow history, Tab completion, clean `exit` | **PASS** |
| `switched stack` trace line | `min_rsp` far above `limit`, depth ≪ 64 MB | not yet read |

**Sweep green — the switch is now the default for MSVC.** `PY_UEFI_MSVC_STACK_SWITCH` and
`PY_UEFI_MSVC_368_ENTRY` are both gone; the MSVC-specific pieces are keyed on plain `_MSC_VER`.

The retired configuration is **bit-identical to what was signed off**: the old guard was
`#if defined(PY_UEFI_MSVC_368_ENTRY) && !defined(PY_UEFI_MSVC_STACK_SWITCH)`, and the validated
build defined *both*, so the 368 branch was already dead in the tested image. Removing it changes
no generated code.

## Removed

1. `PY_UEFI_MSVC_368_ENTRY` — both INFs and its branch in `edk2main.c`
2. `PY_UEFI_MSVC_STACK_SWITCH` — the opt-in gate, now unconditional under `_MSC_VER`
3. `PY_UEFI_FIRMWARE_STACK_BUDGET` — dead: `stack_limit` derives from the real allocated base
   instead of a budget guessed down from entry `rsp`
4. `g_edk2_globals.stack_entry_rsp` — only the 368 high-water print used it
5. the *"`MemoryError: stack overflow` is expected"* rows, the §4 shallow-sessions-only warning,
   and the firmware-stack deviation in `..._Toolchain_Deviations.md` §11.1

## Still open

**MIN is untested on this path.** It carried `PY_UEFI_MSVC_368_ENTRY` too, so removing the branch
moved MIN onto the switched stack without a hardware run. Only FULL was swept. Flagged in
`Python312_VS2022_MIN_Build.md`.

**The IDT is still skipped under MSVC** (`PY_UEFI_MSVC_IDT` opts in). The `idtr` helpers now have
the right ABI so it may work, and it would restore fault reporting — but the sweep was signed off
with it off, and it carries its own boot risk. Separate change.

**The GCC alignment expression is still wrong.** `stack + (stack % 512)` offsets by an arbitrary
amount rather than aligning; it should become the same round-up the MSVC path uses, with a GCC
re-test. Harmless today only because GCC has never faulted on it.

**`edk2_alloc_environ()` is called twice**, at `:200` and `:209`. Pre-existing — GCC has always run
both — but MSVC now does too, where the early return used to skip the second. Left alone
deliberately: both signed-off images are built this way.

### Acceptance tests

| Test | Expected if the switch works |
|------|------------------------------|
| `Python312.efi -S -c "import json; print('ok')"` | **`ok`** — no `MemoryError`. 64 MB is ~500× the old headroom |
| `Python312.efi -S -c "import logging; print('ok')"` | **`ok`** |
| **T7:** `Python312.efi -S`, `import json`, `raise SystemExit`, Shell `exit` | **no hang** — the whole point |
| Phase 8 sweep (smoke doc §3) | unchanged, all pass |
| pyreadline §5.3 / §5.4 | `Readline True`, and interactive editing works |
| `stack high-water` trace line | `min_rsp` far **above** `limit`; `used` a small fraction of 64 MB |

A clean `import json` is the single clearest signal: it has been impossible on VS2022 for the
entire port.

## If it works, what becomes obsolete

- `PY_UEFI_MSVC_368_ENTRY` can be retired for X64 MSVC
- `PY_UEFI_FIRMWARE_STACK_BUDGET` becomes dead on that path — `stack_limit` comes from the real
  allocated base instead of a guessed budget
- the `MemoryError: stack overflow` behaviour documented in the smoke doc stops being expected
- VS2022 and GCC finally run the same entry path, so "build parity does not imply runtime parity"
  narrows considerably
