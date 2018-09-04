/*0===========================================================================
**
** File:       coder.h
**
** Purpose:    
**
**
**
**
** Date:       March 3, 1994
**
** Author:     Alan D. Graves
**
**
** Rev  Date      Id    Description
** ---  --------- ---   -----------
** 1.0  03-Mar-94 ADG   Created.
**	
**============================================================================
*/

#ifdef MS_DOS
#include <stdio.h>                  /* C standard I/O includes */
#else
//#include "fio.h"
#include <stdio.h>                  /* C standard I/O includes */
#define FIO_FILE	FILE
#endif

#define DECODER         0
#define ENCODER         1

#define MAXBUFFERSIZE   128


class Coder
{
   FIO_FILE *ifp;
   int ibufsize;
   unsigned char *ibuffer;
   short iptr,icnt;
   short inbyte();
   void inbuffer();

   FIO_FILE *ofp;
   int obufsize;
   unsigned char *obuffer;
   short optr;
   long icountmax;
   void outbyte(short);
   void outbuffer();

   friend class NoCoder;
   friend class RLLCoder;

public:
   long icount;
   long ocount;

   Coder(FIO_FILE *ifp,FIO_FILE *ofp,int IBUFSIZE=MAXBUFFERSIZE,int OBUFSIZE=MAXBUFFERSIZE)
    { initialize(ifp,ofp,IBUFSIZE,OBUFSIZE); }

   ~Coder()
    { terminate(); }

   void initialize(FIO_FILE *,FIO_FILE *,int,int);
   void terminate();

   void seticountmax(long n) { icountmax = n; }

   int process(char);

   virtual void flushin();
   virtual void flushout();
   virtual int encode() = 0;
   virtual int decode() = 0;
};


/* No coding method */

class NoCoder : public Coder

{
public:
   NoCoder(FIO_FILE *ifp,FIO_FILE *ofp,int IBUFSIZE,int OBUFSIZE) : Coder(ifp,ofp,IBUFSIZE,OBUFSIZE)
      {}

   ~NoCoder()
      {}

   //---------------------------------------------------------------------
   // virtual functions required by Coder base class.
   //---------------------------------------------------------------------
   int encode();
   int decode() { return encode(); }
};

class RLLCoder : public Coder
{
public:
   RLLCoder(FIO_FILE *ifp,FIO_FILE *ofp,int IBUFSIZE,int OBUFSIZE) : Coder(ifp,ofp,IBUFSIZE,OBUFSIZE)
      {}

   ~RLLCoder()
      {}

   //---------------------------------------------------------------------
   // virtual functions required by Coder base class.
   //---------------------------------------------------------------------
   int encode();
   int decode();
};

