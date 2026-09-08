# Lab: the VS2022 stack switch never worked because the NASM used the wrong ABI (2026-09-08)

**Branch:** `feature/python-3.12.13-vs2022`
**Toolchain:** VS2022 X64, all flavours (also affects `Python312_MIN.inf`)
**Result:** **Root enabler of the entire VS2022 bug family identified.** `edk2_switch_stack()`,
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
