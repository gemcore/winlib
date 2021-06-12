// Platform configuration

#ifndef __PLATFORM_CONF_H__
#define __PLATFORM_CONF_H__

#include "type.h"

// *****************************************************************************
// Define here what components you want for this platform

//ADG!
#if HAS_UIP == 1
#define BUILD_UIP
#define BUILD_DHCP
#endif
//#define BUILD_SHELL
//#define BUILD_ROMFS
//#define BUILD_CON_GENERIC
#define BUILD_TERM
#define BUILD_LINENOISE

#define TERM_LINES    25
#define TERM_COLS     80

#define HAS_TRC   0         // include TRACE output

#ifdef WIN32
#define strndup(P1,P2)        strdup(P1)
#define TRACE(format,...)
#else
#include "dbg.h"            // Debug serial I/O
#if HAS_TRC == 1
#define TRACE(format,...)     do{CON_trace(format,##__VA_ARGS__);CON_Flush();}while(0)
#else
#define TRACE(format,...)
#endif
#define printf(format,...)    do{CON_printf(format,##__VA_ARGS__);CON_Flush();}while(0)
#define puts(P0)              CON_puts(P0)
#define putchar(P0)           CON_putc(P0)
#define gets(P0,P1)           CON_gets(P0,P1)
#define getchar()             CON_getc()
#define kbhit()               CON_kbhit()

#define fopen(P0,P1)          fio_open(P0,P1)
#define freopen
#define fclose(P0)            fio_close(P0)
#define fread(P0,P1,P2,P3)    fio_read(P3,P0,P1,P2)
#define fwrite(P0,P1,P2,P3)   fio_write(P3,P0,P1,P2)
#define fprintf(P0,P1,...)    fio_printf(P0,P1,##__VA_ARGS__)
#define fgetc(P0)             fio_getc(P0)
#define fgets(P0,P1,P2)       fio_gets(P2,P0,P1)
#define fscanf(P0,P1,...)     fio_scanf(P0,P1,##__VA_ARGS__)
#define remove(P0)            fio_remove(P0)
#define rename
#define rewind(P0)            fio_seek(P0,0,SEEK_SET)
#define tmpfile()             fio_tmpfile()
#define clearerr(P0)          fio_clearerr(P0)
#define feof(P0)              fio_eof(P0)
#define ferror(P0)            fio_error(P0)
#define fileno(p)
#define fflush(P0)            fio_flush(P0)
#define fgetpos
#define fsetpos
#define fputc(P0,P1)          fio_putc(P1,P0)
#define fputs(P0,P1)          fio_puts(P1,P0)
#define ftell(P0)             fio_tell(P0)
#define fseek(P0,P1,P2)       fio_seek(P0,P1,P2)
#define setbuf
#define vsetbuf(P0,P1,P2,P3)  fio_vsetbuf(P0,P1,P2,P3)
#define ungetc(P0,P1)         fio_ungetc(P1,P0)
#define putc(P0,P1)           fio_putc(P1,P0)

#define perror(P0)

#define FILE    FIO_FILE

// Lua Platform configuration
#define NUM_TIMER       0
#define CPU_FREQUENCY   0
#endif

#endif // #ifndef __PLATFORM_CONF_H__
