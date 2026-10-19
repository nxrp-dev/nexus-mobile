/* Source configuration for the retained native toolchains. */
#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>

#define INDEX_SIZE 1000
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_LIMITS_H 1
#if !defined(_WIN32)
#define HAVE_UNISTD_H 1
#endif

#define SIZEOF_INT32_T 4
#if defined(_WIN64)
#define SIZEOF_LONG 4
#define SIZEOF_OFF_T 4
#define SIZEOF_SIZE_T 8
/* The bundled MSVC mpg123 header declares ssize_t as long. */
#define SIZEOF_SSIZE_T 4
#elif defined(__LP64__)
#define SIZEOF_LONG 8
#define SIZEOF_OFF_T 8
#define SIZEOF_SIZE_T 8
#define SIZEOF_SSIZE_T 8
#else
#define SIZEOF_LONG 4
#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS == 64
#define SIZEOF_OFF_T 8
#else
#define SIZEOF_OFF_T 4
#endif
#define SIZEOF_SIZE_T 4
#define SIZEOF_SSIZE_T 4
#endif
