/* Minimal main program -- everything is loaded from the library */

#include "Python.h"

#ifdef UEFI_C_SOURCE
int
main(int argc, char **argv)
{
   *stderr = *stdout;
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
