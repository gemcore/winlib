/*
 * trm.cpp
 *
 *  Created on: Jan 06, 2020
 *     Author: Alan
 * 
 *  ANSI Terminal interface (platform specific).
 */

#include <stdbool.h>
#include <ctype.h>
#ifndef WIN32
#include "ustdlib.h"
#else
#include <stdio.h>
#endif
#include "bsp.h"            // board support functions

#if HAS_PTS == 1
#include "pts.hpp"          // Scheduler
#endif
#include "dbg.h"            // debug console functions

/*******************************************************************************
 * Type definitions
 ******************************************************************************/
extern "C"
{
#include "third_party\term\term.h"
#include "third_party\term\platform_conf.h"

#define TRM_LINES           TERM_LINES
#define TRM_COLS            TERM_COLS

#define TRM_TIMEOUT         200    /* 0.200 seconds in msec ticks */
#define TRM_TIMER_ID        trm_tmrid

#define CON_UART_ID         0

#undef TRACE
//#define TRACE(format,...)  do{CON_printf(format,##__VA_ARGS__);CON_Flush();}while(0)   // wait for data to be sent
#define TRACE(format,...)

/*******************************************************************************
 * Local Variables
 ******************************************************************************/
static bool trm_initialized = false;
static byte trm_tmrid;

int platform_uart_recv(byte conid, byte tmrid, unsigned int timeout)
{
    BSP_reset_msec_cnt(tmrid);
    while (BSP_get_msec_cnt(tmrid) < timeout)
    {
        if (!CON_kbhit())
        {
            uint8_t id = PTS_GetCurTaskId();
            PTS_SuspendTask(id);
            PTS_Process();
            PTS_ResumeTask(id);
            continue;
        }
        return CON_getc();
    }
    return -1;
}

void platform_uart_send(byte conid, u8 data)
{
    CON_putc(data);
}

static void TRM_out(u8 data)
{
    CON_putc(data);
    CON_Flush();
}

static int TRM_in(int mode)
{
    if (mode == TERM_INPUT_DONT_WAIT)
    {
        return (CON_kbhit())? CON_getc() : -1;
    }
    else
    {
        return CON_getc();
    }
}

static int TRM_translate(int data)
{
    int c;

    TRACE("TRM_translate(%02x) ", data);
#ifdef WIN32
    if (data & 0x100)
    {
        data &= 0xFF;
        switch (data)
        {
        case 0x26:
            TRACE(" KC_UP\n");
            c = KC_UP;
            break;
        case 0x28:
            TRACE(" KC_DOWN\n");
            c = KC_DOWN;
            break;
        case 0x25:
            TRACE(" KC_LEFT\n");
            c = KC_LEFT;
            break;
        case 0x27:
            TRACE(" KC_RIGHT\n");
            c = KC_RIGHT;
            break;
        case 0x24:
            TRACE(" KC_HOME\n");
            c = KC_HOME;
            break;
        case 0x23:
            TRACE(" KC_END\n");
            c = KC_END;
            break;
        case 0x2E:
            TRACE(" KC_DEL\n");
            c = KC_DEL;
            break;
        default:
            TRACE(" %02x KC_UNKNOWN\n". data);
            return KC_UNKNOWN;
        }
        return c;
    }
    if (isprint(data))
    {
        TRACE(" '%c'\n", data);
        return data;
    }
    else
    {
        switch (data)
        {
        case 0x0D:
            // Check for CR/LF sequence, read the second char (LF) if applicable
            c = platform_uart_recv(CON_UART_ID, TRM_TIMER_ID, TRM_TIMEOUT);
            if (c != -1)
            {
                // Only swallow up LFs.
                if (c != 0x0A)
                {
                    CON_ungetc(c);
                }
                else
                {
                    TRACE(" <LF>");
                }
            }
            TRACE(" KC_ENTER\n");
            return KC_ENTER;
        case 0x08:
            TRACE(" KC_BACKSPACE\n");
            return KC_BACKSPACE;
        case 0x09:
            TRACE(" KC_TAB\n");
            return KC_TAB;
        case 0x01:
            TRACE(" KC_CTL_A\n");
            return KC_CTL_A;
        case 0x02:
            TRACE(" KC_CTL_B\n");
            return KC_CTL_B;
        case 0x03:
            TRACE(" KC_CTL_C\n");
            return KC_CTL_C;
        case 0x04:
            TRACE(" KC_CTL_D\n");
            return KC_CTL_D;
        case 0x05:
            TRACE(" KC_CTL_E\n");
            return KC_CTL_E;
        case 0x06:
            TRACE(" KC_CTL_F\n");
            return KC_CTL_F;
        case 0x07:
            TRACE(" KC_CTL_G\n");
            return KC_CTL_G;
        case 0x0B:
            TRACE(" KC_CTL_K\n");
            return KC_CTL_K;
        case 0x0C:
            TRACE(" KC_CTL_L\n");
            return KC_CTL_L;
        case 0x0E:
            TRACE(" KC_CTL_N\n");
            return KC_CTL_N;
        case 0x0F:
            TRACE(" KC_CTL_O\n");
            return KC_CTL_O;
        case 0x10:
            TRACE(" KC_CTL_P\n");
            return KC_CTL_P;
        case 0x11:
            TRACE(" KC_CTL_Q\n");
            return KC_CTL_Q;
        case 0x12:
            TRACE(" KC_CTL_R\n");
            return KC_CTL_R;
        case 0x13:
            TRACE(" KC_CTL_S\n");
            return KC_CTL_S;
        case 0x14:
            TRACE(" KC_CTL_T\n");
            return KC_CTL_T;
        case 0x15:
            TRACE(" KC_CTL_U\n");
            return KC_CTL_U;
        case 0x16:
            TRACE(" KC_CTL_V\n");
            return KC_CTL_V;
        case 0x17:
            TRACE(" KC_CTL_W\n");
            return KC_CTL_W;
        case 0x18:
            TRACE(" KC_CTL_X\n");
            return KC_CTL_X;
        case 0x19:
            TRACE(" KC_CTL_Y\n");
            return KC_CTL_Y;
        case 0x1A:
            TRACE(" KC_CTL_Z\n");
            return KC_CTL_Z;
        }
    }
    TRACE(" KC_UNKNOWN\n");
    return KC_UNKNOWN;
    
#else
    if (isprint(data))
    {
        TRACE(" '%c'\n", data);
        return data;
    }
    else if (data == 0x1B) // escape sequence
    {
        // If we don't get a second char, we got a simple "ESC", so return KC_ESC
        // If we get a second char it must be '[', the next one is relevant for us
        if ((c = platform_uart_recv(CON_UART_ID, TRM_TIMER_ID, TRM_TIMEOUT)) == -1)
        {   
            TRACE("KC_ESC\n");
            return KC_ESC;
        }
        TRACE("<ESC>%c", c);
        if ((c = platform_uart_recv(CON_UART_ID, TRM_TIMER_ID, TRM_TIMEOUT)) == -1)
        {
            TRACE(" KC_UNKNOWN\n");
            return KC_UNKNOWN;
        }
        else if (c >= 0x41 && c <= 0x44)
        {
            TRACE("%c", c);
            switch(c)
            {
            case 'A':
                TRACE(" KC_UP\n");
                return KC_UP;
            case 'B':
                TRACE(" KC_DOWN\n");
                return KC_DOWN;
            case 'C':
                TRACE(" KC_RIGHT\n");
                return KC_RIGHT;
            case 'D':
                TRACE(" KC_LEFT\n");
                return KC_LEFT;               
            }
        }
        else if (c > 0x30 && c < 0x37)
        {
            int c2;
            TRACE("%c", c);
            // Extended sequence: read another byte
            if ((c2 = platform_uart_recv(CON_UART_ID, TRM_TIMER_ID, TRM_TIMEOUT)) != 126)
            {
                if (c2 != -1)
                {
                    TRACE("%c", c2);
                }
                TRACE(" KC_UNKNOWN\n");
                return KC_UNKNOWN;
            }
            TRACE("%c", c2);
            switch(c)
            {
            case 0x31:
                TRACE(" KC_HOME\n");
                return KC_HOME;
            case 0x33:
                TRACE(" KC_DEL\n");
                return KC_DEL;
            case 0x34:
                TRACE(" KC_END\n");
                return KC_END;
            case 0x35:
                TRACE(" KC_PAGEUP\n");
                return KC_PAGEUP;
            case 0x36:
                TRACE(" KC_PAGEDOWN\n");
                return KC_PAGEDOWN;  
            }
        }
        TRACE(" KC_UNKNOWN\n");
        return KC_UNKNOWN;
    }
    else if (data == 0x0D) 
    {
        // Check for CR/LF sequence, read the second char (LF) if applicable
        c = platform_uart_recv(CON_UART_ID, TRM_TIMER_ID, TRM_TIMEOUT);
        if (c != -1)
        {
            // Only swallow up LFs.
            if (c != 0x0A)
            {
                CON_ungetc(c);
            }
            else
            {
                TRACE(" <LF>");
            }
        }
        TRACE(" KC_ENTER\n");
        return KC_ENTER;
    }
    else
    {
        switch(data)
        {
        case 0x09:
            TRACE(" KC_TAB\n");
            return KC_TAB;
        case 0x7F:
            TRACE(" KC_DEL\n");
            return KC_DEL;
        case 0x08:
            TRACE(" KC_BACKSPACE\n");
            return KC_BACKSPACE;
        case 0x01:
            TRACE(" KC_CTL_A\n");
            return KC_CTL_A;
        case 0x02:
            TRACE(" KC_CTL_B\n");
            return KC_CTL_B;
        case 0x03:
            TRACE(" KC_CTL_C\n");
            return KC_CTL_C;
        case 0x04:
            TRACE(" KC_CTL_D\n");
            return KC_CTL_D;
        case 0x05:
            TRACE(" KC_CTL_E\n");
            return KC_CTL_E;
        case 0x06:
            TRACE(" KC_CTL_F\n");
            return KC_CTL_F;
        case 0x07:
            TRACE(" KC_CTL_G\n");
            return KC_CTL_G;
        case 0x0B:
            TRACE(" KC_CTL_K\n");
            return KC_CTL_K; 
        case 0x0C:
            TRACE(" KC_CTL_L\n");
            return KC_CTL_L;
        case 0x0E:
            TRACE(" KC_CTL_N\n");
            return KC_CTL_N;
        case 0x0F:
            TRACE(" KC_CTL_O\n");
            return KC_CTL_O;
        case 0x10:
            TRACE(" KC_CTL_P\n");
            return KC_CTL_P;
        case 0x11:
            TRACE(" KC_CTL_Q\n");
            return KC_CTL_Q;
        case 0x12:
            TRACE(" KC_CTL_R\n");
            return KC_CTL_R;
        case 0x13:
            TRACE(" KC_CTL_S\n");
            return KC_CTL_S;
        case 0x14:
            TRACE(" KC_CTL_T\n");
            return KC_CTL_T;
        case 0x15:
            TRACE(" KC_CTL_U\n");
            return KC_CTL_U;
        case 0x16:
            TRACE(" KC_CTL_V\n");
            return KC_CTL_V;
        case 0x17:
            TRACE(" KC_CTL_W\n");
            return KC_CTL_W;
        case 0x18:
            TRACE(" KC_CTL_X\n");
            return KC_CTL_X;
        case 0x19:
            TRACE(" KC_CTL_Y\n");
            return KC_CTL_Y;
        case 0x1A:
            TRACE(" KC_CTL_Z\n");
            return KC_CTL_Z;
        }
  }
  TRACE(" KC_UNKNOWN\n");
  return KC_UNKNOWN;
#endif
}


bool TRM_IsInitialized(void)
{
    return trm_initialized;
}

void TRM_Init()
{
    if (!trm_initialized)
    {
        TRACE("TRM_Init()\n");
        TRM_TIMER_ID = BSP_claim_msec_cnt();
        term_init(TRM_LINES, TRM_COLS, TRM_out, TRM_in, TRM_translate);
        trm_initialized = true;
    }
}

void TRM_Term()
{
    if (trm_initialized)
    {
        TRACE("TRM_Term()\n");
        trm_initialized = false;
        term_term();
        BSP_return_msec_cnt(TRM_TIMER_ID);
    }
}

}

