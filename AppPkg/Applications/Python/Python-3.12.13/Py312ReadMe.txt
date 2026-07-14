Python 3.12.13 for UEFI — AppPkg (Iteration 1)
===============================================

This tree is being migrated from edk2-py312 into the edk2-libc AppPkg layout
(same pattern as Python-3.6.8).

Status and detailed steps:
  ../Python312_AppPkg_Migration_Plan.md
  ../Python312_AppPkg_Migration_Status.md

Iteration 1 constraints
-----------------------
* GCC build path first (VS2022 later).
* PACKAGES_PATH = <edk2>;<edk2-libc> only.
* Do NOT enable modules needing:
  edk2-libffi (_ctypes), edk2-openssl (_ssl / _hashopenssl),
  edk2-zlib (zlib), edk2-pyreadline (readline).

Prep (when PyMod is populated)
------------------------------
  cd AppPkg/Applications/Python/Python-3.12.13
  python srcprep.py
  # apply patches/*.patch to edk2-libc as documented in the plan
  # run frozen/ when Phase 3 is complete

Build (after Phase 4–5 complete)
--------------------------------
  set PACKAGES_PATH=<edk2>;<edk2-libc>
  set EDK2_LIBC_PATH=<edk2-libc>
  build -a X64 -b NOOPT -t GCC -p AppPkg/AppPkg.dsc -D BUILD_PYTHON312

PREFIX (from current 3.12 port pyconfig; locked for Iteration 1)
-----------------------------------------------------------------
  PLATFORM    uefi
  PREFIX      fs0:\EFI
  EXEC_PREFIX fs0:\EFI
  lib dir     lib\python3.12

Package (stage FAT / QEMU rootfs)
---------------------------------
  export WORKSPACE=<edk2>
  export EDK2_LIBC_PATH=<edk2-libc>
  cd $EDK2_LIBC_PATH/AppPkg/Applications/Python/Python-3.12.13
  ./create_python_pkg.sh GCC NOOPT X64 /path/to/out

  Produces:
    <out>/EFI/bin/Python312.efi
    <out>/EFI/lib/python3.12/
    <out>/EFI/stdlib/etc/

REPL smoke (UEFI Shell, fs0: = package EFI tree)
------------------------------------------------
  fs0:
  cd EFI\bin
  Python312.efi

  >>> import sys; print(sys.version); print(sys.platform); print(sys.path)
  >>> import os; print(os.listdir('fs0:\\'))
  >>> # Iteration 1: these must fail to import
  >>> import ssl; import ctypes; import zlib; import readline
