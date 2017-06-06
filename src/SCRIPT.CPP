/*0===========================================================================
**
** File:       Script.hpp
**
** Purpose:    Simple Script parsing class methods.
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
#include "script.hpp"

SCRIPT_GRAMMER sg[]=
{
	"tx",		SCRIPT_TXCHAR,
	"txfile",	SCRIPT_TXFILE,
	"rx",		SCRIPT_RXCHAR,
	"skip",		SCRIPT_SKIP,
	"delay",	SCRIPT_DELAY,
	"timeout",	SCRIPT_TIMEOUT,
	"exit",		SCRIPT_EXIT,
	"label",	SCRIPT_LABEL
};

char *toksep = " ;\t\n";
char *tokptr;

SCRIPT::SCRIPT(SCRIPT_CALLBACK pfnScr)
{
	for (int i = 0; i < SCRIPT_MAX_RECORDS; i++)
	{
		Msg[i].pData = NULL;
	}
	TotalRecord = 0;
	CurrentRecord = 0;
	linenum = 0;
	pfnCallBack = pfnScr;	// function pointer to display a single line of "text"
}

SCRIPT::~SCRIPT(void)
{
}

int SCRIPT::Load(char *szFileName)
{
	return ReadMsgFile(szFileName, Msg, SCRIPT_MAX_RECORDS);
}

void SCRIPT::SetRecord(int iNum) 
{ 
	CurrentRecord = iNum; 
}

int SCRIPT::GetRecord(void) 
{ 
	return CurrentRecord; 
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

int  SCRIPT::FindLabelRecord(int iNext)
{
	SCRIPT_RECORD *pRec;

	// CurrentRecord range:  0----(TotalRecord-1)
	for (int i = 0; i < TotalRecord; i++)
	{
		pRec = &Msg[i];
		if (pRec->iType == SCRIPT_LABEL)
		{
			if (pRec->iNext == iNext)
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

unsigned char *SCRIPT::atobytes(char *s,unsigned char *bytes,int n)
{
   unsigned char *bp = bytes;
   for (int i=0; i < 2*n; i+=2)
   {
      int byte=0;
      if (sscanf(&s[i],"%02x",&byte) == EOF)
         break;
      *bp++ = (unsigned char)byte;
   }
   return bytes;
}

int SCRIPT::ReadMsgFile(char *filename, SCRIPT_RECORD *msg, int MaxMsgCnt )
{
    char *line;
    char *tp;
    SCRIPT_RECORD *ip=msg;
	char szInfo[512];
	char *p;
	
	linenum = 1;
	
	if ( !FileOpen(filename) )
    {
        sprintf(szInfo, "File %s not found!", filename);
		Display( szInfo );
        return -1;
    }
    sprintf(szInfo, "Reading messages from File %s", filename);
	Display(szInfo);		

	printf("sizeof(Data)=%d\n", sizeof(Data));
	TotalRecord = 0;		// when reading failed, TotalRecord=0
	CurrentRecord = 0;
	p = Data;

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
                continue;	// comment line

			if (!strcmp(tp, sg[SCRIPT_TXCHAR].szKey))
			{
				tp=strtok(NULL,toksep);
				char *pDest = p;
				char *pSrc = tp;
				cnt = 0;
				while (*pSrc)
				{
					if (*pSrc != '\\')
					{
						*pDest++ = *pSrc++;
						cnt++;
					}
					else
					{
						pSrc++;
						if ( *pSrc!='x' && *pSrc!='X' )
						{
							sprintf(szInfo, "Wrong Escape string %s", tp );
							ReadError(szInfo);			
							FileClose();
							return -2;
						}
						else
						{							
							pSrc++;

							char szByte[3];
							szByte[0]= *pSrc++;	// high nibble
							szByte[1]= *pSrc++;	// low nibble
							szByte[2]= 0;
							
							if ( !isxdigit(szByte[0]) || !isxdigit(szByte[1]) )
							{
								sprintf(szInfo, "Wrong Escape string %s", tp );
								ReadError(szInfo);			
								FileClose();
								return -3;
							}
							sscanf(szByte, "%02X", pDest);
							pDest++;
							cnt++;
						}
					}
				}				
				*pDest=0;
				Msg[CurrentRecord].iType = sg[SCRIPT_TXCHAR].iType;
			}
			else if (!strcmp(tp, sg[SCRIPT_TXFILE].szKey))
			{
				tp = strtok(NULL,toksep);
				cnt = strlen(tp) + 1;
				strcpy(p, tp);
				Msg[CurrentRecord].iType = sg[SCRIPT_TXFILE].iType;
			}
			else if (!strcmp(tp, sg[SCRIPT_RXCHAR].szKey))
			{
				int token = 0;
				int onerror = -1;

				/* Get next token. */
				cnt = 0;
				while ((tp = strtok(NULL, toksep)) != NULL)
				{
					if (token == 0)
					{
						char *pDest = p;// Data[CurrentRecord].szWaitChars;
						char *pSrc = tp;
						while (*pSrc)
						{
							if (*pSrc != '\\')
							{
								*pDest++ = *pSrc++;
								cnt++;
							}
							else
							{
								pSrc++;
								if (*pSrc != 'x' && *pSrc != 'X')
								{
									sprintf(szInfo, "Wrong Escape string %s", tp);
									ReadError(szInfo);
									FileClose();
									return -4;
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
										ReadError(szInfo);
										FileClose();
										return -5;
									}
									sscanf(szByte, "%02X", pDest);
									pDest++;
									cnt++;
								}
							}
						}
						*pDest = 0;
					}
					else if (token == 1)
					{
						sscanf(tp, "%d", &onerror);
					}
					else
					{
						break;
					}
					token++;
				}
				Msg[CurrentRecord].iType = sg[SCRIPT_RXCHAR].iType;
				Msg[CurrentRecord].iNext = onerror;
			}
			else if (!strcmp(tp, sg[SCRIPT_SKIP].szKey))
			{
				tp = strtok(NULL, toksep);
				cnt = sizeof(int);
				sscanf(tp, "%d", p);
				Msg[CurrentRecord].iType = sg[SCRIPT_SKIP].iType;
			}
			else if (!strcmp(tp, sg[SCRIPT_DELAY].szKey))
			{
				tp = strtok(NULL,toksep);
				cnt = sizeof(int);
				sscanf(tp, "%d", p);
				Msg[CurrentRecord].iType = sg[SCRIPT_DELAY].iType;
			}
			else if (!strcmp(tp, sg[SCRIPT_TIMEOUT].szKey))
			{
				tp = strtok(NULL,toksep);
				cnt = sizeof(int);
				sscanf(tp, "%d", p);
				Msg[CurrentRecord].iType = sg[SCRIPT_TIMEOUT].iType;
			}
			else if (!strcmp(tp, sg[SCRIPT_EXIT].szKey))
			{
				tp = strtok(NULL, toksep);
				cnt = 0;
				Msg[CurrentRecord].iType = sg[SCRIPT_EXIT].iType;
			}
			else  if (!strcmp(tp, sg[SCRIPT_LABEL].szKey))
			{
				int n;
				tp = strtok(NULL, toksep);
				cnt = 0;
				sscanf(tp, "%d", &n);
				Msg[CurrentRecord].iType = sg[SCRIPT_LABEL].iType;
				Msg[CurrentRecord].iNext = n;
			}
			else
			{
				sprintf(szInfo, "Incorrect Message Type %s", tp);
				ReadError(szInfo);
				FileClose();
				return -6;
			}
        }
		Msg[CurrentRecord].iLen = cnt;
		Msg[CurrentRecord].pData = p;
		p += cnt;

		if (++CurrentRecord >= MaxMsgCnt)
        {
			ReadError("Too many messages!");
			FileClose();
			return -7;
        }
    }

//eof:
	TotalRecord = CurrentRecord;
	sprintf( szInfo, "Total messages   %d Ok", TotalRecord);
	Display ( szInfo );
	FileClose();
	return 0;
}

