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
#include <ctype.h>
#include "LOG.H"		// Capture putchar & printf output to a log file

#undef TRACE
#define TRACE

#ifdef _DEBUG
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#else
#define DBG_NEW new
#endif
#include "script.hpp"

unsigned char *atobytes(char *s, unsigned char *bytes, int n);
char *bytestoa(unsigned char *bytes, char *s, int n);
void MEM_Dump(unsigned char *data, int len, long base);

SCRIPT_GRAMMER sg[]=
{
	"data",		// SCRIPT_DATA
	"tx",		// SCRIPT_TXCHAR
	"txfile",	// SCRIPT_TXFILE
	"txbuf",	// SCRIPT_TXBUF
	"rx",		// SCRIPT_RXCHAR
	"skip",		// SCRIPT_SKIP
	"delay",	// SCRIPT_DELAY
	"timeout",	// SCRIPT_TIMEOUT
	"exit",		// SCRIPT_EXIT
	"var",		// SCRIPT_VAR
	"goto",		// SCRIPT_GOTO
	"test",		// SCRIPT_TEST
	"buf"		// SCRIPT_BUF
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
	TRACE("DataSpace() called, ");
	Size = size;						// Size of available space
	Space = DBG_NEW char[Size];			// Start of space
	Ptr = Space;						// First available position
	TRACE(" Size=%d Space=%08x\n", Size, Space);
}

DataSpace::~DataSpace()
{
	TRACE("~DataSpace() called, Ptr=%08x\n",Ptr);
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

SCRIPT::SCRIPT(bool dump)
{
	dflag = dump;

	TRACE("SCRIPT() called\n");
	Data = NULL;

	for (int i = 0; i < SCRIPT_MAX_REC; i++)
	{
		Rec[i].pData = NULL;
	}
	RecCount = 0;
	RecIndex = 0;
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
	return ReadRecFile(szFileName, Rec, SCRIPT_MAX_REC);
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
	bf.ReadfromFile(sizeof(int), (BYTE *)&RecCount);
	TRACE("Loadbin RecCount=%d ", RecCount);
	int Size = bf.Filesize() - sizeof(int) - (RecCount * sizeof(SCRIPT_REC));
	TRACE("Size=%d\n", Size);
	Data = DBG_NEW DataSpace(Size);

	for (int i = 0; i < RecCount; i++)
	{
		SCRIPT_REC rec;
		bf.ReadfromFile(sizeof(SCRIPT_REC), (BYTE *)&rec);
		TRACE("Rec[%02d] iType=%-4d iLen=%-4d iNext=%-4d pData=%08x\n", i, Rec[i].iType, Rec[i].iLen, Rec[i].iNext, Rec[i].pData);
		rec.pData = (void *)(Data->Space + (INT)rec.pData);
		memcpy((BYTE *)&Rec[i], (BYTE *)&rec, sizeof(SCRIPT_REC));
	}
	bf.ReadfromFile(Data->Size, (BYTE *)Data->Space);
	bf.CloseFile();
	RecIndex = 0;
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
	TRACE("Savebin RecCount=%d ", RecCount);
	bf.WritetoFile(sizeof(int), (BYTE *)&RecCount);
	TRACE("Size=%d\n", Data->Size);
	for (int i = 0; i < RecCount; i++)
	{
		SCRIPT_REC rec;
		memcpy((BYTE *)&rec, (BYTE *)&Rec[i], sizeof(SCRIPT_REC));
		// Convert rec Data pointer to a offset value before writing to file.
		rec.pData = (void *)((BYTE *)Rec[i].pData - (BYTE *)Data->Space);
		TRACE("Rec[%02d] iType=%-4d iLen=%-4d iNext=%-4d pData=%08x\n", i, Rec[i].iType, Rec[i].iLen, Rec[i].iNext, Rec[i].pData);
		bf.WritetoFile(sizeof(SCRIPT_REC), (BYTE *)&rec);
	}
	bf.WritetoFile(Data->Size, (BYTE *)Data->Space);
	bf.CloseFile();
	RecIndex = 0;
	if (Data)
	{
		delete Data;
	}
	return 0;
}

SCRIPT_REC *SCRIPT::GetRec(int iNum)
{
	if (iNum < RecCount)
		return &Rec[iNum];
	else
		return NULL;
}

void SCRIPT::SetRecord(int iNum)
{ 
	RecIndex = iNum; 
}

int SCRIPT::GetRecord(void) 
{ 
	return RecIndex; 
}

int SCRIPT::GetVar(int iNum, int *val, int i)
{
	if (RecCount != 0)
	{
		// RecIndex range:  0----(RecCount-1)
		if (iNum < RecCount)
		{
			*val = ((SCRIPT_RECDATA *)Rec[iNum].pData)->iArray[i];
			return TRUE;
		}
	}
	return FALSE;
}

int SCRIPT::GetVar(int iNum, byte *val, int i)
{
	if (RecCount != 0)
	{
		// RecIndex range:  0----(RecCount-1)
		if (iNum < RecCount)
		{
			*val = ((SCRIPT_RECDATA *)Rec[iNum].pData)->bArray[i];
			return TRUE;
		}
	}
	return FALSE;
}

int SCRIPT::SetVar(int iNum, int val, int i)
{
	if (RecCount != 0)
	{
		// RecIndex range:  0----(RecCount-1)
		if (iNum < RecCount)
		{
			((SCRIPT_RECDATA *)Rec[iNum].pData)->iArray[i] = val;
			return TRUE;
		}
	}
	return FALSE;
}

int SCRIPT::SetVar(int iNum, byte val, int i)
{
	if (RecCount != 0)
	{
		// RecIndex range:  0----(RecCount-1)
		if (iNum < RecCount)
		{
			((SCRIPT_RECDATA *)Rec[iNum].pData)->bArray[i] = val;
			return TRUE;
		}
	}
	return FALSE;
}

int  SCRIPT::GetNextRec(SCRIPT_REC *pRec)
{
	if (RecCount != 0 )
	{
		// RecIndex range:  0----(RecCount-1)
		if ( RecIndex < RecCount )
		{
			pRec->iType = Rec[RecIndex].iType;
			pRec->iNum  = Rec[RecIndex].iNum;
			pRec->iLen  = Rec[RecIndex].iLen;
			pRec->pData = Rec[RecIndex].pData;
			pRec->iNext = Rec[RecIndex].iNext;
			RecIndex++;
			return TRUE;
		}
	}
	return FALSE;
}

int  SCRIPT::FindTypeRecord(int type, int next)
{
	SCRIPT_REC *pRec;

	// RecIndex range:  0----(RecCount-1)
	for (int i = 0; i < RecCount; i++)
	{
		pRec = &Rec[i];
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

// Read a line of text (ie. a record) from the file
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

#define MAX_ERRSTR_LEN		256

void SCRIPT::ReadError(char *s)
{
	char szErrStr[MAX_ERRSTR_LEN];
	
	if (strlen(s) < (MAX_ERRSTR_LEN-56))
		sprintf(szErrStr, "Syntax Line %03d: '%s'", FileLineNum(), s);
	else
		strcpy(szErrStr, "ERR: String Overflow");

	Display(szErrStr);		
}

bool SCRIPT::NextChar(int *cnt, char **pp)
{
	*cnt += sizeof(char);
	if (!IsDataSpace(*cnt))
	{
		return false;
	}
	sscanf(tp, "%c", *pp);
	*pp += sizeof(char);
	return true;
}

bool SCRIPT::NextInt(int *cnt, char **pp)
{
	*cnt += sizeof(int);
	if (!IsDataSpace(*cnt))
	{
		return false;
	}
	sscanf(tp, "%d", *pp);
	*pp += sizeof(int);
	return true;
}

bool SCRIPT::NextVar(int *cnt, char **pp)
{
	int i;
	*cnt += sizeof(int);
	if (!IsDataSpace(*cnt))
	{
		return false;
	}
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
	*((int *)(*pp)) = i;
	*pp += sizeof(int);
	return true;
}

bool SCRIPT::NextBuf(int *cnt, char **pp)
{
	char *pSrc = tp;
	while (*pSrc)
	{
		if (*pSrc != '\\')
		{
			*cnt += 1;
			if (!IsDataSpace(*cnt)) return false;
			*(*pp)++ = *pSrc++;
		}
		else
		{
			pSrc++;
			if (*pSrc != 'x' && *pSrc != 'X')
			{
				*cnt += 1;
				if (!IsDataSpace(*cnt)) return false;
				*(*pp)++ = '\\';
				*cnt += 1;
				if (!IsDataSpace(*cnt)) return false;
				*(*pp)++ = *pSrc++;
			}
			else
			{
				pSrc++;

				char szByte[3];
				szByte[0] = *pSrc++;	// high nibble
				szByte[1] = *pSrc++;    // low nibble
				szByte[2] = '\0';

				if (isxdigit(szByte[0]) && isxdigit(szByte[1]))
				{
					*cnt += 1;
					if (!IsDataSpace(*cnt)) return false;
					sscanf(szByte, "%02X", *pp);
					*(*pp)++;
				}
				else
				{
					*cnt += 1;
					if (!IsDataSpace(*cnt)) return false;
					*(*pp)++ = szByte[0];
					*cnt += 1;
					if (!IsDataSpace(*cnt)) return false;
					*(*pp)++ = szByte[1];
				}
			}
		}
	}
	*(*pp) = '\0';
	return true;
}

int SCRIPT::ReadRecFile(char *filename, SCRIPT_REC *rec, int MaxRecCnt )
{
    char *line;
    SCRIPT_REC *ip=rec;
	char szInfo[512];
	int rc = 0;
	char *pDest = NULL;

	linenum = 0;
	
	if ( !FileOpen(filename) )
    {
        sprintf(szInfo, "File %s not found!", filename);
		Display( szInfo );
        return -1;
    }
    TRACE("Reading records from File %s\n", filename);

	RecCount = 0;		// when reading failed, RecCount=0
	RecIndex = 0;

	while ((line = FileGetLine()) != NULL)
    {
		int cnt;
		sprintf(szInfo, "%03d: %s", linenum, line);
		Display(szInfo);

		if ((tp=strtok(line,toksep)) != NULL)
        {
			cnt = 0;
			
			//if (*tp==0x1A)
            //    goto eof;

			if (*(tp+0)=='/' && *(tp+1)=='/' )
                continue;	// skip over comments

			if (pDest == NULL)
			{
				if (!strcmp(tp, sg[SCRIPT_DATA].szKey))
				{
					/* The 'data' script is a directive and must be located at the beginning. */
					if (RecIndex == 0)
					{
						/* Make sure there is only one directive. */
						if (Data == NULL)
						{
							int n = 0;
							if ((tp = strtok(NULL, toksep)) != NULL)
							{
								sscanf(tp, "%d", &n);
							}
							Data = DBG_NEW DataSpace(n);
							pDest = Data->Ptr;
							continue;
						}
					}
					sprintf(szInfo, "Invalid Data directive");
					rc = -6;
					goto err;
				}
				else
				{
					sprintf(szInfo, "Expecting Record Type data");
					rc = -8;
					goto err;
				}
			}
			else if (!strcmp(tp, sg[SCRIPT_TXCHAR].szKey))
			{
				tp=strtok(NULL,toksep);
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
				Rec[RecIndex].iType = SCRIPT_TXCHAR;
				Rec[RecIndex].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_TXFILE].szKey))
			{
				tp = strtok(NULL,tokeol);
				cnt += strlen(tp) + 1;
				if (!IsDataSpace(cnt)) goto errmem;
				strcpy(Data->Ptr, tp);
				Rec[RecIndex].iType = SCRIPT_TXFILE;
				Rec[RecIndex].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_TXBUF].szKey))
			{
				int n = 0;
				while ((tp = strtok(NULL, toksep)) != NULL)
				{
					if (!NextInt(&cnt, &pDest)) goto errmem;	 // MaxCnt
					if ((tp = strtok(NULL, toksep)) != NULL)
					{
						if (!NextVar(&cnt, &pDest)) goto errmem; // X;x
					}
					n++;
				}
				Rec[RecIndex].iType = SCRIPT_TXBUF;
				Rec[RecIndex].iNext = n;						 // number of bufs
			}
			else if (!strcmp(tp, sg[SCRIPT_RXCHAR].szKey))
			{
				int label = -1;
				if ((tp = strtok(NULL, tokeol)) != NULL)
				{
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
					if ((tp = strtok(NULL, tokeol)) != NULL)
					{
						sscanf(tp, "%d", &label);
					}
				}
				Rec[RecIndex].iType = SCRIPT_RXCHAR;
				Rec[RecIndex].iNext = label;
			}
			else if (!strcmp(tp, sg[SCRIPT_SKIP].szKey))
			{
				int label = -1;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					if (!NextInt(&cnt, &pDest)) goto errmem;
					if ((tp = strtok(NULL, toksep)) != NULL)
					{
						if (!NextVar(&cnt, &pDest)) goto errmem;
						if ((tp = strtok(NULL, toksep)) != NULL)
						{
							sscanf(tp, "%d", &label);
						}
					}
				}
				Rec[RecIndex].iType = SCRIPT_SKIP;
				Rec[RecIndex].iNext = label;
			}
			else if (!strcmp(tp, sg[SCRIPT_DELAY].szKey))
			{
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					if (!NextInt(&cnt, &pDest)) goto errmem;
				}
				Rec[RecIndex].iType = SCRIPT_DELAY;
				Rec[RecIndex].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_TIMEOUT].szKey))
			{
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					if (!NextInt(&cnt, &pDest)) goto errmem;
				}
				Rec[RecIndex].iType = SCRIPT_TIMEOUT;
				Rec[RecIndex].iNext = 0;
			}
			else if (!strcmp(tp, sg[SCRIPT_EXIT].szKey))
			{
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					if (!NextInt(&cnt, &pDest)) goto errmem;
				}
				Rec[RecIndex].iType = SCRIPT_EXIT;
				Rec[RecIndex].iNext = 0;
			}
			else  if (!strcmp(tp, sg[SCRIPT_VAR].szKey))
			{
				int var = 0;
				while ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &var);
					if ((tp = strtok(NULL, tokval)) != NULL)
					{
						if (!NextInt(&cnt, &pDest)) goto errmem;
					}
				}
				Rec[RecIndex].iType = SCRIPT_VAR;
				Rec[RecIndex].iNext = var;
			}
			else  if (!strcmp(tp, sg[SCRIPT_GOTO].szKey))
			{
				int label = 0;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &label);
				}
				if ((tp = strtok(NULL, tokval)) != NULL)
				{
					if (!NextVar(&cnt, &pDest)) goto errmem;
					while ((tp = strtok(NULL, tokval)) != NULL)
					{
						if (!NextInt(&cnt, &pDest)) goto errmem;
					}
				}
				Rec[RecIndex].iType = SCRIPT_GOTO;
				Rec[RecIndex].iNext = label;
			}
			else  if (!strcmp(tp, sg[SCRIPT_TEST].szKey))
			{
				int label = 0;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &label);
				}
				if ((tp = strtok(NULL, tokval)) != NULL)
				{
					if (!NextVar(&cnt, &pDest)) goto errmem;
					while ((tp = strtok(NULL, tokval)) != NULL)
					{
						if (!NextInt(&cnt, &pDest)) goto errmem;
					}
				}
				Rec[RecIndex].iType = SCRIPT_TEST;
				Rec[RecIndex].iNext = label;
			}
			else  if (!strcmp(tp, sg[SCRIPT_BUF].szKey))
			{
				int buf = 0;
				if ((tp = strtok(NULL, toksep)) != NULL)
				{
					sscanf(tp, "%d", &buf);
					while ((tp = strtok(NULL, tokval)) != NULL)
					{
						if (*tp == '[')
						{
							int j;
							tp++;
							sscanf(tp, "%d", &j);
							char *p = strchr(tp, ']');
							if (p == NULL)
							{
								break;
							}
							p++;
							tp = p;
							int k = 0;
							if (!NextBuf(&k, &pDest)) goto errmem;
							TRACE("cnt=%d j=%d k=%d\n", cnt, j, k);
							cnt += max(j, k);
							if (!IsDataSpace(cnt)) goto errmem;
							if (j > k)
							{
								for (; k < j; k++)
								{
									*pDest++ = '\0';
								}
							}
							continue;
						}
						if (!NextBuf(&cnt, &pDest)) goto errmem;
					}
				}
				Rec[RecIndex].iType = SCRIPT_BUF;
				Rec[RecIndex].iNext = buf;
			}
			else  if (isdigit(*tp))
			{
				/* If first token starts with a digit then it is a 'label' value. */
				int label = 0;
				sscanf(tp, "%d", &label);
				Rec[RecIndex].iType = SCRIPT_LABEL;
				Rec[RecIndex].iNext = label;
			}
			else
			{
				sprintf(szInfo, "Incorrect Record Type %s", tp);
				rc = -8;
				goto err;
			}
        }
		Rec[RecIndex].iNum = linenum;
		Rec[RecIndex].iLen = cnt;
		Rec[RecIndex].pData = Data->Ptr;
		Data->Ptr += cnt;

		if (++RecIndex >= MaxRecCnt)
        {
			ReadError("Too many records!");
			rc = -7;
			goto err;
        }
    }
	RecCount = RecIndex;
	TRACE("%d records, bytes %d used / %d remaining\n", RecCount, Data->Ptr - Data->Space, Data->Size - (Data->Ptr - Data->Space));
	FileClose();
	return rc;

errmem:
	sprintf(szInfo, "Out of Data space!");
	rc = -9;
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

void SCRIPT::Dump(void)
{
	if (dflag && Data != NULL)
	{
		MEM_Dump((unsigned char *)Data->Space, Data->Size, 0L);
	}
}

 SCRIPT_REC *SCRIPT::GetVarRecPtr(int iNum)
{
	int j = FindTypeRecord(SCRIPT_VAR, iNum);
	return (j >= 0) ? GetRec(j) : NULL;
}

void MEM_Dump(unsigned char *data, int len, long base)
{
	int i, j;

	//printf("MEM: @%08x len=%04x\n",data,len);
	for (i = 0; i < len; i += 16)
	{
		printf(" %06x: ", base + i);
		for (j = 0; j < 16; j++)
		{
			if (j != 0)
			{
				if (!(j % 8))
					putchar(' ');
				if (!(j % 1))
					putchar(' ');
			}
			if ((i + j) < len)
				printf("%02x", data[i + j]);
			else
				printf("  ");
		}
		printf("  ");
		for (j = 0; j < 16 && (i + j) < len; j++)
		{
			if ((i + j) < len)
			{
				int c = data[i + j] & 0xFF;
				if (isprint(c))
					putchar(c);
				else
					putchar('.');
			}
			else
				putchar(' ');
		}
		putchar('\n');
	}
}

