#include "../interpreter.h"
#include "../picoc.h"

//#include <stdio.h>

#define HAS_LINENOISE

#ifdef HAS_LINENOISE
#include "../../term/cli.h"
#include "../../term/linenoise.h"

#define PIC_PROMPT			""
#define PIC_HISTORY_FNAME	"pic.his"
#define PIC_HISTORY_SIZE	16
#endif

static Picoc *debug_pc = NULL;

void PlatformInitEx(Picoc *pc)
{
    debug_pc = pc;
	linenoise_setup(PIC_PROMPT, PIC_HISTORY_FNAME, PIC_HISTORY_SIZE);
}

void PlatformCleanupEx(Picoc *pc)
{
	linenoise_cleanup();
}

void PlatformSleepEx(void)
{
}

#ifdef CONSOLE_PORT
	RingBuffer_t ConsoleTxBuffer;
	RingBuffer_t ConsoleRxBuffer;
#endif

void PlatformInitEx(Picoc *pc);
void PlatformCleanupEx(Picoc *pc);
void PlatformSleepEx(void);

//void fputcEx(int ch);


#include "../interpreter.h"
#include "../picoc.h"

//void PlatformLibrarySetupEx(Picoc *pc);
//void PlatformLibraryInitEx(Picoc *pc);

/* Platform depending functions */

void Cdelay(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	BSP_msec_delay(Param[0]->Val->UnsignedLongInteger);
}

void Cheartbeat(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
	BSP_enable_heartbeat_led(Param[0]->Val->Integer);
}

void Cdebug(struct ParseState *Parser, struct Value *ReturnValue, struct Value **Param, int NumArgs)
{
    if (Param[0]->Val->Integer > 0)
        debug_pc->DebugManualBreak = 1;
    else if (Param[0]->Val->Integer == 0)
        debug_pc->DebugManualBreak = 0;
}

/*
 *
 *
 */
struct LibraryFunction PlatformLibrary[] =
{
    { Cdelay,		"void delay_ms(unsigned long);" },
    { Cheartbeat,	"void heartbeat(int);" },
    { Cdebug,       "void debug(int);" },
    { NULL,         NULL }
};

/*
 *
 *
 */
void PlatformLibrarySetup(Picoc *pc)
{
	//PlatformLibrarySetupEx(pc);
}

/*
 *
 *
 */
void PlatformLibraryInit(Picoc *pc)
{
   IncludeRegister(pc, "stk.h", &PlatformLibrarySetup, &PlatformLibrary[0], NULL);
	//PlatformLibraryInitEx(pc);
}


void PlatformInit(Picoc *pc)
{
#ifdef CONSOLE_PORT
	memset(&ConsoleTxBuffer, '\0', sizeof(ConsoleTxBuffer));
	memset(&ConsoleRxBuffer, '\0', sizeof(ConsoleRxBuffer));
#endif
	PlatformInitEx(pc);
}


void PlatformCleanup(Picoc *pc)
{
	PlatformCleanupEx(pc);
}

void PlatformSleep(void)
{
	PlatformSleepEx();
}

#if 0
int fputc(int ch, FIO_FILE *f)
{
#ifdef CONSOLE_PORT
	uint16_t next;

	if (ch == '\n')
		fputc('\r', f);

	next = (ConsoleTxBuffer.idx_in + 1) & (CONSOLE_BUFFER_SIZE - 1);
	while (next == ConsoleTxBuffer.idx_out)
	{
		__NOP();
	}

	ConsoleTxBuffer.data[ConsoleTxBuffer.idx_in] = ch;
	ConsoleTxBuffer.idx_in = next;

#endif
	fputcEx(ch);
	return (ch);
}
#endif

#ifdef HAS_LINENOISE

/* get a line of interactive input */
char *PlatformGetLine(char *Buf, int MaxLen, const char *Prompt)
{
   /* use GNU readline to read the line */
   int rc = linenoise_getline(Buf,MaxLen,Prompt);
	if (rc == -1)
		return NULL;

	if (Buf[0] != '\0')
   {
		linenoise_addhistory(Buf);
	}
	return Buf;   
}
#else

int putch(int ch)
{
   return putchar(ch);
}

char getch(void)
{
#ifdef CONSOLE_PORT
	char ch;
	int next;
	while (ConsoleRxBuffer.idx_in == ConsoleRxBuffer.idx_out)
		PlatformSleep();
	ch = ConsoleRxBuffer.data[ConsoleRxBuffer.idx_out];
	next = (ConsoleRxBuffer.idx_out + 1) & (CONSOLE_BUFFER_SIZE - 1);
	ConsoleRxBuffer.idx_out = next;
	return ch;
#else
	while (!kbhit())
	{
		PlatformSleep();
	}
	return getchar();
#endif
}

/* get a line of interactive input */
char *PlatformGetLine(char *line, int length, const char *prompt)
{
   int ix = 0;
   char *cp = line;
	char ch;
	char lastch = 0;

   printf("%s", prompt);
	
	length -= 2;

   while (1)
	{
      ch = getch();
		if (ch == 0x08)
		{	// Backspace pressed
			if (ix > 0)
			{
				putch(ch);
				putch(' ');
				putch(ch);
				--ix;
				--cp;
			}
		}
      else 
		{
			if (ch == 0x04)
			{	// Ctrl-D - exit
				printf(INTERACTIVE_PROMPT_LEAVE);
         	break;
      	}

			if (ix < length)
			{
				if (ch == '\r' || ch == '\n')
				{
					if (ch == '\r');
					{
						*cp++ = '\n';  // if newline, send newline character followed by null
						*cp = 0;
						putch('\n');
						return line;
					}
				}
				else
				{
					*cp++ = ch;
					ix++;
					putch(ch);
				}
			}
			else
			{
				printf("Line too long\n");
				printf("%s", prompt);
				ix = 0;
				cp = line;
				ch = 0;
			}
		}
		lastch = ch;
   }
   return NULL;
}

/* write a character to the console */
void PlatformPutc(unsigned char OutCh, union OutputStreamInfo *Stream)
{
   if (OutCh == '\n')
      putch('\r');
   putch(OutCh);
}

/* read a character */
int PlatformGetCharacter()
{
	return getch();
}
#endif

#ifndef WIN32
#include "fio.h"
#else
#include <stdio.h>

long f_size(FILE *fp)
{
	long int len;
	fpos_t pos;
	fgetpos(fp, &pos);
	fseek(fp, 0, SEEK_END);	//TODO: Check for errors!
	len = ftell(fp) + 1;	//TODO: Check for errors!
	fsetpos(fp, &pos);
	return (long)len;
}

#endif

static char *ReadText = NULL;

/* read a file into memory */

void PicocPlatformScanFile(Picoc *pc, const char *FileName)
{
   FILE *fp;
   int BytesRead;
   char *ReadText;

	fp = fopen(FileName, "r");
	if (fp == NULL)
	{
	   ProgramFailNoParser(pc, "can't read file %s\n", FileName);
	}

	size_t size = f_size(fp);
	ReadText = HeapAllocMem(pc, size + 1);
	if (ReadText == NULL)
	{
	   fclose(fp);
	   ProgramFailNoParser(pc, "out of memory\n");
	}
	BytesRead = fread(ReadText, 1, size, fp);
	ReadText[BytesRead] = '\0';
   fclose(fp);

	PicocParse(pc, FileName, ReadText, strlen(ReadText), true, true, true, true);
}

/* exit the program */
void PlatformExit(Picoc *pc, int RetVal)
{
   pc->PicocExitValue = RetVal;
	//pc->PicocExitBuf[40] = 1;
   longjmp(pc->PicocExitBuf, 1);
}

