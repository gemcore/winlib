/*
 * linenoise.cpp
 *
 * This CPP module is a ProtoThread implementation of the elua C module.
 *
 *  Created on: Nov 28, 2020
 *      Author: Alan
 */

/* linenoise.c -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * You can find the latest source code at:
 * 
 *   http://github.com/antirez/linenoise
 *
 * Does a number of crazy assumptions that happen to be true in 99.9999% of
 * the 2010 UNIX computers around.
 *
 * Copyright (c) 2010, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * References:
 * - http://invisible-island.net/xterm/ctlseqs/ctlseqs.html
 * - http://www.3waylabs.com/nw/WWW/products/wizcon/vt220.html
 *
 * Todo list:
 * - Switch to gets() if $TERM is something we can't support.
 * - Filter bogus Ctrl+<char> combinations.
 * - Win32 support
 *
 * Bloat:
 * - Completion?
 * - History search like Ctrl+r in readline?
 *
 * List of escape sequences used by this program, we do everything just
 * with three sequences. In order to be so cheap we may have some
 * flickering effect with some slow terminal, but the lesser sequences
 * the more compatible.
 *
 * CHA (Cursor Horizontal Absolute)
 *    Sequence: ESC [ n G
 *    Effect: moves cursor to column n
 *
 * EL (Erase Line)
 *    Sequence: ESC [ n K
 *    Effect: if n is 0 or missing, clear from cursor to end of line
 *    Effect: if n is 1, clear from beginning of line to cursor
 *    Effect: if n is 2, clear entire line
 *
 * CUF (CUrsor Forward)
 *    Sequence: ESC [ n C
 *    Effect: moves cursor forward of n chars
 *
 * [eLua] code adapted to eLua by bogdanm
 * 
 */

#include <errno.h>
#include <string.h>
#include <ctype.h>
#include "bsp.h"
#ifndef WIN32
#include "fio.h"
#include "ustdlib.h"
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#include "evt.h"            // Event handling
#include "pts.hpp"          // Scheduler
#include "term.h"
#include "linenoise.h"
#include "platform_conf.h"
#include "cli.h"

#undef TRACE
//#define TRACE(format,...)  do{CON_printf(format,##__VA_ARGS__);CON_Flush();}while(0)   // wait for data to be sent
#define TRACE(format,...)

extern "C" void MEM_Dump(uint8_t *data,uint16_t len,uint32_t base);

#define EDIT_MAX_LINE                  HISTORY_MAX_LINE

#define EDIT_IDLE                      0

#define EDIT_ERROR                     -1
#define EDIT_ENTER                     -2
#define EDIT_CTL_Z                     -3
#define EDIT_CTL_X                     -4
#define EDIT_CTL_C                     -5
#define EDIT_UP                        -6
#define EDIT_DOWN                      -7
#define EDIT_JOIN                      -8
#define EDIT_CTL_E                     -9

LineNoise_Thread::LineNoise_Thread()
{
   TRACE("LIN::LIN() @%x\n", this);
   Name("LIN ");
   //setprompt("");
   prompt = NULL;
   status = EDIT_IDLE;
   cmd[0] = '\0';
   cols = term_get_cols();
   /* Make sure there is always space for the nulterm */
   cmdlen = EDIT_MAX_LINE - 1;
   pos = 0;          // the current cursor position
   //x_cur = 0;
   //y_cur = 0;
   //text_field = 0;
   //dflag = 0;        // debug not enabled
}

LineNoise_Thread::~LineNoise_Thread()
{
   TRACE("LIN::~LIN()\n");
   history.cleanup();
}

int LineNoise_Thread::Init()
{
   TRACE("LIN::Init()\n");
   return 0;
}

void LineNoise_Thread::Prompt(void)
{
   term_putstr(prompt, plen);
}

void LineNoise_Thread::refresh(void)
{
   /* Cursor to absolute left position. */
   term_gotoxy(1, 0);
   /* Write the prompt and the current buffer content */
   term_putstr(prompt, plen);
   term_putstr(cmd, len);
   /* Erase to end of line */
   term_clrline(0);
   /* Cursor to absolute left position. */
   term_gotoxy(1, 0);
   term_right(plen+pos);
}

void LineNoise_Thread::cleanup(void)
{
   history.cleanup();
}

bool LineNoise_Thread::Run()
{
   static int index;

   PTS_BEGIN();

   while (1)
   {
      pos = 0;
      len = 0;
      index = 0;

      cmd[0] = '\0';

      /* The latest history entry is always our current buffer, that
       * initially is just an empty string. */
      history.internal_add("", HISTORY_PUSH_EMPTY);

      Prompt();

      while (1)
      {
         int c;
         int dir;
         PTS_WAIT_UNTIL((c = term_getch(TERM_INPUT_DONT_WAIT)) != -1);

         if (c == KC_ENTER || c == KC_CTL_C || c == KC_CTL_Z)
         {
            history.lines--;
            free( history.Lines[history.lines]);
            if (c == KC_CTL_C)
            {
               TRACE("^C");
               status = EDIT_CTL_C;
            }
            else if (c == KC_CTL_Z)
            {
               TRACE("^Z");
               status = EDIT_CTL_Z;
            }
            else
            {
               TRACE("^M");
               status = 0;
            }
            break;
         }

         // Handle any line edit commands.
         if (c == KC_UP)
         {
            TRACE("UP");
            dir = 1;
            goto arrow;
         }
         else if (c == KC_DOWN)
         {
            TRACE("DN");
            dir = -1;
         arrow:
            switch (history.move(dir, &index, cmd, cmdlen))
            {
            case 2:
               len = pos = strlen(cmd);
            case 1:
               TRACE("refreshLine('%s',%d,%d,%d)\n\n",cmd,len,pos,cols);
               refresh();
            case 0:
               break;
            }
         }
         else if  (c == KC_BACKSPACE)
         {
            if (pos == 0)
            {
               status = EDIT_JOIN;
               break;
            }
            if (pos > 0 && len > 0) 
            {
               memmove(cmd+pos-1,cmd+pos,len-pos);
               pos--;
               len--;
               cmd[len] = '\0';
               refresh();
            }
         }
         else if (c == KC_CTL_T)   /* Ctl-T */
         {
            /*
            // This seems to be rather useless and also a bit buggy.
            if (pos > 0 && pos < len) {
               int aux = cmd[pos-1];
               cmd[pos-1] = cmd[pos];
               cmd[pos] = aux;
               if (pos != len-1)
                  pos++;
               refresh();
            }
            */
         }
         else if (c == KC_LEFT)
         {
            /* left arrow */
            if (pos > 0) 
            {
               pos--;
               refresh();
            }         
         }
         else if (c == KC_RIGHT)
         {
            /* right arrow */
            if (pos < len) 
            {
               pos++;
               refresh();
            }
         }
         else if (c == KC_DEL)
         {
            if (len == 0)
            {
               status = EDIT_JOIN;
               break;
            }
            if (len > 0 && pos < len) 
            {
               memmove(cmd+pos,cmd+pos+1,len-pos-1);
               len--;
               cmd[len] = '\0';
               refresh();
            }
         }
         else if (c == KC_HOME)
         {
            /* Ctl-A, go to the start of the line */
            pos = 0;
            refresh();
         }
         else if (c == KC_END)
         {
            /* Ctl-E, go to the end of the line */
            pos = len;
            refresh();
         }
         else if (c == KC_CTL_U)
         {
            /* Ctl-U, delete the whole line. */
            cmd[0] = '\0';
            pos = len = 0;
            refresh();
         }
         else if (c == KC_CTL_K)
         {
            /* Ctl-K, delete from current to end of line. */
            cmd[pos] = '\0';
            len = pos;
            refresh();
         }
         else if (c == KC_ESC)
         {
         }
         else
         {
            // Insert printable ascii characters into the buffer.
            c &= 0xFF;
            if (isprint(c) && len < cmdlen)
            {
               if (len == pos) 
               {
                  cmd[pos] = c;
                  pos++;
                  len++;
                  cmd[len] = '\0';
                  if (plen+len < cols) 
                  {
                     /* Avoid full updates of the line in trivial cases. */
                     term_putch(c);
                  } 
                  else 
                  {
                     refresh();
                  }
               } 
               else 
               {
                  memmove(cmd+pos+1,cmd+pos,len-pos);
                  cmd[pos] = c;
                  len++;
                  pos++;
                  cmd[len] = '\0';
                  refresh();
               }
            }
         }
      }

      if ( status != EDIT_CTL_Z )
      {
         term_putch('\n');
      }
      if ( status != EDIT_CTL_C )
      {  
         break;
      }
   }

   if (len > 0 && cmd[len] != '\0')
   {
      cmd[len] = '\0';
   }

   // Keep a persistent history of command lines.
   history.internal_add( cmd, HISTORY_DONT_PUSH_EMPTY ); 

   if (Exec() == -1)
   {
      PTS_RESTART();
   }

   PTS_END();
}

bool LineNoise_Thread::sendcmd(uint8_t id)
{
   uint8_t evt[1+1+1+HISTORY_MAX_LINE+1];
   int len = strlen(cmd);
   evt[0] = len+2;
   evt[1] = EVT_LIN;
   evt[2] = (uint8_t)(-status);
   if (len > 0)
   {
      memcpy(&evt[3], cmd, len);
   }
   if (PTS_PutMsg(id, evt))
   {
      return true;
   }
   else
   {
     return false;
   }
}

extern "C"
{
void linenoise_setup(const char *prompt, const char *filename, int maxlines)
{
   TRACE("linenoise_setup('%s',%s,%d)\n",prompt,filename,maxlines);
   CLI_Save();
   CLI_Setup(prompt, filename, maxlines);
}

int linenoise_getline(char* buffer, int maxinput, char *prompt)
{
   int rc = CLI_GetLine(buffer, maxinput, prompt);
   TRACE("linenoise_getline('%s',%d,'%s') rc=%d\n",buffer,maxinput,prompt,rc);
   return rc;
}

int linenoise_addhistory(const char *line)
{
   TRACE("linenoise_addhistory(%s)\n",line);
   return CLI_AddLine(line);
}

int linenoise_savehistory(void)
{
   TRACE("linenoise_savehistory()\n");
   return CLI_Save();
}

void linenoise_cleanup(void)
{
   TRACE("linenoise_cleanup()\n");
   CLI_Cleanup();
}
}

