/*0===========================================================================
**
** File:       coder.cpp
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
** 1.00 03-Mar-94 ADG   Created.
**      04-Apr-94 ADG   Fixed bug in Coder::inbuffer() routine, which was
**                      causing a buffer overrun on ibuffer[] array.
**	
**============================================================================
*/

/*==========================================================================
|
|  include headers
|
===========================================================================*/

#include <windows.h>
#include "LOG.H"		// Capture putchar & printf output to a log file
#include "coder.h"                  /* Coder De/compression filter class i/f */
#include <stdlib.h>                 /* C standard library includes */
#include <string.h>                 /* C string manipulation interface */
#include <limits.h>                 /* C intrinsic type ranges */
//#include "fio.h"
#define FIO_FILE	FILE
#define FR_OK		0
#define fio_open	fopen
#define fio_close	fclose
#define fio_read(A,B,C,D)	fread(B,C,D,A)
#define fio_write(A,B,C,D)	fwrite(B,C,D,A)
#define fio_seek	fseek
#define fio_tell	ftell
#define fio_getc	fgetc
#define fio_putc(A,B)	fputc(B,A)
#define fio_error	ferror
#define fio_size	fsize
#define HAS_STD		1

#define CON_printf	printf
#define	CON_putc	putchar

#undef TRACE
//#define TRACE(format,...)	printf(format,##__VA_ARGS__)
//#define TRACEF(format,...)  printf(format,##__VA_ARGS__);CON_Flush()   // wait for data to be sent
#define TRACE(format,...)
#define TRACEF(format,...)

/*==========================================================================
|
|  constant definitions
|
===========================================================================*/

/*==========================================================================
|
|  function definitions
|
===========================================================================*/

/*==========================================================================
|
|  variable definitions
|
===========================================================================*/

/*==========================================================================
|
|  class methods
|
===========================================================================*/

void Coder::initialize(FIO_FILE *ifilefp,FIO_FILE *ofilefp,int IBUFSIZE,int OBUFSIZE)
{
   TRACE("\nCoder::initialize() ");
   ifp = ifilefp;
   ibufsize = IBUFSIZE;
   icountmax= 0L;
   ibuffer = new unsigned char[ibufsize];
   iptr = icnt = 0;
   ofp = ofilefp;
   obufsize = OBUFSIZE;
   obuffer = new unsigned char[obufsize];
   if (!obuffer || !ibuffer)
   {
      CON_printf("Coder: out of memory\n");
      return;
   }
   TRACE("\nCoder::initialize ok ");
}

void Coder::terminate()
{
   TRACE("\nCoder::terminate() ");
   delete obuffer;
   delete ibuffer;
}

short Coder::inbyte()
{
   TRACE("\ninbyte()");
   if (iptr >= icnt)
   {
      inbuffer();
   }
   short c;
   if (icnt == 0 || icount >= icountmax)
   {
      TRACE(" EOF");
      return EOF;
   }
   TRACE(" iptr=%d",iptr);
   c = ibuffer[iptr++];
   TRACE(" c=%02x",c);
   icount++;
   return c;
}

void Coder::inbuffer()
{
   TRACE("\ninbuffer()");
   long n;
   if ((n = icountmax-icount) > (long)ibufsize)
   {
      n = (long)ibufsize;
   }
   TRACE(" n=%ld",n);

   if ((icnt = fio_read(ifp,(char *)ibuffer,sizeof(char),(int)n)) != n)
   {
      if (fio_error(ifp))
      {
         CON_printf("Coder: inbuffer file read\n");
      }
      TRACE("EOF found");
   }
   iptr = 0;
   flushin();                       // give chance for additional processing!
   TRACE(" icnt=%d",icnt);
}

void Coder::outbuffer()
{
   flushout();                       // give chance for additional processing!

   if (optr)
   {	  
      if (ofp && fio_write(ofp,(char *)obuffer,1,optr) != optr)
      {
         CON_printf("Coder: outbuffer file write\n");
      }
      ocount += optr;
   }
   optr = 0;
}

void Coder::outbyte(short c)
{
   obuffer[optr] = (char)c;
   if (++optr >= obufsize)
   {
      outbuffer();
   }
}

int Coder::process(char compflag)
{
   TRACE("\nCoder::process() ");
   icountmax = (icountmax)? icountmax : LONG_MAX;
   icount = 0L;
   iptr = icnt = 0;
   ocount = 0L;
   optr = 0;
   switch (compflag)
   {
   case ENCODER:
      encode();
      break;

   case DECODER:
      decode();
      break;
   }
   if (optr > 0)
   {
      outbuffer();
   }
   return 0;
}

int NoCoder::encode()
{
   short c;
   TRACE("\nde/encode() ");
   while ((c = inbyte()) != EOF)
   {
      outbyte(c);
   }
   return 0;
}

/* Run Length coding method */

#define PACK            0x80
#define PACKED          0x80

#define MASK            0x7f

int RLLCoder::encode()
{
   short j;
   short c,n;

   j = 0;

   TRACE("\nencode() ");
nextrll:
   if ((c = inbyte()) != EOF)
   {
      j++;
      while ((n = inbyte()) != EOF)
      {
         if (n == c)
         {
            if (++j == 127)
            {
               TRACE(" Pack(%d)",j);
               outbyte(PACK|j);
               TRACE(" byte=%02x",c);
               outbyte(c);
               j = 0;
               goto nextrll;
            }
         }
         else
         {
            if (j > 1)
            {
               TRACE(" Pack(%d)",j);
               outbyte(PACK|j);
            }
            else
            {
               if (c >= PACK)
               {
                  TRACE(" Pack(1)");
                  outbyte(PACK|1);
               }
            }
            TRACE(" byte=%02x",c);
            outbyte(c);
            j = 1;
            c = n;
         }
      }
   }
   if (j > 1)
   {
      TRACE(" Pack(%d)",j);
      outbyte(PACK|j);
   }
   else
   {
      if (c >= PACK)
      {
         TRACE(" Pack(1)");
         outbyte(PACK|1);
      }
   }
   TRACE(" byte=%02x",c);
   outbyte(c);
   return 0;
}

int RLLCoder::decode()
{
   short j;
   short c;

   TRACE("\ndecode() ");
   while ((c = inbyte()) != EOF)
   {
      if ((c&PACKED) != PACKED)
      {
         outbyte(c);
      }
      else
      {
         j = c&MASK;
         if ((c = inbyte()) != EOF)
         {
            while (j-- > 0)
            {     
               outbyte(c);
            }
         }
         else
         {
            CON_printf("RLLCoder: decode RLL last byte missing\n");
         }
      }
   }
   return 0;
}
