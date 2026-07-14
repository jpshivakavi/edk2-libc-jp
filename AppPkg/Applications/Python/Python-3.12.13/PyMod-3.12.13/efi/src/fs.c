#include <wchar.h>
#include <wctype.h>
#include <errno.h>

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>

char*
wgetcwd (char *buf, size_t size)
{
  const wchar_t *Cwd;
  size_t len;
  
  if (size == 0 || buf == NULL) {
    errno = EINVAL;
    return NULL;
  }

  Cwd = ShellGetCurrentDir(NULL);
  if (Cwd == NULL) {
    errno = ENOENT;
    return NULL;
  }
  len = wcslen(Cwd);
  if (size < ((len + 1) * sizeof (wchar_t))) {
    errno = ERANGE;
    return (NULL);
  }

  wmemcpy((wchar_t*)buf, Cwd, len);
  ((wchar_t*)buf)[len] = 0;
  
  return buf;
}

char *
path_to_ascii(char* dst, wchar_t *src, size_t size)
{
   int status = UnicodeStrToAsciiStrS(src, dst, size);
   if(status != RETURN_SUCCESS)
      return NULL;

   return dst;
}
