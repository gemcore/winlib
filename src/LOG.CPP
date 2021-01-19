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
#undef puts
#undef putchar
#undef printf
#define _INC_STDIO
#include <stdio.h>                  /* C Standard I/O definitions */
#include <stdarg.h>                 /* C ANSI standard variable args macros */
#include "syserr.h"
#include "thread.h"

static Mutex mutex;

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
LOG::LOG(char *filename) : BASEFILE(false, filename),
	bflag(true)
{
}

void LOG::Putchar(int c)
{ 
	if (bflag)
	{
		/* Log character to the file. */
		BASEFILE::WritetoFile((BYTE)c);
	}
	/* Out character to the console. */
	if (!qflag)
	{
		fputc(c,stderr);
	}
}

void LOG::Puts(char *s)
{
	Lock lock(mutex);

	/* Output a string. */
	while (*s)
	{
		Putchar(*s++);
	}
}

int LOG::Printf(const char *format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	char s[512];                     /* formatted output buffer */
	int num_chars = vsprintf_s(s, format, arg_ptr);
	/* Output a string. */
	Puts(s);
	va_end(arg_ptr);
	return num_chars;
}

void LOG::Flush(void)
{
	FlushFileBuffers(BASEFILE::hbase);
}

LOG *logger = NULL;

extern "C"
{
	void LOG_Init(char *filename)
	{
		if (logger == NULL)
		{
			logger = new LOG(filename);
		}
	}

	void LOG_Term(void)
	{
		if (logger)
		{
			logger->Flush();
			delete logger;
			logger = NULL;
		}
	}

	void LOG_Enable(bool flag)
	{
		if (logger)
		{			
			logger->Enable(flag);
		}
	}

	void LOG_Flush(void)
	{
		logger->Flush();
	}

	void LOG_Putchar(char c)
	{
		if (logger)
		{
			logger->Putchar(c);
		}
	}

	void LOG_Puts(char *s)
	{
		if (logger)
		{
			logger->Puts(s);
		}
	}

	void LOG_Printf(const char *format, ...)
	{
		va_list arg_ptr;

		//
		// Start the varargs processing.
		//
		va_start(arg_ptr, format);

		char s[256];                     /* formatted output buffer */
		int num_chars = vsnprintf(s, sizeof(s), format, arg_ptr);

		if (logger)
		{
			logger->Puts(s);
		}

		//
		// We're finished with the varargs now.
		//
		va_end(arg_ptr);
	}
}

extern "C"
{
#include "term/cli.h"
#include "term/bsp.h"

	static void usage(void)
	{
		CON_printf("open   : open file\n");
		CON_printf("put    : output line of text\n");
		CON_printf("flush  : flush\n");
		CON_printf("close  : close file\n");
		CON_printf("quiet  : disable console output\n");
		CON_printf("verbose: enable console output\n");
	}

	int Cmd_log(int argc, char *argv[])
	{
		int rc = 0;
		char *p;

		if (argc > 1)
		{
			p = argv[1];
			if (!strcmp(p, "open"))
			{
				LOG_Init((char *)"sys.log");
			}
			else if (!strcmp(p, "close"))
			{
				LOG_Term();
			}
			else if (!strcmp(p, "flush"))
			{
				LOG_Flush();
			}
			else if (!strcmp(p, "put"))
			{
				if (argc > 2)
				{
					char *p = argv[2];
					LOG_Printf("%s\n", p);
				}
				else
				{
					return(CMDLINE_TOO_FEW_ARGS);
				}
			}
			else if (!strcmp(p, "quiet"))
			{
				qflag = true;
			}
			else if (!strcmp(p, "verbose"))
			{
				qflag = false;
			}
			else if (!strcmp(p, "?"))
			{
				usage();
			}
			else
			{
				return(CMDLINE_INVALID_ARG);
			}
		}
		else
		{
			return(CMDLINE_TOO_FEW_ARGS);
		}
		return rc;
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