/*0===========================================================================
**
** File:       circbuf.hpp
**
** Purpose:    Circular Buffer base class definition.
**
**
**
**
** Date:       April 16, 1991
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#ifndef CIRCBUF_HPP
#define CIRCBUF_HPP
#include "syserr.h"                 /* System wide error definitions */

/* Circular Buffer base class definition */

#pragma check_stack(off)            // turn stack checking off!

class circbuf
{
   char *bufptr;
   char *get;
   char *put;
   char *end;
   long space;

public:
   circbuf();
   circbuf(long);
   ~circbuf();

   long getbuffer() { return (long)bufptr; }
   long getspace() { return space; }
   long getcount();
   long getpos() { return (long)get; }
   long putpos() { return (long)put; }

   void init(void) { get = bufptr; put = bufptr; space = (long)(end-bufptr); }
   long getsize();
   int insert(int);
   int remove(int *);
   int peek();
   int rewind(long offset);
	int forward(long offset);
	int operator[](int i);
};

#pragma check_stack( )


#endif

