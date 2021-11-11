#ifndef _COMPAT_STRING_H_
#define _COMPAT_STRING_H_

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__linux__) || defined(_WIN32)
#include <sys/types.h>
size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);
#endif

#ifdef __cplusplus
}
#endif

#endif // _COMPAT_STRING_H_