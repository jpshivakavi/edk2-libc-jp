# CHIPSEC platform API: 3.6.8 `edk2` module vs 3.12.13 `uefi` module

Status: **SCOPED, not yet implemented.** 2026-09-09.
Scope decisions taken (§9): **all 19 APIs**, **FULL only** (`Python312.inf`; MIN untouched),
**naming shim option 1** (`PyMod-3.12.13/Lib/edk2.py`).
Source of truth for the old side: `Python-3.6.8/PyMod-3.6.8/Modules/edk2module.c` (4576 lines,
built by `Python368.inf:165`).
Source of truth for the new side: `Python-3.12.13/PyMod-3.12.13/Modules/posixmodule.c`.

Related: `Python312_SEH_Fault_Recovery_Design.md` (the guarded-access primitives this port
should be built on), `Python312_VS2022_GCC_Toolchain_Deviations.md`.

---

## 1. Verdict

`edk2module.c` is **two modules welded together**:

1. An `os`-module implementation for UEFI (~250 entries: `access`, `chdir`, `open`, `read`,
   `stat`, `listdir`, the `W*` wait macros, …). **This half needs nothing carried forward.**
   3.12.13 replaced it wholesale with the upstream CPython 3.12 `posixmodule.c`, adapted for
   UEFI, which is a strict superset — every name in the 3.6.8 table has an equivalent
   `OS_*_METHODDEF` in `posixmodule.c:16268`ff.

2. **19 platform-level functions** at the tail of the table (`edk2module.c:4896-4914`) that have
   no upstream equivalent and that CHIPSEC drives directly. **None of these exist in 3.12.13.**
   This is the entire porting gap.

So the answer to "does anything need carrying forward" is: yes, exactly those 19 functions, plus
a module-naming shim (§5). Nothing else.

## 2. The gap

All 19 are absent from 3.12.13. Signatures are the 3.6.8 docstrings verbatim, because CHIPSEC
calls them positionally and the return shapes (notably the split 32-bit halves) are its ABI —
they must be preserved even where a 64-bit return would be nicer.

| Function | Signature | 3.6.8 implementation | Dependency |
|---|---|---|---|
| `rdmsr` | `rdmsr(msr) -> (lo32, hi32)` | `AsmReadMsr64` | BaseLib |
| `wrmsr` | `wrmsr(msr, lo32, hi32) -> None` | `AsmWriteMsr64` | BaseLib |
| `rdmsr_ex` | `rdmsr_ex(cpu, msr) -> (lo32, hi32)` | `StartupThisAP` onto target AP, else direct | **MP Services** |
| `wrmsr_ex` | `wrmsr_ex(cpu, msr, lo32, hi32) -> None` | same | **MP Services** |
| `cpuid` | `cpuid(eax, ecx) -> (eax, ebx, ecx, edx)` | `AsmCpuidEx` | BaseLib |
| `cpuid_ex` | `cpuid_ex(cpu, eax, ecx) -> (...)` | `CPUIDToRunOnAP` via `StartupThisAP` | **MP Services** |
| `readpci` | `readpci(bus, dev, func, off, size) -> int` | `PciRead8/16/32` + `PCI_LIB_ADDRESS` (`:3881`) | **PciLib** |
| `writepci` | `writepci(bus, dev, func, addr, value, len) -> None` | `PciWrite8/16/32` (`:3897`) | **PciLib** |
| `readio` | `readio(addr, size) -> int` | `IoRead8/16/32`, addr masked to 16 bits | IoLib |
| `writeio` | `writeio(addr, size, value) -> None` | `IoWrite8/16/32` | IoLib |
| `readmem` | `readmem(addr_lo, addr_hi, len) -> bytes` | `malloc` + byte copy loop | none |
| `readmem_dword` | `readmem_dword(addr_lo, addr_hi) -> int` | raw `*(unsigned int*)addr` | none |
| `writemem` | `writemem(addr_lo, addr_hi, buf) -> None` | byte copy loop | none |
| `writemem_dword` | `writemem_dword(addr_lo, addr_hi, val) -> None` | raw store | none |
| `swsmi` | `swsmi(smi_code_data, rax, rbx, rcx, rdx, rsi, rdi) -> None` | `_swsmi` in `cpu.nasm` | **asm (already present, §4)** |
| `allocphysmem` | `allocphysmem(length, max_pa) -> (va,)` | plain `malloc` — being reimplemented (§6.2) | Boot Services |
| `GetVariable` | `(Status, Attributes, Data, DataSize) = GetVariable(name, guid, size)` | `gRT->GetVariable` | Runtime Services |
| `GetNextVariableName` | `(Status, NameSize, Name, Guid) = GetNextVariableName(sz, name, guid)` | `gRT->GetNextVariableName` | Runtime Services |
| `SetVariable` | `(Status, DataSize, Guid) = SetVariable(name, guid, attrs, data, size)` | `gRT->SetVariable` | Runtime Services |

Note the address convention: memory addresses are passed as a **split `(lo32, hi32)` pair**, then
recombined under `#ifdef MDE_CPU_X64`. That is CHIPSEC's convention and should be kept for the
CHIPSEC-facing names, even though our newer `uefi.mem_read` takes a single 64-bit integer.

## 3. Scope: all 19, FULL only

**All 19 are in scope** — the target CHIPSEC version needs the complete surface, so nothing is
deferred and the `_ex` variants are not optional. Two consequences follow directly:

- **MP Services is a hard dependency**, not a maybe. It was the largest single piece of risk in
  the original analysis and it cannot be dropped. That makes the lazy-location requirement in
  §6.1 **mandatory**: the three `_ex` functions (`rdmsr_ex`, `wrmsr_ex`, `cpuid_ex`) must locate
  the protocol themselves, on first call, and raise `OSError` if it is absent. It must not be
  touched at module init, because in 3.12.13 a failure there does not disable three functions, it
  prevents the interpreter from starting at all.
- **`allocphysmem` must be reimplemented, not copied.** Being in scope means the 3.6.8 version's
  32-bit pointer truncation and ignored `max_pa` (§6.2) are now defects we have to fix rather
  than a function we can skip.

**FULL only.** The APIs go in `Python312.inf`; `Python312_MIN.inf` is untouched. This keeps MIN
what it is today — the small, few-dependency image — and means the whole port carries no risk to
the MIN configuration, which is convenient given MIN was only swept for the first time this week.
CHIPSEC is a large pure-Python package that needs a filesystem `Lib` and the FULL module set
anyway, so MIN was never a plausible host for it. Note this also means the port needs testing on
**two** configurations rather than four (VS2022 FULL and GCC FULL), not the 2×2 that items 36-39
in the status doc required.

## 4. What is already in the 3.12.13 tree

Three pieces of the port are already done, which materially reduces the work:

**`cpu.nasm` is already there and already compiled.** `PyMod-3.12.13/Modules/cpu.nasm` (plus
`cpu_gcc.s`, and the IA32 pair) is listed in the `[Sources]` of *both* `Python312.inf:1058` and
`Python312_MIN.inf:317`, for both toolchains. It exports exactly one symbol, `_swsmi`, and
**nothing in C calls it** — it is dead code in every image we ship today, the same pattern
`edk2_seh_*` was in before this month. It is also already written to the MS x64 ABI
(`rcx`/`rdx`/`r8`/`r9` + stack), so it needs none of the NASM ABI corrections that
`edk2stack.nasm` and `edk2handler.nasm` needed. Wiring `swsmi` up is a C wrapper and a prototype,
nothing more.

**The guarded-access machinery exists.** `uefi.mem_read` / `mem_write` / `mem_probe` and
`uefi.FaultError` (`posixmodule.c:16147`ff) already give us fault-survivable access to arbitrary
physical addresses, backed by `edk2_seh_try`/`edk2_seh_catch` and the IDT that is now installed by
default on both toolchains. §6.3 explains why the CHIPSEC `readmem`/`writemem` family should be
layered on top of this rather than copied from 3.6.8.

**The library dependencies resolve.** `AppPkg.dsc:69-70` already maps both `IoLib` and `PciLib`
(`BasePciLibCf8`, i.e. CF8/CFC port access), so `readpci`/`readio` need only an INF-level
`[LibraryClasses]` entry, not a DSC change. `BaseLib` is already in use on the entry path
(`EnableInterrupts` in `edk2excep.c`), and `gRT` resolves through the `UefiLib` chain.

## 5. The module was renamed: `edk2` → `uefi`

This is a hard compatibility break independent of the functions. In 3.6.8 the module is
`edk2` (`edk2module.c:5131-5132`); in 3.12.13 it is `uefi`
(`posixmodule.c:538-539`: `PyInit_uefi` / `MODNAME "uefi"`), and that name is baked into
`Lib/os.py:102`, `Lib/importlib/_bootstrap_external.py:43` and `Lib/pathlib.py:14`.

CHIPSEC does `import edk2`, so even a complete port leaves it broken.

**Decision: option 1, a pure-Python shim** at `PyMod-3.12.13/Lib/edk2.py`. Not `from uefi import *`
— that copies names, misses anything underscore-prefixed, and leaves `edk2 is not uefi`. Use the
`sys.modules` replacement idiom instead:

```python
"""Compatibility alias: CHIPSEC imports `edk2`; this build names the module `uefi`."""
import sys as _sys
import uefi as _uefi

_sys.modules[__name__] = _uefi
```

This gives **true aliasing** — `import edk2` yields the actual `uefi` module object, so
`edk2 is uefi` is `True`, `from edk2 import rdmsr` works, and there is exactly one module object
and one set of state. The idiom is explicitly supported by the import machinery, not a trick:
`_bootstrap._load` re-reads `sys.modules[spec.name]` after `exec_module` and returns that
(`Lib/importlib/_bootstrap.py:870`), and `_load_unlocked` does the same at `:946` under a comment
that names "the sys.modules replacement case" directly. This removes the identity downside that
originally made option 1 the weaker choice, so option 2 — dual registration in C — buys nothing
and costs a second entry in the builtin table and a second init path.

**It ships with no build-system change.** `create_python_pkg.bat:92-93` already copies
`PyMod-3.12.13/Lib/*` to `EFI\lib\python3.12\` on the ESP, which is also the directory CHIPSEC's
own `.py` files land in. `PyMod-3.12.13/Lib/` is the correct home per the PyMod-source-of-truth
convention; do not put it in the stock `Python-3.12.13/Lib/`.

**One consequence for testing.** Being a filesystem module, `import edk2` needs a packaged
deployment, so it will not work on a bare `Python312.efi` the way the §5.9 tests run today. That
is not a limitation in practice — CHIPSEC cannot run without a filesystem `Lib` regardless, so
the shim is available exactly when CHIPSEC is. It does split the test plan: exercise the 19
functions as `uefi.*` in the existing bare-image one-liner style, and cover the alias with a
single separate check on a packaged image.

## 6. Defects in the 3.6.8 reference — do not copy these

The old module is a useful specification of *what* the APIs are, not of *how* to implement them.
Four things in it should not be carried forward.

### 6.1 Module init hard-fails the whole interpreter without MP Services

`PyEdk2__Init` (`edk2module.c:5142-5150`) does `gBS->LocateProtocol(&gEfiMpServiceProtocolGuid…)`
and on failure prints and `return NULL`s — **before** `PyModule_Create`. So on any platform that
does not publish MP Services, importing the module fails outright.

In 3.6.8 that was already bad. In 3.12.13 it would be far worse: `uefi` *is* the `os` module, and
it is imported by `importlib._bootstrap_external` during interpreter startup. A hard failure
there does not disable three APIs, it bricks the interpreter — on a platform we would have no way
to diagnose from Python, because Python would not start.

The `_ex` variants are in scope (§3), so this is binding: MP Services must be located **lazily,
inside those three functions**, raising `OSError` on failure. It must never be touched at module
init.

### 6.2 `allocphysmem` silently truncates its return value on X64

```c
va = malloc(length);
// return Py_BuildValue("(K)",  (unsigned long)va);   <-- commented out in the original
return Py_BuildValue("(I)",  (unsigned long)va);
```

`'I'` is `unsigned int`, i.e. 32 bits, so any allocation above 4 GB is returned truncated and the
caller writes to the wrong address. The commented-out `"(K)"` line shows the author knew. Beyond
the truncation the function does not do what its name claims: plain `malloc` ignores `max_pa`
entirely, guarantees neither page alignment nor physical contiguity, and there is no companion
free — so every call leaks for the rest of the boot. It is in scope (§3), so it must be
reimplemented on `gBS->AllocatePages` with `AllocateMaxAddress` and given a matching free.

### 6.3 The `readmem`/`writemem` family dereferences unvalidated addresses

`posix_readmem_dword` is representative:

```c
addr = (unsigned int*)((UINT64)addr_lo | ((UINT64)addr_hi << 32));
result = *addr;
```

No probe, no guard. A bad address from a CHIPSEC script is a `#GP`/`#PF` that, before this
month's work, was a silent hang or reset.

We are now in a strictly better position than 3.6.8 was: these four functions should be
implemented on top of `uefi_guarded_access()`, so a bad address raises `uefi.FaultError` with
vector/rip/cr2 and the interpreter survives. That turns the single most dangerous part of the
CHIPSEC surface into something recoverable, and it is the reason to port rather than copy. It
also means the port has a real test story — the §5.9 smoke tests already exercise exactly this
path on all four configurations.

### 6.4 Cosmetic sloppiness (verified harmless — do not propagate, but do not panic)

Two things in the CHIPSEC block look like bugs and are not; recording the analysis so nobody
re-derives it:

- `edk2module.c:4158` and `:4174` end with `";);` — a stray `;` *inside* the `PyDoc_STRVAR`
  argument list. It expands to a valid declaration followed by a stray file-scope semicolon,
  which MSVC and non-pedantic GCC both accept. It compiles.
- `edk2module.c:4169` passes the format `"(IIII))"` to `Py_BuildValue` — one `)` too many. It
  still returns the correct 4-tuple: `countformat` (`modsupport.c:42`) counts the top-level `(`
  as a single item and stops at the `\0`, so `va_build_value` takes the single-value path and
  `do_mktuple` consumes only up to the *first* `)`. The extra one is never parsed.

## 7. Build-system deltas required

**`Python312.inf` only.** `Python312_MIN.inf` gets no changes (§3).

- `[LibraryClasses]`: add `PciLib` (3.6.8 declared it, 3.12.13 does not) and `IoLib`. Both already
  resolve via `AppPkg.dsc:69-70`; no DSC change needed.
- `[Protocols]`: add `gEfiMpServiceProtocolGuid` — required, since the `_ex` variants are in scope.
- `[Sources]`: no change — `cpu.nasm`/`cpu_gcc.s` are already listed (§4).
- Packaging: no change — the shim ships via the existing `create_python_pkg.bat` copy (§5).

Declaring the MP Services GUID in `[Protocols]` is a build-time declaration only; it does not
make the protocol's presence a runtime requirement. Keeping it that way is the whole point of
§6.1.

## 8. Proposed sequencing

All eight phases are in scope (§3). Ordered by risk, cheapest and safest first; each is
independently testable and shippable, so the ordering is about getting verified ground under the
port early, not about deciding what to include.

1. **Naming shim** (§5). `PyMod-3.12.13/Lib/edk2.py`. No C change.
2. **Zero-dependency APIs**: `rdmsr`, `wrmsr`, `cpuid`, `readio`, `writeio`. BaseLib/IoLib only,
   all trivially verifiable from the REPL (`cpuid(0,0)` returns the vendor string as four
   registers; `rdmsr(0x1B)` returns the APIC base).
3. **PCI**: `readpci`, `writepci`. Adds `PciLib`. Verifiable by reading vendor/device at 0:0.0
   and comparing against the Shell's `pci` command.
4. **Guarded memory**: `readmem`, `readmem_dword`, `writemem`, `writemem_dword`, layered on
   `uefi_guarded_access()` per §6.3, keeping the split-address signature.
5. **`swsmi`**: C wrapper over the `_swsmi` already in the image. Widen the arguments to 64-bit
   (`"K"`), since the asm takes `UINT64` and 3.6.8's `"(IIIIIII)"` narrowed them to 32.
   Needs care in testing — it triggers a real SMI.
6. **UEFI variables**: `GetVariable`, `GetNextVariableName`, `SetVariable`. Straightforward
   `gRT` calls, but the 3.6.8 argument parsing (`"uu#K"`) uses formats that changed in Python 3
   and must be rewritten, not copied.
7. **`allocphysmem`**, reimplemented on `gBS->AllocatePages` with `AllocateMaxAddress` per §6.2,
   with a matching free. Not a transcription — the 3.6.8 version does not do what its name says.
8. **`_ex` variants + MP Services** (`rdmsr_ex`, `wrmsr_ex`, `cpuid_ex`), protocol located
   lazily per §6.1. Deliberately last: highest risk, and the only phase that can affect
   interpreter startup if implemented wrongly. Everything else is verified by the time it lands.

## 9. Decisions taken

| Question | Decision |
|---|---|
| Which of the 19 to port? | **All 19.** MP Services becomes a hard dependency; §6.1 lazy location is mandatory (§3). |
| FULL only, or MIN too? | **FULL only** — `Python312.inf`. MIN untouched (§3, §7). |
| Naming shim | **Option 1**, `PyMod-3.12.13/Lib/edk2.py` using `sys.modules[__name__] = uefi` for true aliasing (§5). |

No open questions remain before implementation. The next step is phase 1.
