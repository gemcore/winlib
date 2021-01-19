/* linenoise.h -- guerrilla line editing library against the idea that a
 * line editing lib needs to be 20,000 lines of C code.
 *
 * See linenoise.c for more information.
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
 */

#ifndef __LINENOISE_H
#define __LINENOISE_H

#ifdef __cplusplus
#include "history.h"
#include "pts.hpp"

class LineNoise_Thread : public PTS_Thread
{
public:
   size_t cmdlen = HISTORY_MAX_LINE - 1;
   size_t pos;
   size_t len;
   size_t cols;

   LineNoise_Thread();
   ~LineNoise_Thread();

   virtual int Init();
   virtual bool Run();
   virtual void Prompt(void);
   virtual int Exec(void) { return -1; }

   void setup(const char *prompt, char *filename, int maxlines);
   void cleanup(void);

   void refresh(void);

   bool sendcmd(uint8_t id);

   void setprompt(const char *p)
   {
      if (p != NULL)
      {
         prompt = p;
         plen = strlen(p);
      }
   }
   
   const char *getprompt(void)
   {
      return prompt;
   }

   const char *prompt;
   size_t plen;

   int status;
   char cmd[HISTORY_MAX_LINE + 1];

   History history;
};

extern "C" {
#endif

void linenoise_setup(const char *prompt, const char *filename, int maxlines);
int linenoise_getline(char* buffer, int maxinput, char *prompt);
int linenoise_addhistory(const char *line);
int linenoise_savehistory(void);
void linenoise_cleanup(void);

#ifdef    __cplusplus
}
#endif // __cplusplus

#endif /* __LINENOISE_H */
