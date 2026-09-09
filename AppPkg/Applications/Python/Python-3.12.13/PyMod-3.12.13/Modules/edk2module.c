/* edk2module.c — platform-level UEFI operations (MSR, CPUID, PCI, port I/O,
 * physical memory, SMI, UEFI variables) for firmware tooling such as CHIPSEC.
 *
 * Python 3.6.8 shipped a single module named `edk2` that was two things at
 * once: an `os` implementation for UEFI, and a set of platform primitives. In
 * 3.12.13 that module is split. The `os` half became `uefi`
 * (PyMod-3.12.13/Modules/posixmodule.c, MODNAME "uefi"), which is upstream
 * CPython adapted for UEFI. This file is the other half, and it keeps the name
 * `edk2` so that the tools consuming those primitives import it unchanged.
 *
 * These functions deliberately do NOT live in posixmodule.c. `uefi` is the `os`
 * module, and importlib._bootstrap_external imports it while the interpreter is
 * still starting, so anything added there executes during startup: a failure
 * would not disable a function, it would stop Python from starting, on exactly
 * the sort of platform where there is then no Python left to investigate with.
 * This module is reached only by an explicit `import edk2`, so the worst case
 * is an import that raises while the interpreter stays usable. The inittab in
 * PyMod-3.12.13/Modules/config.c is a static table that is scanned only on
 * import, so the entry costs nothing at boot. Keep it that way: nothing in
 * here may be required for startup.
 *
 * Built only from Python312.inf (FULL). MIN does not compile this file, and the
 * matching config.c entry is gated on BUILD_PYTHON312_FULL to keep the MIN link
 * closed.
 *
 * See Python312_Chipsec_Platform_API_Port.md for the port plan, the 3.6.8
 * defects that are deliberately not reproduced here, and the phase ordering.
 */

#include "Python.h"

PyDoc_STRVAR(module_doc,
"Platform-level UEFI operations: MSR, CPUID, PCI config, port I/O, physical\n\
memory, software SMI and UEFI variables.\n\
\n\
This is the platform half of the module that Python 3.6.8 called `edk2`. The\n\
operating-system half of that module — open, read, stat, listdir and the rest\n\
— is now the `uefi` module, which is what `os` is built on. Code looking for\n\
file and process operations wants `uefi` (or just `os`); code looking for\n\
firmware primitives is in the right place.\n\
\n\
Guarded memory access raises uefi.FaultError, re-exposed here as\n\
edk2.FaultError, when an access takes a CPU fault that would otherwise have\n\
stopped the machine.");

/* Single-phase init with m_size = -1, following edk2console.c. This build has
 * one interpreter and no subinterpreter support, so per-module state buys
 * nothing over file statics here. */
static PyMethodDef edk2_methods[] = {
    {NULL, NULL}            /* Sentinel */
};

static struct PyModuleDef edk2module = {
    PyModuleDef_HEAD_INIT,
    "edk2",
    module_doc,
    -1,
    edk2_methods,
    NULL,
    NULL,
    NULL,
    NULL
};

PyMODINIT_FUNC
PyInit_edk2(void)
{
    return PyModule_Create(&edk2module);
}
