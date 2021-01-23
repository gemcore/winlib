/*
 * dbg.c
 *
 *  Created on: Dec 9, 2014
 *      Author: Alan
 */

#ifdef WIN32
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

//int CON_Init(void)
//{
//    return 0;
//}

//int CON_kbhit()
//{
//    return 0;
//}

//int CON_getc()
//{
//    return 0;
//}

int CON_putc(char c)
{
    return putchar(c);
}

void CON_ungetc(char c)
{
}

void CON_printf(const char *format, ...)
{
    va_list ap;
    // Start the varargs processing.
    va_start(ap, format);
    {
        char buf[BUFSIZ];
        vsprintf(buf, format, ap);
        term_putstr(buf, strlen(buf));
    }
    // We're finished with the varargs now.
    va_end(ap);
}

void CON_trace(const char *pcFormat, ...)
{
}

void CON_Flush(void)
{
}

void CON_EchoSet(bool state)
{
}

bool CON_EchoGet(void)
{
    return false;
}

void CON_NLSet(bool state)
{
}

bool CON_NLGet(void)
{
    return false;
}

#else
#include <ctype.h>
#include <string.h>
#include "ustdlib.h"
#include "dbg.h"
#include "inc/hw_memmap.h"
#include "driverlib/uart.h"
#if HAS_USB == 1
#if HAS_CDC == 1
#include "usb.h"
#endif
#endif
#if HAS_NET == 1
#include "net.h"            // Network interface interface (WiFi)
#endif
#include "cfg.h"

#define MAX_CONSOLES    3

ConsoleEntry Consoles[MAX_CONSOLES] =
{
    {   CONID_DBG,
        DBGgets,
        DBGgetc,
        DBGputs,
        DBGputc,
        DBGkbhit,
        DBGvprintf,
        DBGFind,
        DBGRxBytesAvail,
        DBGFlushTx,
        DBGEchoSet,
        DBGEchoGet,
        DBGNLSet,
        DBGNLGet,
        DBGGetStats,
        DBGungetc
    },
#if HAS_USB == 1
    {   CONID_USB,
        USBgets,
        USBgetc,
        USBputs,
        USBputc,
        USBkbhit,
        USBvprintf,
        USBFind,
        USBRxBytesAvail,
        USBFlushTx,
        USBEchoSet,
        USBEchoGet,
        USBNLSet,
        USBNLGet,
        USBGetStats,
        USBungetc
    },
#else
    {   CONID_USB,
        DBGgets,
        DBGgetc,
        DBGputs,
        DBGputc,
        DBGkbhit,
        DBGvprintf,
        DBGFind,
        DBGRxBytesAvail,
        DBGFlushTx,
        DBGEchoSet,
        DBGEchoGet,
        DBGNLSet,
        DBGNLGet,
        DBGGetStats,
        DBGungetc
    },
#endif 
#if HAS_NET == 1   
    {   CONID_NET,
        NETgets,
        NETgetc,
        NETputs,
        NETputc,
        NETkbhit,
        NETvprintf,
        NETFind,
        NETRxBytesAvail,
        NETFlushTx,
        NETEchoSet,
        NETEchoGet,
        NETNLSet,
        NETNLGet,
        NETGetStats,
        NETungetc
    }
#else    
    {   CONID_NET,
        DBGgets,
        DBGgetc,
        DBGputs,
        DBGputc,
        DBGkbhit,
        DBGvprintf,
        DBGFind,
        DBGRxBytesAvail,
        DBGFlushTx,
        DBGEchoSet,
        DBGEchoGet,
        DBGNLSet,
        DBGNLGet,
        DBGGetStats,
        DBGungetc
    }
#endif    
};

ConsoleEntry *con;

#define CONsetid(id)        con = &Consoles[id]

/*****************************************************************************
 * Debug functions
 *****************************************************************************/
uint8_t gethexnibble(char ch)
{
   // Return hex nibble.
   uint8_t usNib = tolower(ch);

   // convert ascii -> hex
   if (usNib >= '0' && usNib <= '9' )
      return usNib - '0';
   else if (usNib >= 'a' && usNib <= 'f' )
      return usNib - 'a' + 10;
   else
      return 0xff;
}

unsigned char *atobytes(char *s,uint8_t *bytes,int n)
{
   int i;
   unsigned char *bp = bytes;
   for (i=0; i < 2*n; i+=2)
   {
      int byte = gethexnibble(s[i+1]) + (gethexnibble(s[i+0]) << 4);
      *bp++ = (unsigned char)byte;
   }
   return bytes;
}

char *hextoa(char *s,uint8_t val)
{
   unsigned char _al,_ah;
   if ((_ah = (val>>4)) > 9)
      _ah += 'a'-0x0a;
   else
      _ah += '0';
   if ((_al = (val&0x0F)) > 9)
      _al += 'a'-0x0a;
   else
      _al += '0';
   *(s+0) = _ah;
   *(s+1) = _al;
   *(s+2) = '\0';
   return s;
}

char *bytestoa(uint8_t *bytes,char *s,int n)
{
    int i;
    uint8_t *bp = bytes;
    s[0] = '\0';
    for (i=0; i < 2*n; i+=2)
    {
        s[i] = '\0';
        hextoa(&s[i],(*bp++)&0xff);
        //sprintf(&s[i],"%02x",(*bp++)&0xff);
    }
    return s;
}

bool UARTIsConnected(void)
{
    return true;
}

void DBGtrace(const char *pcFormat, ...)
{
    if (CFG_IsTrace(DFLAG_TRC))
    {
        va_list ap;
        // Start the varargs processing.
        va_start(ap, pcFormat);
        {
        char buf[BUFSIZ];
        vsprintf(buf, pcFormat, ap);
        DBGputs(buf);
        }
        // We're finished with the varargs now.
        va_end(ap);
    }
}

uint8_t DBGgetline(uint8_t len,char *buf,bool echo)
{
    uint8_t ch;
    uint8_t i = 0;
    do
    {
        if (i >= (len-1))
        {
            ch = 0;
        }
        else
        {
            ch = DBGgetc();
            if (echo)
                DBGputc(ch);
            if (ch == '\r')
                ch = 0;
        }
        buf[i++] = ch;
#if HAS_WDT == 1        
        WDT_Reset();
#endif        
    }
    while(ch);
    if (echo)
    {
        DBGputc('\r');
        DBGputc('\n');
    }
    return i;
}

#if 0
//Note: This works but, it isn't a good implementation.

void strinsert(char *str,char ch)
{
    char s[20];
    s[0] = ch;
    s[1] = '\0';
    strcat(s,str);
    strcpy(str,s);
}
#else
void strinsert(char *str,char ch)
{
    int i;
    for (i=strlen(str)-1; i >= 0; i--)
    {
        char c = str[i+1];
        str[i+1] = str[i];
        str[i] = c;
    }
    str[0] = ch;
}
#endif

uint8_t DBGgethex(char *buffer,uint8_t len,bool echo)
{
    char s[20];
    uint8_t ch;
    uint8_t i = 0;
    while (i < len)
    {
        ch = DBGgetc();
        if (ch == '\r')
            break;
        if (!isxdigit(ch))
        {
            continue;
        }
        if (echo)
        {
            DBGputc(ch);
        }
        s[i++] = ch;
#if HAS_WDT == 1        
        WDT_Reset();
#endif        
    }
    s[i] = '\0';
    if (echo)
    {
        DBGputc('\r');
        DBGputc('\n');
    }
    if (i != 0)
    {
        // Make sure that there are the proper number of digits, add leading '0's if necessary.
        while (i < len)
        {
            strinsert(s,'0');
            i += 1;
        }
        // Add leading '0' if odd number of digits.
        if (i&1)
        {
            strinsert(s,'0');
            len += 1;
        }
        // Convert ascii string to binary bytes.
        len >>= 1;
        atobytes(s,(uint8_t *)buffer,len);
    }
    return i;
}

uint8_t DBGgetbool(char *buffer,bool *flag)
{
    uint8_t i = DBGgethex(buffer,1,true);
    if (i >= 1)
    {
        *flag = (buffer[0])?1:0;
    }
    else
    {
        DBGputc('0'+(*flag));
    }
    return i;
}

// CON console I/O.

void CON_SetId(char id)
{
    CFG_SetConId(id);
}

char CON_GetId(char id)
{
    return CFG_GetConId();
}

ConsoleEntry *CONptr(void)
{    
    uint8_t id = CFG_GetConId();        // get the configured Console Id

    /* Look for the highest priority Console that is active. */
#if HAS_USB == 1
#if HAS_CDC == 1
    if (id == 1 && USBIsConnected())
    {
        return &Consoles[CONID_USB];
    }
#endif
#endif
#if HAS_NET == 1
    if (id == 2 && NETIsConnected())
    {
        return &Consoles[CONID_NET];
    }
#endif
    /* Fall back to the Debug UART, which is always available. */
    return &Consoles[CONID_DBG];
}

void CON_vprintf(const char *pcString, va_list ap)
{
    CONptr()->vprintf(pcString,ap);
}

void CON_printf(const char *pcFormat, ...)
{
    va_list ap;    
    // Start the varargs processing.
    va_start(ap, pcFormat);
    {
    char buf[BUFSIZ];
    vsprintf(buf, pcFormat, ap);
    CONptr()->puts(buf);
    }
    // We're finished with the varargs now.
    va_end(ap);
}

void CON_trace(const char *pcFormat, ...)
{
    if (CFG_IsTrace(DFLAG_TRC))
    {
        va_list ap;
        va_start(ap, pcFormat);
        CONptr()->vprintf(pcFormat,ap);
        va_end(ap);
    }
}
#endif
