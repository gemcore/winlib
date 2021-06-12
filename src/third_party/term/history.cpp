/*
 * history.cpp
 *
 *  Created on: Nov 28, 2020
 *      Author: Alan
 */

#include <errno.h>
#include <string.h>
#include <ctype.h>
#ifndef WIN32
#include "fio.h"
#include "ustdlib.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp.h"
#endif
#include "evt.h"            // Event handling
#include "platform_conf.h"
#include "history.h"

#undef TRACE
#define TRACE(format,...)  do{CON_trace(format,##__VA_ARGS__);CON_Flush();}while(0)   // wait for data to be sent
//#define TRACE(format,...)

extern "C" void MEM_Dump(uint8_t *data,uint16_t len,uint32_t base);

#define MAX_SEQ_LEN           16

History::History()
{
   filename = NULL;
   maxlines = 0;
   lines = 0;
   Lines = NULL;   
}

History::~History()
{
}

int History::internal_add(const char *line, int force_empty) 
{
   char *linecopy;
   const char *p;

   if (!strcmp(line,"quit"))
   {
      return 0;
   }
   if (maxlines == 0)
   {
      return 0;
   }
   if (Lines == NULL)
   {
      if ((Lines = (char **)malloc(sizeof(char *) * maxlines)) == NULL)
      {
         fprintf(stderr, "out of memory in linenoise while trying to allocate history buffer.\n");
         return 0;
      }
      memset(Lines, 0, (sizeof(char *) * maxlines));
   }
   while (1)
   {
      if ((p = strchr(line, '\n')) == NULL)
         p = line + strlen(line);
      if (p > line || force_empty == HISTORY_PUSH_EMPTY)
      {
         if ((linecopy = strndup(line, p - line)) == NULL)
         {
            fprintf(stderr, "out of memory in linenoise while trying to add a line to history.\n");
            return 0;
         }
         if (lines == maxlines) 
         {
            free(Lines[0]);
            memmove(Lines, Lines+1, sizeof(char *) * (maxlines-1));
            lines--;
         }
         Lines[lines] = linecopy;
         lines++;
      }
      if (*p == 0)
      {
         break;
      }
      line = p+1;
      if (*line == 0)
      {
         break;
      }
   }
   return 1;
}

void History::setup(char *fname, int len)
{
   if (filename)
   {
      free(filename);
   }
   filename = strdup(fname);
   maxlines = len;

   if (filename != NULL)
   {
      int rc = load();
      if (rc == 0)
         TRACE("History load from %s\n", filename);
      else
         TRACE("Unable to load history from %s\n", filename);
   }
}

int History::load(void)
{
   FILE *fp;
   char cmd[HISTORY_MAX_LINE+1];  // Don't clobber the linenoise.cmd[] buffer!

   fp = fopen(filename, "r");
   if (fp == NULL) 
   {
      return -1;
   }
   \
   while (fgets(cmd, sizeof(cmd), fp) != NULL)
   {
      char *p;
      p = strchr(cmd, '\r');
      if (!p) 
      {
         p = strchr(cmd, '\n');
      }
      if (p) 
      {
         *p ='\0';
      }
      add(cmd);
   }
   fclose(fp);
   return 0;
}

int History::add(const char *line)
{
   return internal_add(line, HISTORY_DONT_PUSH_EMPTY);
}

/* Save the history in the specified file. 
 * On success 0 is returned, otherwise -1 is returned. */
int History::save(void)
{  
   FILE *fp;
   int j;
   
   if (maxlines == 0)
   {
      return HISTORY_NOT_ENABLED;
   }
   if (Lines == NULL || lines == 0)
   {
      return HISTORY_EMPTY;
   }
   if (filename == NULL)
   {
      return 0;
   }
   if ((fp = fopen(filename, "wb")) == NULL)
   {
      TRACE("Unable to save history to %s\n", filename);
      return -1;
   }
   for (j = 0; j < lines; j++)
   {
      fprintf(fp, "%s\n", Lines[j]);
   }
   fclose(fp);
   TRACE("History saved to %s\n", filename);
   return 0;
}

void History::cleanup(void)
{
   if (filename)
   {
      save();
      free(filename);
      filename = NULL;
   }    

   if (Lines)
   {
      for (int j=0; j < lines; j++)
      {
         free(Lines[j]);
      }
      free(Lines);
      Lines = NULL;
   }
   lines = 0;     
}

int History::view(void)
{
   int j = -1;
   if (Lines)
   {
      for (j=0; j < lines; j++)
      {
         CON_printf(" %2d: %s\n", j, Lines[j]);
      }
   }
   return j;
}

int History::move(int c, int *index, char *cmd, size_t cmdlen)
{
   if (lines > 1) 
   {
      int i = lines-1-(*index);
      TRACE("\ncmd='%s' index=%d i=%d\n", cmd, *index, i);
      for (int j=0; j < lines; j++)
      {
         if (i == j)
            TRACE(">");
         else
             TRACE(" ");
         TRACE("%2d: %s\n", j, Lines[j]);
      }
      if (c > 0)
      {
          TRACE("UP ");
          if ((*index) == lines)
          {
              return 1;
          }
      }
      else
      {
          TRACE("DN ");
          if (i == -1)
          {
              i = 0;
          }
      }
      /* Update the current history entry before to overwrite it with the next one. */
      free(Lines[i]);
      Lines[i] = strdup(cmd);
      TRACE("new histories[%d]='%s' ", i, Lines[i]);
      /* Show the new entry */
      *index += c;
      TRACE("new index=%d ", *index);
      if (*index < 0) 
      {
         *index = 0;
         TRACE("< 0 index=%d\n", *index);
         return 0;
      }
      else if ((*index) >= lines)
      {
         *index = lines-1;
         TRACE("> %d index=%d\n", lines, *index);
         return 0;
      }
      TRACE("!=%d index=%d ", lines, *index);
      i = lines-1-(*index);
      cmd[0] = '\0';
      strncat(cmd, Lines[i], cmdlen);
      TRACE("histories[%d]='%s'\n", i, cmd);
      return 2;
   }
   return 0;
}
