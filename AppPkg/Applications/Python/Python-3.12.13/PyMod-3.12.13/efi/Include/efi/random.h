#pragma once

int uefi_urandom(unsigned char *buffer, ssize_t size, int raise);
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);
