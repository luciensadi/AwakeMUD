#ifndef COMPAT_STRL_H
#define COMPAT_STRL_H

// Portable replacements for BSD strlcpy/strlcat on GNU/Linux and others.
// These functions return the length of the source string; the destination
// is always NUL-terminated when size > 0.

#include <string.h>
#include <stddef.h>

// If your system already provides strlcpy/strlcat, compile with
// -DHAVE_STRLCPY to avoid redefining them.
#ifndef HAVE_STRLCPY
static inline size_t strlcpy(char *dst, const char *src, size_t siz) {
  size_t srclen = strlen(src);
  if (siz != 0) {
    size_t copylen = (srclen >= siz) ? (siz - 1) : srclen;
    if (copylen > 0) {
      memcpy(dst, src, copylen);
    }
    dst[copylen] = '\0';
  }
  return srclen;
}

static inline size_t strlcat(char *dst, const char *src, size_t siz) {
  size_t dstlen = strnlen(dst, siz);
  size_t srclen = strlen(src);
  if (dstlen == siz) {
    // No space to concatenate; return the length we tried to create.
    return dstlen + srclen;
  }
  size_t space = siz - dstlen - 1;
  size_t copylen = (srclen > space) ? space : srclen;
  if (copylen > 0) {
    memcpy(dst + dstlen, src, copylen);
  }
  dst[dstlen + copylen] = '\0';
  return dstlen + srclen;
}
#endif // HAVE_STRLCPY

#endif // COMPAT_STRL_H
