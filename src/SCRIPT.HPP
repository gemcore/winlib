/*0===========================================================================
**
** File:       Script.hpp
**
** Purpose:    Simple Script Support
**
**
**
**
** Date:       Nov. 15, 1999
**
** Author:     Yi He
**
**
** Rev  Date      Id    Description
** ---  --------- ---   -----------
** 1.00 Nov-15-99 YH    Created.
**============================================================================
*/

#ifndef _SCRIPT_HPP
#define _SCRIPT_HPP
#include <windows.h>
#include <stdio.h>

#define SCRIPT_MAX_REC		256
#define SCRIPT_MAX_CALLS	8

enum SCRIPT_TYPE
{
	/* Reserved words. */
	SCRIPT_DATA = 0,		// 00 data
	SCRIPT_TXCHAR,			// 01 tx
	SCRIPT_TXFILE,			// 02 txfile
	SCRIPT_TXBUF,			// 03 txbuf
	SCRIPT_RXCHAR,			// 04 rx
	SCRIPT_SKIP,			// 05 skip
	SCRIPT_DELAY,			// 06 delay
	SCRIPT_TIMEOUT,			// 07 timeout
	SCRIPT_EXIT,			// 08 exit
	SCRIPT_VAR,				// 09 var
	SCRIPT_GOTO,			// 10 goto
	SCRIPT_TEST,			// 11 test
	SCRIPT_BUF,				// 12 buf
	SCRIPT_CALL,			// 13 call
	SCRIPT_RET, 			// 14 call

	/* Special tokens. */
	SCRIPT_LABEL,			// 15 label
};

typedef struct
{
	char *szKey;			// key word
} SCRIPT_GRAMMER;

typedef struct
{   
   SCRIPT_TYPE iType;		// type
   char bFmt[2];			// data format info (eg. for 'var' type 0=int, 1=byte)
   int iNum;				// line number (only needed for reporting)
   int iLen;				// length of record data
   int iNext;				// next record if > 0
   void *pData;				// pointer to this record's data
} SCRIPT_REC;

#define SCRIPT_MAX_CHAR		256

typedef union 
{
	char cArray[SCRIPT_MAX_CHAR];
	BYTE bArray[0];
	int iArray[0];
	int iValue;
} SCRIPT_RECDATA;

typedef void (* SCRIPT_CALLBACK)(char *szDisplay);

struct DataSpace
{
	int Size;
	char *Space;
	char *Ptr;
	DataSpace(int size);
	~DataSpace();
};

class SCRIPT 
{
private:
	SCRIPT_REC Rec[SCRIPT_MAX_REC];
	int RecCount;			// the total number of records parsed
	int	RecIndex;			// the current record index being parsed/run
	int RecCalls;			// the running call count (0=none)
	int RecReturn[SCRIPT_MAX_CALLS];	// the running call/return record FiFo
	FILE *fp;				// script file handle pointer
	char linebuf[4096];		// line buffer for parsing tokens
	int linenum;			// line number
	int lasterr;			// last scan error
	char *tp;				// pointer to current token in line buffer
	char *pData;			// pointer to current Data space destination

	bool IsDataSpace(int cnt) { return (Data->Size - (Data->Ptr - Data->Space) - cnt) >= 0; }
	int FileLineNum(void) { return linenum; }
	int FileOpen(char *filename) {	return ((fp = fopen(filename, "r")) == NULL) ? 0 : 1; }
	void FileClose() { fclose(fp); }
	char *FileGetLine();
	int ReadRecFile(char *filename, SCRIPT_REC *rec, int MaxRecCnt );
	void ReadError(char *s);
	void Display(char *s);
	bool AllocData(int *cnt, int size);
	bool ScanChar(int *cnt, char **pp);
	bool ScanInt(int *cnt, char **pp);
	bool ScanVar(int *cnt, char **pp);
	bool ScanStr(int *cnt, char **pp);

public:
	bool dflag;				// flag to call Dump() after parsing results
	struct DataSpace *Data;	// allocated Data space for each record's specific data

	SCRIPT(bool dump=true);
	~SCRIPT();

	void Dump(void);
	int Load(char *szFileName);
	int Loadbin(char *szFilename);
	int Savebin(char *szFilename);

	SCRIPT_REC *GetRec(int iNum);
	int GetNextRec(SCRIPT_REC *pRec);
	int FindTypeRecord(int type, int next);
	void SetRecord(int iNum);
	int GetRecord(void);
	int GetVar(int iNum, int *var, int i=0);
	int GetVar(int iNum, byte *var, int i = 0);
	int SetVar(int iNum, int val, int i=0);
	int SetVar(int iNum, byte val, int i = 0);
	int GetRecIndex(void) { return RecIndex; }
	int	GetRecCount(void) { return RecCount; }
	SCRIPT_REC *GetVarRecPtr(int iNum);
	bool PushReturn(void);
	bool PopReturn(void);
};

#endif //#ifndef _SCRIPT_HPP 