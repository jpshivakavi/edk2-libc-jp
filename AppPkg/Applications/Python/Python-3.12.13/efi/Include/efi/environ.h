#pragma once

extern wchar_t **environ;

void edk2_alloc_environ();
void edk2_free_environ();

int unsetenv(const char *name);
