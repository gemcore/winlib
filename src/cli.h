/*
 * cli.h
 *
 *  Created on: Oct 26, 2017
 *      Author: Alan
 */

#ifndef CLI_H_
#define CLI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <errno.h>
#include "bsp.h"	   // For byte

#define CLI_MAX_ARGS      10
#define CLI_MAXSIZE       128
#define CLI_PROMPT        "> "
#define CLI_HISTORY_FNAME "cli.his"
#define CLI_HISTORY_SIZE  16

//*****************************************************************************
// Defines command stream ids.
//*****************************************************************************
#define MAX_STREAMS         3

#define FFS_STREAM_ID       0
#define NET_STREAM_ID       1
#define MDM_STREAM_ID       2

#define ENODATA             61        /* No data available */

//*****************************************************************************
// Command Line processing error definitions
//*****************************************************************************
#define CMDLINE_BAD_CMD       (-101)
#define CMDLINE_TOO_MANY_ARGS (-102)
#define CMDLINE_TOO_FEW_ARGS  (-103)
#define CMDLINE_INVALID_ARG   (-104)
#define CMDLINE_PATH_TOO_LONG (-105)
#define CMDLINE_INV_QUOTES    (-106)

//*****************************************************************************
// Command error definitions
//*****************************************************************************
#ifndef OK
#define OK                    (0)
#endif
#define CMD_FAILED_EXEC       (-111)
#define CMD_BUSY              (-112)
#define CMD_OUT_OF_MEMORY     (-113)
#define CMD_WRITE_ERROR       (-114)
#define CMD_OPEN_RD_ERROR     (-115)
#define CMD_OPEN_WR_ERROR     (-116)
#define CMD_FILE_NOT_OPEN     (-117)
#define CMD_FILE_ALREADY_OPEN (-118)
#define CMD_FILE_PROTECTED    (-119)
#define CMD_FAILED_TO_PROTECT (-120)
#define CMD_VOL_UNPROTECTED   (-121)

//*****************************************************************************
// Standard error definitions
//*****************************************************************************
#define ERR_EIO               (-EIO)
#define ERR_EFAULT            (-EFAULT)
#define ERR_ENOENT            (-ENOENT)
#define ERR_EEXIST            (-EEXIST)
#define ERR_ENOTDIR           (-ENOTDIR)
#define ERR_EISDIR            (-EISDIR)
#define ERR_ENOTEMPTY         (-ENOTEMPTY)
#define ERR_EBADF             (-EBADF)
#define ERR_EFBIG             (-EFBIG)
#define ERR_EINVAL            (-EINVAL)
#define ERR_ENOSPC            (-ENOSPC)
#define ERR_ENOMEM            (-ENOMEM)
#define ERR_ENODATA           (-ENODATA)
#define ERR_ENAMETOOLONG      (-ENAMETOOLONG)

// Shell command handler function
typedef int (*p_shell_handler)(int argc, char **argv);

// Command/handler pair structure
typedef struct
{
    const char* cmd;
    p_shell_handler handler_func;
   const char* help;
} SHELL_COMMAND;

//*****************************************************************************
//! This is the command table that must be provided by the application.  The
//! last element of the array must be a structure whose 'cmd' field contains
//! a NULL pointer.
//*****************************************************************************
extern SHELL_COMMAND g_psShellCmds[];

//*****************************************************************************
// API functions
//*****************************************************************************
void CLI_SetPrompt(bool state);
bool CLI_GetPrompt(void);
void CLI_Prompt(bool state);
int CLI_Exec(void);
int CLI_ExecBuf(char *buf,int len);
void CLI_SetResult(bool state);
bool CLI_GetResult(void);
bool CLI_GetEnable(void);
void CLI_SetEnable(bool state);
void CLI_SetStream(int id);
bool CLI_IsStream(int id);
void CLI_ClrStream(int id);
void CLI_Result(int nStatus);
int CLI_GetLastError(void);
int CLI_ExecLine(char *p);
int CLI_Init(void);
void CLI_Term(void);
int CLI_ExecBuf(char *buf,int len);
int CLI_Exec(void);
int CLI_GetId(void);
bool CLI_IsActive(void);
void CLI_Setup(const char *prompt, const char *filename, int max_lengths);
int CLI_AddLine(const char *line);
int CLI_Save(void);
int CLI_View(void);
int CLI_GetLine(char* buffer, int maxinput, char *prompt);
void CLI_Cleanup(void);

//*****************************************************************************
// Definition of CLI commands
//*****************************************************************************
int Cmd_cli(int argc, char *argv[]);

#ifdef    __cplusplus
}
#endif // __cplusplus

#endif /* CLI_H_ */
