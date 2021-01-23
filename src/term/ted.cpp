#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include "bsp.h"            // board support functions
#ifndef WIN32
#include "ustdlib.h"
#include "fio.h"
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#if HAS_EVT == 1
#include "evt.h"            // Event handling
#endif
#if HAS_PTS == 1
#include "pts.hpp"          // Scheduler
#endif
#if HAS_CLI == 1
#include "cli.h"            // Console handling
#endif
#include "dbg.h"            // debug console functions
#include "platform_conf.h"
#include "term.h"

#undef TRACE
//#define TRACE(format,...)  do{CON_trace(format,##__VA_ARGS__);CON_Flush();}while(0)   // wait for data to be sent
#define TRACE(format,...)

#define EDIT_MAX_LINE                  81

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

// The line structure type definition.
typedef struct _tLine
{
   int n;
   char *p;
   struct _tLine *next;
} tLine;

extern "C"
{
// General terminal functions.
void term_putbox(unsigned x0, unsigned y0, unsigned xm, unsigned ym)
{
   int x,y;

   // Draw top horizontal line.
   term_gotoxy(x0+1,y0);
   for (x=x0+1; x < xm; x++)
   {
      term_putch('-');
   }
   // Draw left and right vertical sides.
   for (y=y0+1; y < ym; y++)
   {
      term_gotoxy(x0,y);
      term_putch('|');
      term_gotoxy(xm,y);
      term_putch('|');
   }
   // Draw bottom horizontal line.
   term_gotoxy(x0+1,ym);
   for (x=x0+1; x < xm; x++)
   {
      term_putch('-');
   }
   // Draw the box vertices (corners).
   term_gotoxy(x0,y0);
   term_putch('+');
   term_gotoxy(xm,y0);
   term_putch('+');
   term_gotoxy(x0,ym);
   term_putch('+');
   term_gotoxy(xm,ym);
   term_putch('+');
}

void term_putspace(int n)
{
   unsigned x0 = term_get_cx();
   unsigned y0 = term_get_cy();

   for (int i=0; i < n; i++)
   {
      term_putch(' ');
   }
   term_gotoxy(x0, y0);
}

// Editor specific Terminal functions.
unsigned x_lft = (1+1);
unsigned y_top = (1+1);
unsigned x_rgt = (80-1);
unsigned y_bot = (24-1);

void term_setbox(unsigned x0, unsigned y0, unsigned x1, unsigned y1)
{
   x_lft = x0 + 1;
   y_top = y0 + 1;
   x_rgt = x1 - 1;
   y_bot = y1 - 1;
   term_putbox(x0, y0, x1, y1);
}

void term_putprompt(char *p)
{
   int k = strlen(p);
   unsigned x0 = x_rgt/2;
   term_gotoxy(x0, y_bot);
   term_putstr(p, k);
   term_putspace(x_rgt-x0-k);
   term_gotoxy(x0+k, y_bot);
}

void term_puttitle(char *p)
{
   unsigned x0 = x_lft+30;
   term_gotoxy(x0, y_top-1);
   for (unsigned x=x0; x <= x_rgt; x++)
   {
      term_putch('-');
   }
   term_gotoxy(x0, 0);
   term_putstr(" ",1);
   term_putstr(p, strlen(p));
   term_putstr(" ",1);
}

void term_putline(char *p, int i)
{
   unsigned y = y_top + i;
   term_gotoxy(x_lft, y);
   term_clrline(0);
   term_putstr(p, strlen(p));
   term_gotoxy(x_rgt+1, y);
   term_putstr("|", 1);
   term_gotoxy(x_lft, y);
}

void term_putstatus1(int n, int pos)
{
   char str[14+1];
   if (n > 0)
   {
       sprintf(str,"Ln-%03d, Col %02d", n, pos);
       term_gotoxy(x_lft, y_bot);
       term_putstr(str,14);
   }
   else
   {
       sprintf(str,"%02d", pos);
       term_gotoxy(x_lft+12, y_bot);
       term_putstr(str,2);
   }
}

void term_putstatus2(int m)
{
   char str[4+1];
   sprintf(str,"%03d", m);
   term_gotoxy(x_lft+15, y_bot);
   term_putstr(str,4);
}

// Terminal keycodes
const char *term_kctostr[]
{
  "UP",
  "DOWN",
  "LEFT",
  "RIGHT",
  "HOME",
  "END",
  "PAGEUP",
  "PAGEDOWN",
  "ENTER",
  "TAB",
  "BACKSPACE",
  "ESC",
  "CTL_A",
  "CTL_B",
  "CTL_C",
  "CTL_D",
  "CTL_E",
  "CTL_F",
  "CTL_G",
  "CTL_K",
  "CTL_L",
  "CTL_N",
  "CTL_O",
  "CTL_P",
  "CTL_Q",
  "CTL_R",
  "CTL_S",
  "CTL_T",
  "CTL_U",
  "CTL_V",
  "CTL_W",
  "CTL_X",
  "CTL_Y",
  "CTL_Z",
  "DEL",
  "UNKNOWN"
};

void term_putstatus3(int c)
{
   char str[9+1];
   unsigned x0 = term_get_cx();
   unsigned y0 = term_get_cy();

   term_gotoxy(x_lft+18, y_bot);

   if (c >= KC_UP && c <= KC_UNKNOWN)
   {
      snprintf(str,sizeof(str),"%9s", term_kctostr[c&0xFF]);
      term_putstr(str,9);
   }
   else
   {
      term_putstr("      [",7);
      term_putch(c);
      term_putch(']');
   }
   term_gotoxy(x0, y0);
}

void term_putstatus4(int sflag)
{
   char str[2+1];
   unsigned x0 = term_get_cx();
   unsigned y0 = term_get_cy();

   term_gotoxy(x_lft+18+9, y_bot);

   term_putstr((sflag)?" *":"  ",2);
   term_gotoxy(x0, y0);
}

void term_clearlines(unsigned x, int k)
{
   unsigned x0 = term_get_cx();
   unsigned y0 = term_get_cy();

   for (int i=0; i < k; i++)
   {
      term_gotoxy(0, x+i);
      term_clrline(0);
   }

   term_gotoxy(x0, y0);
}

void term_dumplines(tLine *head)
{
   // Renumber the lines.
   tLine *lines = head;
   int i = 0;
   while (lines->next != NULL)
   {
      lines = lines->next;
      term_printfxy(0,29+i,"%03d: '%s'", lines->n, lines->p);
      i++;
   }
}
}

class TED_Thread : public PTS_Thread
{
public:
   TED_Thread();
   ~TED_Thread();

   virtual int Init();
   virtual bool Run();

   int setup(unsigned x0, unsigned y0, unsigned x1, unsigned y1, char *filename, char flag);
   int setup(unsigned x0, unsigned y0, char *filename, char flag);

   // Editor termination.
   virtual int Enter(void) { return EDIT_ENTER; }
   virtual int Up(void)    { return EDIT_UP; }
   virtual int Down(void)  { return EDIT_DOWN; }
   virtual int Ctl_Z(void) { return EDIT_CTL_Z; }
   virtual int Ctl_X(void) { return EDIT_CTL_X; }
   virtual int Ctl_C(void) { return EDIT_CTL_C; }
   virtual int Ctl_E(void) { return EDIT_CTL_E; }

   // Operations on the list of lines.
   void initlines(void);
   int loadlines(char *filename);
   int savelines(char *filename);
   int showlines(int n);
   void freelines(void);
   // Editor for the list of lines.
   int edit(void);
   // Helper functions for working with the list of lines.
   tLine *makeline(tLine *lines, char *buf);
   tLine *findline(int n, tLine **prevline);
   void saveline(int n, char *buf);
   bool splitline(int n, char *buf, int pos);
   void joinline(int n, char *buf);
   void loadline(int n);

   // Editor for working with a buffer (ie. prompts such as filename).
   int editbuf(char *buf);

   // Refresh the current active line display.
   void refresh(void);

   void setpos(size_t x) { pos = x; }
   const char *getprompt(void) { return prompt; }
   void setprompt(const char *p) { if (p != NULL) { prompt = p; plen = strlen(p); } }
#if 0
   uint8_t msgid;

   void setmsgid(uint8_t id) { msgid = id; }
   bool sendcmd(uint8_t id);
#endif
   int n_cur;     // current line (first line is 1)
   tLine head;    // the head of the list of lines.
   size_t nlines; // total number of lines

   bool active;
   uint8_t id;

   unsigned x_cur;
   unsigned y_cur;

   size_t cmdlen;
   size_t pos;
   size_t len;
   size_t cols;

   const char *prompt;
   size_t plen;
   int text_field;

   char dflag;      // debug output enable
   char eflag;      // save filename prompt needed
   char sflag;      // save buffer needed

   int status;
   char cmd[EDIT_MAX_LINE + 1];
};

TED_Thread *ted_pt;

TED_Thread::TED_Thread()
{
   Name("TED ");
   setprompt("");
   status = EDIT_IDLE;
   cmd[0] = '\0';
   cols = term_get_cols();
   /* Make sure there is always space for the nul terminator */
   cmdlen = EDIT_MAX_LINE - 1;
   x_cur = 0;
   y_cur = 0;
   pos = 0;          // the current cursor position
   text_field = 0;
   dflag = 0;        // debug not enabled
   initlines();
}

TED_Thread::~TED_Thread()
{  
   term_gotoxy(x_lft, y_top);
   term_clrscr();
   // Free up allocated lines in list.
   freelines();
}

int TED_Thread::Init(void)
{
   TRACE("TED: Init\n");
   return 0;
}

int TED_Thread::setup(unsigned x0, unsigned y0, unsigned x1, unsigned y1, char *fname, char flag)
{
   term_clrscr();
   term_setbox(1, 1, 80, 24);
   term_gotoxy(x_lft, y_top);
   x_cur = term_get_cx();
   y_cur = term_get_cy();

   dflag = flag;

   int rc = loadlines(fname);
   if (rc == 0)
      term_puttitle(fname);
   else
      term_puttitle(" New file ");

   active = true;
   return rc;
}

int TED_Thread::setup(unsigned x0, unsigned y0, char *buffer, char flag)
{
   x_cur = term_get_cx();
   y_cur = term_get_cy();
   term_gotoxy(x0, y0);
   term_clrline(0);

   dflag = flag;

   active = true;
   return 0;
}

// Process a text line from the user.
bool TED_Thread::Run()
{
   PTS_BEGIN();
   PTS_WAIT_UNTIL(active);
   PTS_WAIT_WHILE(status!=EDIT_IDLE);

   len = strlen(cmd);

   refresh();

   while (1)
   {
      int c;
      PTS_WAIT_UNTIL((c = term_getch(TERM_INPUT_DONT_WAIT)) != -1);
      term_putstatus3(c);

      // Look for termination command.
      status = EDIT_IDLE;
      if (c == KC_ENTER)         status = Enter();
      else if (c == KC_UP)       status = Up();
      else if (c == KC_DOWN)     status = Down();
      else if (c == KC_CTL_Z)    status = Ctl_Z();
      else if (c == KC_CTL_C)    status = Ctl_C();      
      else if (c == KC_CTL_X)    status = Ctl_X();
      else if (c == KC_CTL_E)    status = Ctl_E();
      {
      }
      if (status != EDIT_IDLE)
      {
         break;
      }

      // Handle any line edit commands.
      if  (c == KC_BACKSPACE)
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

   if (len > 0 && cmd[len] != '\0')
   {
      cmd[len] = '\0';
   }
#if 0
   sendcmd(msgid);
#endif
   PTS_END();
}

void TED_Thread::initlines(void)
{   
   // Empty list.
   head.n = 0;
   head.p = NULL;
   head.next = NULL;
}

int TED_Thread::loadlines(char *filename)
{    
   char buf[82];
   FILE *fp;
   int rc = -1;

   if ((fp = fopen(filename,"r")) != NULL)
   {
      initlines();

      // Get head of lines list.
      tLine *lines = &head;

      // Read line of text from the file.
      while (fgets(buf, 80, fp))
      {
         // Remove trailing end of line.
         char *p = strchr(buf,'\n');
         if (p)
         {
            *p = '\0';
         }
         // Create a new line of text.
         lines = makeline(lines, buf);
         if (lines == NULL)
         {
            CON_printf("Out of memory\n");
            break;
         }
      }
      rc = fclose(fp);
   }
   return rc;
}

int TED_Thread::savelines(char *filename)
{
   FILE *fp;
   int rc = -1;

   if ((fp = fopen(filename,"w+")) != NULL)
   {
      tLine *lines = &head;
      CON_printf("\n");
      while (lines->next != NULL)
      {
         tLine *line = lines->next;
         TRACE("%03d: '",line->n);
         if (line->p)
         {
            TRACE("%s", line->p);
         }
         TRACE("'\n");
         fwrite(line->p, sizeof(char), strlen(line->p), fp);
         fputc('\n', fp);
         lines = lines->next;
      }
      sflag = 0;
      term_putprompt("Write successful");
      term_putstatus4(sflag);
      rc = fclose(fp);
   }
   return rc;
}

int TED_Thread::showlines(int n)
{
   // Show line number and cursor position.
   tLine *line = findline(n, NULL);
   if (line)
   {
      int k = strlen(line->p);
      if (pos > k)
      {
         pos = k;
      }
   }
   term_putstatus1(n, pos);

   unsigned x0=x_lft;
   unsigned y0=y_top;
   int j = -1;
   int i = 0;
   int m = (y_bot - y_top);
   tLine *lines = &head;

   while (lines->next != NULL)
   {
      tLine *line = lines->next;
      if (line->p && (line->n >= n))
      {
         if (i < m)
         {
            if (line->n == n)
            {
               x0 = x_lft;
               y0 = y_top + i;
               j = n;
            }
            term_putline(line->p, i);
         }
         i++;
      }
      lines = lines->next;
   }
   if (i < m)
   {
      // Show blank lines if not enough lines for a full display.
      for (; i < m; i++)
      {
         term_putline("", i);
      }
   }
   // Show total number of lines.
   nlines = lines->n;
   term_putstatus2(nlines);

   term_gotoxy(x0, y0);
   return j;
}

void TED_Thread::freelines(void)
{
   // Free up allocated lines in list (starting at the head of the list).
   while (head.next != NULL)
   {
      tLine *line = head.next;
      if (line != NULL)
      {
         head.next = line->next;
         if (line->p)
         {
            delete line->p;
         }
         delete line;
      }
      else
      {
         head.next = NULL;
      }      
   }
}

tLine *TED_Thread::makeline(tLine *lines, char *buf)
{
   tLine *line = new tLine;
   if (line)
   {
      line->n = lines->n + 1;
      line->p = strdup(buf);
      if (line->p)
      {
         line->next = NULL;
         TRACE("%03d: '%s'\n",line->n,line->p);
         // Link in the new line of text.
         lines->next = line;
         lines = lines->next;
         return line;
      }
      delete line;
   }
   return NULL;
}

tLine *TED_Thread::findline(int n, tLine **prevline)
{
   // Locate the specified line number in the list.
   tLine *line = NULL;
   tLine *lines = &head;
   // Stop at the end of the list.
   while (lines->next != NULL)
   {
      line = lines->next;
      if (line->p && (line->n >= n))
      {
         if (line->n == n)
         {
            // A line with a matching number was found.
            break;
         }
      }
      // Note that 'lines' points to the previous line.
      lines = lines->next;
   }
   if (prevline)
   {
      *prevline = lines;
   }
   return line;
}

void TED_Thread::saveline(int n, char *buf)
{
   tLine *line = findline(n, NULL);
   if (line)
   {
      if (line->p)
      {
         if (strcmp(line->p,buf))
         {
            free(line->p);
            line->p = strdup(buf);
            sflag = 1;
         }
      }
   }
}

bool TED_Thread::splitline(int n, char *buf, int pos)
{
   sflag = 1;
   term_putstatus4(sflag);
   if (dflag)
   {
      term_clearlines(25, 20);
      term_printfxy(0,25,"splitline(%03d,'%s',%02d) ", n, buf, pos);
   }
   // Locate the specified line in the list.
   tLine *line = findline(n, NULL);
   // Check that the line was found in the list.
   if (line)
   {
      if (line->p)
      { 
         free(line->p);
      }
      // Split the buffer at the specified position.
      line->p = (char *)malloc(pos + 1);
      line->p[0] = '\0';
      strncat(line->p, buf, pos);
      if (dflag)
      {
         term_printfxy(0,26," line='%s' ", line->p);
      }
      // Remember the pointer to the line next in the list (could be NULL if end of list).
      tLine *temp = line->next;
      if (dflag)
      {
         if (temp)
         {
            term_printfxy(0,27," temp n=%03d ", temp->n);
         }
         else
         {
            term_printfxy(0,27," temp is NULL ");
         }
      }
      // Create a new line with text after the specified position.
      tLine *lines = new tLine;
      int k = strlen(buf) - pos;
      char *p = (char *)malloc(k + 1);
      p[0] = '\0';
      strncat(p, &buf[pos], k);
      lines->p = p;
      lines->n = n + 1;
      if (dflag)
      {
         term_printfxy(0,28," new n=%03d '%s' ", lines->n, lines->p);
      }
      // Link new line into the list.
      line->next = lines;
      lines->next = temp;
      // Renumber the lines.
      lines = &head;
      nlines = 0;
      while (lines->next != NULL)
      {
         lines = lines->next;
         lines->n = nlines + 1;
         if (dflag)
         {
            term_printfxy(0,29+nlines,"%03d: '%s'", lines->n, lines->p);
         }
         nlines++;
      }
      // Update total number of lines.
      term_putstatus2(nlines);
      return true;      
   }
   // Add line at head of lines list.
   if (makeline(&head, buf) != NULL)
   {
      return true;
   }
   CON_printf("Out of memory\n");
   return false;
}

void TED_Thread::joinline(int n, char *buf)
{
   sflag = 1;
   term_putstatus4(sflag);
   if (dflag)
   {
      term_clearlines(25, 20);
      term_printfxy(0,25,"joinline(%03d,'%s') pos=%02d ", n, buf, pos);
   }
   // Locate the specified line in the list.
   tLine *lines;
   tLine *line = findline(n, &lines);
   // Check that the line was found in the list.
   if (line)
   {
      if (line->p)
      {         
         // Check for a previous line.
         if (n > 1 && lines)
         {
            // Remove line from the list.
            lines->n = line->n - 1;
            if (dflag)
            {
               term_printfxy(0,26,"lines n=%03d '%s' ", lines->n, lines->p);
            }
            lines->next = line->next;
            free(line->p);
            free(line);
            // Concatenate the new line buffer to the previous line.
            int k = strlen(lines->p) + strlen(buf);
            char *p = (char *)malloc(k + 1);
            strcpy(p, lines->p);
            free(lines->p);
            strcat(p, buf);
            lines->p = p;
            if (dflag)
            {
               term_printfxy(0,27,"lines n=%03d '%s' ", lines->n, lines->p);
            }
            // Renumber the lines.
            lines = &head;
            nlines = 0;
            while (lines->next != NULL)
            {
               lines = lines->next;
               lines->n = nlines + 1;
               if (dflag)
               {
                  term_printfxy(0,29+nlines,"%03d: '%s'", lines->n, lines->p);
               }
               nlines++;
            }
            // Show total number of lines.
            term_putstatus2(nlines);
         }
      }
   }
}

void TED_Thread::loadline(int n)
{
   // Load the current line of text into the line editor.
   int k;
   tLine *line = findline(n, NULL);
   cmd[0] = '\0';
   if (line)
   {
      if (line->p)
      {
         strcpy(cmd, line->p);
      }
   }

   // Make sure the cursor position is within the line length.
   k = strlen(cmd);
   if (pos > k)
   {
      pos = k;
      term_putstatus1(n, k);
   }
}

int TED_Thread::edit(void)
{
   int c;

   eflag = 1;      // filename prompt when saving
   sflag = 0;      // buffer does not need to be saved yet
   term_putstatus4(sflag);

   // Set current line number to top of the file (ie. line #1).
   n_cur = 1;

   while (1)
   {
      // Display lines from the file starting at the specified line number.
      showlines(n_cur);

      // Load the current line of text into the editor.
      loadline(n_cur);

      // Start the text line editor and 'Wait' for termination.
      Restart();
#if 0
      static uint8_t evt[EDIT_MAX_LINE+1];
      PTS_WaitMsg(ted_pt->id, evt);

      // Get position of editted text into the line buffer.
      int k = evt[0] - 2;
      char *p = (char *)&evt[3];
      *(p+k) = '\0';
      c = -(evt[2]);   // get the editor status
#else
      status = EDIT_IDLE;
      while (status == EDIT_IDLE)
      {
         PTS_Process();
      }
      char *p = cmd;
      c = status;
#endif
      // Process the editor status value.
      if (c == EDIT_CTL_X)         // KC_CTL_X
      {
         saveline(n_cur, p);

         // Quit the editor.
         return 0;
      }
      if (c == EDIT_CTL_C)
      {
         // Don't save edits to the current line.
         continue;    
      }
      if (c == EDIT_CTL_E)         // KC_CTL_E
      {
         saveline(n_cur, p);

         // Set flag to prompt for filename when quitting the editor.
         eflag = 1;
      }
      else if (c == EDIT_ENTER)    // KC_ENTER
      {
         // Insert line at the current cursor position.
         if (splitline(n_cur, p, pos))
         {
            goto down;
         }
         // Got a memory allocation error!
         return EDIT_ERROR;
      }
      else if (c == EDIT_JOIN)     // KC_JOIN
      {
         joinline(n_cur, p);
         goto up;
      }
      else
      {
         saveline(n_cur, p);
         if (c == EDIT_UP)         // KC_UP
         {
            if (dflag)
            {
               term_clearlines(25, 20);
               term_printfxy(0,25,"up   n=%03d pos=%02d ", n_cur, pos);
               term_dumplines(&head);
            }
   up:            
            // Move up a line in the file.
            n_cur -= 1;
            if (n_cur <= 0)
               n_cur = 1;
         }
         else if (c == EDIT_DOWN)  // KC_DOWN
         {
            if (dflag)
            {
               term_clearlines(25, 20);
               term_printfxy(0,25,"down n=%03d pos=%02d ", n_cur, pos);
               term_dumplines(&head);
            }
   down:
            // Move down a line in the file.
            if (n_cur < nlines)
            {
               n_cur += 1;
            }
         }
      }
   }
}

int TED_Thread::editbuf(char *buf)
{
   // Edit the buffer.
   Restart();
   // Save previous cursor position.
   unsigned x = x_cur;
   unsigned y = y_cur;
   // Set current cursor position for buffer editor.
   x_cur = term_get_cx();
   y_cur = term_get_cy();
   strcpy(cmd, buf);
   pos = strlen(buf);   // start edit position at end of buffer
   text_field = 1;      // edit only the cmd[] 
   status = EDIT_IDLE;
   while (status == EDIT_IDLE)
   {
      PTS_Process();
   }
   text_field = 0;      // switch back to allow edit of the list of lines
   // Restore previous cusor position.
   x_cur = x;
   y_cur = y;
   // Check for termination of edit by <Enter> key pressed
   if (status == EDIT_ENTER)    // KC_ENTER
   {
      strcpy(buf, cmd);
      return 0;
   }
   return EDIT_ERROR;
}

#define MAX_SEQ_LEN           16

void TED_Thread::refresh(void)
{  
   if (text_field)
   {
      term_gotoxy(x_cur, y_cur);       // cursor to left edge (of text field)
      term_putspace(strlen(cmd)+1);    // add 1 in case a character was deleted
   }
   else
   {
      /* Update status with current cursor position only. */
      term_putstatus1(0, pos);

      /* Erase the line. */
      term_gotoxy(x_rgt, y_cur);
      term_clrline(1);
      term_gotoxy(1, 0);
      term_putch('|');
      term_gotoxy(x_cur, y_cur);

      /* Write the prompt */
      term_putstr(prompt, plen);
   }
   /* Write the current buffer content */
   term_putstr(cmd, len);
   term_gotoxy(x_cur+pos, y_cur);   // restore cursor position
}

#if 0
bool TED_Thread::sendcmd(uint8_t id)
{
   TRACE("sendcmd(%02d) cmd='%s'\n", id, cmd);
   uint8_t evt[1+1+1+EDIT_MAX_LINE+1];
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
#endif

extern "C"
{

int Cmd_ted(int argc,char *argv[])
{
   char filename[40];
   char dflag = 0;
   char bflag = 0;
   filename[0] ='\0';
   for (int i=1; (argc-i) > 0; i++)
   {
      if (!strcmp("-d", argv[i]))
      {
         dflag = 1;
      }
     else if (!strcmp("-b", argv[i]))
     {
        bflag = 1;
     }
     else
      {
         strncat(filename, argv[i], 40);
      }
   }

#if HAS_CLI == 1
   // Suspend CLI processing.
   CLI_SetEnable(false);
   PTS_GetTask(CLI_GetId())->pt->Suspend();
#endif
   ted_pt = new TED_Thread();
   ted_pt->id = PTS_AddTask(ted_pt, 0x01, 32);
   if (bflag)
   {
      unsigned x0 = term_get_cx();
      ted_pt->setup(1, 0, filename, dflag);
      if (ted_pt->editbuf(filename) == 0)
      {
      }
      goto quit;
   }

   ted_pt->setup(1, 1, 80, 24, filename, dflag);
#if 0
   ted_pt->setmsgid(ted_pt->id);
#endif

   while (ted_pt->active)
   {
      int c;

      if (ted_pt->edit() != 0)
      {
         //TODO: Edit error occurred!
        ted_pt->active = false;
        continue;
      }

      if (ted_pt->sflag || ted_pt->eflag)
      {
         if (ted_pt->sflag)
         {
            term_putprompt("Buffer is not saved. Exit [ynw]?");
         }
         else
         {
            term_putprompt("Buffer is unchanged. Exit [ynw]?");
         }
         c = term_getch(TERM_INPUT_WAIT);
         term_putstatus3(c);
         c = toupper(c);
         if (c == 'W')
         {
            if (ted_pt->eflag)
            {
               // Prompt for changing the filename.
               term_putprompt("Enter filename: ");
               if (ted_pt->editbuf(filename) != 0)
               {
                  continue;
               }
               term_puttitle(filename);
               ted_pt->eflag = 0;
            }
            if (ted_pt->savelines(filename) == 0)
            {
               //TODO: Save error occurred!
            }
         }
      }
      else
      {
         term_putprompt("Exit [yn]?");
         c = term_getch(TERM_INPUT_WAIT);
         term_putstatus3(c);
         c = toupper(c);
      }
      if (c == 'Y')
      {
         ted_pt->active = false;
      }
   }

quit:
   term_gotoxy(x_lft, y_top);
   term_clrscr();

   ted_pt->Stop();
   PTS_DeleteTask(ted_pt->id);

#if HAS_CLI == 1
   // Resume CLI processing.
   PTS_GetTask(CLI_GetId())->pt->Resume();
   CLI_SetEnable(true);
#endif
   return 0;
}

}
