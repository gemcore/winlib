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
#define _INC_STDIO
#include <stdio.h>                  /* C Standard I/O definitions */
#include <stdarg.h>                 /* C ANSI standard variable args macros */
#include "syserr.h"
#include "log.hpp"

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
LogToFile::LogToFile(char *filename, int flag) : BASEFILE(false, filename),
	bflag(flag)
{
}

LogToFile::~LogToFile()
{
	Flush();                         // flush buffer
}

void LogToFile::PutChar(int c)
{  
	putchar(c);
	BASEFILE::WritetoFile((BYTE)c);
}

void LogToFile::PutStr(char *s)
{
	while (*s)
	{
		PutChar(*s++);
	}
}

int LogToFile::Printf(const char *format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	char s[512];                     /* formatted output buffer */
	int num_chars = vsprintf_s(s, format, arg_ptr);
	PutStr(s);
	va_end(arg_ptr);
	return num_chars;
}

void LogToFile::Flush(void)
{
}

#ifdef __TESTING_LOG__

void main()
{
   LogToFile log("LOG.LOG");

   log.Printf("\nThis is written to the log file.");
   log.Flush();
   log.Printf("\nAnd so is this!");

}  

#endif


