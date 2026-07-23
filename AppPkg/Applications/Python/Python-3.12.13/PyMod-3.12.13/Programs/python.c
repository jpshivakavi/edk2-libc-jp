/* Minimal main program -- everything is loaded from the library */

#include "Python.h"

#ifdef UEFI_C_SOURCE
#include "efi/py312boot.h"
#include "efi/edk2console_api.h"
#include "internal/pycore_pymem.h"

extern char *(*PyOS_ReadlineFunctionPointer)(FILE *, FILE *, const char *);

void
py312_uefi_reentry_cleanup(void)
{
    if (!Py_IsInitialized()) {
        return;
    }
    edk2_console_detach_readline();
    (void)Py_FinalizeEx();
}

int
main(int argc, char **argv)
{
    int rc;
    py312_boot_print_ascii("main enter");
    /* Match 3.6.8 UEFI entry: malloc allocator before interpreter bootstrap. */
    (void)_PyMem_SetupAllocators(PYMEM_ALLOCATOR_MALLOC);
    py312_boot_print_ascii("after _PyMem_SetupAllocators");
    *stderr = *stdout;
    PyOS_ReadlineFunctionPointer = NULL;
    edk2_console_detach_readline();
    fputs("Python312: enter main\n", stdout);
    fflush(stdout);
    py312_boot_print_ascii("before Py_BytesMain");
    rc = Py_BytesMain(argc, argv);
    py312_boot_print_ascii("after Py_BytesMain");
    return rc;
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
