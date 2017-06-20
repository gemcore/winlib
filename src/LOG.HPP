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

#ifndef LOG_HPP
#define LOG_HPP

#include <string.h>
#include <stdarg.h>
#include "BASEFILE.HPP"

/*==========================================================================
|
|  class definition
|
===========================================================================*/

class LogToFile : BASEFILE
{
   int fh;
   int bflag;

public:
	LogToFile(char *filename, int flag=1);
   ~LogToFile();
   
   void Flush(void);
   void PutChar(int c);
   void PutStr(char *s);
   int Printf(const char *format,...);
};

#endif

