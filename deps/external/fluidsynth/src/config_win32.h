#define VERSION FLUIDSYNTH_VERSION

#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STDIO_H 1
#define HAVE_MATH_H 1
#define HAVE_STDARG_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_IO_H 1
#define HAVE_WINDOWS_H 1

#define DSOUND_SUPPORT 0
#define WINMIDI_SUPPORT 0
#define WITH_FLOAT 1

//#define snprintf _snprintf
#define strcasecmp _stricmp

#if _MSC_VER < 1500
#define vsnprintf _vsnprintf
#endif

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2


#define WITH_PROFILING 0

#pragma warning(disable : 4244)
#pragma warning(disable : 4101)
#pragma warning(disable : 4305)
#pragma warning(disable : 4996)

#ifndef inline
#define inline __inline
#endif

typedef int socklen_t;

#include <stdint.h>

typedef uint8_t u_int8_t;
typedef uint32_t u_int32_t;
