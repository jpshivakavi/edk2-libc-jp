# CHIPSEC platform API: 3.6.8 `edk2` module vs 3.12.13 `uefi` module

Status: **Phase 1 closed** — VS2022 FULL green and the MIN gate confirmed by a clean link
(2026-09-09), §10. **Phase 2 written, not yet built or tested**, §11.

Decisions (§9): **all 19 APIs**, **FULL only** (`Python312.inf`; MIN untouched), and — superseding
an earlier recommendation in this document — **a separate non-bootstrap builtin module named
`edk2`** rather than an extension of `uefi`/`posixmodule.c`. That last decision removes the need
for a naming shim altogether; see §5.

Source of truth for the old side: `Python-3.6.8/PyMod-3.6.8/Modules/edk2module.c` (4576 lines,
built by `Python368.inf:165`).
Source of truth for the new side: `Python-3.12.13/PyMod-3.12.13/Modules/posixmodule.c`.

Related: `Python312_SEH_Fault_Recovery_Design.md` (the guarded-access primitives this port
builds on), `Python312_VS2022_GCC_Toolchain_Deviations.md`.

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

So: exactly those 19 functions, in a module of their own (§5). Nothing else.

A useful way to hold the whole thing: 3.6.8's `edk2` module is being **split in two**. The `os`
half already moved and was renamed `uefi`. The platform half is what remains to port, and it
**keeps the name `edk2`** — which is why CHIPSEC ends up needing no changes at all.

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

**All 19 are in scope.** The `_ex` variants are **confirmed needed** — not by CHIPSEC, but by a
second tool that consumes the same module. That closes the one question that could have removed
the MP Services dependency, so it stays, and §6.1's lazy-location requirement is binding rather
than advisory. Being fully in scope also means `allocphysmem` has to be *rebuilt* (§6.2) rather
than skipped, since the 3.6.8 version does not do what its name claims.

**FULL only.** The APIs go in `Python312.inf`; `Python312_MIN.inf` is untouched. With the
separate-module design this is a single `[Sources]` line rather than an `#ifdef` inside a shared
file, so the split costs nothing to maintain. It keeps MIN what it is today — the small,
few-dependency image — and means the port carries no risk to the MIN configuration at all, which
is worth having given MIN was only swept for the first time this week. It also halves the
verification matrix to **VS2022 FULL and GCC FULL**, versus the 2×2 that items 36-39 in the
status doc needed.

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
default on both toolchains. §5.2 covers how the new module shares it; §6.3 covers why the CHIPSEC
`readmem`/`writemem` family must be built on it rather than copied from 3.6.8.

**The library dependencies resolve.** `AppPkg.dsc:69-70` already maps both `IoLib` and `PciLib`
(`BasePciLibCf8`, i.e. CF8/CFC port access), so `readpci`/`readio` need only an INF-level
`[LibraryClasses]` entry, not a DSC change. `BaseLib` is already in use on the entry path
(`EnableInterrupts` in `edk2excep.c`), and `gRT` resolves through the `UefiLib` chain.

## 5. Module architecture: a separate, non-bootstrap `edk2` builtin

**Decision: the 19 functions go in a new C file of their own,
`PyMod-3.12.13/Modules/edk2module.c`, registered as the builtin module `edk2`.** They do *not* go
into `posixmodule.c`. An earlier revision of this document proposed adding them to `posixmodule.c`
and papering over the name difference with a `Lib/edk2.py` shim; that approach is **withdrawn**,
and the shim is no longer needed in any form.

### 5.1 Why this is the right structure

**It takes the port off the interpreter's startup path entirely.** This is the main argument.
`uefi` *is* the `os` module and is imported by `importlib._bootstrap_external` while the
interpreter is coming up, so anything added to `posixmodule.c` runs during startup: a failed
`PyModule_AddObjectRef`, a mistimed protocol lookup, a bad exception registration, and Python
does not start — on a platform where you then have no Python with which to investigate. A
separate module is inert until something imports it. `_PyImport_Inittab`
(`PyMod-3.12.13/Modules/config.c:111`) is a static table that is only scanned on import, so the
new entry costs **nothing at startup** and cannot affect it. The worst failure becomes
`import edk2` raising, with a working interpreter and a traceback to read. Given the whole
history in this repo of debugging pre-`main` hangs through boot traces, moving 19 privileged
functions *off* that path is worth more than the file split costs.

**It makes `import edk2` work natively, so no shim exists to go wrong.** CHIPSEC and the second
tool both already import `edk2`; naming the module `edk2` means neither needs any change. This is
strictly better than the withdrawn shim on four counts: no filesystem `Lib` dependency, so it
works on a bare `Python312.efi`; no `sys.modules` manipulation to reason about; testable with the
same bare-image one-liners as the existing §5.9 tests; and one module object rather than an alias
to explain.

**It keeps our code out of upstream code.** `posixmodule.c` is ~17.5k lines of CPython that we
carry with UEFI adaptations, and every line we add is permanent merge friction against future
CPython updates. A new file is entirely ours, with zero conflict surface — which is the same
reasoning behind the existing PyMod-source-of-truth convention and the pre-upstream-push cleanup
item about keeping the stock tree pristine.

**It makes the FULL/MIN split and the review story trivial.** Inclusion is one `[Sources]` line
per INF (§7) instead of conditional compilation inside a shared file. And a firmware-security
tool's privileged primitives — arbitrary MSR writes, arbitrary physical memory writes, SMI
triggers — sitting in one auditable file is worth a great deal more than the same code diffused
through the `os` module.

The one real cost: `edk2` was the *os* module's name in 3.6.8, so old code doing `edk2.listdir()`
now gets `AttributeError` rather than working. That is a documentation matter (§1 has the framing:
the module split in two, and each half took a name), and arguably better than silently working.

### 5.2 Sharing the guarded-access path with `uefi`

The new module must not duplicate the fault-recovery code — that path is verified on four
configurations and having two copies is how one of them silently rots. Of the three helpers
currently in `posixmodule.c`:

- `uefi_guarded_access` (`:16056`, ~35 lines) is **pure C with no Python in it** — `setjmp` around
  a sized load or store, wrapped in `edk2_seh_try`/`edk2_seh_catch`. **Move it** to the EFI glue
  (`efi/src/edk2excep.c`, declared in `edk2excep.h`) as `edk2_guarded_access()` and have both
  modules call it. Behaviour-preserving relocation of already-verified code.
- `uefi_check_access_size` (`:16035`, 10 lines) raises `ValueError`; small enough that either
  sharing or a local copy is defensible.
- `uefi_set_fault_error` (`:16092`, ~45 lines) is Python-specific and currently reaches into
  `get_posix_state(module)->FaultErrorType`. Generalise it to take the exception type as a
  parameter instead of the module, after which it is shareable.

**Reuse `uefi.FaultError` rather than defining a second exception type.** The new module fetches
it from `uefi` at its own init and re-exposes it as `edk2.FaultError` — the *same object*, so
`edk2.FaultError is uefi.FaultError`. One type, no hierarchy, and `except uefi.FaultError`
continues to catch everything including faults from the new APIs. This is the "internally use the
`uefi` module as needed" shape, and it is safe because `uefi` is loaded during startup, long
before anything can import `edk2`.

Because this moves code on the verified fault path, the §5.9 smoke tests must be re-run on both
FULL configurations after the move — not because anything should change, but because that path is
the one place in this project where a silent regression has real consequences.

Two things were checked up front, both of which make the move genuinely behaviour-preserving
rather than merely intended to be:

- **`setjmp` resolves to the same implementation in the destination.** `edk2excep.c:1` already
  includes `<setjmp.h>`, as does `edk2excep.h:4`, and `posixmodule.c` only sees `jmp_buf` through
  that same header. So there is no second `setjmp` for the code to land on — this is EDK2's
  `SetJump`/`LongJump` on both sides of the move, on both toolchains. The declaration also needs
  no new includes in `edk2excep.h`: `EFI_SYSTEM_CONTEXT_X64` arrived with
  `<Protocol/DebugSupport.h>` and `uint64_t` with `<stdint.h>`.
- **Move the function whole. Never split it.** `setjmp` has to be called by the function that owns
  the frame `longjmp` returns to, and in `uefi_guarded_access` the `setjmp`, the guarded load or
  store, and the return are all in one frame by design. Relocating the entire function preserves
  that. Refactoring it into a "set up the guard" helper plus a "do the access" helper would
  compile cleanly, pass a casual read, and be wrong — the guard would be armed against a frame
  that has already returned. If this function ever looks like it wants tidying, this is the reason
  it is shaped the way it is.

## 6. Defects in the 3.6.8 reference — do not copy these

The old module is a useful specification of *what* the APIs are, not of *how* to implement them.
Four things in it should not be carried forward.

### 6.1 Module init hard-fails without MP Services

`PyEdk2__Init` (`edk2module.c:5142-5150`) does `gBS->LocateProtocol(&gEfiMpServiceProtocolGuid…)`
and on failure prints and `return NULL`s — **before** `PyModule_Create`. So on any platform that
does not publish MP Services, importing the module fails outright.

The separate-module design (§5) already defuses the worst version of this: a failure here can no
longer prevent the interpreter from starting, only `import edk2`. **The requirement stands
anyway**, for a reason that survives the redesign: 16 of the 19 functions have nothing to do with
MP Services, and a platform without that protocol should still get `rdmsr`, `readpci`, `readmem`
and the rest. Locating it at init trades 16 working functions for 3 unavailable ones.

So: locate MP Services **lazily, inside the three `_ex` functions** (`rdmsr_ex`, `wrmsr_ex`,
`cpuid_ex`), on first call, caching the result; raise `OSError` if absent. Never at module init.

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

We are now in a strictly better position than 3.6.8 was: these four functions must be implemented
on the shared guarded path (§5.2), so a bad address raises `uefi.FaultError` with vector/rip/cr2
and the interpreter survives. That turns the single most dangerous part of the CHIPSEC surface
into something recoverable, and it is the reason to port rather than copy. It also means the port
inherits a real test story — the §5.9 smoke tests already exercise exactly this path.

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

- `[Sources]`: add `PyMod-3.12.13/Modules/edk2module.c`. `cpu.nasm`/`cpu_gcc.s` need no change —
  already listed (§4).
- `[LibraryClasses]`: add `PciLib` (3.6.8 declared it, 3.12.13 does not) and `IoLib`. Both already
  resolve via `AppPkg.dsc:69-70`; no DSC change needed.
- `[Protocols]`: add `gEfiMpServiceProtocolGuid` — required, since the `_ex` variants are in scope.
- `PyMod-3.12.13/Modules/config.c`: one `extern PyObject* PyInit_edk2(void);` alongside the others
  at `:97`, and one `{"edk2", PyInit_edk2},` entry in `_PyImport_Inittab` at `:111`. Follow the
  `edk2console` precedent at `:204`.
- Packaging: no change. Nothing ships to the ESP — it is a builtin.

Declaring the MP Services GUID in `[Protocols]` is a build-time declaration only; it does not
make the protocol's presence a runtime requirement. Keeping it that way is the point of §6.1.

## 8. Proposed sequencing

All phases are in scope (§3). Ordered by risk, cheapest first; each is independently testable, so
the ordering is about getting verified ground under the port early, not about what to include.

1. **Module skeleton** — **WRITTEN, not yet built or tested.** `edk2module.c` with an empty method
   table, the `config.c` inittab entry, and the `Python312.inf` `[Sources]` line. Acceptance in
   §10; it proves the zero-startup-cost claim in §5.1 before any real code lands.
2. **Share the guarded path** (§5.2) — **WRITTEN, not yet built or tested.** Acceptance in §11.
3. **Zero-dependency APIs**: `rdmsr`, `wrmsr`, `cpuid`, `readio`, `writeio`. BaseLib/IoLib only,
   all verifiable from a bare-image one-liner (`cpuid(0,0)` returns the vendor string as four
   registers; `rdmsr(0x1B)` returns the APIC base).
4. **PCI**: `readpci`, `writepci`. Adds `PciLib`. Verifiable by reading vendor/device at 0:0.0
   and comparing against the Shell's `pci` command.
5. **Guarded memory**: `readmem`, `readmem_dword`, `writemem`, `writemem_dword` on the shared path
   from phase 2, keeping the split `(lo32, hi32)` signature.
6. **`swsmi`**: C wrapper over the `_swsmi` already in the image. Widen the arguments to 64-bit
   (`"K"`), since the asm takes `UINT64` and 3.6.8's `"(IIIIIII)"` narrowed them to 32.
   Needs care in testing — it triggers a real SMI.
7. **UEFI variables**: `GetVariable`, `GetNextVariableName`, `SetVariable`. Straightforward `gRT`
   calls, but the 3.6.8 argument parsing (`"uu#K"`) uses formats that changed in Python 3 and must
   be rewritten, not copied.
8. **`allocphysmem`**, reimplemented on `gBS->AllocatePages` with `AllocateMaxAddress` per §6.2,
   with a matching free.
9. **`_ex` variants + MP Services** (`rdmsr_ex`, `wrmsr_ex`, `cpuid_ex`), protocol located lazily
   per §6.1. Last because it is the highest-risk phase; by then everything else is verified.

## 9. Decisions taken

| Question | Decision |
|---|---|
| Which of the 19 to port? | **All 19.** The `_ex` variants are confirmed needed by a second tool, so MP Services stays a hard dependency and §6.1 is binding (§3). |
| FULL only, or MIN too? | **FULL only** — `Python312.inf`. MIN untouched (§3, §7). |
| Where do the functions live? | **A separate non-bootstrap builtin module, `edk2`**, in its own `PyMod-3.12.13/Modules/edk2module.c` — not in `posixmodule.c` (§5). |
| Naming shim | **Not needed.** Withdrawn; the module is named `edk2`, so both consuming tools import it unchanged (§5.1). |
| Fault exception type | **Reuse `uefi.FaultError`**, re-exposed as the same object on `edk2` (§5.2). |

No open questions remain before implementation.

## 10. Phase 1 acceptance

Three files changed, no behaviour added: `PyMod-3.12.13/Modules/edk2module.c` (new, empty method
table), `PyMod-3.12.13/Modules/config.c` (extern + `{"edk2", PyInit_edk2}`, both inside the
existing `#if defined(BUILD_PYTHON312_FULL)` blocks that already gate `zlib`/`_ctypes`/`_ssl`),
and `Python312.inf` `[Sources]`. `srcprep.py` needs no change — the INF compiles the PyMod path in
place, exactly as it does `posixmodule.c`.

### 10.1 Build

Both builds run from an existing EDK2 console — the one where `edksetup` was run. The EDK2
environment is per-shell, but nothing about this change needs new variables: `OPENSSL_ROOT`,
`LIBFFI_INC` and `LIBFFI_MSVC_INC` are DSC/INF `DEFINE`s (`Python312.inf:17-21`), not environment
variables. **`srcprep.py` does not need re-running** either: the INF compiles
`PyMod-3.12.13/Modules/edk2module.c` in place, exactly as it does `posixmodule.c`, and no overlay
header changed.

FULL:

```
build -t VS2022 -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312 -D BUILD_PYTHON312_FULL=TRUE
```

MIN — note that MIN is the **default**, so it is the *absence* of the FULL flag rather than
`FALSE` (`AppPkg.dsc:32` defines `BUILD_PYTHON312_FULL = FALSE`, and `:136` selects the INF from
it):

```
build -t VS2022 -a X64 -b NOOPT -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312
```

**Build MIN even though it gains no functionality.** MIN is the configuration that can actually
break here: `config.c` is shared between the two INFs, so if the `BUILD_PYTHON312_FULL` gate
around either the extern or the inittab entry were wrong, MIN would fail to link with an
unresolved `PyInit_edk2`. A clean MIN link *is* the test of the gate, and it costs one build.

Incremental is expected to be enough — `Python312.inf` was edited, so AutoGen regenerates the
module makefile and picks up the new `[Sources]` entry. If FULL instead fails to link on
`PyInit_edk2`, that is a stale makefile rather than a real problem: delete that module's output
directory under `Build\AppPkg\NOOPT_VS2022\X64\…\Python312\` and rebuild.

### 10.1.1 Do not mix up the two images

Both configurations produce a file called **`Python312.efi`**; only the directory differs.

| Build | Path under `edk2\Build\AppPkg\NOOPT_VS2022\X64\edk2-libc-jp-vsfix\AppPkg\Applications\Python\Python-3.12.13\` |
|---|---|
| FULL | `Python312\DEBUG\Python312.efi` |
| MIN | `Python312_MIN\DEBUG\Python312.efi` |

This matters more than usual for this phase, because the FULL and MIN expectations are
*opposites*: `import edk2` must succeed on one and raise `ModuleNotFoundError` on the other.
Deploying the wrong file makes a pass look like a failure, or worse, the reverse.

### 10.2 On hardware — FULL — **PASSED 2026-09-09**

Tests 1-5 all observed as expected on VS2022 FULL, teardown included, no hang. The two that
carry the argument — `'edk2' in sys.modules` → `False` and `'edk2' in sys.builtin_module_names`
→ `True` — both landed, so §5.1's claim is now measured rather than asserted: the module is
registered and genuinely is not loaded until something imports it. The `edk2` module machinery
is known good, and phase 3 onward can treat a failure as being about the function it just added
rather than about the plumbing.

| # | Command | Expected |
|---|---------|----------|
| 1 | `Python312.efi -S -c "import sys; print('edk2' in sys.modules)"` | **`False`** |
| 2 | `Python312.efi -S -c "import sys; print('edk2' in sys.builtin_module_names)"` | `True` |
| 3 | `Python312.efi -S -c "import edk2; print(edk2)"` | `<module 'edk2' (built-in)>` |
| 4 | `Python312.efi -S -c "print(1+1)"` | `2` and nothing else — boot still silent |
| 5 | `import edk2` → `exit()` → Shell `exit` | no hang, returns to firmware |
| 6 | `Python312.efi -S -c "import sys; print(len(sys.modules))"` | recorded, not asserted |

**Tests 1 and 2 together are the whole proof, and they are worth reading as a pair**: the module
is *registered* (2) but *not loaded* (1). That is precisely the property §5.1 claims — an inittab
entry that costs nothing until something imports it — stated as a direct observation.

An earlier revision of this section instead asked for `len(sys.modules)` before and after the
change, on the theory that the count would rise by one if `edk2` were being pulled in at startup.
That works, but it is a proxy measurement dressed up as a proof: it needs a same-day baseline from
the outgoing image, and the count drifts for unrelated reasons as modules are added, so a
mismatch would be ambiguous rather than informative. Test 1 measures the thing itself. Test 6 is
kept only as a recorded data point for future comparison — the historically observed value on
VS2022 FULL was **23** — and nothing is asserted about it.

### 10.3 On hardware — MIN — **gate PASSED 2026-09-09 (link), runtime check not run**

MIN **built and linked clean**, which is the half that matters: `config.c` is shared between the
two INFs, so a wrong `BUILD_PYTHON312_FULL` bracket around either the extern or the inittab entry
would have surfaced as an unresolved `PyInit_edk2` at MIN link time. It did not, so the gate is
correct and phase 1 is closed on both configurations. The runtime `ModuleNotFoundError` check
below was deliberately skipped rather than forgotten — it needs an image swap to learn something
the link already established.

| # | Command | Expected |
|---|---------|----------|
| 6 | `Python312.efi -S -c "import edk2"` | `ModuleNotFoundError: No module named 'edk2'` |
| 7 | `Python312.efi -S -c "print(1+1)"` | `2` and nothing else |

Test 6 is the runtime half of the gate check — MIN linking clean proves the build gate, this
proves the module genuinely is not there. A `ModuleNotFoundError` is the pass condition.

### 10.4 What phase 1 does not test

Nothing about MSR, PCI, memory or the fault path — there is no code for any of it yet. The point
of the phase is to land the plumbing and the startup-cost claim separately from anything that
could fail for an interesting reason, so that when phase 3 misbehaves, the module machinery is
already known good.

## 11. Phase 2 acceptance — the refactor

**Status: written, not yet built or tested.**

Four files. Nothing gains a new capability; one attribute appears.

| File | Change |
|---|---|
| `efi/src/edk2excep.c` | `edk2_guarded_access()` added — `uefi_guarded_access` moved whole, body unchanged |
| `efi/Include/efi/edk2excep.h` | declares it; no new includes needed |
| `Modules/posixmodule.c` | its copy deleted; three call sites retargeted; `uefi_set_fault_error` now takes the exception type instead of the module |
| `Modules/edk2module.c` | `PyInit_edk2` binds `edk2.FaultError` to `uefi.FaultError` |

`uefi_set_fault_error` stays `static` in `posixmodule.c`. Generalising its signature is the
enabling step and is worth doing while the file is open, but choosing *how* to export it would be
speculative until something else raises a fault — that decision belongs to phase 5, which is the
first phase with a second caller.

### 11.1 Both toolchains, FULL and MIN

All four configurations must build: `posixmodule.c` and `edk2excep.c` are in **both** INFs, so
unlike phase 1 this touches MIN's compiled code. MIN gains nothing and must break nothing.

### 11.2 The refactor changed nothing — re-run §5.9

Re-run the §5.9 guarded-memory sequence from `Python312_Smoke_Tests.md` on **VS2022 FULL and GCC
FULL**. Every value must match what §7.5-§7.7 already recorded, `rip` aside, which differs because
the images do. This is the whole point of the phase: `mem_read`, `mem_write`, `mem_probe` and
`uefi.FaultError` are verified on four configurations, and the code underneath them just moved
translation units. A difference here is a real regression, not a curiosity.

The §5.8 fault-report check is worth including too, since it exercises the same IDT path from the
other direction.

### 11.3 The sharing works — one new check

```
Python312.efi -S -c "import edk2, uefi; print(edk2.FaultError is uefi.FaultError)"
```

Expected: `True`. This is the only *positive* assertion in the phase — everything else is an
absence of change — and it is what makes the design in §5.2 real rather than intended: one
exception type, so `except uefi.FaultError` catches faults raised by either module, and there is
no second type to drift.

Two failure modes worth recognising. `False` would mean two distinct type objects exist, i.e. the
new module built its own instead of borrowing — which would still *work* for anyone catching
`edk2.FaultError` and silently fail for anyone catching `uefi.FaultError`, the worse of the two
because it only shows up in someone else's error handling. An `ImportError` or `AttributeError`
from `import edk2` means `uefi.FaultError` was not found, which is the deliberate loud failure
described in `PyInit_edk2` — a missing fault-recovery type is exactly what should stop this
module from loading rather than be worked around.

### 11.4 Why the move is safe

Recorded in §5.2 and worth not re-deriving: `setjmp` resolves to the same EDK2
`SetJump`/`LongJump` in the destination file, and the function was moved **whole**, keeping the
`setjmp`, the access and the return in one frame. Both were checked before the move rather than
after.
