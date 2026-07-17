Vendored libffi (UEFI / ctypes) — Phase 8.5
============================================

Upstream reference (source list from LibFFI.inf):

  https://github.com/intel-sandbox/firmware.boot.uefi.python38.edk2-libffi
  Commit: 1fcd48b (EFI/LibFFI layout)

Layout in this tree:

  include/     ffi.h, fficonfig.h, ffitarget.h (from edk2-libffi/EFI/LibFFI/Include)
  src/         closures.c, debug.c, prep_cif.c, raw_api.c, types.c
  src/x86/     ffi64.c, ffiw64.c, unix64.S, win64.S

Linked into monolithic Python312.inf (no LibFFI.dec on PACKAGES_PATH).
Python312.inf adds -I.../Modules/libffi/include and -DNO_MSABI_VA_FUNCS.

_ctypes C sources with UEFI deltas live in PyMod-3.12.13/Modules/_ctypes/
(copied from edk2-cpython 3.12.13 UEFI port).

License: libffi license in upstream edk2-libffi repository.
