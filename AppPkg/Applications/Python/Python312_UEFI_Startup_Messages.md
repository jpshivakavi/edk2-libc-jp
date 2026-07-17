# Python312 UEFI startup console output

## Expected at launch (normal)

After **`UefiMain`** completes protocol setup and calls **`ShellCEntryLib`**, the REPL shows CPython’s usual banner and **`>>>`** prompt (from **`PyMod-3.12.13/Programs/python.c`** / interpreter startup). No extra firmware debug lines are intended before that banner.

## Removed debug prints (AppPkg)

These were **development-only** address dumps in the EFI entry point. They were removed so launch matches a clean REPL experience.

| File | Statements (removed) |
|------|----------------------|
| **`PyMod-3.12.13/efi/src/edk2main.c`** | `Print(L"Image base: 0x%lx\n", loaded_image_protocol->ImageBase);` |
| **`PyMod-3.12.13/efi/src/edk2main.c`** | `Print(L"UefiMain: 0x%lx\n", UefiMain);` |

Location in source: inside **`UefiMain()`**, after opening **`EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL`**, before shell parameters / **`ShellCEntryLib`**.

Same lines exist in **edk2-py312** `edk2-cpython/efi/PythonPkg/src/edk2main.c` (reference port); AppPkg drops them deliberately.

## Remaining `Print()` in `edk2main.c` (errors only)

Only emitted on failure paths:

- `Print(L"Failed to access image info: %r\n", status);`
- `Print(L"Failed to find CPU protocol: %r\n", status);`
- `Print(L"Failed to access CPU protocol: %r\n", status);`
- `Print(L"Failed to open input console: %r\n", status);` (non-fatal; startup continues)
- `Print(L"Failed to allocate stack memory\n");`

Rebuild **`Python312.efi`** after editing **`edk2main.c`**.
