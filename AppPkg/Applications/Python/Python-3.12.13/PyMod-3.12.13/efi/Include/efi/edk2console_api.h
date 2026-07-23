/** Minimal API for Python core (no UEFI types). */
#pragma once

void edk2_console_detach_readline(void);

/** Call at UefiMain after console_in is opened (Shell re-launch after REPL). */
void edk2_console_prepare_for_launch(void);

/** Detach readline hooks and restore ConIn/ConOut for Shell (also safe before Py_Finalize). */
void edk2_console_handoff_to_shell(void);

/** ConOut restore only (after edk2_console_detach_readline). */
void edk2_console_restore_for_shell(void);
