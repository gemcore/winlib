/*0===========================================================================
**
** File:       ecc.c
**
** Purpose:    Error Correction Code calculation methods.
**
**
**
**
** Date:       July 6, 1993
**
** Author:     Alan D. Graves
**
**
** Rev  Date      Id    Description
** ---  --------- ---   -----------
** 1.0  06-Jul-93 ADG   Created.
**	
**============================================================================
*/

/*
** Define CRC-16 polynomial.
*/

#define	CRC16_GEN_POLY	0x0A001

//#pragma check_stack(off)            // turn stack checking off!

/* Function prototypes */

void init_crctable(void);
int crc_byte(unsigned int,unsigned int);
unsigned int crc16_byte(unsigned char,unsigned int);
unsigned int crc16_buf(unsigned char *,int,unsigned int);
#if 0
unsigned char bcc_buf(unsigned char *,unsigned int);
#endif

/* Local variables */

static int initialized=0;
unsigned int poly=CRC16_GEN_POLY;   /* CRC-16:     1+X2+X15 + X16 */
unsigned int crc0=0;                /* crc initial value */
unsigned int crc_table[256];        /* crc byte lookup table */


/*1===========================================================================
**
** Function:	init_crctable
**
** Purpose:    Generate table of crc lookup values.
**
** Returns:    n/a
**
** Name        I/O   Description
** ----------- ---   -----------
**
**============================================================================
*/

void init_crctable()
{
   int i;

   if (!initialized)
   {
      for (i=0; i < 256; i++)       /* Generate lookup table */
      {
         unsigned char byte = i;
			crc_table[byte] = crc_byte(byte,crc0);
		}
   }
}

/*1===========================================================================
**
** Function:	crc_byte
**
** Purpose:    Generate crc value for specified data byte.
**
** Returns:    crc value.
**
** Name        I/O   Description
** ----------- ---   -----------
** data        i     input data value
** result      i     previous crc value.
**
**============================================================================
*/

int crc_byte(unsigned int data,unsigned int result)
{
   int   i;
   int   bit;

   for (i=0; i < 8; i++)
   {
      bit = (result ^ data) & 1;    /* test XOR of bit #0's */
      result >>= 1;                 /* do shift */
		if (bit)
      {
         result ^= poly;            /* feedback polynomial terms */
      }
      data >>= 1;                   /* next data bit */
   }
   return(result);
}

/*============================================================================
**
** Function:	crc16_byte(unsigned char byte, unsigned int crc)
**
** Purpose:    to calculate a 16 bit CRC on a byte
**
** Returns:    unsigned int crc value
**
** Name        		I/O   Description
** ----------- 		---   -----------
**
**============================================================================
*/

unsigned int crc16_byte(unsigned char byte,unsigned int crc)
{
   byte ^= crc&0xff;
   crc >>= 8;
   crc ^= crc_table[byte];
   return crc;
}

/*============================================================================
**
** Function:	crc16_buf(unsigned char *buffer, int buflen, unsigned int crc)
**
** Purpose:    to calculate a 16 bit CRC on a buffer
**
** Returns:    unsigned int crc value
**
** Name        		I/O   Description
** ----------- 		---   -----------
**
**============================================================================
*/

unsigned int crc16_buf(unsigned char *bufptr,int buflen,unsigned int crc)
{
   while (buflen-- > 0)
   {
      crc = crc16_byte(*bufptr++,crc);
   }
   return crc;
}

#if 0
/* This routines is only used by DataPac driver! */

/*======================================================================
**
** Function:	bcc_calc( unsigned char *ptr, unsigned int cnt )
**
** Purpose:    to calculate a 8 bit LRC BCC on a string of cnt long
**
** Returns:    unsigned char bcc value
**
** Name        		I/O   Description
** ----------- 		---   -----------
**
**======================================================================
*/

unsigned char	bcc_buf( unsigned char *ptr, unsigned int cnt )
{
	unsigned int i=0;
	register	unsigned char	bcc_val=0;

	for (i=0; i < cnt;)
   {
		bcc_val ^= *(ptr+i);
		i++;
	}
	return(bcc_val);
}
#endif

//#pragma check_stack( )              // restore stack checking


#ifdef __TESTING_CRC__
#include <stdio.h>                  /* C standard I/O includes */
#include <stdlib.h>                 /* C standard library includes */
#include <stddef.h>                 /* C standard definitions */

unsigned long crcfile(FILE *fp)
{
   register unsigned int result=crc0;
   register unsigned char data;
   unsigned long count=0;

   while (1)
   {
      data = fgetc(fp)&0xff;
      if (feof(fp))
         break;

      result = crc16_byte(data,result);
      count++;
   }
   printf("%-4x",result);
   return(count);
}

void main()
{
   FILE *fp;

   init_crctable();

   if ((fp = fopen("ecc.c","rb")) != NULL)
   {
      printf("CRC of file 'ecc.c' is ");
      printf("\n%ld bytes processed",crcfile(fp));
      close(fp);
   }
}

#endif

