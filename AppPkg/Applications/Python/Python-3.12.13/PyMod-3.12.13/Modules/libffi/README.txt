Vendored libffi (UEFI / ctypes) — Phase 8.5
============================================

Upstream reference (source list from LibFFI.inf):

  https://github.com/intel-sandbox/firmware.boot.uefi.python38.edk2-libffi
  Commit: 1fcd48b (EFI/LibFFI layout)

Layout in this tree:

  include/           ffi.h, fficonfig.h, ffitarget.h (edk2-libffi/EFI/LibFFI/Include)
  libffi/include/    ffi_common.h, tramp.h (edk2-libffi/EFI/LibFFI/libffi/include)
  src/               closures.c, debug.c, prep_cif.c, raw_api.c, types.c
  src/x86/           ffi64.c, ffiw64.c, unix64.S, win64.S, internal64.h

Linked into monolithic Python312.inf (no LibFFI.dec on PACKAGES_PATH).
Python312.inf adds -I.../libffi/include, -I.../libffi/libffi/include (same as
edk2-libffi LibFFI.inf GCC flags) and -DNO_MSABI_VA_FUNCS.

_ctypes C sources with UEFI deltas live in PyMod-3.12.13/Modules/_ctypes/
(copied from edk2-cpython 3.12.13 UEFI port).

License: libffi license in upstream edk2-libffi repository.
