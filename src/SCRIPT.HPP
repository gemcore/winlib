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
	SCRIPT_SERIAL,			// 01 serial
	SCRIPT_TXCHAR,			// 02 tx
	SCRIPT_TXFILE,			// 03 txfile
	SCRIPT_TXBUF,			// 04 txbuf
	SCRIPT_RXCHAR,			// 05 rx
	SCRIPT_SKIP,			// 06 skip
	SCRIPT_DELAY,			// 07 delay
	SCRIPT_TIMEOUT,			// 08 timeout
	SCRIPT_EXIT,			// 09 exit
	SCRIPT_VAR,				// 10 var
	SCRIPT_GOTO,			// 11 goto
	SCRIPT_TEST,			// 12 test
	SCRIPT_BUF,				// 13 buf
	SCRIPT_CALL,			// 14 call
	SCRIPT_RET, 			// 15 ret
	SCRIPT_LOG, 			// 16 log
	SCRIPT_ECHO,            // 17 echo

	/* Special tokens. */
	SCRIPT_LABEL,			// 15 label
};

// Eg. 'var'  type 0=int, 1=byte[]

enum FMT_VAR_TYPE
{
	FMT_VAR_INT = 0,
	FMT_VAR_BYTE
};

// Eg. 'goto' default expression is "val1 >= val2"
// Eg. 'test' default expression is "val1 != val2"

enum FMT_EXPR
{
	FMT_EXPR_EQ	= 0,	// "=="
	FMT_EXPR_NOT_EQ,	// "!="
	FMT_EXPR_GR_OR_EQ,	// ">="
	FMT_EXPR_LS_OR_EQ,	// "<="
	FMT_EXPR_GR,		// ">"
	FMT_EXPR_LS			// "<"
};

typedef struct
{
	char *szKey;			// key word
} SCRIPT_GRAMMER;

typedef struct
{   
   SCRIPT_TYPE iType;		// type
   char bFmt;				// data format info
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

	int ReadBinFile(char *filename);
	int SaveBinFile(char *filename);
	bool IsDataSpace(int cnt) { return (Data->Size - (Data->Ptr - Data->Space) - cnt) >= 0; }
	int FileLineNum(void) { return linenum; }
	int FileOpen(char *filename);
	void FileClose(void);
	char *FileGetLine(void);
	int ReadRecFile(char *filename, SCRIPT_REC *rec, int MaxRecCnt );
	void ReadError(char *s);
	void Display(char *s);
	bool AllocData(int *cnt, int size);
	char GetExpr(void);
	bool ScanChar(int *cnt, char **pp);
	bool ScanInt(int *cnt, char **pp);
	bool ScanVar(int *cnt, char **pp);
	bool ScanStr(int *cnt, char **pp);

public:
	bool dflag;				// flag to call Dump() after parsing results
	struct DataSpace *Data;	// allocated Data space for each record's specific data

	SCRIPT();
	virtual ~SCRIPT();

	void Dump(void);
	int Load(char *fileName);
	int Loadbin(char *filename);
	int Savebin(char *filename);

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
	bool PopReturn(int i=0);
	bool EvalExpr(char expr, int var, int val);
	bool EvalExpr(char expr, byte var, byte val);
};

#endif //#ifndef _SCRIPT_HPP 