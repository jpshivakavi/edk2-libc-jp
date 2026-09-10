# CHIPSEC platform API: 3.6.8 `edk2` module vs 3.12.13 `uefi` module

Status: **Phases 1-4 green on VS2022 FULL.** Phase 2 additionally verified on GCC FULL and compiled
clean in both MINs (§11); the GCC runs for phases 3 and 4 were skipped by decision, with the
reasoning and residual gap in §12.8. **Phase 5 written, not yet built or tested** (§14) — and it is
the first phase since 2 to touch code MIN compiles, so both MIN builds are back in scope.
2026-09-10.

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
3. **Zero-dependency APIs**: `rdmsr`, `wrmsr`, `cpuid`, `readio`, `writeio` — **green on VS2022
   FULL**; GCC run skipped by decision (§12.8). Acceptance in §12.
4. **PCI**: `readpci`, `writepci`. Adds `PciLib` — **green on VS2022 FULL.** Acceptance in §13,
   cross-checked against both the Shell's `pci` command and phase 3's manual CF8 read.
5. **Guarded memory**: `readmem`, `readmem_dword`, `writemem`, `writemem_dword` on the shared path
   from phase 2, keeping the split `(lo32, hi32)` signature — **WRITTEN, not yet built or tested.**
   Acceptance in §14. The one phase where a fault used to end the session, so the guard is the
   deliverable rather than a safety net.
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

**Status: CLOSED 2026-09-09 — green on both FULL toolchains, clean in both MINs.** VS2022 FULL
covered §5.9 tests 0-8, the `ctypes` write row, §2/§3/§4, and the whole `edk2` surface (§11.3,
§11.3.1, §11.3.2), recorded in `Python312_Smoke_Tests.md` §7.9; **GCC FULL then matched it row for
row** (§7.10). **VS2022 MIN and GCC MIN build and link clean**, which is all that was asked of
them: they compile the changed `posixmodule.c` and `edk2excep.c` but gain no functionality.

GCC FULL was the run that carried information, for the reason in §11.4: the move changes which
translation unit owns the frame `setjmp` captures, and while `SetJump`/`LongJump` is identical EDK2
assembly on both toolchains, the frame layout around it is not — so a relocation problem was
likelier to show on one compiler than on both.

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

### 11.3.1 The borrowed type actually works — end to end — **PASSED 2026-09-09 (VS2022 FULL)**

Identity (§11.3) proves the two names point at one object. It does not prove that object behaves
as the exception for a real fault raised through the *other* module. That needs one more line, and
it needs no `try`/`except` — after an unhandled error the REPL leaves the instance in
`sys.last_value`, the same convention §5.9 uses to avoid multi-line blocks at a UEFI prompt:

```text
Python312.efi -S
>>> import sys, uefi, edk2
>>> uefi.mem_read(0x800000000000, 8)
uefi.FaultError: CPU exception 13 (rip=0x... cr2=0x...)
>>> print(isinstance(sys.last_value, edk2.FaultError))
True
>>> print(issubclass(edk2.FaultError, OSError))
True
```

The first `print` is the real assertion: a fault raised by `uefi.mem_read` is caught by
`edk2.FaultError`, which is what any consumer writing `except edk2.FaultError` around the
memory APIs is relying on. Note the traceback still says `uefi.FaultError` — the type keeps the
name it was created with, which is correct and worth not "fixing".

`issubclass(..., OSError)` matters for a different audience: tooling that catches `OSError`
broadly and does not know about this build should still catch a fault. That is why `OSError` was
chosen as the base, and this is the one-line confirmation.

### 11.3.2 Module surface inventory — a standing check for every remaining phase — **`['FaultError']` confirmed 2026-09-09**

```text
Python312.efi -S -c "import edk2; print(sorted(n for n in dir(edk2) if not n.startswith('_')))"
```

After phase 2 this must print exactly:

```text
['FaultError']
```

Worth running at the end of **every** phase from here on, because it is the cheapest possible
guard against the two mistakes this port is most likely to make: a function that was added to the
method table but spelled wrong, and a function that was written but never reached the table at
all. Both are invisible to any test that calls the API by its intended name — the first raises
`AttributeError` on the name you expected, the second does too, and neither tells you the table
is the problem. The expected list grows by a known set each phase (phase 3 adds `cpuid`, `rdmsr`,
`readio`, `wrmsr`, `writeio`), so a diff against the previous phase is a complete statement of
what that phase added.

### 11.4 Why the move is safe

Recorded in §5.2 and worth not re-deriving: `setjmp` resolves to the same EDK2
`SetJump`/`LongJump` in the destination file, and the function was moved **whole**, keeping the
`setjmp`, the access and the return in one frame. Both were checked before the move rather than
after.

---

## 12. Phase 3 acceptance — MSR, CPUID, port I/O

**Status: PASSED on VS2022 FULL, 2026-09-09 — all of §12.2 through §12.6. GCC run skipped, §12.8.**
FULL only, as with every phase, so there is no MIN build to do: `Python312_MIN.inf` is untouched
and does not compile `edk2module.c`.

What the passing run establishes, beyond "five functions work":

- **The tuple ABI is right.** `b'GenuineIntel'` can only come out if the leaf argument, all four
  result registers and their order are all correct. This is the check that would have caught the
  `(IIII))` malformed format string that 3.6.8 shipped in its `cpuid`.
- **Port I/O is right against an independent witness.** The 0xCF8/0xCFC value agreed with the
  Shell's own `pci 00 00 00`, so port number, access width and data are confirmed by something
  other than this document's expectations.
- **The three new argument checks fire before the access, and the machine survives all four.**
  That is the deviation from 3.6.8 doing its job: `readio(0x81, 2)` there does not return a wrong
  value, it trips the `ASSERT` in `IoRead16` and stops the box.

Still unproven and unprovable here: `wrmsr` (§12.7 is optional by design), and any behaviour on a
*non-existent* MSR or port, which is not survivable on either toolchain.

### 12.8 GCC FULL for phase 3 — deliberately not run

Decided 2026-09-09, and recorded as a gap rather than a pass. **Most of what it would have told us
is answerable without a build, and was checked instead:** `IoLib` and `PciLib` are in
`AppPkg.dsc`'s *common* `[LibraryClasses]` (lines 69-70), not in any toolchain-conditional block,
so library resolution is identical on GCC and MSVC — which was the main thing a GCC link would
have established. The code itself is straight-line calls into `BaseLib` and `IoLib`, with no
`setjmp` frame whose layout differs between compilers; that is what made GCC FULL load-bearing in
phase 2 (§11.4) and what makes it near-formality here.

**What genuinely remains unverified on GCC:** that the file compiles under it at all — GCC is
stricter than MSVC about some format-specifier and constant-folding warnings, and this build treats
warnings as errors. Any such problem is a *build* failure, not a runtime one, so it will surface
the moment a GCC FULL build is done for any later phase and cannot escape into a shipped image
unnoticed. That is the whole reason skipping it here is safe: a compile error cannot hide.

`rdmsr`, `wrmsr`, `cpuid`, `readio` and `writeio` are in
`PyMod-3.12.13/Modules/edk2module.c`. The only build-system change is `IoLib` added to
`Python312.inf`'s `[LibraryClasses]`; `BaseLib` needs no entry because it is already in
`UefiLib`'s dependency closure, which is what lets `edk2excep.c` call `EnableInterrupts()`
today.

### 12.1 Three deviations from 3.6.8, all deliberate

These are behaviour changes, not cleanups, so a consumer that depended on the old behaviour will
notice. Each replaces something that failed silently or fatally.

| 3.6.8 | Here | Why |
|---|---|---|
| `addrs = (short)(addr & 0xffff)` | `ValueError` if port > 0xFFFF | The mask was meant to stop out-of-range ports, but the cast is to a **signed** short, so every port from 0x8000 up went negative and then sign-extended inside `IoRead8(UINTN)`. The entire upper half of the I/O space addressed something else entirely. |
| No alignment check | `ValueError` if port is not size-aligned | MdePkg's `IoRead16`/`IoRead32` `ASSERT` on a misaligned port, and this build sets `PcdDebugPropertyMask` to 0x0F, so asserts are live. `readio(0x81, 2)` on 3.6.8 does not return a wrong value — it stops the machine in `DebugAssert()`. |
| `IoWrite8(port, val & 0xFF)` | `OverflowError` if the value does not fit | A silently truncated write to hardware is worse than a refused one. This also matches `uefi.mem_write`, which made the same call for the same reason. |

Unchanged and still true: an unimplemented port reads back all ones and swallows writes, and
neither faults, so port I/O has no error reporting beyond these argument checks.

### 12.2 Surface inventory

```text
Python312.efi -S -c "import edk2; print(sorted(n for n in dir(edk2) if not n.startswith('_')))"
```

```text
['FaultError', 'cpuid', 'rdmsr', 'readio', 'writeio', 'wrmsr']
```

Exactly five names more than phase 2, which is the whole of what phase 3 adds.

`wrmsr` sorts **last**, after `writeio`: the comparison reaches `'i'` versus `'m'` at the third
character. Worth stating because the intuitive reading puts the two MSR functions together.

### 12.3 CPUID — self-validating, needs no knowledge of the platform

```text
Python312.efi -S -c "import edk2, struct; r = edk2.cpuid(0,0); print(b''.join(struct.pack('<I', x) for x in (r[1], r[3], r[2])))"
```

```text
b'GenuineIntel'
```

or `b'AuthenticAMD'`. This is the strongest cheap test in the phase: leaf 0 returns the vendor
string spread across ebx, edx, ecx in that order, so a correct result is a **recognisable
English string**, and any mistake in argument passing, register ordering or the return tuple
produces garbage rather than a plausible-looking number. It also proves `AsmCpuidEx` is being
reached with the right leaf.

Then the subleaf argument, which leaf 0 ignores and leaf 4 does not:

```text
Python312.efi -S -c "import edk2; print([hex(x) for x in edk2.cpuid(1,0)])"
```

eax is family/model/stepping and will be non-zero; edx bit 0 (FPU) is set on anything that can
run this. The point is only that it differs from the leaf-0 result.

### 12.4 rdmsr — a value with checkable structure

```text
Python312.efi -S -c "import edk2; print([hex(v) for v in edk2.rdmsr(0x1B)])"
```

```text
['0xfee00900', '0x0']
```

MSR 0x1B is IA32_APIC_BASE. The exact value varies, but the shape does not: the top of the low
half is the APIC base (0xFEE00000 on essentially every platform), bit 11 is the enable bit and
bit 8 marks the bootstrap processor — which is what the Shell runs on, so it should be set. The
high half is 0 unless the platform has more than 36 physical address bits in use for it.

A plain hex dump would tell you nothing; this MSR was chosen because a wrong answer looks wrong.

### 12.5 readio and writeio — validated against the Shell, not against this document

Rather than trusting an expected constant, read something the firmware can independently report.
`0xCF8`/`0xCFC` are the PCI configuration address and data ports, so this is a PCI config read of
bus 0, device 0, function 0, offset 0 — vendor and device ID:

```text
Python312.efi -S
>>> import edk2
>>> edk2.writeio(0xCF8, 4, 0x80000000)
>>> hex(edk2.readio(0xCFC, 4))
'0x0c008086'
>>> exit()
Shell> pci 00 00 00
```

The Shell's `pci` command prints the same vendor and device ID for 00:00.0. **If the two agree,
`readio` and `writeio` are both correct**, including the port number, the width and the value —
and the check does not depend on anything written here being right about your platform. In the
example, 0x8086 is the vendor (Intel) in the low half and 0x0c00 the device in the high half.

Both ports are 4-byte aligned and below 0x10000, so the new argument checks pass. Writing 0xCF8
is not a side effect to worry about: it is an address latch, and setting it is how every PCI
configuration access on the machine already works.

### 12.6 The argument checks, which are the new behaviour

```text
Python312.efi -S
>>> import edk2
>>> edk2.readio(0x81, 2)
>>> edk2.readio(0x10000, 1)
>>> edk2.readio(0x80, 3)
>>> edk2.writeio(0x80, 1, 0xFFFF)
```

| Call | Expected |
|---|---|
| `readio(0x81, 2)` | `ValueError: port 0x81 is not 2-byte aligned; ...` |
| `readio(0x10000, 1)` | `ValueError: I/O port must be 0x0000-0xFFFF, not 0x10000` |
| `readio(0x80, 3)` | `ValueError: size must be 1, 2 or 4, not 3` |
| `writeio(0x80, 1, 0xFFFF)` | `OverflowError: value does not fit in 1 byte(s)` |

**`0xFFFF` rather than the obvious `0x100`, and the reason generalises to every overflow test in
this document.** The UEFI console drops characters intermittently, and a dropped digit in `0x100`
gives `0x10`, which fits in a byte and so *correctly* does not raise — yielding a failure report
against working code, which is exactly what happened once on the phase 4 equivalent of this row.
Every single-character deletion of `0xFFFF` still exceeds 0xFF, so the test cannot be turned into
a false negative that way. Choose overflow values with that property.

**Every one of these must raise before touching the port.** That is the entire point of the row:
the first would have stopped the machine on 3.6.8, and the interpreter surviving all four with
the prompt still responsive is the result being checked. Confirm the session is still alive
afterwards with `print(1+1)` and a clean `exit()`.

### 12.7 wrmsr — optional, and skipping it is defensible

There is no MSR that is safe to write on an arbitrary platform, so `wrmsr` has no default test.
Its implementation is three lines sharing `rdmsr`'s plumbing, and `rdmsr` passing is most of the
evidence that the pair is wired correctly.

If you want it exercised on a machine you are willing to risk, TSC_AUX is about as benign as this
gets — it only feeds the aux value that `RDTSCP` returns, and the test restores it:

```text
Python312.efi -S
>>> import edk2
>>> edk2.cpuid(0x80000001, 0)[3] >> 27 & 1        # RDTSCP present, so TSC_AUX exists
1
>>> before = edk2.rdmsr(0xC0000103)
>>> edk2.wrmsr(0xC0000103, 0x1234, 0)
>>> edk2.rdmsr(0xC0000103)
(4660, 0)
>>> edk2.wrmsr(0xC0000103, before[0], before[1])
>>> edk2.rdmsr(0xC0000103) == before
True
```

Reading back exactly what was written, then restoring it and confirming the restore, is a
complete test of the write path. **Do not run it if the first line prints 0** — TSC_AUX does not
exist without RDTSCP, and writing a non-existent MSR raises #GP, which is a CPU fault taken
outside any guard and therefore stops the machine.

That last sentence is the general rule for this whole phase: `rdmsr` and `wrmsr` on a reserved or
unimplemented MSR are **not** survivable. The guarded path from phase 2 covers memory accesses
only. Making MSR access survivable is possible with the same mechanism and is not in scope here.

---

## 13. Phase 4 acceptance — PCI configuration space

**Status: PASSED on VS2022 FULL, 2026-09-10 — all of §13.3 through §13.7.** FULL only; `PciLib`
added to `Python312.inf`.

One correction came out of the run and is recorded in §13.6: the overflow row was originally
written with `0x10000`, the console dropped a zero, `0x1000` fits in two bytes and so correctly did
not raise, and it was reported as a defect against working code. Overflow test values now survive
any single dropped character.

`readpci` and `writepci` in `edk2module.c`, over `PciLib`, which `AppPkg.dsc` already maps to
`BasePciLibCf8` — so no DSC change was needed, only the INF `[LibraryClasses]` entry.

### 13.1 The CF8 mechanism sets two hard limits, and both of them are ASSERTs

This is the important content of the phase, because in 3.6.8 both limits were reachable from
Python and one of them stopped the machine.

`BasePciCf8Lib` validates every address with:

```c
#define ASSERT_INVALID_PCI_ADDRESS(A, M) \
  ASSERT (((A) & (~0xffff0ff | (M))) == 0)
```

`PCI_LIB_ADDRESS` places the register offset in bits 0..11, and `M` is 0 for the 8-bit accessors,
**1 for the 16-bit and 3 for the 32-bit** ones. So:

| Condition | Consequence in MdePkg | 3.6.8 | Here |
|---|---|---|---|
| offset >= 0x100 | bits 8..11 set, **ASSERT** — the MdePkg header says so outright: "If the register specified by Address >= 0x100, then ASSERT()" | `unsigned char off` wrapped it, so 0x100 read register 0x00 and **returned it as if it were the register asked for** | `ValueError` naming the CF8 limitation |
| offset misaligned for the width | `M` catches it, **ASSERT** | nothing — `readpci(0,0,0,2,4)` stops the machine | `ValueError` |
| bus/dev/func out of range | `PCI_LIB_ADDRESS` masks silently | truncated to `unsigned char` then masked, so it became a **different, possibly populated** device | `ValueError` |

The third row matters far more for `writepci` than for `readpci`: a silently-masked *write* lands
on real hardware that the caller never named.

Extended configuration space (0x100-0xFFF, where PCIe capabilities live) is therefore not
reachable at all in this build. That is a property of the DSC's `PciLib` choice, not of this code
— remapping `PciLib` to an ECAM implementation would extend the reach with **no change to
`edk2module.c`**, and the range check exists so that the limit is reported rather than guessed at.
Worth flagging to whoever is driving CHIPSEC: anything that walks PCIe extended capabilities will
need that remap.

### 13.2 Argument order — do not tidy this

```text
readpci(bus, dev, func, offset, size)
writepci(bus, dev, func, offset, value, size)
```

`writepci` takes **value before size**, the opposite of `writeio(port, size, value)`. It is
inconsistent, it is 3.6.8's published signature, and callers pass positionally, so it stays. A
"cleanup" here silently swaps two integers in every existing call site.

### 13.3 Surface inventory

```text
Python312.efi -S -c "import edk2; print(sorted(n for n in dir(edk2) if not n.startswith('_')))"
```

```text
['FaultError', 'cpuid', 'rdmsr', 'readio', 'readpci', 'writeio', 'writepci', 'wrmsr']
```

Note `wrmsr` last, for the reason in §12.2 — `'i' < 'm'` puts both `write*` names ahead of it.

### 13.4 readpci cross-checked against two independent sources

Phase 3 already read 00:00.0 vendor/device by driving 0xCF8/0xCFC by hand. `readpci` must agree
with **that** and with the Shell's `pci`, which is three separate paths to one number:

```text
Python312.efi -S
>>> import edk2
>>> hex(edk2.readpci(0, 0, 0, 0, 4))
'0x0c008086'
>>> hex(edk2.readpci(0, 0, 0, 0, 2))
'0x8086'
>>> hex(edk2.readpci(0, 0, 0, 0, 1))
'0x86'
```

The three widths of the same register are a complete width test on their own: 2 bytes must be the
low half of the 4-byte value and 1 byte the low quarter, so the numbers check each other and no
external reference is needed to spot a wrong accessor.

Then a register that is not the vendor ID, to confirm the offset reaches the hardware:

```text
>>> hex(edk2.readpci(0, 0, 0, 8, 4))
```

Offset 0x08 is revision/class code; the top byte is the base class, `0x06` for a host bridge.
Anything that ignored the offset would return the vendor ID again.

### 13.5 An absent device — how probing is meant to work

Pick a bus:dev.func that `pci` does **not** list, and **read back the same b/d/f you wrote**, not a
neighbouring one. Run `pci` first; do not assume 00:1F.7 is empty, and note that 00:1F.0 is the LPC
controller on most Intel platforms and is very much present — a read of `(0,31,0)` returning a real
vendor ID says nothing about whether `(0,31,7)` is empty:

```text
>>> hex(edk2.readpci(0, 31, 7, 0, 4))
'0xffffffff'
```

All ones, and **no exception** — an absent device is not an error at this level, it is the
documented way to detect one. There is no fault protection here to lean on either: PCI config
access to nothing does not fault, so this is the only signal available.

### 13.6 The argument checks — the row that used to halt the box

```text
>>> edk2.readpci(0, 0, 0, 0x100, 4)
>>> edk2.readpci(0, 0, 0, 2, 4)
>>> edk2.readpci(0, 32, 0, 0, 4)
>>> edk2.readpci(0, 0, 8, 0, 4)
>>> edk2.readpci(0, 0, 0, 0, 3)
>>> edk2.writepci(0, 0, 0, 0, 0xFFFFFF, 2)
>>> print(1+1)
>>> exit()
```

| Call | Expected |
|---|---|
| `readpci(0,0,0,0x100,4)` | `ValueError: register offset 0x100 exceeds 0xFF; extended configuration space is unreachable...` |
| `readpci(0,0,0,2,4)` | `ValueError: register offset 0x2 is not 4-byte aligned; ...` |
| `readpci(0,32,0,0,4)` | `ValueError: bus/device/function 0/32/0 out of range (max 255/31/7); ...` |
| `readpci(0,0,8,0,4)` | `ValueError: bus/device/function 0/0/8 out of range ...` |
| `readpci(0,0,0,0,3)` | `ValueError: size must be 1, 2 or 4, not 3` |
| `writepci(0,0,0,0,0xFFFFFF,2)` | `OverflowError: value does not fit in 2 byte(s)` |

**The second row is the one to watch:** on 3.6.8 that exact call trips the alignment `ASSERT` in
`BasePciCf8Lib` and stops the machine. Surviving all six with a live prompt afterwards is the
result, same as §12.6.

**The last row was originally written as `0x10000` and that was a mistake — see §12.6.** The
console dropped a zero, the call became `0x1000`, which fits in two bytes and so correctly did not
raise, and it was reported as a defect in working code. `0xFFFFFF` survives any single dropped
character.

### 13.7 writepci — a write that is safe because nothing is listening

Writing PCI config on a live platform is not something to do casually, so the default test writes
to a **device that is not there** — the same absent b/d/f from §13.5:

```text
>>> edk2.writepci(0, 31, 7, 0, 0x1234, 2)
>>> hex(edk2.readpci(0, 31, 7, 0, 2))
'0xffff'
```

No exception, and the read still returns all ones. This proves the call path executes and the
argument checks pass, without touching anything real.

**Optional, stronger, and only on a machine you accept some risk on:** the vendor ID at offset
0x00 is architecturally read-only, so writing it should be a no-op that the read-back confirms:

```text
>>> before = edk2.readpci(0, 0, 0, 0, 2)
>>> edk2.writepci(0, 0, 0, 0, 0x1234, 2)
>>> edk2.readpci(0, 0, 0, 0, 2) == before
True
```

`True` means the write reached a real device and was correctly ignored, which is as close to
proving the write data path as can be done without changing platform state. **`False` means the
host bridge accepted a write to a read-only register and 00:00.0's vendor ID is now wrong** —
recoverable by a power cycle, since config space is not persistent, but stop and power-cycle
rather than continuing.

---

## 14. Phase 5 acceptance — physical memory, and the point of the whole exercise

**Status: WRITTEN, not yet built or tested.** FULL only for the API, but **both MIN builds are
required this time** — see §14.7.

`readmem`, `readmem_dword`, `writemem`, `writemem_dword`. These are the functions phases 1 and 2
existed to make safe: in 3.6.8, `readmem` dereferenced a caller-supplied address byte by byte with
nothing in the way, so a wrong address did not raise — it took a page fault and stopped the
machine, on a box that was being poked at precisely because something was already suspect about
it. Here a fault becomes `FaultError` with `vector`, `rip` and `cr2` attached, and the prompt comes
back.

### 14.1 What changed beyond the guard

| 3.6.8 | Here |
|---|---|
| `readmem` `malloc`s a scratch buffer and, on failure, returns NULL **with no exception set** — a confusing `SystemError` instead of `MemoryError` | reads straight into the `bytes` object; allocation failure is an ordinary `MemoryError` |
| `readmem` takes `int len`; a negative length makes `while(index--)` run about four billion times **writing memory** | `Py_ssize_t`, negative is `ValueError` |
| `writemem` parses `s#`, so it accepts `str` and writes its UTF-8 encoding — a different byte count than the string has characters, the moment one is non-ASCII | `y#`: bytes only, `TypeError` for `str` |

Two new C primitives back these: `edk2_guarded_copy()` in `efi/src/edk2excep.c` for the
variable-length pair, and the existing `edk2_guarded_access()` for the `_dword` pair. The copy uses
**one `setjmp` for the whole block** rather than one per byte — a fault anywhere fails the whole
call either way, since a partially filled buffer is not usable — and copies byte at a time through
`volatile` pointers rather than calling `memcpy`, because `memcpy` may use wide or vector moves and
a caller reaching into MMIO may be talking to a device that cares about access width.

### 14.2 Surface inventory — twelve names

```text
Python312.efi -S -c "import edk2; print(sorted(n for n in dir(edk2) if not n.startswith('_')))"
```

```text
['FaultError', 'cpuid', 'rdmsr', 'readio', 'readmem', 'readmem_dword', 'readpci', 'writeio', 'writemem', 'writemem_dword', 'writepci', 'wrmsr']
```

### 14.3 The headline: a bad address raises instead of halting

```text
Python312.efi -S
>>> import edk2, sys
>>> edk2.readmem(0, 0x8000, 16)
uefi.FaultError: CPU exception 13 (rip=0x... cr2=0x...)
>>> print(sys.last_value.vector, hex(sys.last_value.cr2))
>>> print(1+1)
```

`addr_lo=0, addr_hi=0x8000` is physical address 0x8000_00000000, the same non-canonical address
§5.9 uses. **On 3.6.8 this call ends the session and the machine needs a power cycle.** Here it
must raise, expose the fault attributes, and leave the prompt working — `print(1+1)` returning `2`
is as much a part of the test as the exception is.

Repeat the faulting call several times before moving on. A guard that leaks its slot would show up
as `RuntimeError: fault guard nesting depth (...) exceeded` after a few tries rather than on the
first.

### 14.4 Reads validated against a buffer whose contents we chose

Rather than reading unknown platform memory and hoping the answer looks plausible, point these at
a `ctypes` buffer — UEFI is identity-mapped, so its virtual address is its physical address:

```text
>>> import ctypes, edk2
>>> buf = ctypes.create_string_buffer(b'DEADBEEF')
>>> a = ctypes.addressof(buf)
>>> lo, hi = a & 0xFFFFFFFF, a >> 32
>>> edk2.readmem(lo, hi, 8)
b'DEADBEEF'
```

Exact, known, and it proves the two address halves are assembled in the documented order — get
that wrong and the address is nonsense, which faults rather than returning the right bytes.

Then the two primitives against each other, which is the check no external reference can give:

```text
>>> edk2.readmem(lo, hi, 4)
b'DEAD'
>>> hex(edk2.readmem_dword(lo, hi))
'0x44414544'
>>> int.from_bytes(edk2.readmem(lo, hi, 4), 'little') == edk2.readmem_dword(lo, hi)
True
```

`readmem` goes through `edk2_guarded_copy` byte at a time; `readmem_dword` goes through
`edk2_guarded_access` as a single 4-byte load. **They must agree**, and they reach the same memory
by different code paths, so this catches a mistake in either one.

### 14.5 Writes, into our own buffer, so nothing is at risk

```text
>>> edk2.writemem(lo, hi, b'12345678')
>>> buf.raw[:8]
b'12345678'
>>> edk2.writemem_dword(lo, hi, 0x41424344)
>>> buf.raw[:4]
b'DCBA'
>>> edk2.readmem(lo, hi, 4)
b'DCBA'
```

Python can see the result independently through `buf.raw`, so the write path is confirmed without
touching anything the platform depends on. The `b'DCBA'` is little-endian byte order and is the
expected answer, not a bug.

### 14.6 Argument handling

```text
>>> edk2.readmem(lo, hi, 0)
b''
>>> edk2.readmem(lo, hi, -1)
ValueError: length must not be negative
>>> edk2.writemem(lo, hi, 'abcd')
TypeError: ...
>>> edk2.readmem(hi, lo, 8)
uefi.FaultError: ...
>>> print(1+1)
>>> exit()
```

Zero length returns empty bytes without engaging the guard at all. The negative length is the row
worth dwelling on: on 3.6.8 that call does not raise, it runs a copy loop roughly four billion
times **writing** into a `malloc(-1)` buffer. The swapped-halves call is a sanity check on §14.4's
ordering claim — with a normal address, exchanging the halves produces an address in the high
petabytes, which faults.

### 14.7 Both MIN builds are required for this phase

Unlike phases 3 and 4, this one edits files MIN compiles: `efi/src/edk2excep.c` gains
`edk2_guarded_copy()`, and `posixmodule.c` has `uefi_set_fault_error` un-`static`'d and now
includes the new `Modules/edk2fault.h`. So:

- **VS2022 MIN and GCC MIN must at least build**, and a MIN image should re-run §5.9 tests 0-1 to
  confirm `uefi.mem_read`'s fault path still reports correctly after the helper changed linkage.
- **FULL must re-run §5.9 in full**, for the same reason.

`edk2fault.h` exists because the helper has to be callable from both modules while remaining
*defined* in `posixmodule.c`: MIN compiles `posixmodule.c` and not `edk2module.c`, yet MIN still
has `uefi.mem_read` and so still needs it. Defining it in `edk2module.c` instead would leave MIN
with an unresolved symbol, and it would point the dependency the wrong way — `uefi` is the `os`
module and is imported during startup, so it must not depend on a module that may be absent.
