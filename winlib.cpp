// winlib.cpp : Defines the entry point for the console application.
//

#define _CRTDBG_MAP_ALLOC  
#include "stdafx.h"
#include "windows.h"
#include "src\LOG.H"		// Capture putchar & printf output to a log file
#include "src\TMR.H"
#include "src\comport.h"
//#include <iostream>     /* C++ stream input/output class */
//using namespace std;

#ifdef _DEBUG
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
// Replace _NORMAL_BLOCK with _CLIENT_BLOCK if you want the
// allocations to be of _CLIENT_BLOCK type
#else
#define DBG_NEW new
#endif

#undef TRACE
#define TRACE(...)
#undef DEBUG
#define DEBUG(...)

#define RUN_TESTING
//#define COM_TESTING
//#define TMR_TESTING

ComPort *com = NULL;

int rxpacket_len = 0;
int rxpacket_cnt = 0;
byte rxpacket[80];

extern void MEM_Dump(unsigned char *data, int len, long base);

/*
** ComPort virtual thread processing function.
*/
void ComPort::Process(void)
{
	/* Rx serial processing - rx data block received is put into circular buffer. */
	int n = Read((LPSTR)rxpacket, sizeof(rxpacket) - 1);
	if (n > 0)
	{
		RxWrite(n, (char *)rxpacket);
	}

	/* Tx serial processing - tx data is sent immediately by app, so nothing to do. */

}

extern "C"
{
	int COM_Init(int port,long baud)
	{
		if (com == NULL)
		{
			com = new ComPort(port, baud);
			if (com->Start())
			{
				com->Resume();
				return 0;
			}
		}
		return -1;
	}

	void COM_Term(void)
	{
		if (com)
		{
			com->Stop();
			com->Sleep(100);

			delete com;
			com = NULL;
		}
	}

	int COM_recv_char(void)
	{
		return com->RxGetch();
	}

	int COM_send_char(byte c)
	{
		return com->Write(c);
	}

	bool COM_Write(char *buf, int len)
	{
		return com->Write(buf, len);
	}
}


#ifdef RUN_TESTING
/*
** A simple SCRIPTing language based thread for processing serial output and input data.
*/
#include "src\BASEFILE.HPP"
#include "src\SCRIPT.HPP"
#include "src\TMR.H"

/* xScript Rx initial timeout maximum value. */
#define TMR_TIMEOUT_MAX	32000

/* Used in defining the Read block size. */
#define BUFSIZE			128

/* xScript return result codes. */
#define XSCRIPT_PROCESSING       1
#define XSCRIPT_SUCCESS          0
#define XSCRIPT_EXIT			 -1
#define XSCRIPT_FILE_NOT_FOUND  -2
#define XSCRIPT_ERROR           -3
#define XSCRIPT_COMPORT_NA      -4
#define XSCRIPT_TIMEOUT         -5
#define XSCRIPT_LABEL_NOT_FOUND -6
#define XSCRIPT_VAR_NOT_FOUND   -7
#define XSCRIPT_BUF_NOT_FOUND   -8
#define XSCRIPT_VAL_UNDEFINED   -9
#define XSCRIPT_RET_UNDEFINED  -10
#define XSCRIPT_CALL_OVERFLOW  -11
#define XSCRIPT_ABORTED        -12
#define XSCRIPT_OTHER          -13

class xScript : public ActiveObject
{
public:
	SCRIPT *pScript;
	int Result;
	Event e;

	xScript()
	{
		TRACE("xScript() this=%08x\n", this); 
	}

	~xScript()
	{
		TRACE("~xScript() this=%08x\n", this);
	}

	char *getname() { return "xScript"; }

	int RunScript(SCRIPT *script)
	{
		pScript = script;
		Result = XSCRIPT_PROCESSING;
		/* Run thread. */
		Resume();
		/* Wait for result from script processing. */
		return WaitResult();
	}

	int GetResult() { return Result; }
	int WaitResult() { e.Wait(); return GetResult(); }

private:
	void InitThread() { }
	void Run();
	void FlushThread() { e.Release(); }

};

void xScript::Run()
{
	SCRIPT_REC rec;
	SCRIPT_RECDATA *pRecData;
	BOOL bSuccess = true;

	/* Define a processing timer. */
	Tmr tmr = TMR_New();
	int Timeout = TMR_TIMEOUT_MAX;
	TMR_Start(tmr, Timeout);

	//TRACE("%s: Running...\n", getname());
	printf("Run\n");
	pScript->SetRecord(0);

	while (bSuccess && pScript->GetNextRec(&rec))
	{
		if (log)
		{
			log->Flush();
		}

		if (_isDying)
		{
			printf("%s: Aborted!\n", getname());
			Result = XSCRIPT_ABORTED;
			break;
		}

		if (rec.iType == SCRIPT_LOG)
		{
			char *filename = "";
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			int enable = 0;
			if (rec.iLen >= 4)
			{
				LOG_Enable(true);
				enable = pRecData->iArray[0];					/* the Enable: 0=off, 1=on */
				if (rec.iLen > 4)
				{
					filename = &pRecData->cArray[sizeof(int)];	/* the Filename */
					LOG_Term();
					LOG_Init(filename);
				}
				printf("> %03d: log %d", rec.iNum, enable);

				if (enable)
				{
					printf(",%s\n", filename);
					log->Flush();
				}
				else
				{
					printf("\n> ...\n");
					log->Flush();
					LOG_Enable(enable);
				}
			}
			continue;
		}

		printf("> %03d: ", rec.iNum);

		switch (rec.iType)
		{
		case SCRIPT_CALL:
		{
			int c;
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("call %d ", rec.iNext);
			int i;
			// call label record specified 
			i = pScript->FindTypeRecord(SCRIPT_LABEL, rec.iNext);
			if (i >= 0)
			{
				/* Push our current record location in a fifo so we can 'ret' later. */
				if (!pScript->PushReturn())
				{
					printf("\n%s: Call_Overflow!\n", getname());
					Result = XSCRIPT_CALL_OVERFLOW;
					bSuccess = false;
					break;
				}
				TRACE("\njump to record %d", i);
				pScript->SetRecord(i);
			}
			else
			{
				TRACE("\n%s: [%d] Label_Not_Found!\n", getname(), rec.iNext);
				Result = XSCRIPT_LABEL_NOT_FOUND;
				break;
			}
			putchar('\n');
		}
		break;

		case SCRIPT_RET:
		{
			int i;
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("ret ");
			/* Pop the return record location. */
			if (!pScript->PopReturn())
			{
				printf("\n%s: Ret_Undefined!\n", getname());
				Result = XSCRIPT_RET_UNDEFINED;
				bSuccess = false;					 
				break;
			}
			TRACE("\njump to record %d", pScript->GetRecord());
			putchar('\n');
		}
		break;

		case SCRIPT_TXCHAR:
		{
			int c;
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("tx %d ", rec.iLen);
			for (int i = 0; i < rec.iLen; i++)
			{
				c = pRecData->cArray[i] & 0xFF;
				if (isprint(c))
				{
					putchar(c);
				}
				else
				{
					printf("\\x%02X", c);
				}
			}
			putchar('\n');
			COM_Write(pRecData->cArray, rec.iLen);
		}
		break;

		case SCRIPT_TXBUF:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("txbuf");
			for (int k=0; k < rec.iNext; k++)
			{
				int MaxCnt = 0;
				int Cnt = 0;
				int buf = -1;
				int idx = 0;
				int j;
				if (rec.iLen >= ((2*(k+1))*sizeof(int)))
				{
					MaxCnt = pRecData->iArray[(2*k)+0];
					buf    = pRecData->iArray[(2*k)+1];
					printf(" %d", MaxCnt);
					idx = buf >> 16;
					buf &= 0xFFFF;
				}
				if (buf != -1)
				{
					j = pScript->FindTypeRecord(SCRIPT_VAR, buf);
					if (j >= 0)
					{
						putchar(',');
						TRACE("buf ");
						printf("%d@%d:", buf, idx);
					}
					else
					{
						printf("\n%s: [%d] Buf_Not_Found!\n", getname(), buf);
						Result = XSCRIPT_BUF_NOT_FOUND;
						bSuccess = false;
						break;
					}
				}
				if (MaxCnt == 0)
				{
					/* if maximum count is zero use the record's data Len. */
					MaxCnt = pScript->GetRec(j)->iLen;
				}
				MaxCnt = max(MaxCnt, idx);
				for (Cnt = 0; Cnt < MaxCnt; Cnt++)
				{
					int c;
					if (pScript->GetRec(j)->bFmt[0] == 0)
					{
						// Get int from int array.
						pScript->GetVar(j, (int *)&c, idx + Cnt);
					}
					else
					{
						// Get char from byte array.
						pScript->GetVar(j, (byte *)&c, idx + Cnt);
					}
					c &= 0xFF;
					if (isprint(c))
					{
						putchar(c);
					}
					else
					{
						printf("\\x%02X", c);
					}
					COM_send_char((byte)c);
				}
			}
			putchar('\n');
			break;
		}

		case SCRIPT_TXFILE:
		{
			BASEFILE bf;
			char *filename;
			long size;
			BYTE buf[BUFSIZE];
			int buflen;
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			filename = pRecData->cArray;	/* the image filename */
			bool bDone = false;
			printf("txfile %s ", filename);
			/* Determine the size of the image file. */
			bf.InitReadFile(filename);
			if (!bf.IsFile())
			{
				printf("\n%s: File_Not_Found!\n", getname());
				bSuccess = false;
				Result = XSCRIPT_FILE_NOT_FOUND;
				break;
			}
			size = bf.Filesize();
			printf("size=%08x bytes\n", size);
			while (size)
			{
				if (size > BUFSIZE)
				{
					bf.ReadfromFile(BUFSIZE, buf);
					COM_Write((char *)&buf[0], BUFSIZE);
					size -= BUFSIZE;
				}
				else
				{
					bf.ReadfromFile(size, buf);
					COM_Write((char *)&buf[0], size);
					break;
				}
				if (_isDying)
				{
					printf("%s: Aborted!\n", getname());
					Result = XSCRIPT_ABORTED;
					bSuccess = false;
					break;
				}
			}
		}
		break;

		case SCRIPT_RXCHAR:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			TMR_Stop(tmr);
			if (Timeout > 0)
			{
				TMR_Start(tmr, Timeout);
			}
			int c;
			int Cnt = 0;
			int MaxCnt = rec.iLen;
			bool bFound = false;
			printf("rx %d ", MaxCnt);
			while (!bFound)
			{
				if (_isDying)
				{
					printf("\n%s: Aborted!\n", getname());
					Result = XSCRIPT_ABORTED;
					bSuccess = false;
					break;
				}
				if (TMR_IsTimeout(tmr))
				{
					// if label value not set
					if (rec.iNext == -1)
					{
						// then do Timeout error.
						printf("\n%s: Timeout!\n", getname());
						Result = XSCRIPT_TIMEOUT;
						bSuccess = false;
						break;
					}
					// else goto label record specified.
					int i = pScript->FindTypeRecord(SCRIPT_LABEL, rec.iNext);
					if (i >= 0)
					{
						TRACE("\njump to record %d", i);
						pScript->SetRecord(i);
					}
					else
					{
						printf("\n%s: [%d] Label_Not_Found!\n", getname(), rec.iNext);
						Result = XSCRIPT_LABEL_NOT_FOUND;
						bSuccess = false;
						break;
					}
					bFound = true;
					break;
				}
				while ((c = COM_recv_char()) != -1)
				{
					if (c == (pRecData->cArray[Cnt] & 0xFF))
					{
						if (isprint(c))
							putchar(c);
						else
							printf("\\x%02X", c);
						Cnt++;
					}
					else
					{
						putchar('[');
						if (isprint(c))
							putchar(c);
						else
							printf("%02X", c);
						putchar(']');
						while (Cnt > 0)
						{
							Cnt--;
						}
					}
					if (Cnt >= MaxCnt)
					{
						bFound = true;
						break;
					}
				}
				Sleep(5);
			}
			putchar('\n');
		}
		break;

		case SCRIPT_SKIP:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			TMR_Stop(tmr);
			if (Timeout > 0)
			{
				TMR_Start(tmr, Timeout);
			}
			int c;
			int Cnt = 0;
			int MaxCnt = pRecData->iArray[0];
			BOOL bFound = false;
			printf("skip %d", MaxCnt);
			int j;
			int var = -1;
			int idx = 0;
			if (rec.iLen > 4)
			{
				var = pRecData->iArray[1];
				idx = var >> 16;
				var &= 0xFFFF;
			}
			if (var != -1)
			{
				j = pScript->FindTypeRecord(SCRIPT_VAR, var);
				putchar(',');
				if (j >= 0)
				{
					TRACE("var ");
					printf("%d@%d:", var, idx);
				}
				else
				{
					printf("\n%s: [%d] Var_Not_Found!\n", getname(), var);
					Result = XSCRIPT_VAR_NOT_FOUND;
					bSuccess = false;
					break;
				}
			}
			while (bSuccess && !bFound)
			{
				if (_isDying)
				{
					printf("\n%s: Aborted!\n", getname());
					Result = XSCRIPT_ABORTED;
					bSuccess = false;
					break;
				}
				if (TMR_IsTimeout(tmr))
				{
					// if label value not set
					if (rec.iNext == -1)
					{
						// then do Timeout error.
						printf("\n%s: Timeout!\n", getname());
						Result = XSCRIPT_TIMEOUT;
						bSuccess = false;
						break;
					}
					// else goto label record specified.
					int i = pScript->FindTypeRecord(SCRIPT_LABEL, rec.iNext);
					if (i >= 0)
					{
						TRACE("\njump to record %d", i);
						pScript->SetRecord(i);
					}
					else
					{
						printf("\n%s: [%d] Label_Not_Found!\n", getname(), rec.iNext);
						Result = XSCRIPT_LABEL_NOT_FOUND;
						bSuccess = false;
						break;
					}
					bFound = true;
					break;
				}
				while (!bFound && (c = COM_recv_char()) != -1)
				{
					if (isprint(c))
						putchar(c);
					else
						printf("\\x%02X", c);
					if (var != -1)
					{
						// Save char to var or buf array.
						if (pScript->GetRec(j)->bFmt[0] == 0)
						{
							pScript->SetVar(j, c, idx + Cnt);
						}
						else
						{
							pScript->SetVar(j, (byte)c, idx + Cnt);
						}
					}
					Cnt++;
					if (Cnt >= MaxCnt)
					{
						bFound = true;
						break;
					}
				}
				Sleep(5);
			}
			putchar('\n');
		}
		break;

		case SCRIPT_DELAY:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("delay %d", pRecData->iValue);
			int i;
			for (i = 0; i < pRecData->iValue; i++)
			{
				if (_isDying)
				{
					printf("\n%s: Aborted!\n", getname());
					Result = XSCRIPT_ABORTED;
					break;
				}
				Sleep(100L);
				putchar('.');
			}
			putchar('\n');
		}
		break;

		case SCRIPT_TIMEOUT:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			Timeout = pRecData->iValue;
			printf("timeout %d\n", Timeout);
			TMR_Stop(tmr);
			if (Timeout > 0)
			{
				TMR_Start(tmr, Timeout);
			}
		}
		break;

		case SCRIPT_EXIT:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			if (rec.iLen == 0)
			{
				Result = XSCRIPT_EXIT;
			}
			else
			{
				Result = pRecData->iValue;
			}
			printf("exit %d\n", Result);
			pScript->SetRecord(pScript->GetRecCount());
		}
		break;

		case SCRIPT_VAR:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			if (rec.bFmt[0] == 0)
			{
				printf("var %d:", rec.iNext);
				int i;
				int n = rec.iLen / sizeof(int);
				for (i = 0; i < n; i++)
				{
					printf("%d", pRecData->iArray[i]);
					putchar(((i + 1) < n) ? ',' : ' ');
				}
			}
			else
			{
				printf("buf %d:", rec.iNext);
				int i;
				int n = rec.iLen / sizeof(char);
				for (i = 0; i < n; i++)
				{
					int c = pRecData->bArray[i];
					if (isprint(c))
						putchar(c);
					else
						printf("\\x%02X", c);
				}
			}
			putchar('\n');
		}
		break;

		case SCRIPT_BUF:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("buf %d:", rec.iNext);
			int i;
			int n = rec.iLen / sizeof(byte);
			for (i = 0; i < n; i++)
			{
				int c = pRecData->bArray[i] & 0xFF;
				if (isprint(c))
					putchar(c);
				else
					printf("\\x%02X", c);
			}
			putchar('\n');
		}
		break;

		case SCRIPT_GOTO:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("goto %d", rec.iNext);
			int i;
			int n = rec.iLen / sizeof(int);
			for (i = 0; i < n; i++)
			{
				if (i > 0)
				{
					printf(",%d", pRecData->iArray[i]);
				}
				else
				{
					printf(",%d@%d", pRecData->iArray[i] & 0xFFFF, pRecData->iArray[i] >> 16);
				}
			}

			// goto label record specified if variable meets condition
			i = pScript->FindTypeRecord(SCRIPT_LABEL, rec.iNext);
			if (i >= 0)
			{
				if (rec.iLen >= 4)
				{
					int var = -1;
					int val = 0;
					int inc = 0;
					int j;
					int idx;
					var = pRecData->iArray[0];
					idx = var >> 16;
					var &= 0xFFFF;
					j = pScript->FindTypeRecord(SCRIPT_VAR, var);
					if (j >= 0)
					{
						if (rec.iLen >= 8)
						{
							val = pRecData->iArray[1];
							if (rec.iLen >= 12)
							{
								inc = pRecData->iArray[2];
							}
						}
						TRACE(" %d@%d=", var, idx);
						if (pScript->GetRec(j)->bFmt[0] == 0)
						{
							pScript->GetVar(j, &var, idx);
							TRACE("(%d", var);
							if (var >= val)
							{
								TRACE(">=%d) continue", val);
							}
							else
							{
								TRACE("< %d) inc=%d, new val=%d", val, inc, var + inc);
								pScript->SetVar(j, var + inc, idx);
								TRACE("\njump to record %d", i);
								pScript->SetRecord(i);		// Goto to record
							}
						}
						else
						{
							pScript->GetVar(j, (byte *)&var, idx);
							var &= 0xFF;
							TRACE("(%d", var);
							if (var >= val)
							{
								TRACE(">=%d) continue", val);
							}
							else
							{
								TRACE("< %d) inc=%d, new val=%d", val, inc, var + inc);
								pScript->SetVar(j, (byte)(var + inc), idx);
								TRACE("\njump to record %d", i);
								pScript->SetRecord(i);		// Goto to record
							}
						}
					}
					else
					{
						printf("\n%s: [%d] Var_Not_Found!\n", getname(), var);
						Result = XSCRIPT_VAR_NOT_FOUND;
						bSuccess = false;
						break;
					}
				}
				else
				{
					TRACE("\njump to record %d", i);
					pScript->SetRecord(i);		// Goto to record
				}
			}
			else
			{
				TRACE("\n%s: [%d] Label_Not_Found!\n", getname(), rec.iNext);
				Result = XSCRIPT_LABEL_NOT_FOUND;
				bSuccess = false;
				break;
			}
			putchar('\n');
			break;
		}
		case SCRIPT_TEST:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("test %d", rec.iNext);
			int i;
			int n = rec.iLen / sizeof(int);
			for (i = 0; i < n; i++)
			{
				if (i > 0)
				{
					printf(",%d", pRecData->iArray[i]);
				}
				else
				{
					printf(",%d@%d", pRecData->iArray[i]&0xFFFF, pRecData->iArray[i]>>16);
				}
			}

			// test and skip over records if variable meets condition
			int var = -1;
			int val = 0;
			int inc = 0;
			int j;
			if (rec.iLen >= 4)
			{
				int idx;
				var = pRecData->iArray[0];
				idx = var >> 16;
				var &= 0xFFFF;
				j = pScript->FindTypeRecord(SCRIPT_VAR, var);
				if (j >= 0)
				{
					if (rec.iLen >= 8)
					{
						val = pRecData->iArray[1];
						if (rec.iLen >= 12)
						{
							inc = pRecData->iArray[2];
						}
					}
					TRACE(" %d@%d=", var, idx);
					if (pScript->GetRec(j)->bFmt[0] == 0)
					{
						pScript->GetVar(j, &var, idx);
						TRACE("(%d", var);
						if (var == val)
						{
							TRACE("==%d) continue", val);
						}
						else
						{
							TRACE("!=%d), ", val);
							if (rec.iLen >= 12)
							{
								TRACE("new val=%d, ", inc);
								pScript->SetVar(j, inc, idx);
							}
							TRACE("skip %d records", rec.iNext);
							pScript->SetRecord(pScript->GetRecord() + rec.iNext);	// Skip over records
						}
					}
					else
					{
						pScript->GetVar(j, (byte *)&var, idx);
						var &= 0xFF;
						TRACE("(%d", var);
						if (var == val)
						{
							TRACE("==%d) continue", val);
						}
						else
						{
							TRACE("!=%d), ", val);
							if (rec.iLen >= 12)
							{
								TRACE("new val=%d, ", inc);
								pScript->SetVar(j, (byte)inc, idx);
							}
							TRACE("skip %d records", rec.iNext);
							pScript->SetRecord(pScript->GetRecord() + rec.iNext);	// Skip over records
						}
					}
				}
				else
				{
					printf("\n%s: [%d] Var_Not_Found!\n", var);
					Result = XSCRIPT_VAR_NOT_FOUND;
					bSuccess = false;
					break;
				}
			}
			putchar('\n');
		}
		break;

		case SCRIPT_SERIAL:
		{
			pRecData = (SCRIPT_RECDATA*)(rec.pData);
			printf("serial %d", rec.iNext);
			int i;
			int n = rec.iLen / sizeof(int);
			for (i = 0; i < n; i++)
			{
				printf(",%d", pRecData->iArray[i]);
			}
			putchar('\n');
			if (rec.iNext == 1)
			{
				/* Serial port testing. */
				if (n >= 1)
				{
					if (COM_Init(pRecData->iArray[0], (n >= 2)?(long)pRecData->iArray[1]:CBR_9600))
					{
						Result = XSCRIPT_COMPORT_NA;
						bSuccess = FALSE;
					}
				}
			}
			else if (rec.iNext == 0)
			{
				COM_Term();
			}
			Sleep(100);
		}
		break;

		case SCRIPT_LABEL:
		{
			printf("%d:\n", rec.iNext);
		}
		break;

		default:
			printf("%s: [%d] Type Error!\n", getname(), rec.iType);
			Result = XSCRIPT_ERROR;
			bSuccess = FALSE;
		}
	}
	if (bSuccess)
	{
		TRACE("%s: Success\n", getname());
		Result = XSCRIPT_SUCCESS;
	}
	else
	{
		TRACE("%s: Failed\n", getname());
	}
	TMR_Stop(tmr);
	TMR_Delete(tmr);
	if (Result)
		printf("Run rc=%d\n", Result);
	e.Release();
}
#endif

#ifdef TMR_TESTING
class ATmr : CTimerFunc
{
	void Func(void)
	{
		putchar('.');
	}
};

ATmr atmr;
#endif

/* Local variables */

SCRIPT spt;

char cflag = 0;  						// compile script
char eflag = 0;  						// execute script
char dflag = 0;  						// dump data output
char qflag = 0;  						// quiet mode
char *lfile = NULL;						// debug log text output filename
char *ifile = NULL;						// script source text input filename
char oflag = 0;  						// save script output 
char *ofile = NULL;						// script output filename (compiled binary format)

char lfname[80];
char ifname[80];
char ofname[80];

int main(int argc, char *argv[])
{
	char *p;
	int i;

	for (i = 1; --argc > 0; i++)          // parse command line options
	{
		p = argv[i];
		if (*p == '-')
		{
			while (*++p)
			{
				switch (*p)
				{
				case 'o':
					oflag++;
					if (strlen(p+1))
					{
						ofile = p+1;
					}
					p = " ";
					break;

				case 'l':
					if (strlen(p+1))
					{
						lfile = p+1;
					}
					p = " ";
					break;

				case 'i':
					if (strlen(p + 1))
					{
						ifile = p + 1;
					}
					p = " ";
					break;

				case 'c':
					cflag++;
					break;

				case 'e':
					eflag++;
					break;

				case 'd':
					dflag++;
					break;

				case 'q':
					qflag++;
					break;

				default:
					fprintf(stderr, "Usage: winlib [-i][file] [-o][file] [-l][file] [-c] [-e] [-d]\n");
					fprintf(stderr, "[-i][file] source input\n");
					fprintf(stderr, "-o[file]   binary output\n");
					fprintf(stderr, "-l[file]   log output\n");
					fprintf(stderr, "-c         compile script\n");
					fprintf(stderr, "-e         execute script\n");
					fprintf(stderr, "-d         dump data output\n");
					fprintf(stderr, "-1         quiet mode\n");
					exit(1);
				}
			}
		}
		else
		{
			ifile = p;
		}
	}
	if (ifile)
	{
		strcpy(ifname, ifile);
		if (!strchr(ifname, '.'))
		{
			strcat(ifname, (cflag)? ".spt" : ".out");
		}
		ifile = ifname;
		DEBUG("ifile='%s'\n", ifile);
	}
	else
		DEBUG("ifile=stdin\n");

	if (cflag)
	{
		if (oflag)
		{
			if (ofile)
			{
				strcpy(ofname, ofile);
				if (!strchr(ofname, '.'))
				{
					strcat(ofname, ".out");
				}
				ofile = ofname;
				DEBUG("ofile='%s'\n", ofile);
			}
			else
				DEBUG("ofile=stdout\n");
		}
	}

	if (lfile)
	{
		strcpy(lfname, lfile);
		if (!strchr(lfile, '.'))
		{
			strcat(lfname, ".log");
		}
		lfile = lfname;
		DEBUG("lfile='%s'\n", lfile);
	}
	else
		DEBUG("lfile=stdout\n");

	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init(lfile);

	TMR_Init(100);	// 100ms timebase

	int rc;

	if (cflag)
	{
		/* Load the source SCRIPT records from a file and parse. */
		rc = spt.Load(ifile);
	}
	else
	{
		/* Load the binary SCRIPT records from a file. */
		rc = spt.Loadbin(ifile);
	}
	if (rc == 0)
	{
		if (cflag && oflag)
		{
			spt.Savebin(ofile);
		}
		if (eflag)
		{
			/* Initialize the xScript thread. */
			xScript *txscript = new xScript();

			/* Execute the script processing. */
			rc = txscript->RunScript(&spt);

			/* Terminate the xScript thread. */
			delete txscript;
#if 0
			/* Check variable 2. */
			SCRIPT_REC *r = spt.GetVarRecPtr(6);
			if (r != NULL)
			{
				SCRIPT_RECDATA *d = (SCRIPT_RECDATA *)r->pData;
				if (d->bArray[0] != 0x00)
				{
					//MEM_Dump((unsigned char *)d->iArray, r->iLen, 0L);
					printf("var 2:");
					//for (int i = 0; i < 5; i++)
					//{
					//	printf("\\x%08X,", d->iArray[i]);
					//}
					//printf(" as char:");
					for (int i = 0; i < 5; i++)
					{
						printf("%c,", (char)d->bArray[i]);
					}
					printf("\nublox version: %c.%c.%c.%c%c\n",
						(char)d->bArray[0], (char)d->bArray[1], (char)d->bArray[2], (char)d->bArray[3], (char)d->bArray[4]);
				}
			}
#endif
		}
		if (dflag)
		{
			spt.Dump();
		}
	}

#ifdef COM_TESTING
	unsigned char buf[80];
	com.Write("AT\r", 3);
	com.Sleep(2000);
	int n = com.Read((LPSTR)buf, sizeof(buf));
	if (n > 0)
	{
		for (int i = 0; i < n; i++)
		{
			printf("%c", buf[i]);
		}
		putchar('\n');
	}
#endif

#ifdef TMR_TESTING
	/* Timer testing. */
	printf("\nset 1 second timer ");
	Tmr t = TMR_New();
	TMR_Start(t, 10);
	while (!TMR_IsTimeout(t))
	{
		TMR_Delay(1);
		putchar('.');
	}
	printf("done\n");

	printf("periodic heartbeat ");
	TMR_Event(5, (CTimerEvent *)&atmr, PERIODIC);
	TMR_Delay(20);
	printf("2 seconds ");
	TMR_Delay(50);
	printf("5 seconds ");
	TMR_Event(0, (CTimerEvent *)&atmr, PERIODIC);
	putchar('\n');
#endif
	COM_Term();
	TMR_Term();

	// Close capture log file.
	LOG_Term();
	
	return 0;
}
#if 0
int main(int argc, char *argv[])
{
	argv[0] = "winlib";
	argv[1] = "c:\\temp\\test.spt";
	argv[2] = "-lc:\\temp\\test.log";
	argv[3] = "-c";
	argv[4] = "-o";
	argv[5] = "-q";
	argc = 6;

	return _main(argc, argv);
}
#endif

