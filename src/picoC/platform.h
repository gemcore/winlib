/* all platform-specific includes and defines go in this file */
#ifndef PLATFORM_H
#define PLATFORM_H

#define EMBEDDED_HOST   // Host has no OS (ie. 'bare-metal' MCU platform)

#ifndef EMBEDDED_HOST
#include <stdio.h>
#include <stdlib.h>
#else
//#include <sys/types.h>
//#include <sys/stat.h>
#endif
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <setjmp.h>
#include <math.h>
#include <stdbool.h>

//#define EMBEDDED_HOST

/* host platform includes */
#ifdef UNIX_HOST
# include <stdint.h>
# include <unistd.h>
//#elif defined(WIN32) /*(predefined on MSVC)*/
//#include <stdint.h>
#elif defined(EMBEDDED_HOST)  /* Generic Embedded Host */
# include <stdint.h>

#include "../term/bsp.h"           // board support functions
#ifndef WIN32
#include "fio.h"           // stdio mapping to Flash based FATFS functions
#include "ustdlib.h"       // standard C runtime functions
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#include "../term/platform_conf.h"

#ifndef PATH_MAX
#define PATH_MAX	80
#endif

#else
# error ***** A platform must be explicitly defined! *****
#endif


/* configurable options */
/* select your host type (or do it in the Makefile):
 #define UNIX_HOST
 #define DEBUGGER
 #define USE_READLINE (defined by default for UNIX_HOST)
 */
#define USE_READLINE

#if defined(WIN32) /*(predefined on MSVC)*/
#undef USE_READLINE
#endif

/* undocumented, but probably useful */
#undef DEBUG_HEAP
#undef DEBUG_EXPRESSIONS
#undef FANCY_ERROR_MESSAGES
#undef DEBUG_ARRAY_INITIALIZER
#undef DEBUG_LEXER
#undef DEBUG_VAR_SCOPE


#if defined(__hppa__) || defined(__sparc__)
/* the default data type to use for alignment */
#define ALIGN_TYPE double
#else
/* the default data type to use for alignment */
#define ALIGN_TYPE void*
#endif

#define GLOBAL_TABLE_SIZE (97)                /* global variable table */
#define STRING_TABLE_SIZE (97)                /* shared string table size */
#define STRING_LITERAL_TABLE_SIZE (97)        /* string literal table size */
#define RESERVED_WORD_TABLE_SIZE (97)         /* reserved word table size */
#define PARAMETER_MAX (16)                    /* maximum number of parameters to a function */
#define LINEBUFFER_MAX (256)                  /* maximum number of characters on a line */
#define LOCAL_TABLE_SIZE (11)                 /* size of local variable table (can expand) */
#define STRUCT_TABLE_SIZE (11)                /* size of struct/union member table (can expand) */

#define INTERACTIVE_PROMPT_START     "Starting picoc " PICOC_VERSION " (Ctrl+Z to exit)\n"
#define INTERACTIVE_PROMPT_STATEMENT "picoc> "
#define INTERACTIVE_PROMPT_LINE      "c> "
#define INTERACTIVE_PROMPT_LEAVE     "\nLeaving picoc"

extern jmp_buf ExitBuf;

#endif /* PLATFORM_H */
