# Design — surviving CPU faults in UEFI Python (`edk2_seh_*`)

**Status: §2.1 and §2.2 defect fixes APPLIED and BUILT 2026-09-09 (VS2022 MIN, swept clean —
[`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) §7.1). API in §4 is design only — not
implemented.** Decision recorded 2026-09-09: implement guarded memory primitives on the
existing builtin **`uefi`** module. The two defect fixes were taken first and independently, because
they are wrong regardless of whether the API is ever built; they land in a **dead path**, so they
change no observable behaviour and ride along with the next MIN rebuild rather than needing a sweep
of their own.

Context: deviations
[`Python312_VS2022_GCC_Toolchain_Deviations.md`](./Python312_VS2022_GCC_Toolchain_Deviations.md)
§11.8 #3/#4 — as of 2026-09-08 both toolchains install the custom IDT and **report** a CPU fault
(`unhandled CPU exception N rip=… cr2=…`) and then spin forever. Reporting was the whole win;
**recovery** is what this document is about.

---

## 1. The mechanism already exists — it has no caller

`efi/src/edk2excep.c` implements a complete setjmp/longjmp fault-recovery path:

| Piece | Where | What it does |
|-------|-------|--------------|
| `edk2_seh_try()` | `edk2excep.c:25` | Pushes a context onto `g_context[10]`, stamps `EDK2_SEH_MAGIC`, returns `&entry->ret_context` for the caller to `setjmp()` |
| `edk2_seh_catch()` | `edk2excep.c:42` | Pops the context; with `should_raise` non-zero, hands back `exc_kind` and a copy of `EFI_SYSTEM_CONTEXT_X64` |
| `py_handle_exception()` recovery branch | `edk2excep.c:123-129` | When `g_context_index >= 0`: records `exc_kind` and the full context into the top entry, then `longjmp(entry->ret_context, 1)` |
| `py_handle_exception()` unhandled branch | `edk2excep.c:75-120` | When `g_context_index < 0`: reports, `raise(signum)`, then `while (exc_trap)` spins |

**`edk2_seh_try` and `edk2_seh_catch` are referenced nowhere outside their own definition and
header** — confirmed by searching the whole `Python-3.12.13` tree. So `g_context_index` is
permanently `-1`, the recovery branch is dead, and every fault takes the spin path. The work is not
to write a recovery mechanism; it is to give the existing one a caller, and to fix what has never
been exercised.

Intended call shape:

```c
jmp_buf *jb = (jmp_buf *)edk2_seh_try();
if (jb == NULL) { /* nesting depth exceeded */ }
if (setjmp(*jb) == 0) {
    /* guarded region */
    edk2_seh_catch(0, NULL, NULL, 0);          /* no fault */
} else {
    uint64_t kind = 0;
    uint8_t  ctx[sizeof(EFI_SYSTEM_CONTEXT_X64)];
    edk2_seh_catch(1, &kind, ctx, sizeof ctx); /* faulted */
}
```

`setjmp` here is **EDK2's `SetJump`**, not a libc implementation — `StdLib/Include/setjmp.h:45`
defines `setjmp(env)` as `(INTN)SetJump((env))` with `jmp_buf` = `BASE_LIBRARY_JUMP_BUFFER[1]`. That
matters twice over: semantics are identical on MSVC and GCC (no SEH-based unwinding on either), and
`LongJump` restores `MxCsr` and `XMM6-XMM15` itself.

---

## 2. Two defects to fix before anything calls this

Both are latent only because the path is dead.

### 2.1 Interrupts stay disabled after recovery — **FIXED 2026-09-09**

`py_common_interrupt_entry` starts with `cli` (`edk2handler.nasm:70`). The normal return path
restores `RFlags` into the interrupt frame (`edk2handler.nasm:270`) and returns via `iretq`, which is
what puts `IF` back. **A `longjmp` never reaches that `iretq`**, so `IF` stays clear for the rest of
the run.

Consequence: the firmware timer stops firing. `edk2console` drives a timer event, and `stop_timer` /
console behaviour would silently change after a recovered fault. Worse, it would look like a
*different* bug appearing later in the run.

Fix, as applied in `edk2excep.c` — re-enable interrupts on the recovery path, conditioned on the
state the faulting code was in:

```c
  if(entry->exc_context.Rflags & BIT9)    /* IF */
    EnableInterrupts();                   /* MdePkg BaseLib */
  longjmp(entry->ret_context, 1);
```

Note the field is **`Rflags`**, lowercase `f` (`MdePkg/Include/Protocol/DebugSupport.h:203`).
`BIT9` is `Base.h:355`; `EnableInterrupts()` needs `#include <Library/BaseLib.h>`, added — the
library itself is already in the module's link closure via `UefiLib`/`DebugLib`.

Do not enable unconditionally — a fault taken inside a critical section that had already cleared `IF`
must not come back with interrupts on. Note the stock epilogue re-issues `cli` immediately after
`call py_handle_exception` (`edk2handler.nasm:241`), which is evidence the design already tolerates
the handler enabling interrupts.

### 2.2 `edk2_seh_try()` corrupts its index on overflow — **FIXED 2026-09-09**

```c
if(++g_context_index >= EDK2_SEH_CONTEXT_SIZE)
   return NULL;
```

The increment has already happened. On overflow the index is left at `EDK2_SEH_CONTEXT_SIZE` and
**nothing brings it back down**, because the caller received `NULL` and therefore will not call
`edk2_seh_catch()`. The next fault then evaluates `g_context + 10`, writes a whole
`EFI_SYSTEM_CONTEXT_X64` **out of bounds**, and `longjmp`s through whatever was in that memory.

Fix: decrement before returning, so a failed `try` is a no-op:

```c
if(++g_context_index >= EDK2_SEH_CONTEXT_SIZE) {
   --g_context_index;
   return NULL;
}
```

### 2.3 Verified *not* a problem

- **`jmp_buf` alignment.** x64 `BASE_LIBRARY_JUMP_BUFFER_ALIGNMENT` is **8**
  (`MdePkg/Include/Library/BaseLib.h:59`), and `ret_context` sits at offset 8 in
  `edk2_seh_context_t` after `uint64_t magic`. Aligned. Had it been 16, `SetJump` would have faulted
  *inside the fault handler* — worth having checked.
- **Callee-saved SSE state.** `LongJump` restores `MxCsr` and `XMM6-XMM15`, so skipping the
  handler's `fxrstor` (`edk2handler.nasm:248`) does not lose anything the ABI requires a callee to
  preserve. `XMM0-5` and x87/MMX are volatile and the compiler does not expect them across a call.
- **Skipped CR restores.** The epilogue reloads CR0/CR2/CR3/CR4/CR8 from the saved context
  (`edk2handler.nasm:256-267`), but the handler never modified them, so skipping is harmless.

---

## 3. Why the API must be narrow

**A `longjmp` out of arbitrary Python code cannot be made safe.** If a fault lands deep inside
CPython — mid-`dict` resize, inside an allocator, while a container is temporarily inconsistent —
the `longjmp` discards every intervening C frame with no unwinding. No `DECREF`s run, nothing is
freed, and a half-mutated object or a held internal lock stays that way. The interpreter would
continue running on top of that. It might work for a hundred faults and then produce a corruption
that looks unrelated.

So the guarded region must contain **only leaf code that touches memory and holds no Python
references**. Fortunately that is exactly the use case a UEFI Python is for: probing MMIO, PCI
config space, or a physical address where a wrong guess currently hangs the machine and needs a
power cycle.

**Rejected: `uefi.seh_call(fn, *args)`** guarding an arbitrary callable. Tempting, and would be a
useful lab tool, but it is a corruption-shaped footgun in a manufacturing image and there is no way
to bound the damage from the C side.

---

## 4. Proposed API

Host: the existing builtin **`uefi`** module. It is `Modules/posixmodule.c` compiled with
`UEFI_C_SOURCE`, which sets `INITFUNC PyInit_uefi` / `MODNAME "uefi"` (`posixmodule.c:536-539`), so
adding methods there needs **no `config.c` and no INF changes**.

*Tradeoff accepted:* hardware-poke primitives on the posix module is not a clean home. A separate
`edk2seh` builtin would be tidier but costs `config.c` registration plus `[Sources]` in both INFs.
Chosen for zero plumbing; revisit if the surface grows beyond these three calls.

```python
uefi.mem_read(addr, size)           # size in (1, 2, 4, 8) -> int
uefi.mem_write(addr, size, value)   # -> None
uefi.mem_probe(addr, size=1)        # -> bool, never raises FaultError
```

On a fault, `mem_read` / `mem_write` raise:

```python
uefi.FaultError            # subclass of OSError
    .vector                # EFI_EXCEPTION_TYPE, e.g. 13 #GP, 14 #PF
    .rip                   # faulting instruction
    .cr2                   # faulting address; meaningful only for vector 14
    .error_code            # ExceptionData
```

`mem_probe` is the same guarded access with the result reduced to a bool — the common "is this
address there at all" question, without exception handling at the call site.

**A side benefit worth having: this makes the fault path testable on MIN.** §5.8 injects a fault with
`ctypes.cast(...)`, and MIN has no `_ctypes` — nor any other way to dereference an arbitrary address
from pure Python — so today MIN can only show that `py_install_idt()` *ran* (`before py_install_idt`
in the boot trace) and never that a fault actually **routes** to `py_handle_exception()`. Hosting
these on the `uefi` module, which is built in every configuration, closes that gap: a `FaultError`
carrying the right vector is itself proof the IDT entry is live. Noted 2026-09-09 after §5.8 came
back `ModuleNotFoundError` on MIN as expected.

Caller rules to document:

- **Alignment is the caller's problem.** A misaligned access can fault on its own; that now reports
  instead of hanging.
- **`size` is validated in Python-visible code**, before the guarded region, so a bad argument is a
  `ValueError` and never reaches the fault path.
- **Nesting depth is 10** (`EDK2_SEH_CONTEXT_SIZE`). Exceeding it raises `RuntimeError` rather than
  faulting — which is only true once §2.2 is fixed.

---

## 5. Implementation sketch

```c
static PyObject *
uefi_mem_read(PyObject *self, PyObject *args)
{
    unsigned long long addr;
    int size;
    if (!PyArg_ParseTuple(args, "Ki", &addr, &size))
        return NULL;
    if (size != 1 && size != 2 && size != 4 && size != 8)
        return PyErr_Format(PyExc_ValueError, "size must be 1, 2, 4 or 8");

    jmp_buf *jb = (jmp_buf *)edk2_seh_try();
    if (jb == NULL)
        return PyErr_Format(PyExc_RuntimeError, "SEH nesting depth exceeded");

    volatile unsigned long long out = 0;   /* volatile: written before longjmp */
    if (setjmp(*jb) == 0) {
        switch (size) {
        case 1: out = *(volatile uint8_t  *)(uintptr_t)addr; break;
        case 2: out = *(volatile uint16_t *)(uintptr_t)addr; break;
        case 4: out = *(volatile uint32_t *)(uintptr_t)addr; break;
        case 8: out = *(volatile uint64_t *)(uintptr_t)addr; break;
        }
        edk2_seh_catch(0, NULL, NULL, 0);
        return PyLong_FromUnsignedLongLong(out);
    }

    uint64_t kind = 0;
    EFI_SYSTEM_CONTEXT_X64 ctx;
    edk2_seh_catch(1, &kind, (uint8_t *)&ctx, sizeof ctx);
    return uefi_raise_fault(kind, &ctx);       /* sets FaultError, returns NULL */
}
```

**Every local read after `setjmp` returns non-zero must be `volatile`** — `SetJump` restores
registers, so a non-volatile local held in a register is indeterminate on the recovery path. This is
the single easiest thing to get wrong here and it fails intermittently, which is the worst failure
mode.

---

## 6. Test plan

All on hardware; the fault-injection procedure and its result table are
[`Python312_Smoke_Tests.md`](./Python312_Smoke_Tests.md) §5.8. Both toolchains — `edk2excep.c` and
`posixmodule.c` compile into each.

| # | Test | Expected |
|--:|------|----------|
| 1 | `uefi.mem_read(0x800000000000, 8)` (non-canonical) | `FaultError` with `vector == 13`, **process survives** |
| 2 | `uefi.mem_read` of a known-good address, e.g. the loaded image base | Correct value, no fault |
| 3 | Test 1, then test 2 in the same process | Second call still correct — proves recovery left the interpreter usable |
| 4 | **Interrupts alive after recovery.** Test 1, then something timer-dependent — `time.sleep(2)` returning in about 2 s, or a `pyreadline` session | Normal behaviour. A hang or an instant return is §2.1 regressing |
| 5 | `uefi.mem_probe` on a bad and a good address | `False`, then `True`, no exception either way |
| 6 | 11 nested guarded calls | 11th raises `RuntimeError`, and a *subsequent* bad read still raises `FaultError` rather than hanging — the §2.2 regression test |
| 7 | 100× recovered fault in a loop | No leak, no slowdown, still responsive |
| 8 | Full §3/§4/§5 sweep | Unchanged — this must not disturb anything existing |

Test 4 is the one that would otherwise be missed, and test 6 is the only thing that distinguishes a
fixed §2.2 from a latent out-of-bounds write.

---

## 7. Risks and open questions

- **Faults that are not recoverable at all.** Machine check (18) and double fault (8) may leave the
  CPU in a state where returning to Python is meaningless. Current vector list in `py_install_idt()`
  includes `EXCEPT_X64_MACHINE_CHECK`. Consider excluding it from the guarded path and letting it
  take the spin/report route.
- **Nested fault inside the handler.** If the recovery path itself faults — e.g. a bad
  `entry->ret_context` — behaviour is undefined. The magic check in `edk2_seh_catch()` is a partial
  guard; `py_handle_exception()` does not check the magic before using the entry.
- **Interaction with `PyOS_CheckStack()`.** A stack-overflow `MemoryError` and a guarded #PF are
  different paths and should not interact, but a fault taken while the stack guard is already tripped
  is untested.
- **`raise(signum)` is skipped on the recovery path**, which is correct — the Python-level exception
  replaces it — but means `_signal` handlers never see recovered faults. Worth stating in the
  docstring.

---

## 8. Explicitly out of scope

- Guarding arbitrary Python callables (§3).
- Making the spin path recoverable *without* a `try` in scope — a fault with no guard installed keeps
  reporting and spinning, unchanged.
- I/O port access (`in`/`out`), PCI config helpers, or an MMIO object wrapper. Those become easy once
  the primitives land, but each is its own decision.
- Anything on IA32.

---

*Created 2026-09-09. Design only; §2.1 and §2.2 are real defects in shipped code and should be
fixed even if the API is never built.*
