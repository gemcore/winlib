/*0===========================================================================
**
** File:       ascii.h
**
** Purpose:    ASCII control character equates and other related constants.
**
**
**
**
** Date:       October 7, 1990
**
** Author:     Chris R. Kwong
**
**============================================================================
*/
#ifndef ASCII_H
#define ASCII_H

#ifndef NUL
#define NUL		   0x00
#endif
#ifndef SOH
#define SOH		   0x01
#endif
#ifndef STX
#define STX		   0x02
#endif
#ifndef ETX
#define ETX	  	   0x03
#endif
#ifndef EOT
#define EOT		   0x04
#endif
#ifndef ENQ
#define ENQ		   0x05
#endif
#ifndef ACK
#define ACK	  	   0x06
#endif
#ifndef BEL
#define BELL		0x07
#endif
#ifndef BS 
#define BS			0x08
#endif
#ifndef HT 
#define HT		   0x09
#endif
#ifndef TAB
#define TAB		   0x09
#endif
#ifndef LF
#define LF			0x0A
#endif
#ifndef VT 
#define VT		   0x0B
#endif
#ifndef FF
#define FF			0x0C
#endif
#ifndef CR
#define CR			0x0D
#endif
#ifndef SO 
#define SO			0x0E
#endif
#ifndef SI 
#define SI		   0x0F
#endif
#ifndef DLE
#define DLE		   0x10
#endif
#ifndef DC1
#define DC1		   0x11
#endif
#ifndef DC2
#define DC2	  	   0x12
#endif
#ifndef DC3
#define DC3		   0x13
#endif
#ifndef DC4
#define DC4		   0x14
#endif
#ifndef NAK
#define NAK	  	   0x15
#endif
#ifndef SYN
#define SYN	  	   0x16
#endif
#ifndef ETB
#define ETB		   0x17
#endif
#ifndef CAN
#define CAN		   0x18
#endif
#ifndef EM 
#define EM			0x19
#endif
#ifndef SUB
#define SUB		   0x1A
#endif
#ifndef ESC
#define ESC		   0x1B
#endif
#ifndef FS 
#define FS			0x1C
#endif
#ifndef GS 
#define GS		   0x1D
#endif
#ifndef RS 
#define RS		   0x1E
#endif
#ifndef US 
#define US		   0x1F
#endif
#ifndef SPA
#define SPACE	   0x20
#endif
#ifndef SP 
#define SP	      0x20
#endif
#ifndef DEL
#define DEL		   0x7F
#endif

//ADG! #define FAILED	   0xFF

#define BIT0	   0x0001
#define BIT1	   0x0002
#define BIT2	   0x0004
#define BIT3	   0x0008
#define BIT4	   0x0010
#define BIT5	   0x0020
#define BIT6	   0x0040
#define BIT7	   0x0080
#define BIT8	   0x0100
#define BIT9	   0x0200
#define BIT10	   0x0400
#define BIT11	   0x0800
#define BIT12	   0x1000
#define BIT13	   0x2000
#define BIT14	   0x4000
#define BIT15	   0x8000

#define CHAR_TRUE	   'T'
#define CHAR_FALSE	'F'

#ifndef TRUE
#define TRUE      1
#endif
#ifndef FALSE
#define FALSE     0
#endif

#ifndef BYTE
typedef unsigned char BYTE;
#endif
#ifndef WORD
typedef unsigned short WORD;
#endif
#ifndef DWORD
typedef unsigned long  DWORD;
#endif
#ifndef UINT
typedef unsigned int	 UINT;	      // machine sized unsigned number (preferred)
#endif
#ifndef LONG
typedef long		LONG;	            // 32-bit signed number
#endif

//#if !defined(BOOL)
//typedef int 		BOOL;	            // BOOLean (0 or !=0)
//#endif

#endif
