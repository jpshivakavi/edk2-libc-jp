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

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>

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

/* ---------------------------------------------------------------------------
 * MSR and CPUID
 *
 * Signatures follow Python 3.6.8's edk2module.c exactly, including the split
 * 32-bit halves, because consumers call these positionally and the return
 * shape is their ABI. A single 64-bit value would be nicer and is not on offer.
 *
 * No Py_BEGIN_ALLOW_THREADS around any of these, unlike 3.6.8. Threading is
 * stubbed in this build (efi/src/dummy_pthread.c), so releasing the GIL around
 * one instruction buys nothing and adds a moving part.
 * ------------------------------------------------------------------------- */

PyDoc_STRVAR(edk2_rdmsr__doc__,
"rdmsr(msr) -> (lower_32bits, higher_32bits)\n\
\n\
Read the given MSR on the current processor and return it as two 32-bit\n\
halves. Use rdmsr_ex to target a specific processor.\n\
\n\
No validation is possible: a reserved or unimplemented MSR raises #GP, which\n\
is a CPU fault and not catchable here, so it stops the machine.");

static PyObject *
edk2_rdmsr(PyObject *self, PyObject *args)
{
    unsigned int msr;
    UINT64 data;

    if (!PyArg_ParseTuple(args, "I:rdmsr", &msr))
        return NULL;

    data = AsmReadMsr64(msr);

    return Py_BuildValue("(II)",
                         (unsigned int)(data & 0xFFFFFFFFu),
                         (unsigned int)(data >> 32));
}

PyDoc_STRVAR(edk2_wrmsr__doc__,
"wrmsr(msr, lower_32bits, higher_32bits) -> None\n\
\n\
Write higher_32bits:lower_32bits to the given MSR on the current processor.\n\
\n\
There is no undo and no confirmation. Writing an MSR the firmware or a driver\n\
relies on can destabilise the platform in ways that only appear later.");

static PyObject *
edk2_wrmsr(PyObject *self, PyObject *args)
{
    unsigned int msr, eax, edx;

    if (!PyArg_ParseTuple(args, "III:wrmsr", &msr, &eax, &edx))
        return NULL;

    AsmWriteMsr64(msr, ((UINT64)edx << 32) | (UINT64)eax);

    Py_RETURN_NONE;
}

PyDoc_STRVAR(edk2_cpuid__doc__,
"cpuid(eax, ecx) -> (eax, ebx, ecx, edx)\n\
\n\
Execute CPUID with the given leaf in eax and subleaf in ecx on the current\n\
processor, and return all four result registers.\n\
\n\
ecx is significant only for leaves that define a subleaf; pass 0 otherwise.");

static PyObject *
edk2_cpuid(PyObject *self, PyObject *args)
{
    unsigned int leaf, subleaf;
    UINT32 eax = 0, ebx = 0, ecx = 0, edx = 0;

    if (!PyArg_ParseTuple(args, "II:cpuid", &leaf, &subleaf))
        return NULL;

    AsmCpuidEx(leaf, subleaf, &eax, &ebx, &ecx, &edx);

    return Py_BuildValue("(IIII)",
                         (unsigned int)eax, (unsigned int)ebx,
                         (unsigned int)ecx, (unsigned int)edx);
}

/* ---------------------------------------------------------------------------
 * Port I/O
 *
 * Three things are checked here that 3.6.8 did not check, each of which is a
 * real failure rather than pedantry:
 *
 * 1. The port number. 3.6.8 did `addrs = (short)(addr & 0xffff)` into a
 *    *signed* short, so every port from 0x8000 up became negative and then
 *    sign-extended to an enormous UINTN inside IoRead8 — i.e. the whole upper
 *    half of the I/O space addressed the wrong thing. A ValueError beats both
 *    that and the silent truncation the mask was there for.
 * 2. Alignment. MdePkg's IoRead16/IoRead32 ASSERT on a misaligned port, and
 *    this build has PcdDebugPropertyMask 0x0F (asserts enabled), so a
 *    misaligned access does not misbehave subtly — it stops the machine in
 *    DebugAssert(). Checking it here turns that into an exception.
 * 3. The value, for writes. 3.6.8 masked with & 0xFF, which silently writes
 *    something other than what the caller asked for. OverflowError matches
 *    uefi.mem_write, which made the same call for the same reason.
 *
 * An unimplemented port is still not detectable: reads return 0xFF..., writes
 * go nowhere, and neither faults.
 * ------------------------------------------------------------------------- */

static int
edk2_check_port(unsigned int port, unsigned int size)
{
    if (size != 1 && size != 2 && size != 4) {
        PyErr_Format(PyExc_ValueError, "size must be 1, 2 or 4, not %u", size);
        return -1;
    }
    if (port > 0xFFFF) {
        PyErr_Format(PyExc_ValueError,
                     "I/O port must be 0x0000-0xFFFF, not 0x%x", port);
        return -1;
    }
    if (port % size != 0) {
        PyErr_Format(PyExc_ValueError,
                     "port 0x%x is not %u-byte aligned; a misaligned port I/O "
                     "access trips an ASSERT in IoLib and stops the machine",
                     port, size);
        return -1;
    }
    return 0;
}

PyDoc_STRVAR(edk2_readio__doc__,
"readio(port, size) -> int\n\
\n\
Read size bytes (1, 2 or 4) from the given I/O port.\n\
\n\
port must be 0x0000-0xFFFF and size-aligned. An unimplemented port typically\n\
reads back all ones rather than failing.");

static PyObject *
edk2_readio(PyObject *self, PyObject *args)
{
    unsigned int port, size;
    UINT32 value = 0;

    if (!PyArg_ParseTuple(args, "II:readio", &port, &size))
        return NULL;
    if (edk2_check_port(port, size) < 0)
        return NULL;

    switch (size) {
    case 1: value = IoRead8((UINTN)port);  break;
    case 2: value = IoRead16((UINTN)port); break;
    case 4: value = IoRead32((UINTN)port); break;
    }

    return PyLong_FromUnsignedLong((unsigned long)value);
}

PyDoc_STRVAR(edk2_writeio__doc__,
"writeio(port, size, value) -> None\n\
\n\
Write value as size bytes (1, 2 or 4) to the given I/O port.\n\
\n\
port must be 0x0000-0xFFFF and size-aligned, and value must fit in size --\n\
a value too large is an OverflowError rather than a silent truncation, since\n\
a truncated write to hardware is worse than a refused one.");

static PyObject *
edk2_writeio(PyObject *self, PyObject *args)
{
    unsigned int port, size;
    unsigned long long value;

    if (!PyArg_ParseTuple(args, "IIK:writeio", &port, &size, &value))
        return NULL;
    if (edk2_check_port(port, size) < 0)
        return NULL;
    if (value >> (size * 8) != 0) {
        PyErr_Format(PyExc_OverflowError,
                     "value does not fit in %u byte(s)", size);
        return NULL;
    }

    switch (size) {
    case 1: IoWrite8((UINTN)port,  (UINT8 )value); break;
    case 2: IoWrite16((UINTN)port, (UINT16)value); break;
    case 4: IoWrite32((UINTN)port, (UINT32)value); break;
    }

    Py_RETURN_NONE;
}

/* ---------------------------------------------------------------------------
 * PCI configuration space
 *
 * Via PciLib, which AppPkg.dsc maps to BasePciLibCf8 — so every access here
 * goes through the legacy 0xCF8/0xCFC mechanism, and that has consequences the
 * caller has to know about:
 *
 *   - Register offsets stop at 0xFF. Extended configuration space (0x100-0xFFF,
 *     where PCIe capabilities live) is simply not reachable by CF8, and
 *     BasePciCf8Lib enforces that with an ASSERT: its address check is
 *     `ASSERT (((A) & (~0xffff0ff | (M))) == 0)`, and PCI_LIB_ADDRESS puts the
 *     register in bits 0..11, so an offset of 0x100 or more sets bits 8..11 and
 *     trips it. Asserts are live in this build. Reaching extended space means
 *     remapping PciLib in the DSC to an ECAM implementation, not changing this
 *     file — which is why the check below is a range error naming the limit
 *     rather than something that pretends to work.
 *   - That same M parameter is 1 for the 16-bit accessors and 3 for the 32-bit
 *     ones, so a misaligned register offset is an ASSERT too, exactly as in
 *     port I/O.
 *
 * 3.6.8 took `unsigned char` for bus, dev, fun and off. For the offset that
 * accidentally avoided the first ASSERT — 0x100 wrapped to 0x00 — by reading a
 * completely different register and returning it as if it were the one asked
 * for. It did nothing about the second. And bus/dev/fun truncation, combined
 * with the masking inside PCI_LIB_ADDRESS, meant an out-of-range device
 * silently became a *different, existing* device: harmless for a read, not for
 * writepci.
 *
 * Note the argument order of writepci: value comes BEFORE size, the opposite of
 * writeio above. That is inconsistent, it is not a mistake here, and it must not
 * be "fixed" — it is 3.6.8's published signature and callers pass positionally.
 * ------------------------------------------------------------------------- */

static int
edk2_check_pci(unsigned int bus, unsigned int dev, unsigned int func,
               unsigned int off, unsigned int size)
{
    if (size != 1 && size != 2 && size != 4) {
        PyErr_Format(PyExc_ValueError, "size must be 1, 2 or 4, not %u", size);
        return -1;
    }
    if (bus > 0xFF || dev > 0x1F || func > 7) {
        PyErr_Format(PyExc_ValueError,
                     "bus/device/function %u/%u/%u out of range "
                     "(max 255/31/7); PCI_LIB_ADDRESS would mask it down to a "
                     "different, possibly populated device",
                     bus, dev, func);
        return -1;
    }
    if (off > 0xFF) {
        PyErr_Format(PyExc_ValueError,
                     "register offset 0x%x exceeds 0xFF; extended configuration "
                     "space is unreachable through the CF8 mechanism that "
                     "PciLib is mapped to, and would trip an ASSERT in "
                     "BasePciCf8Lib", off);
        return -1;
    }
    if (off % size != 0) {
        PyErr_Format(PyExc_ValueError,
                     "register offset 0x%x is not %u-byte aligned; a misaligned "
                     "PCI config access trips an ASSERT in BasePciCf8Lib and "
                     "stops the machine", off, size);
        return -1;
    }
    return 0;
}

PyDoc_STRVAR(edk2_readpci__doc__,
"readpci(bus, dev, func, offset, size) -> int\n\
\n\
Read size bytes (1, 2 or 4) from PCI configuration space at bus:dev.func,\n\
register offset.\n\
\n\
offset must be 0x00-0xFF and size-aligned: PciLib is mapped to the CF8\n\
mechanism here, which cannot reach extended configuration space at all.\n\
\n\
An absent device is not an error -- it reads back as all ones, which is how\n\
you probe for one.");

static PyObject *
edk2_readpci(PyObject *self, PyObject *args)
{
    unsigned int bus, dev, func, off, size;
    UINT32 value = 0;
    UINTN address;

    if (!PyArg_ParseTuple(args, "IIIII:readpci", &bus, &dev, &func, &off, &size))
        return NULL;
    if (edk2_check_pci(bus, dev, func, off, size) < 0)
        return NULL;

    address = PCI_LIB_ADDRESS(bus, dev, func, off);
    switch (size) {
    case 1: value = PciRead8(address);  break;
    case 2: value = PciRead16(address); break;
    case 4: value = PciRead32(address); break;
    }

    return PyLong_FromUnsignedLong((unsigned long)value);
}

PyDoc_STRVAR(edk2_writepci__doc__,
"writepci(bus, dev, func, offset, value, size) -> None\n\
\n\
Write value as size bytes (1, 2 or 4) to PCI configuration space at\n\
bus:dev.func, register offset.\n\
\n\
Note that value precedes size here, unlike writeio(port, size, value). That\n\
is 3.6.8's signature and callers pass positionally, so it stays.\n\
\n\
Same offset restrictions as readpci. Out-of-range bus/dev/func is an error\n\
rather than a silent mask, because a masked write lands on a different and\n\
possibly real device.");

static PyObject *
edk2_writepci(PyObject *self, PyObject *args)
{
    unsigned int bus, dev, func, off, size;
    unsigned long long value;
    UINTN address;

    if (!PyArg_ParseTuple(args, "IIIIKI:writepci",
                          &bus, &dev, &func, &off, &value, &size))
        return NULL;
    if (edk2_check_pci(bus, dev, func, off, size) < 0)
        return NULL;
    if (value >> (size * 8) != 0) {
        PyErr_Format(PyExc_OverflowError,
                     "value does not fit in %u byte(s)", size);
        return NULL;
    }

    address = PCI_LIB_ADDRESS(bus, dev, func, off);
    switch (size) {
    case 1: PciWrite8(address,  (UINT8 )value); break;
    case 2: PciWrite16(address, (UINT16)value); break;
    case 4: PciWrite32(address, (UINT32)value); break;
    }

    Py_RETURN_NONE;
}

/* Single-phase init with m_size = -1, following edk2console.c. This build has
 * one interpreter and no subinterpreter support, so per-module state buys
 * nothing over file statics here.
 *
 * Kept alphabetical, so that the sorted dir() inventory used as a per-phase
 * acceptance check reads in the same order as this table. */
static PyMethodDef edk2_methods[] = {
    {"cpuid",    edk2_cpuid,    METH_VARARGS, edk2_cpuid__doc__},
    {"rdmsr",    edk2_rdmsr,    METH_VARARGS, edk2_rdmsr__doc__},
    {"readio",   edk2_readio,   METH_VARARGS, edk2_readio__doc__},
    {"readpci",  edk2_readpci,  METH_VARARGS, edk2_readpci__doc__},
    {"wrmsr",    edk2_wrmsr,    METH_VARARGS, edk2_wrmsr__doc__},
    {"writeio",  edk2_writeio,  METH_VARARGS, edk2_writeio__doc__},
    {"writepci", edk2_writepci, METH_VARARGS, edk2_writepci__doc__},
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
    PyObject *m, *uefi, *fault_error;

    m = PyModule_Create(&edk2module);
    if (m == NULL)
        return NULL;

    /* Reuse uefi.FaultError rather than defining a second exception type, so
     * that `except uefi.FaultError` catches faults from either module and
     * `edk2.FaultError is uefi.FaultError` holds. One type, no hierarchy.
     *
     * `uefi` is a builtin that the interpreter loads during startup, so by the
     * time anything can import edk2 this is a sys.modules hit rather than work.
     *
     * Failing the import if it is missing is deliberate. FaultError absent
     * means the fault-recovery infrastructure that this module's memory APIs
     * depend on is not in the image, and an import that raises is far easier to
     * diagnose than memory APIs that stop the machine instead of reporting. */
    uefi = PyImport_ImportModule("uefi");
    if (uefi == NULL)
        goto error;

    fault_error = PyObject_GetAttrString(uefi, "FaultError");
    Py_DECREF(uefi);
    if (fault_error == NULL)
        goto error;

    /* AddObjectRef does not steal, so the local reference is still ours. */
    if (PyModule_AddObjectRef(m, "FaultError", fault_error) < 0) {
        Py_DECREF(fault_error);
        goto error;
    }
    Py_DECREF(fault_error);

    return m;

error:
    Py_DECREF(m);
    return NULL;
}
