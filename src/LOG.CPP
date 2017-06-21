/*0===========================================================================
**
** File:       log.cpp
**
** Purpose:    Log to file class methods.
**
**
**
**
** Date:       March 10, 1998
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#include "log.h"
#undef putchar
#undef printf
#define _INC_STDIO
#include <stdio.h>                  /* C Standard I/O definitions */
#include <stdarg.h>                 /* C ANSI standard variable args macros */
#include "syserr.h"

/*==========================================================================
|
|  function definitions
|
===========================================================================*/

/*==========================================================================
|
|  class methods
|
===========================================================================*/
LOG::LOG(char *filename, int flag) : BASEFILE(false, filename),
	bflag(flag)
{
}

LOG::~LOG()
{
	Flush();                         // flush buffer
}

void LOG::PutChar(int c)
{  
	/* Log character to the file. */
	BASEFILE::WritetoFile((BYTE)c);

	/* Out character to the console. */
	putchar(c);
}

void LOG::PutStr(char *s)
{
	while (*s)
	{
		PutChar(*s++);
	}
}

int LOG::Printf(const char *format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	char s[512];                     /* formatted output buffer */
	int num_chars = vsprintf_s(s, format, arg_ptr);
	PutStr(s);
	va_end(arg_ptr);
	return num_chars;
}

void LOG::Flush(void)
{
	FlushFileBuffers(BASEFILE::hbase);
}

LOG *log = NULL;

extern "C"
{
	void LOG_Init(char *filename)
	{
		if (log == NULL)
		{
			log = new LOG(filename);
			if (log)
			{
				puts("LOG_Init\n");
			}
		}
	}

	void LOG_Term(void)
	{
		if (log)
		{
			puts("LOG_Term\n");
			delete log;
			log = NULL;
		}
	}
}

#ifdef __TESTING_LOG__

void main()
{
   LOG log("log.txt");

   log.Printf("\nThis is written to the log file.");
   log.Flush();
   log.Printf("\nAnd so is this!");

}  

#endif


