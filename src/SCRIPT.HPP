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

enum SCRIPT_TYPE
{
	SCRIPT_DATA = 0,		// data
	SCRIPT_TXCHAR,			// tx
	SCRIPT_TXFILE,			// txfile
	SCRIPT_TXVAR,			// txvar
	SCRIPT_RXCHAR,			// rx
	SCRIPT_SKIP,			// skip
	SCRIPT_DELAY,			// delay
	SCRIPT_TIMEOUT,			// timeout
	SCRIPT_EXIT,			// exit
	SCRIPT_LABEL,			// label
	SCRIPT_VAR,				// var
	SCRIPT_GOTO,			// goto
	SCRIPT_TEST,			// test
	SCRIPT_BUF,				// buf
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
   int iNum;				// line number (only needed for reporting)
   int iLen;				// not include itself and Rec Type
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
	SCRIPT_REC Rec[SCRIPT_MAX_REC];
	int RecCount;			// the total number of records parsed
	int	RecIndex;			// the current record index being parsed
	FILE *fp;				// script file handle pointer
	char linebuf[4096];		// line buffer for parsing tokens
	int linenum;			// line number
	char *tp;				// pointer to current token in line buffer
	char *pData;			// pointer to current Data space destination

	bool IsDataSpace(int cnt) { return (Data->Size - (Data->Ptr - Data->Space) - cnt) >= 0; }
	int FileLineNum(void)	{ return linenum; }
	int FileOpen(char *filename) {	return ((fp = fopen(filename, "r")) == NULL) ? 0 : 1; }
	void FileClose() { fclose(fp); }
	char *FileGetLine();

	int ReadRecFile(char *filename, SCRIPT_REC *rec, int MaxRecCnt );
	void ReadError(char *s);
	void Display(char *s);

	bool NextInt(int *cnt, char **pp);
	bool NextVar(int *cnt, char **pp);

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
	int SetVar(int iNum, int val, int i=0);
	int GetRecIndex(void) { return RecIndex; }
	int	GetRecCount(void) { return RecCount; }
	SCRIPT_REC *GetVarRecPtr(int iNum);
};

#endif //#ifndef _SCRIPT_HPP 