/*0===========================================================================
**
** File:       Script.hpp
**
** Purpose:    Simple Script parsing class definitions.
**
**
**
**
** Date:       Nov. 15, 1999
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#ifndef _SCRIPT_HPP
#define _SCRIPT_HPP
#include <windows.h>
#include <stdio.h>

#define SCRIPT_MAX_RECORDS	256

enum SCRIPT_TYPE
{
	SCRIPT_TXCHAR = 0,		// tx
	SCRIPT_TXFILE,			// txfile
	SCRIPT_RXCHAR,			// rx
	SCRIPT_SKIP,			// skip
	SCRIPT_DELAY,			// delay
	SCRIPT_TIMEOUT,			// timeout
	SCRIPT_EXIT,			// exit
	SCRIPT_LABEL,			// label
	SCRIPT_TOTAL_TYPES
};

typedef struct
{
	char *szKey;			// key word
	SCRIPT_TYPE iType;		// type
} SCRIPT_GRAMMER;

typedef struct
{   
   SCRIPT_TYPE iType;		// type
   int iLen;				// not include itself and MsgType
   int iNext;				// next record if > 0
   void *pData;				// pointer to data
} SCRIPT_RECORD;

#define SCRIPT_MAX_CHAR		256

typedef union 
{
	char szTxChars[SCRIPT_MAX_CHAR];
	char szFileName[SCRIPT_MAX_CHAR];
	char szWaitChars[SCRIPT_MAX_CHAR];
	int iSkip;
	int iDelay;
	int iTimeout;
} RECDATA;

typedef void (* SCRIPT_CALLBACK)(char *szDisplay);

class SCRIPT 
{
	int TotalRecord;
	int	CurrentRecord;
	SCRIPT_RECORD Msg[SCRIPT_MAX_RECORDS];
	char Data[1024];
	SCRIPT_CALLBACK pfnCallBack;
	FILE *fp;
	char linebuf[4096];		// for sparsing
	int linenum;

	int FileLineNum(void)	{ return linenum; }
	int FileOpen(char *filename) {	return ((fp = fopen(filename, "r")) == NULL) ? 0 : 1; }
	void FileClose() { fclose(fp); }
	char *FileGetLine();

	int ReadMsgFile(char *filename, SCRIPT_RECORD *msg, int MaxMsgCnt );
	int ReadByte(BYTE *data);
	int ReadByteArray(BYTE *data,int n);
	void ReadError(char *msg);
	unsigned char *atobytes(char *s,unsigned char *bytes,int n);
	void Display(char *szDisplay)	{ if( pfnCallBack ) pfnCallBack(szDisplay); }

public:
	SCRIPT(SCRIPT_CALLBACK pfnScr);
	~SCRIPT();

	int Load(char *szFileName);
	int GetNextRecord(SCRIPT_RECORD *pRec);
	int FindLabelRecord(int iNum);
	void SetRecord(int iNum);
	int GetRecord(void);
	int	GetTotalRecord(void) {return TotalRecord; }
	void SetCallBack(SCRIPT_CALLBACK pfnScr) { pfnCallBack = pfnScr; }
};

#endif //#ifndef _SCRIPT_HPP 