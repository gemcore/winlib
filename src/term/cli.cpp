/*
 * cli.cpp
 *
 *  Created on: Oct 26, 2017
 *      Author: Alan
 */

#include <string.h>
#include <errno.h>
#ifndef WIN32
#include "ustdlib.h"
#else
#include <stdio.h>
#include <stdlib.h>
#endif
#include "cli.h"            // Console handling
#if HAS_EVT == 1
#include "evt.h"            // event handling
#endif
#if HAS_NET == 1
#include "net.h"            // Network interface interface (WiFi)
#endif
#if HAS_SPT == 1
#include "script.hpp"       // Script processor
#endif
#if HAS_OBD == 1
#include "ecu/ECU.hpp"      // ECU base class
#endif
#if HAS_MNU == 1
#include "menu.hpp"         // Menu definitions
#endif
#if HAS_PTS == 1
#include "pts.hpp"          // Scheduler
#endif
#if HAS_FFS == 1
#include "fatfs/src/ff.h"
//#include "ffs.h"          // Flash File System
#define PATH_BUF_SIZE       80
#endif
#if HAS_MDM == 1
#include "xmodem/xmodem.h"  // XModem interface
#endif
#if HAS_I2C == 1
#include "crypto.h"         // Crypto authentication interface
#if HAS_CAP
#include "cap.h"            // CAP Touch interface
#endif
#endif
#include "cfg.h"
#include "term.h"
#include "linenoise.h"
#include "platform_conf.h"

extern "C" void TRM_Init();
extern "C" void TRM_Term();

#undef TRACE
//#define TRACE(format,...)  do{DBGprintf(format,##__VA_ARGS__);DBGFlushTx(false);}while(0)   // wait for data to be sent
#define TRACE(...)

//struct pt_sem cli_mutex;

/* FFS definitions. */
extern "C" char g_pcCwd[];

#define NULL    0

//*****************************************************************************
// A structure that holds a mapping between an FRESULT numerical code, and a
// string representation.  FRESULT codes are returned from the FatFs FAT file
// system driver.
//*****************************************************************************
typedef struct
{
    int iFResult;
    const char *pcResultStr;
}
rResultString;

//*****************************************************************************
// A macro to make it easy to add result codes to the table.
//*****************************************************************************
#define RESULT_ENTRY(f)    { (f), (#f) }

//*****************************************************************************
// A table that holds a mapping between the numerical FRESULT code and it's
// name as a string.  This is used for looking up error codes for printing to
// the console.
//*****************************************************************************
const rResultString g_psResultString[] =
{
    // ( ) 000
    RESULT_ENTRY(OK),

#if HAS_FFS == 1
    // (+) 001 - 019
    RESULT_ENTRY(FR_DISK_ERR),
    RESULT_ENTRY(FR_INT_ERR),
    RESULT_ENTRY(FR_NOT_READY),
    RESULT_ENTRY(FR_NO_FILE),
    RESULT_ENTRY(FR_NO_PATH),
    RESULT_ENTRY(FR_INVALID_NAME),
    RESULT_ENTRY(FR_DENIED),
    RESULT_ENTRY(FR_EXIST),
    RESULT_ENTRY(FR_INVALID_OBJECT),
    RESULT_ENTRY(FR_WRITE_PROTECTED),
    RESULT_ENTRY(FR_INVALID_DRIVE),
    RESULT_ENTRY(FR_NOT_ENABLED),
    RESULT_ENTRY(FR_NO_FILESYSTEM),
    RESULT_ENTRY(FR_MKFS_ABORTED),
    RESULT_ENTRY(FR_TIMEOUT),
    RESULT_ENTRY(FR_LOCKED),
    RESULT_ENTRY(FR_NOT_ENOUGH_CORE),
    RESULT_ENTRY(FR_TOO_MANY_OPEN_FILES),
    RESULT_ENTRY(FR_INVALID_PARAMETER),
#endif
#if HAS_SPT == 1
    // (+) 100
    RESULT_ENTRY(SPT_PROCESSING),
#endif
#if HAS_LFS == 1
    // (-) 001 - 099  (currently used by LFS)
    RESULT_ENTRY(ERR_EIO),
    RESULT_ENTRY(ERR_EFAULT),
    RESULT_ENTRY(ERR_ENOENT),
    RESULT_ENTRY(ERR_EEXIST),
    RESULT_ENTRY(ERR_ENOTDIR),
    RESULT_ENTRY(ERR_EISDIR),
    RESULT_ENTRY(ERR_ENOTEMPTY),
    RESULT_ENTRY(ERR_EBADF),
    RESULT_ENTRY(ERR_EFBIG),
    RESULT_ENTRY(ERR_EINVAL),
    RESULT_ENTRY(ERR_ENOSPC),
    RESULT_ENTRY(ERR_ENOMEM),
    RESULT_ENTRY(ERR_ENODATA),
    RESULT_ENTRY(ERR_ENAMETOOLONG),
#endif
/*
    RESULT_ENTRY(ERR_EDOM),
    RESULT_ENTRY(ERR_EFPOS),
    RESULT_ENTRY(ERR_EMLINK),
    RESULT_ENTRY(ERR_ENFILE),
    RESULT_ENTRY(ERR_ENOTTY),
    RESULT_ENTRY(ERR_EPIPE),
    RESULT_ENTRY(ERR_ERANGE),
    RESULT_ENTRY(ERR_EROFS),
    RESULT_ENTRY(ERR_ESPIPE),
    RESULT_ENTRY(ERR_E2BIG),
    RESULT_ENTRY(ERR_EACCES),
    RESULT_ENTRY(ERR_EAGAIN),
    RESULT_ENTRY(ERR_EBUSY),
    RESULT_ENTRY(ERR_ECHILD),
    RESULT_ENTRY(ERR_EINTR),
    RESULT_ENTRY(ERR_EMFILE),
    RESULT_ENTRY(ERR_ENODEV),
    RESULT_ENTRY(ERR_ENOEXEC),
    RESULT_ENTRY(ERR_ENXIO),
    RESULT_ENTRY(ERR_EPERM),
    RESULT_ENTRY(ERR_ESRCH),
    RESULT_ENTRY(ERR_EXDEV),
    RESULT_ENTRY(ERR_EBADMSG),   
    RESULT_ENTRY(ERR_ECANCELED), 
    RESULT_ENTRY(ERR_EDEADLK),
    RESULT_ENTRY(ERR_EILSEQ),
    RESULT_ENTRY(ERR_EINPROGRESS),
    RESULT_ENTRY(ERR_EMSGSIZE),
    RESULT_ENTRY(ERR_ENAMETOOLONG),
    RESULT_ENTRY(ERR_ENOLCK),
    RESULT_ENTRY(ERR_ENOSYS),
    RESULT_ENTRY(ERR_ENOTSUP),
    RESULT_ENTRY(ERR_ETIMEDOUT),
*/
    // (-) 101 - 105
    RESULT_ENTRY(CMDLINE_BAD_CMD),
    RESULT_ENTRY(CMDLINE_TOO_MANY_ARGS),
    RESULT_ENTRY(CMDLINE_TOO_FEW_ARGS),
    RESULT_ENTRY(CMDLINE_INVALID_ARG),
    RESULT_ENTRY(CMDLINE_PATH_TOO_LONG),
    // (-) 106 - 110
    // (-) 111 - 121
    RESULT_ENTRY(CMD_FAILED_EXEC),
    RESULT_ENTRY(CMD_BUSY),
    RESULT_ENTRY(CMD_OUT_OF_MEMORY),
    RESULT_ENTRY(CMD_WRITE_ERROR),
    RESULT_ENTRY(CMD_OPEN_RD_ERROR),
    RESULT_ENTRY(CMD_OPEN_WR_ERROR),
    RESULT_ENTRY(CMD_FILE_NOT_OPEN),
    RESULT_ENTRY(CMD_FILE_ALREADY_OPEN),
    RESULT_ENTRY(CMD_FILE_PROTECTED),
    RESULT_ENTRY(CMD_FAILED_TO_PROTECT),
    RESULT_ENTRY(CMD_VOL_UNPROTECTED),
#if HAS_SPT == 1
    // (-) 201 - 222
    RESULT_ENTRY(SPT_WRONG_ESC_CHARACTER),
    RESULT_ENTRY(SPT_INVALID_HEX_DIGIT),
    RESULT_ENTRY(SPT_INVALID_DATA_DIRECTIVE),
    RESULT_ENTRY(SPT_TOO_MANY_RECORDS),
    RESULT_ENTRY(SPT_EXPECTING_RECORD_TYPE_DATA),
    RESULT_ENTRY(SPT_NO_DATA_SPACE_AVAILABLE),
    RESULT_ENTRY(SPT_NO_MEMORY),
    RESULT_ENTRY(SPT_INVALID_RECORD_TYPE),
    RESULT_ENTRY(SPT_DATA_SPACE_EMPTY),    
    RESULT_ENTRY(SPT_EXIT),
    RESULT_ENTRY(SPT_FILE_NOT_FOUND),
    RESULT_ENTRY(SPT_TYPE_ERROR),
    RESULT_ENTRY(SPT_COMPORT_NA),
    RESULT_ENTRY(SPT_TIMEOUT),
    RESULT_ENTRY(SPT_LABEL_NOT_FOUND),
    RESULT_ENTRY(SPT_VAR_NOT_FOUND),
    RESULT_ENTRY(SPT_BUF_NOT_FOUND),
    RESULT_ENTRY(SPT_VAL_UNDEFINED),
    RESULT_ENTRY(SPT_RET_UNDEFINED),
    RESULT_ENTRY(SPT_CALL_OVERFLOW),
    RESULT_ENTRY(SPT_ABORTED),
    RESULT_ENTRY(SPT_OTHER),
#endif
#if HAS_MDM == 1
    // (-) 301 - 304
    RESULT_ENTRY(XMODEM_ERR_CANCEL),
    RESULT_ENTRY(XMODEM_ERR_SYNC),
    RESULT_ENTRY(XMODEM_ERR_SEND),
    RESULT_ENTRY(XMODEM_ERR_RETRY),
#endif
#if HAS_OBD == 1
    // (-) 401 - 416
    RESULT_ENTRY(ECU_ERROR),
    RESULT_ENTRY(ECU_TYPE_UNSUPPORTED),
    RESULT_ENTRY(ECU_COMMAND_UNSUPPORTED),
    RESULT_ENTRY(ECU_COMMAND_FAILED),
    RESULT_ENTRY(ECU_INIT_FAILED),
    RESULT_ENTRY(ECU_DIAG_SESSION_FAILED),
    RESULT_ENTRY(ECU_AUTH_FAILED),
    RESULT_ENTRY(ECU_DL_REQUEST_FAILED),
    RESULT_ENTRY(ECU_DL_SEND_FAILED),
    RESULT_ENTRY(ECU_START_ROUTINE_FAILED),
    RESULT_ENTRY(ECU_EXIT_TRANSFER_FAILED),
    RESULT_ENTRY(ECU_CANT_OPEN_FILE),
    RESULT_ENTRY(ECU_CANT_READ_FILE),
    RESULT_ENTRY(ECU_CANT_WRITE_FILE),
    RESULT_ENTRY(ECU_MEM_READ_FAILED),
    RESULT_ENTRY(ECU_MEM_WRITE_FAILED),
#endif
#if HAS_I2C == 1
    // (-) 501 - 506
    RESULT_ENTRY(CRYPTO_FAIL),
    RESULT_ENTRY(CRYPTO_AUTH_FAIL),
    RESULT_ENTRY(CRYPTO_HEADER_FAIL),
    RESULT_ENTRY(CRYPTO_SERIAL_NUMBER),
    RESULT_ENTRY(CRYPTO_PAYLOAD_FAIL),
    RESULT_ENTRY(CRYPTO_KEY_LOAD_FAIL),    
#endif
#if HAS_NET == 1
    // (-) 601 - 602
    RESULT_ENTRY(MPY_NO_FILE),
    RESULT_ENTRY(MPY_EXEC_ERR),
#endif
#if HAS_USB == 1
    // (-) 701 - 703
    RESULT_ENTRY(USB_NOT_OPEN),
    RESULT_ENTRY(USB_ALREADY_OPEN),
    RESULT_ENTRY(USB_IN_USE),
#endif
};

//*****************************************************************************
// A macro that holds the number of result codes.
//*****************************************************************************
#define NUM_RESULT_ENTRIES   (sizeof(g_psResultString) / sizeof(rResultString))

//*****************************************************************************
// This function returns a string representation of an error code that was
// returned from a function call to FatFs. It can be used for printing human
// readable error messages.
//*****************************************************************************
const char *StringFromResult(int iFResult)
{
    uint_fast8_t ui8Idx;

    // Enter a loop to search the error code table for a matching error code.
    for (ui8Idx = 0; ui8Idx < NUM_RESULT_ENTRIES; ui8Idx++)
    {
        // If a match is found, then return the string name of the error code.
        if (g_psResultString[ui8Idx].iFResult == iFResult)
        {
            return(g_psResultString[ui8Idx].pcResultStr);
        }
    }
    // At this point no matching code was found, so return a string indicating
    // an unknown error.
    return("UNKNOWN ERROR CODE");
}

class CLI_Thread : public LineNoise_Thread
{
public:
    CLI_Thread();
    ~CLI_Thread();

    virtual int Init();
    virtual void Prompt(void);
    virtual int Exec(void);

    bool prompt_enable;     // display prompt
    int lasterror;          // last command error
    bool getline;
    bool active;            // enable command line processing
    uint8_t id;
};

CLI_Thread::CLI_Thread()
{
    TRACE("CLI::CLI() @%x\n", this);
    Name("CLI ");
    active = false; 
    getline = false; 
    prompt_enable = true;
    lasterror = 0;
}

CLI_Thread::~CLI_Thread()
{
    Name("CLI::~CLI()\n");
    CLI_Term();
}

int CLI_Thread::Init(void)
{
    TRACE("CLI: Init\n");
    CLI_Setup(CLI_PROMPT, CLI_HISTORY_FNAME, CLI_HISTORY_SIZE);
    return 0;
}

#if 0
// Process command lines from the user.
bool CLI_Thread::Run()
{
    PTS_BEGIN();

#if HAS_MNU == 1
    if (MNU_IsActive())
    {
        if (CON_kbhit())
        {
            uint8_t evt[]={2,EVT_BTN,0};
            char c = CON_getc();
            switch(c)
            {
            case '1':
                evt[2] = KEY_LFT;
                EVT_PutMsg(evt);
                break;

            case 0x0d:
            case '2':
                evt[2] = KEY_OK;
                EVT_PutMsg(evt);
                break;

            case '3':        // Key 2 (MenuIncr)
                evt[2] = KEY_INC;
                EVT_PutMsg(evt);
                break;

            case '4':       // Key 3 (MenuDecr)
                evt[2] = KEY_DEC;
                EVT_PutMsg(evt);
                break;

            case 0x1b:
            case '5':
                evt[2] = KEY_ESC;
                EVT_PutMsg(evt);
                break;

            case '6':
                evt[2] = KEY_RGT;
                EVT_PutMsg(evt);
                break;
            }
        }
    }
    else
#endif
    {
        PTS_WAIT_UNTIL(active);

        CLI_Prompt( CLI_GetPrompt() );

        // Do simple command line text editing.
        PTS_SPAWN(*linenoise);

        if (getline)
        {
            sendcmd(id);
            PTS_WAIT_UNTIL(PTS_CntMsg(id)==0);
        }
        else
        {
            if (cmd[0] == '\0')
            {
                PTS_RESTART();
            }
            int nStatus = CLI_ExecLine(cmd);
        }

    }
    PTS_RESTART();  // This is needed to keep the Thread from exiting!

    PTS_END();
}
#endif

void CLI_Thread::Prompt(void)
{
    CLI_Prompt( CLI_GetPrompt() );
    LineNoise_Thread::Prompt();
}

int CLI_Thread::Exec(void)
{
    if (getline)
    {
        sendcmd(id);
    }
    else
    {
        if (cmd[0] != '\0')
        {
            int nStatus = CLI_ExecLine(cmd);
        }
    }
    return -1;
}

extern "C"
{
// Shell alternate ' ' char
#define CLI_ALT_SPACE       '\x07'

static CLI_Thread *cli_pt;

static int argc;
static char *argv[CLI_MAX_ARGS];

int CLI_ProcessLine(char *p)
{
   int i, inside_quotes;
   char quote_char;
   char *temp;
   char *cmd = p;

   if (p[strlen(p) - 1] != '\n')
   {
      strcat(p, "\n");
   }
   // Change '\r', '\n' and '\t' chars to ' ' to ease processing
   while (*p)
   {
      if (*p == '\r' || *p == '\n' || *p == '\t')
      {
         *p = ' ';
      }
      p++;
   }
 
   // Transform ' ' characters inside a '' or "" quoted string in
   // a 'special' char. We do this to let the user execute something
   // like "lua -e 'quoted string'" without disturbing the quoted
   // string in any way.
   for (i = 0, inside_quotes = 0, quote_char = '\0'; i < strlen(cmd); i ++)
   {
      if ((cmd[i] == '\'') || (cmd[i] == '"'))
      {
         if (!inside_quotes)
         {
            inside_quotes = 1;
            quote_char = cmd[i];
         }
         else
         {
            if (cmd[i] == quote_char)
            {
               inside_quotes = 0;
               quote_char = '\0';
            }
         }
      }
      else if ((cmd[i] == ' ') && inside_quotes)
      {
         cmd[i] = CLI_ALT_SPACE;
      }
   }
   if (inside_quotes)
   {
      // Invalid quoted string.
      return CMDLINE_INV_QUOTES;
   }
 
   // Transform consecutive sequences of spaces into a single space
   p = strchr(cmd, ' ');
   while (p)
   {
      temp = p + 1;
      while (*temp && *temp == ' ')
      {
         memmove(temp, temp + 1, strlen(temp));
      }
      p = strchr(p + 1, ' ');
   }
   if (!strcmp(cmd, " "))
   {
      // Empty line (only spaces).
      return 0;
   }
 
   // Skip over the initial space char if it exists
   p = cmd;
   if (*p == ' ')
   {
      p++;
   }
 
   // Add a final space if it does not exist
   if (p[strlen(p) - 1] != ' ')
   {
      strcat(p, " ");
   }
 
   // Compute argc/argv
   for (argc = 0; argc < CLI_MAX_ARGS; argc ++)
   {
      argv[argc] = NULL;
   }
   argc = 0;
   while ((temp = strchr(p, ' ')) != NULL)
   {
      *temp = 0;
      if (argc == CLI_MAX_ARGS)
      {
         return CMDLINE_TOO_MANY_ARGS;
      }
      argv[argc ++] = p;
      p = temp + 1;
   }
 
   // Additional argument processing happens here
   for (i = 0; i < argc; i++)
   {
      p = argv[i];
      // Put back spaces if needed
      for (inside_quotes = 0; inside_quotes < strlen(argv[i]); inside_quotes++)
      {
         if (p[inside_quotes] == CLI_ALT_SPACE)
         {
            argv[i][inside_quotes] = ' ';
         }
      }
      // Remove quotes
      if ((p[0] == '\'' || p [0] == '"') && (p[0] == p[strlen(p) - 1]))
      {
         argv[i] = p + 1;
         p[strlen(p) - 1] = '\0';
      }
   }
   return 0;
}

int CLI_ExecLine(char *p)
{
    // Process command line into (argc,argv) format.
    int nStatus = CLI_ProcessLine(p);
    if (nStatus == 0)
    {
        // Match command with known shell commands.
        int i = 0;
        while (1)
        {
           SHELL_COMMAND* pcmd = &g_psShellCmds[i];
           if (pcmd->cmd == NULL)
           {
              nStatus = CMDLINE_BAD_CMD;
              break;
           }
           if (!strcmp(pcmd->cmd, argv[0]))
           {
              // Execute the shell command.
              if (pcmd->handler_func)
              {
                 nStatus = pcmd->handler_func(argc, argv);
              }
              break;
           }
           i++;
        }
    }
    // Display any errors returned.
    if (CFG_GetCliResult())
    {
        if (nStatus == 0)
        {
            /* Everything ok. */
            if (CFG_GetCliVerbosity() > 0)
            {
                CON_printf("[Ok]\n");
            }
        }
        else
        {
            /* Display any errors returned. */
            CLI_Result(nStatus);
        }
    }
    return nStatus;
}

void CLI_Result(int nStatus)
{
    switch (CFG_GetCliVerbosity())
    {
    case 3:
        CON_printf("CLI:[%d,%s]\n", nStatus, StringFromResult(nStatus));
        break;
    case 2:
        CON_printf("[%d,%s]\n", nStatus, StringFromResult(nStatus));
        break;
    case 1:
        CON_printf("[%d]\n", nStatus);
        break;
    case 0: // Fall through
    default:
        CON_printf("%d\n", nStatus);
        break;
    }
}

int CLI_GetLastError(void)
{
    return cli_pt->lasterror;
}

void CLI_SetEnable(bool state)
{
    cli_pt->active = state;
}

bool CLI_GetEnable(void)
{
    return cli_pt->active;
}

int stream_id[MAX_STREAMS] = {false, false, false};

void CLI_SetStream(int id)
{
    stream_id[id] = true;
}

void CLI_ClrStream(int id)
{
    stream_id[id] = false;
}

bool CLI_IsStream(int id)
{
    return stream_id[id];
}

void CLI_SetPrompt(bool state)
{
    cli_pt->prompt_enable = state;
}

bool CLI_GetPrompt(void)
{
    return cli_pt->prompt_enable;
}

void CLI_Prompt(bool state)
{
    if (state)
    {
#if HAS_FFS == 1
        static char prompt[PATH_BUF_SIZE];
        sprintf(prompt, "%s%s", g_pcCwd, CLI_PROMPT);
#else
        static char *prompt = CLI_PROMPT;
#endif
        cli_pt->setprompt(prompt);
    }
    CLI_SetPrompt(state);
}

int CLI_Init(void)
{
#if HAS_TRM == 1
    TRM_Init();
#endif
    cli_pt = new CLI_Thread;
    cli_pt->id = PTS_AddTask(cli_pt, 0x01, 256);
    cli_pt->active = true;
    return cli_pt->id;
}

void CLI_Term(void)
{
    cli_pt->active = false;
    cli_pt->Stop();
    PTS_DeleteTask(cli_pt->id);
#if HAS_TRM == 1
    TRM_Term();
#endif
}

int CLI_GetId(void)
{
    return cli_pt->id;
}

bool CLI_IsActive(void)
{
    return cli_pt->active;
}

bool save_active = false;
char *save_prompt;
char *save_filename;
int  save_maxlines;

void CLI_Setup(const char *prompt, const char *filename, int max_lengths)
{
    TRACE("CLI_Setup('%s','%s',%d)\n", prompt, filename, max_lengths);
    CON_EchoSet(false);
    if (save_active == false && cli_pt->getprompt() != NULL)
    {
        save_prompt = (char *)cli_pt->getprompt();
        save_filename = strdup(cli_pt->history.filename);
        save_maxlines = cli_pt->history.maxlines;
        save_active = true;
        CLI_SetPrompt(false);
    }
    cli_pt->setprompt(prompt);
    cli_pt->history.setup((char *)filename, max_lengths);
    cli_pt->cmd[0] = '\0';
}

int CLI_AddLine(const char *line)
{
    TRACE("CLI_Addline(%s)\n", line);
    return cli_pt->history.add(line);
}

int CLI_Save(void)
{
    TRACE("CLI_Save()\n");
    return cli_pt->history.save();
}

int CLI_View(void)
{
    TRACE("CLI_View()\n");
    return cli_pt->history.view();
}

void CLI_Cleanup(void)
{
    TRACE("CLI_Cleanup()\n");
    cli_pt->getline = false;
    cli_pt->cleanup();
    if (save_active == true)
    {
        CLI_SetPrompt(true);
        save_active = false;
        cli_pt->setprompt(save_prompt);
        cli_pt->history.setup(save_filename, save_maxlines);
        cli_pt->cmd[0] = '\0';
        free(save_filename);
    }
}

int CLI_GetLine(char* buffer, int maxinput, char *prompt)
{
    int len;
    TRACE("CLI_GetLine()\n");
    cli_pt->getline = true;
    cli_pt->setprompt(prompt);
    cli_pt->cmd[0] = '\0';
    cli_pt->Resume();

    uint8_t evt[80];
    PTS_WaitMsg(cli_pt->id, evt);
    
    len = evt[0];
    TRACE(" Got evt=%02x,%02x,%02x '", evt[0], evt[1], evt[2]);
    len -= 2;
    for (int i=0; i < len; i++)
    {
        TRACE("%c", evt[3+i]);
        buffer[i] = evt[3+i];
    }
    TRACE("'\n");
    buffer[len] = '\0';
    return (evt[2] == 3)? -1 : len; // check for ^Z termination
}

int CLI_ExecBuf(char *buf, int len)
{
    memcpy(cli_pt->cmd, buf, len);
    cli_pt->cmd[len] = '\0';
    return CLI_Exec();
}

int CLI_Exec(void)
{
    // Pass the line from the user to the command processor.  It will be
    // parsed and valid commands executed.
    char *p = cli_pt->cmd;
    int nStatus = CLI_ExecLine(p);
    return nStatus;
}

static void usage(void)
{
    CON_printf("prompt : enable on/off\n");
    CON_printf("echo   : enable on/off\n");
    CON_printf("nl     : enable on/off\n");
    CON_printf("error  : get last error\n");
    CON_printf("exec   : execute a string\n");
    CON_printf("save   : save history\n");
    CON_printf("view   : view history\n");
}

int Cmd_cli(int argc, char *argv[])
{
    if (argc > 1)
    {
        char *p = argv[1];

        if (!strcmp(p,"prompt"))
        {
            if (argc > 2)
            {
                p = argv[2];
                if (!strcmp(p,"on"))
                {
                    CLI_SetPrompt(true);
                }
                else if (!strcmp(p,"off"))
                {
                    CLI_SetPrompt(false);
                }
            }
            CON_printf("prompt=");
            if (CLI_GetPrompt())
            {
                CON_printf("on\n");
            }
            else
            {
                CON_printf("off\n");
            }
        }
        else if (!strcmp(p,"echo"))
        {
            if (argc > 2)
            {
                p = argv[2];
                if (!strcmp(p,"on"))
                {
                    CON_EchoSet(true);
                }
                else if (!strcmp(p,"off"))
                {
                    CON_EchoSet(false);
                }
            }
            CON_printf("echo=");
            if (CON_EchoGet())
            {
                CON_printf("on\n");
            }
            else
            {
                CON_printf("off\n");
            }
            return 0;
        }
        else if (!strcmp(p,"nl"))
        {
            if (argc > 2)
            {
                p = argv[2];
                if (!strcmp(p,"on"))
                {
                    CON_NLSet(true);
                }
                else if (!strcmp(p,"off"))
                {
                    CON_NLSet(false);
                }
            }
            CON_printf("nl=");
            if (CON_NLGet())
            {
                CON_printf("on\n");
            }
            else
            {
                CON_printf("off\n");
            }
            return 0;
        }
        else if (!strcmp(p,"exec"))
        {
            if (argc > 2)
            {
                p = argv[2];
                CLI_ExecBuf(p, strlen(p)+1);
            }
        }
        else if (!strcmp(p,"save"))
        {
            CLI_Save();
        }
        else if (!strcmp(p,"view"))
        {
            CLI_View();
        }
        else if (!strcmp(p,"error"))
        {
           return CLI_GetLastError();
        }
        else if (!strcmp(p,"?"))
        {
            usage();
        }
        else
        {
            return(CMDLINE_INVALID_ARG);
        }
    }
    return(0);
}

}
