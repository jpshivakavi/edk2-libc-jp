/* Minimal main program -- everything is loaded from the library */

#include "Python.h"

#ifdef UEFI_C_SOURCE
#include "efi/py312boot.h"
int
main(int argc, char **argv)
{
    py312_boot_print_ascii("main enter");
    /* Match 3.6.8 UEFI entry: malloc allocator before interpreter bootstrap. */
    (void)_PyMem_SetupAllocators("malloc");
    py312_boot_print_ascii("after _PyMem_SetupAllocators");
    *stderr = *stdout;
    fputs("Python312: enter main\n", stdout);
    fflush(stdout);
    py312_boot_print_ascii("before Py_BytesMain");
    return Py_BytesMain(argc, argv);
}
#elif defined(MS_WINDOWS)
int
wmain(int argc, wchar_t **argv)
{
    return Py_Main(argc, argv);
}
#else
int
main(int argc, char **argv)
{
    return Py_BytesMain(argc, argv);
}
#endif
