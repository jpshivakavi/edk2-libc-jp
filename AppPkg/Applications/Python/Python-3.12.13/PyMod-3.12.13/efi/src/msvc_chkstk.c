/*
 * UEFI + MSVC: stack probe helper referenced by compiler-generated code
 * (e.g. libmpdec transpose.c large stack frames) and libffi win64.asm.
 * FULL builds supply this from libffi_msvc/ffi.c; MIN omits ctypes.
 */

#if defined(_MSC_VER) && defined(UEFI_C_SOURCE)

void __chkstk(void)
{
}

#endif
