/*0===========================================================================
**
** File:       log.hpp
**
** Purpose:    Log to file class definitions.
**
**
**
**
** Date:       March 10, 1998
**
** Author:     Alan D. Graves
**
** Tools:      Built with MSVC 8.00C (1.52)
**
** MKS    $Revision: 1.1 $
**            $Date: 1999/12/18 00:52:56 $
**          $Author: Alan $
**          $Locker: $
**           $State: Rel $
**          $Source: E:/Projects/tpkit/Sys/rcs/LOG.HPP $
**     $ProjectName: E:\Projects/tpkit/Sys/project.pj $
** $ProjectRevision: 1.71 $
**
**             $Log: LOG.HPP $
**             Revision 1.1  1999/12/18 00:52:56  Alan
**             Initial revision
**
**============================================================================
*/

#ifndef LOG_H
#define LOG_H

#include <string.h>
#include <stdarg.h>
#include "BASEFILE.HPP"

#define putchar(c)	      do{ if (log) log->PutChar(c); }while(0)
#define printf(fmt, ...)  do{ if (log) log->Printf(fmt, __VA_ARGS__); }while(0)
#define TRACE(fmt, ...)   do{ if (log) log->Printf(fmt, __VA_ARGS__); }while(0)

/*==========================================================================
|
|  class definition
|
===========================================================================*/

class LOG : BASEFILE
{
   int bflag;

public:
	LOG(char *filename, int flag=1);
   ~LOG();
   
   void Flush(void);
   void PutChar(int c);
   void PutStr(char *s);
   int Printf(const char *format,...);
};

extern LOG *log;

extern "C"
{
	void LOG_Init(char *filename);
	void LOG_Term(void);
}

#endif

