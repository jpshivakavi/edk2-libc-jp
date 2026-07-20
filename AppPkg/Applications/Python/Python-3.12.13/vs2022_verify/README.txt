Phase V2 proof — pyconfig.h SIZEOF_* and PLATFORM
=================================================

Run after `srcprep.py` (so `Include/pyconfig.h` matches PyMod).

MSVC (matches Python312.inf `/DUEFI_MSVC_64` on X64):

  cd AppPkg\Applications\Python\Python-3.12.13\vs2022_verify
  REM From VS2022 dev shell or after edksetup.bat:
  verify_pyconfig_msft.bat

GCC reference port (no UEFI_MSVC_*):

  ./verify_pyconfig_gcc.sh

Both compile `verify_pyconfig_sizes.c`, which `#error`s if any expected
macro value is wrong. Success prints `OK: V2 ... pyconfig verify passed`.

Source of truth for edits: PyMod-3.12.13/Include/pyconfig.h and
PyMod-3.12.13/efi/Include/pyconfig.h — then re-run srcprep.py.
