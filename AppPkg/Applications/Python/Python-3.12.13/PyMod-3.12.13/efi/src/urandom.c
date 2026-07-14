#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include <Uefi.h>
#include <Library/UefiLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/Cpu.h>
#include <Protocol/Shell.h>
#include <Protocol/Rng.h>

#include <efi/edk2main.h>
#include <efi/random.h>

int
uefi_urandom(unsigned char *buffer, ssize_t size, int raise)
{
  static int seed = 0;
  uint32_t rnd;
  ssize_t remaining = size;

  if (!seed && g_edk2_globals.rng != NULL) {
    EFI_STATUS status;

    status = g_edk2_globals.rng->GetRNG(g_edk2_globals.rng,
                                        NULL, /* default rng algo */
                                        sizeof(rnd),
                                        (unsigned char*)&rnd);
    if (!EFI_ERROR(status)) {
      srand(rnd);
      seed = 1;
    }
  }

  if (!seed) {
    seed = 1;
    srand(time(NULL));
  }

  do {
    rnd = rand();
    if (remaining >= sizeof(uint32_t)) {
      *((uint32_t *)buffer) = rnd;
      buffer += sizeof(uint32_t);
      remaining -= sizeof(uint32_t);
    } else {
      memcpy((char *)buffer, (char *)&rnd, remaining);
      buffer += remaining;
      remaining = 0;
    }
  } while (remaining > 0);

  return size;
}

ssize_t
getrandom(void *buf, size_t buflen, unsigned int flags)
{
  return uefi_urandom(buf, buflen, 0);
}
