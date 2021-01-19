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

#define putchar(c)	      do{ if (logger) logger->Putchar(c); }while(0)
#define puts(s)		      do{ if (logger) logger->Puts(s); }while(0)
#define printf(fmt, ...)  do{ if (logger) logger->Printf(fmt, __VA_ARGS__); }while(0)

#define TRACE(fmt, ...)   do{ if (!qflag) fprintf(stderr, fmt, __VA_ARGS__); }while(0)
#define DEBUG(fmt, ...)   do{ if (!qflag) fprintf(stderr, fmt, __VA_ARGS__); }while(0)

/*==========================================================================
|
|  class definition
|
===========================================================================*/

class LOG : BASEFILE
{
   bool bflag;

public:
	LOG(char *filename);
	~LOG() { Flush(); }
   
   void Flush(void);
   void Putchar(int c);
   void Puts(char *s);
   int Printf(const char *format,...);
   void Enable(bool flag) { bflag = flag; }
};

extern char qflag;
extern LOG *logger;

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
// API functions
//*****************************************************************************

void LOG_Init(char *filename);
void LOG_Term(void);
void LOG_Enable(bool flag);
void LOG_Flush(void);
void LOG_Putchar(char c);
void LOG_Puts(char *s);
void LOG_Printf(const char *format, ...);

//*****************************************************************************
// Definition of CLI commands
//*****************************************************************************
int Cmd_log(int argc, char *argv[]);

#ifdef    __cplusplus
}
#endif

#endif //#ifndef _LOG_H 
