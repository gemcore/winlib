/*0===========================================================================
**
** File:       circbuf.cpp
**
** Purpose:    Circular Buffer class methods.
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
#include "circbuf.hpp"              /* Circular Buffer base class i/f */

/*==========================================================================
|
|  constants
|
===========================================================================*/

/*==========================================================================
|
|  functions definitions
|
===========================================================================*/

/*==========================================================================
|
|  class methods
|
===========================================================================*/

/*
** Initialize a circular buffer.
*/

circbuf::circbuf()
{
}

circbuf::circbuf(long size)
{
   bufptr = new char[(int)size];
   
   end = bufptr+size-1; 
   init();
}

circbuf::~circbuf()
{
	if (bufptr)
   {
      delete bufptr;
   }
}

#pragma check_stack(off)            // turn stack checking off!

/*
** Get size of circular buffer.
*/

long circbuf::getsize()
{
   return (long)(end-bufptr);
}

/*
** Get count of bytes in circular buffer.
*/

long circbuf::getcount()
{
   return getsize()-space;
}

/*
** Insert next byte into circular buffer.
*/

int circbuf::insert(int c)
{
   if (space)
   {
      *put = c;
      if (++put >= end)
         put = bufptr;
      --space;
      return 0;
	}
   else
      return ERR_BUFFER;
}

/*
** Remove next byte from circular buffer.
*/

int circbuf::remove(int *c)
{
   if (space && get == put)
      return ERR_BUFFER;
	else
   {
      *c = (unsigned char)*get;
      if (++get >= end)
         get = bufptr;
      ++space;
      return 0;
   }
}

/*
** Return value of next byte from circular buffer.
*/

int circbuf::peek()
{
   return (space && get == put)? ERR_BUFFER : (unsigned char)*get;
}

int circbuf::forward(long offset)
{
   if (offset > getcount())
      return ERR_BUFFER;
      
   space += offset;
   get += offset;
   if (get > end)
   {
      get -= getsize();
   }
   return 0;
}

int circbuf::rewind(long offset)
{
   if ((space-offset) < 0)
      return ERR_BUFFER;
   space -= offset;
   if (get <= put)
   {
      get -= offset;
      if (get < bufptr)
      {
         get += getsize();
      }
   }
   else
   {
      get += offset;
      if (get > end)
      {
         get -= getsize();
      }
   }
   return 0;
}

int circbuf::operator[](int i)
{
    if (i+1 > getcount())
    {
      return ERR_BUFFER;
    }
    
    if ( (get+i) >= end )
    {
        return ((unsigned char)*(get+i-getsize() ));
    }
    else
    {
        return ( (unsigned char)*(get+i) );
    }
}





#pragma check_stack( )
