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

Required setup: apply libc patches (do NOT commit StdLib)
---------------------------------------------------------
Python 3.12 needs four edk2-libc patches that are not in upstream edk2-libc
yet. Keep StdLib vanilla on the branch; apply them locally before every
fresh checkout / before the first GCC build.

  cd <edk2-libc>   # must be the same tree as EDK2_LIBC_PATH / PACKAGES_PATH
  git apply --check --ignore-whitespace \
    AppPkg/Applications/Python/Python-3.12.13/patches/*.patch
  git apply --ignore-whitespace \
    AppPkg/Applications/Python/Python-3.12.13/patches/*.patch

  # Verify (patch 0001 is required to link):
  ls StdLib/LibC/Uefi/upipe.c

Patches (self-contained under this tree):
  patches/0001-Implement-minimal-emulation-of-pipe-functionality.patch
  patches/0002-Introduce-support-for-ANSI-escape-codes-for-console.patch
  patches/0003-Fix-uninitialized-static-variable.patch
  patches/0004-Fix-ioctl-vararg-handling-for-Console-and-Shell-devi.patch

Policy: leave these as a local build prerequisite until they land upstream
(tianocore/edk2-libc). Do not fold StdLib diffs into the Python migration
commits. Discard local StdLib dirt with `git checkout -- StdLib
StdLibPrivateInternalFiles` and `git clean -fd StdLib` if you need a clean
tree, then re-apply as above.

Prep
----
  cd AppPkg/Applications/Python/Python-3.12.13
  python3 srcprep.py
  # freeze/deepfreeze: see Python312_WSL_GCC_Build_Guide.md (artifacts are gitignored)

Build
-----
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
