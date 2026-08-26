#include "efi/py312boot.h"
#include "efi/edk2console_api.h"

#if defined(BUILD_PYTHON312_FULL)
#include <openssl/err.h>
#endif

static int py312_openssl_loaded;

void
py312_uefi_note_ssl_used(void)
{
    py312_openssl_loaded = 1;
}

void
py312_uefi_openssl_disarm(void)
{
    py312_boot_print_ascii("py312_uefi_openssl_disarm enter");
    py312_boot_print_ascii("py312_uefi_openssl_disarm leave");
}

void
py312_uefi_phase8_after_finalize(void)
{
#if defined(BUILD_PYTHON312_FULL)
    if (!py312_openssl_loaded) {
        return;
    }
    py312_boot_print_ascii("py312_uefi_phase8_after_finalize");
    ERR_clear_error();
    edk2_console_handoff_to_shell();
    py312_openssl_loaded = 0;
#endif
}
