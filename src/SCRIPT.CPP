/*0===========================================================================
**
** File:       Script.cpp
**
** Purpose:    Simple scripting support.
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
#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h>  
#include "LOG.H"		// Capture putchar & printf output to a log file

#ifdef _DEBUG
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#else
#define DBG_NEW new
#endif
#include "script.hpp"

#define TRACE	printf

unsigned char *atobytes(char *s, unsigned char *bytes, int n);
char *bytestoa(unsigned char *bytes, char *s, int n);

SCRIPT_GRAMMER sg[]=
{
	"data",		SCRIPT_DATA,
	"tx",		SCRIPT_TXCHAR,
	"txfile",	SCRIPT_TXFILE,
	"txvar",	SCRIPT_TXVAR,
	"rx",		SCRIPT_RXCHAR,
	"skip",		SCRIPT_SKIP,
	"delay",	SCRIPT_DELAY,
	"timeout",	SCRIPT_TIMEOUT,
	"exit",		SCRIPT_EXIT,
	"label",	SCRIPT_LABEL,
	"var",		SCRIPT_VAR,
	"goto",		SCRIPT_GOTO,
	"test",		SCRIPT_TEST
};

/*

var 0; 1, 2,3,4
txvar 0
txvar 0;0
txvar 0;1,2,4
var   0;0,1
txvar 0;1,0
var 2
label 1
test 1;0;2;1
exit 3
test 2;0;2;1
exit 2
exit 1
test 1;0;1
exit 0
test 0;0;0
label 2
goto 1;0;0
exit 4

*/

char *toksep = " :,\t\n";
char *tokval = " ,\t\n";
char *tokeol = " \n";
char *tokptr;

DataSpace::DataSpace(int size)
{ 
	//TRACE("DataSpace() called, ");
	Size = size;						// Size of available space
	Space = DBG_NEW char[Size];			// Start of space
	Ptr = Space;						// First available position
	//TRACE(" Size=%d Space=%08x\n", Size, Space);
}

DataSpace::~DataSpace()
{
	//TRACE("~DataSpace() called, Ptr=%08x\n",Ptr);
	//ADG! delete Space;
}

/*
** Function to display output from parsing the SCRIPT file.
*/
void SCRIPT::Display(char *s)
{
	printf("| ");
	while (*s)
	{
		printf("%c", *s++);
	}
	printf("\n");
}

SCRIPT::SCRIPT()
{
	//TRACE("SCRIPT() called\n");
	Data = NULL;

	for (int i = 0; i < SCRIPT_MAX_RECORDS; i++)
	{
		Msg[i].pData = NULL;
	}
	TotalRecord = 0;
	CurrentRecord = 0;
	linenum = 0;
}

SCRIPT::~SCRIPT(void)
{
	//TRACE("~SCRIPT() called\n");
	if (Data)
	{
		delete Data;
	}
}

int SCRIPT::Load(char *szFileName)
{
	return ReadMsgFile(szFileName, Msg, SCRIPT_MAX_RECORDS);
}

#include "BASEFILE.HPP"

int SCRIPT::Loadbin(char *szFilename)
{
	BASEFILE bf;
	bf.InitReadFile(szFilename);
	if (!bf.IsFile())
	{
		Display("Loadbin File_Not_Found!\n");
		return -1;
	}
	bf.ReadfromFile(sizeof(int), (BYTE *)&TotalRecord);
	TRACE("Loadbin TotalRecord=%d ", TotalRecord);
	int Size = bf.Filesize() - sizeof(int) - (TotalRecord * sizeof(SCRIPT_RECORD));
	TRACE("Size=%d\n", Size);
	Data = DBG_NEW DataSpace(Size);

	for (int i = 0; i < TotalRecord; i++)
	{
		SCRIPT_RECORD rec;
		bf.ReadfromFile(sizeof(SCRIPT_RECORD), (BYTE *)&rec);
		TRACE("Msg[%02d] iType=%-4d iLen=%-4d iNext=%-4d pData=%08x\n", i, Msg[i].iType, Msg[i].iLen, Msg[i].iNext, Msg[i].pData);
		rec.pData = (void *)(Data->Space + (INT)rec.pData);
		memcpy((BYTE *)&Msg[i], (BYTE *)&rec, sizeof(SCRIPT_RECORD));
	}
	bf.ReadfromFile(Data->Size, (BYTE *)Data->Space);
	bf.CloseFile();
	CurrentRecord = 0;
	return 0;
}

int SCRIPT::Savebin(char *szFilename)
{
	BASEFILE bf;
	bf.InitWriteFile(szFilename);
	if (!bf.IsFile())
	{
		Display("Savebin File_Not_Found!\n");
		return -1;
	}
	TRACE("Savebin TotalRecord=%d ", TotalRecord);
	bf.WritetoFile(sizeof(int), (BYTE *)&TotalRecord);
	TRACE("Size=%d\n", Data->Size);
	for (int i = 0; i < TotalRecord; i++)
	{
		SCRIPT_RECORD rec;
		memcpy((BYTE *)&rec, (BYTE *)&Msg[i], sizeof(SCRIPT_RECORD));
		// Convert rec Data pointer to a offset value before writing to file.
		rec.pData = (void *)((BYTE *)Msg[i].pData - (BYTE *)Data->Space);
		TRACE("Msg[%02d] iType=%-4d iLen=%-4d iNext=%-4d pData=%08x\n", i, Msg[i].iType, Msg[i].iLen, Msg[i].iNext, Msg[i].pData);
		bf.WritetoFile(sizeof(SCRIPT_RECORD), (BYTE *)&rec);
	}
	bf.WritetoFile(Data->Size, (BYTE *)Data->Space);
	bf.CloseFile();
	CurrentRecord = 0;
	if (Data)
	{
		delete Data;
	}
	return 0;
}

SCRIPT_RECORD *SCRIPT::GetMsg(int iNum)
{
	if (iNum < TotalRecord)
		return &Msg[iNum];
	else
		return NULL;
}

void SCRIPT::SetRecord(int iNum)
{ 
	CurrentRecord = iNum; 
}

int SCRIPT::GetRecord(void) 
{ 
	return CurrentRecord; 
}

int SCRIPT::GetVar(int iNum, int *val, int i)
{
	if (TotalRecord == 0)
	{
		return FALSE;
	}
	else
	{
		// CurrentRecord range:  0----(TotalRecord-1)
		if (iNum < TotalRecord)
		{
			*val = ((RECDATA *)Msg[iNum].pData)->iArray[i];
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
}

int SCRIPT::SetVar(int iNum, int val, int i)
{
	if (TotalRecord == 0)
	{
		return FALSE;
	}
	else
	{
		// CurrentRecord range:  0----(TotalRecord-1)
		if (iNum < TotalRecord)
		{
			((RECDATA *)Msg[iNum].pData)->iArray[i] = val;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
}

int  SCRIPT::GetNextRecord(SCRIPT_RECORD *pRec)
{
	if ( TotalRecord == 0 )
	{
		return FALSE;
	}
	else
	{
		// CurrentRecord range:  0----(TotalRecord-1)
		if ( CurrentRecord < TotalRecord )
		{
			pRec->iType = Msg[CurrentRecord].iType;
			pRec->iNum  = Msg[CurrentRecord].iNum;
			pRec->iLen  = Msg[CurrentRecord].iLen;
			pRec->pData = Msg[CurrentRecord].pData;
			pRec->iNext = Msg[CurrentRecord].iNext;
			CurrentRecord++;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
}

int  SCRIPT::FindTypeRecord(int type, int next)
{
	SCRIPT_RECORD *pRec;

	// CurrentRecord range:  0----(TotalRecord-1)
	for (int i = 0; i < TotalRecord; i++)
	{
		pRec = &Msg[i];
		if (pRec->iType == type)
		{
			if (pRec->iNext == next)
			{
				// Record found.
				return i;
			}
		}
	}
	return -1;
}

char *SCRIPT::FileGetLine()
{
   int i = 0;
   char *t;

   linebuf[0] = '\0';

concat:
   if ((t = fgets(&linebuf[i],sizeof(linebuf),fp)) != NULL)
   {
      int k = strlen(t);
      while (k > 0 && (*(t+k-1) == '\n' || *(t+k-1) == '\r'))
      {
         k--;
         *(t+k) = '\0'; // remove carriage-returns and newlines from EOL!
      }
      linenum++;
      if (k == 0)
      {
         goto concat;
      }
		if (*(t+k-1) == '\\')
		{
         i += k-1;
         goto concat;
      }
   }
   else
   {
      if (i == 0)
      {
         return NULL;
      }
   }
   return linebuf;
}

// for reading msg from file
int SCRIPT::ReadByte(BYTE *data)
{  
   if ((tokptr = strtok(NULL,toksep)) == NULL)
   {
      return 0;
   }
   if (data)
   {
      atobytes(tokptr,data,1);
   }
   return 1;
}

int SCRIPT::ReadByteArray(BYTE *data,int n)
{  
   int k = 0;
   while (n-- > 0)
   {
      if (ReadByte(data))
      {
         k++;                       // count number of tokens actually read
      }
      else
      {
         *data = 0xFF;
      }
      data++;
   }
   return k;
}

#define MAX_ERR_MSG		256

void SCRIPT::ReadError(char *msg)
{
	char szErrMsg[MAX_ERR_MSG];
	
	if ( strlen(msg) < MAX_ERR_MSG-56 )
		sprintf(szErrMsg, "Syntax Line %03d: '%s'", FileLineNum(), msg);
	else
		strcpy(szErrMsg, "FATAL: Message Overflow");

	Display(szErrMsg);		
}

int SCRIPT::ReadMsgFile(char *filename, SCRIPT_RECORD *msg, int MaxMsgCnt )
{
    char *line;
    char *tp;
    SCRIPT_RECORD *ip=msg;
	char szInfo[512];
	int rc = 0;
	
	linenum = 0;
	
	if ( !FileOpen(filename) )
    {
        sprintf(szInfo, "File %s not found!", filename);
		Display( szInfo );
        return -1;
    }
    sprintf(szInfo, "Reading messages from File %s", filename);
	Display(szInfo);		

	TotalRecord = 0;		// when reading failed, TotalRecord=0
	CurrentRecord = 0;

	while ((line = FileGetLine()) != NULL)
    {
		int cnt;
		sprintf(szInfo, "%03d: %s", linenum, line);
		Display(szInfo);

		if ((tp=strtok(line,toksep)) != NULL)
        {
            //if (*tp==0x1A)
            //    goto eof;

			if (*(tp+0)=='/' && *(tp+1)=='/' )
                continue;	// skip over comments

			if (!strcmp(tp, sg[SCRIPT_TXCHAR].szKey))
			{
				/* Get next token. */
				tp=strtok(NULL,toksep);
				char *pDest = Data->Ptr;
				char *pSrc = tp;
				cnt = 0;
				while (*pSrc)
				{
					if (*pSrc != '\\')
					{
						cnt += 1;
						if (!IsDataSpace(cnt)) goto errmem;
						*pDest++ = *pSrc++;
					}
					else
					{
						pSrc++;
						if (*pSrc!='x' && *pSrc!='X')
						{
							sprintf(szInfo, "Wrong Escape string %s", tp );
							rc = -2;
							goto err;
						}
						else
						{							
							pSrc++;

							char szByte[3];
							szByte[0] = *pSrc++;	// high nibble
							szByte[1] = *pSrc++;	// low nibble
							szByte[2] = 0;

							if (!isxdigit(szByte[0]) || !isxdigit(szByte[1]))
							{
								sprintf(szInfo, "Wrong Escape string %s", tp);
								rc = -3;
								goto err;
							}
							cnt += 1;
							if (!IsDataSpace(cnt)) goto errmem;
							sscanf(szByte, "%02X", pDest);
							pDest++;
						}
					}
				}				
				*pDest=0;
				Msg[CurrentRecord].iType = sg[SCRIPT_TXCHAR].iType;
				Msg[CurrentRecord].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_TXFILE].szKey))
			{
				/* Get next token. */
				tp = strtok(NULL,tokeol);
				cnt = strlen(tp) + 1;
				if (!IsDataSpace(cnt)) goto errmem;
				strcpy(Data->Ptr, tp);
				Msg[CurrentRecord].iType = sg[SCRIPT_TXFILE].iType;
				Msg[CurrentRecord].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_TXVAR].szKey))
			{
				int var = -1;
				char *pDest = Data->Ptr;
				/* Get next token. */
				cnt = 0;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &var);
					TRACE("var %d:", var);
					/* Get next token. */
					while ((tp = strtok(NULL, tokval)) != NULL)
					{
						cnt += sizeof(int);
						if (!IsDataSpace(cnt)) goto errmem;
						sscanf(tp, "%d", pDest);
						TRACE("%s ", tp);
						pDest += sizeof(int);
					}
					TRACE("\n");
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_TXVAR].iType;
				Msg[CurrentRecord].iNext = var;				
			}
			else if (!strcmp(tp, sg[SCRIPT_RXCHAR].szKey))
			{
				int label = -1;
				/* Get next token. */
				cnt = 0;
				if ((tp = strtok(NULL, tokeol)) != NULL)
				{
					char *pDest = Data->Ptr;
					char *pSrc = tp;
					while (*pSrc)
					{
						if (*pSrc != '\\')
						{
							cnt += 1;
							if (!IsDataSpace(cnt)) goto errmem;
							*pDest++ = *pSrc++;
						}
						else
						{
							pSrc++;
							if (*pSrc != 'x' && *pSrc != 'X')
							{
								sprintf(szInfo, "Wrong Escape string %s", tp);
								rc = -4;
								goto err;
							}
							else
							{
								pSrc++;

								char szByte[3];
								szByte[0] = *pSrc++;	// high nibble
								szByte[1] = *pSrc++;    // low nibble
								szByte[2] = 0;

								if (!isxdigit(szByte[0]) || !isxdigit(szByte[1]))
								{
									sprintf(szInfo, "Wrong Escape string %s", tp);
									rc = -5;
									goto err;
								}
								cnt += 1;
								if (!IsDataSpace(cnt)) goto errmem;
								sscanf(szByte, "%02X", pDest);
								pDest++;
							}
						}
					}
					*pDest = 0;
					/* Get next token. */
					if ((tp = strtok(NULL, tokeol)) != NULL)
					{
						sscanf(tp, "%d", &label);
					}
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_RXCHAR].iType;
				Msg[CurrentRecord].iNext = label;
			}
			else if (!strcmp(tp, sg[SCRIPT_SKIP].szKey))
			{
				int label = -1;
				char *pDest = Data->Ptr;
				cnt = 0;
				/* Get next token. */
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					cnt = sizeof(int);
					if (!IsDataSpace(cnt)) goto errmem;
					sscanf(tp, "%d", pDest);
					pDest += sizeof(int);
					/* Get next token. */
					if ((tp = strtok(NULL, toksep)) != NULL)
					{
						cnt += sizeof(int);
						if (!IsDataSpace(cnt)) goto errmem;
						sscanf(tp, "%d", pDest);
						/* Get next token. */
						if ((tp = strtok(NULL, toksep)) != NULL)
						{
							sscanf(tp, "%d", &label);
						}
					}
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_SKIP].iType;
				Msg[CurrentRecord].iNext = label;
			}
			else if (!strcmp(tp, sg[SCRIPT_DELAY].szKey))
			{
				/* Get next token. */
				tp = strtok(NULL,toksep);
				cnt = sizeof(int);
				if (!IsDataSpace(cnt)) goto errmem;
				sscanf(tp, "%d", Data->Ptr);
				Msg[CurrentRecord].iType = sg[SCRIPT_DELAY].iType;
				Msg[CurrentRecord].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_TIMEOUT].szKey))
			{
				cnt = 0;
				/* Get next token. */
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					cnt = sizeof(int);
					if (!IsDataSpace(cnt)) goto errmem;
					sscanf(tp, "%d", Data->Ptr);
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_TIMEOUT].iType;
				Msg[CurrentRecord].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_EXIT].szKey))
			{
				cnt = 0;
				/* Get next token. */
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					cnt = sizeof(int);
					if (!IsDataSpace(cnt)) goto errmem;
					sscanf(tp, "%d", Data->Ptr);
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_EXIT].iType;
				Msg[CurrentRecord].iNext = 0;
			}
			else  if (!strcmp(tp, sg[SCRIPT_LABEL].szKey))
			{
				int label;
				cnt = 0;
				/* Get next token. */
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &label);
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_LABEL].iType;
				Msg[CurrentRecord].iNext = label;
			}
			else  if (!strcmp(tp, sg[SCRIPT_VAR].szKey))
			{
				int var = 0;
				char *pDest = Data->Ptr;

				/* Get next token. */
				cnt = 0;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &var);
					//TRACE("var %d:", var);
					/* Get next token. */
					while ((tp = strtok(NULL, tokval)) != NULL)
					{
						cnt += sizeof(int);
						if (!IsDataSpace(cnt)) goto errmem;
						sscanf(tp, "%d", pDest);
						//TRACE("%s,", tp);
						pDest += sizeof(int);
					}
					//TRACE("\n");
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_VAR].iType;
				Msg[CurrentRecord].iNext = var;
			}
			else  if (!strcmp(tp, sg[SCRIPT_GOTO].szKey))
			{
				int label = 0;
				char *pDest = Data->Ptr;

				/* Get next token. */
				cnt = 0;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &label);
				}
				/* Get next token. */
				if ((tp = strtok(NULL, tokval)) != NULL)
				{
					int i;
					cnt += sizeof(int);
					if (!IsDataSpace(cnt)) goto errmem;
					/* Get next token. */
					sscanf(tp, "%d", &i);
					/* Look ahead, if a '@' character appears then expect a var number followed by an index number. */
					char *p = strchr(tp, '@');
					if (p != NULL)
					{
						/* skip past '@' character. */
						p++;
						if (*p != '\0')
						{
							int j;
							j = atoi(p);
							j <<= 16;
							i = (i&0x0000FFFF) | (j&0xFFFF0000);
						}
					}
					*((int *)pDest) = i;
					pDest += sizeof(int);
					/* Get next token. */
					while ((tp = strtok(NULL, tokval)) != NULL)
					{
						cnt += sizeof(int);
						if (!IsDataSpace(cnt)) goto errmem;
						sscanf(tp, "%d", pDest);
						pDest += sizeof(int);
					}
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_GOTO].iType;
				Msg[CurrentRecord].iNext = label;
			}
			else  if (!strcmp(tp, sg[SCRIPT_TEST].szKey))
			{
				int label = 0;
				char *pDest = Data->Ptr;

				/* Get next token. */
				cnt = 0;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &label);
				}
				/* Get next token. */
				if ((tp = strtok(NULL, tokval)) != NULL)
				{
					int i;
					cnt += sizeof(int);
					if (!IsDataSpace(cnt)) goto errmem;
					/* Get next token. */
					sscanf(tp, "%d", &i);
					/* Look ahead, if a '@' character appears then expect a var number followed by an index number. */
					char *p = strchr(tp, '@');
					if (p != NULL)
					{
						/* skip past '@' character. */
						p++;
						if (*p != '\0')
						{
							int j;
							j = atoi(p);
							j <<= 16;
							i = (i & 0x0000FFFF) | (j & 0xFFFF0000);
						}
					}
					*((int *)pDest) = i;
					pDest += sizeof(int);
					/* Get next token. */
					while ((tp = strtok(NULL, tokval)) != NULL)
					{
						cnt += sizeof(int);
						if (!IsDataSpace(cnt)) goto errmem;
						sscanf(tp, "%d", pDest);
						pDest += sizeof(int);
					}
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_TEST].iType;
				Msg[CurrentRecord].iNext = label;
			}
			else  if (!strcmp(tp, sg[SCRIPT_DATA].szKey))
			{
				/* The 'data' script is a directive and must be located at the beginning. */
				if (CurrentRecord == 0)
				{
					/* Make sure there is only one directive. */
					if (Data == NULL)
					{
						int n = 0;
						/* Get next token. */
						if ((tp = strtok(NULL, toksep)) != NULL)
						{
							sscanf(tp, "%d", &n);
						}
						Data = DBG_NEW DataSpace(n);
						continue;
					}
				}
				sprintf(szInfo, "Invalid data directive");
				rc = -6;
				goto err;
			}
			else
			{
				sprintf(szInfo, "Incorrect Message Type %s", tp);
				rc = -7;
				goto err;
			}
        }
		Msg[CurrentRecord].iNum = linenum;
		Msg[CurrentRecord].iLen = cnt;
		Msg[CurrentRecord].pData = Data->Ptr;
		Data->Ptr += cnt;

		if (++CurrentRecord >= MaxMsgCnt)
        {
			ReadError("Too many messages!");
			rc = -7;
			goto err;
        }
    }

	TotalRecord = CurrentRecord;
	sprintf(szInfo, "Total of %d messages, bytes %d used / %d remaining", TotalRecord, Data->Ptr - Data->Space, Data->Size - (Data->Ptr - Data->Space));
	Display(szInfo);
	FileClose();
	return rc;

errmem:
	sprintf(szInfo, "Out of Data space!");
	rc = -8;
	// fall through...
err:
	ReadError(szInfo);
	FileClose();
	return rc;

}

/*****************************************************************************
* Format functions
*****************************************************************************/
unsigned char gethexnibble(char ch)
{
	// Return hex nibble.
	unsigned char usNib = ch;

	// convert ascii -> hex
	if (usNib >= '0' && usNib <= '9')
		return usNib - '0';
	else if (usNib >= 'a' && usNib <= 'f')
		return usNib - 'a' + 10;
	else
		return 0xff;
}

unsigned char *atobytes(char *s, unsigned char *bytes, int n)
{
	int i;
	unsigned char *bp = bytes;
	for (i = 0; i < 2 * n; i += 2)
	{
		int byte = gethexnibble(s[i + 1]) + (gethexnibble(s[i + 0]) << 4);
		*bp++ = (unsigned char)byte;
	}
	return bytes;
}

char *hextoa(char *s, unsigned char val)
{
	unsigned char _al, _ah;
	if ((_ah = (val >> 4)) > 9)
		_ah += 'a' - 0x0a;
	else
		_ah += '0';
	if ((_al = (val & 0x0F)) > 9)
		_al += 'a' - 0x0a;
	else
		_al += '0';
	*(s + 0) = _ah;
	*(s + 1) = _al;
	*(s + 2) = '\0';
	return s;
}

char *bytestoa(unsigned char *bytes, char *s, int n)
{
	int i;
	unsigned char *bp = bytes;
	s[0] = '\0';
	for (i = 0; i < 2 * n; i += 2)
	{
		s[i] = '\0';
		hextoa(&s[i], (*bp++) & 0xff);
		//sprintf(&s[i],"%02x",(*bp++)&0xff);
	}
	return s;
}
