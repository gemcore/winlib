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
#define TRACE(fmt,...)	printf(fmt,__VA_ARGS__)
#undef DEBUG
#define DEBUG(...)

//#define RUN_TESTING
//#define COM_TESTING
//#define TMR_TESTING
//#define MDM_TESTING
//#define ZAR_TESTING
//#define LUA_TESTING
//#define FTL_TESTING
#define TED_TESTING

/* xScript Rx initial timeout maximum value. */
#define TMR_TIMEOUT_MAX	32000

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

/* Used in defining the Read block size. */
#define BUFSIZE			128

/* xScript return result codes. */
#define XSCRIPT_PROCESSING       1
#define XSCRIPT_SUCCESS          0
#define XSCRIPT_EXIT            -1
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

		pRecData = (SCRIPT_RECDATA*)(rec.pData);

		if (rec.iType == SCRIPT_LOG)
		{
			char *filename = "";
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
					if (pScript->GetRec(j)->bFmt == FMT_VAR_INT)
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
						if (pScript->GetRec(j)->bFmt == FMT_VAR_INT)
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
			if (rec.bFmt == FMT_VAR_INT)
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
						if (pScript->GetRec(j)->bFmt == FMT_VAR_INT)
						{
							pScript->GetVar(j, &var, idx);
							if (pScript->EvalExpr(rec.bFmt, var, val))
							{
								TRACE("continue");
							}
							else
							{
								TRACE("val+%d=%d", inc, var + inc);
								pScript->SetVar(j, var + inc, idx);
								TRACE("\njump to record %d", i);
								pScript->SetRecord(i);		// Goto to record
							}
						}
						else
						{
							pScript->GetVar(j, (byte *)&var, idx);
							var &= 0xFF;
							if (pScript->EvalExpr(rec.bFmt, (byte)var, (byte)val))
							{
								TRACE("continue");
							}
							else
							{
								TRACE("val+%d=%d", (byte)inc, (byte)(var + inc));
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
					if (pScript->GetRec(j)->bFmt == FMT_VAR_INT)
					{
						pScript->GetVar(j, &var, idx);
						if (pScript->EvalExpr(rec.bFmt, var, val))
						{
							TRACE("continue");
						}
						else
						{
							if (rec.iLen >= 12)
							{
								TRACE("val=%d", inc);
								pScript->SetVar(j, inc, idx);
							}
							TRACE("\nskip %d records", rec.iNext);
							pScript->SetRecord(pScript->GetRecord() + rec.iNext);	// Skip over records
						}
					}
					else
					{
						pScript->GetVar(j, (byte *)&var, idx);
						var &= 0xFF;
						if (pScript->EvalExpr(rec.bFmt, (byte)var, (byte)val))
						{
							TRACE("continue");
						}
						else
						{
							if (rec.iLen >= 12)
							{
								TRACE("val=%d", inc);
								pScript->SetVar(j, (byte)inc, idx);
							}
							TRACE("\nskip %d records", rec.iNext);
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
			printf("%d:", rec.iNext);
			//TRACE(" record %d", pScript->GetRecord() - 1);
			printf("\n");
		}
		break;

		case SCRIPT_ECHO:
		{
            printf("echo %d ", rec.iLen);
		    for (int i = 0; i < rec.iLen; i++)
		    {
			    int c;
			    c = pRecData->cArray[i] & 0xFF;
			    if (isprint(c))
			    {
				    //CON_putc(c);
					putchar(c);
			    }
			    else
			    {
				    //CON_printf("\\x%02X", c);
					printf("\\x%02X", c);
				}
		    }
		    //CON_putc('\n');
			putchar('\n');
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

int _main(int argc, char *argv[])
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
					if (strlen(p + 1))
					{
						ofile = p + 1;
					}
					p = " ";
					break;

				case 'l':
					if (strlen(p + 1))
					{
						lfile = p + 1;
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
					fprintf(stderr, "-q         quiet mode\n");
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
			strcat(ifname, (cflag) ? ".spt" : ".out");
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

	COM_Term();
	TMR_Term();

	// Close capture log file.
	LOG_Term();

	return 0;
}

int main(int argc, char *argv[])
{
	argv[0] = "winlib";
	argv[1] = "c:\\temp\\test3.spt";
	argv[2] = "-lc:\\temp\\test3.log";
	argv[3] = "-c";
	argv[4] = "-e";
	argv[5] = "-d";
	//argv[6] = "-o";
	//argv[7] = "-q";
	//argc = 8;
	argc = 6;

	return _main(argc, argv);
}
#endif

#ifdef MDM_TESTING
/*
* Copyright 2001-2010 Georges Menie (www.menie.org)
* All rights reserved.
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the University of California, Berkeley nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* this code needs standard functions memcpy() and memset()
and input/output functions rxbyte() and txbyte().

the prototypes of the input/output functions are:
int rxbyte(unsigned short timeout); // msec timeout
void txbyte(int c);

*/

//#include "crc16.h"

/*
** Calculate a 16-bit 'CRC' value on a buffer.
**
** crc16 CCITT results matched with those provided by:
** http://www.lammertbies.nl/comm/info/crc-calculation.html.
*/

unsigned short crc16_ccitt(const unsigned char* data, unsigned char length)
{
	unsigned char x;
	unsigned short crc = 0xFFFF;

	while (length--)
	{
		x = crc >> 8 ^ *data++;
		x ^= x >> 4;
		crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
	}
	return crc;
}


#define SOH  0x01
#define STX  0x02
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18
#define SUB  0x1A

#define DLY_1S 1000
#define MAXRETRANS 25
//#define TRANSMIT_XMODEM_1K

static int check(int crc, const unsigned char *buf, int sz)
{
	if (crc) {
		unsigned short crc = crc16_ccitt(buf, sz);
		unsigned short tcrc = (buf[sz] << 8) + buf[sz + 1];
		if (crc == tcrc)
			return 1;
	}
	else {
		int i;
		unsigned char cks = 0;
		for (i = 0; i < sz; ++i) {
			cks += buf[i];
		}
		if (cks == buf[sz])
			return 1;
	}

	return 0;
}

/* Buffer size 1024 for XModem 1k + 3 head chars + 2 crc + nul */
#define bufsize				(1030)

/* Recv return result codes. */
#define MDM_PROCESSING       1
#define MDM_SUCCESS          0
#define MDM_EXIT            -1
#define MDM_FILE_NOT_FOUND  -2
#define MDM_ERROR           -3
#define MDM_TIMEOUT         -5
#define MDM_ABORTED        -12

class Recv;
class Send;

class XModem : public ActiveObject
{
private:
	unsigned char xbuff[bufsize];

	void InitThread() { }
	void Run();
	void FlushThread() { e.Release(); }

	friend class Recv;
	friend class Send;

public:
	BASEFILE *bf;
	int Result;
	Event e;

	XModem()
	{
	}

	~XModem()
	{
	}

	int Begin(void)
	{
		if (bf)
		{
			if (bf->IsFile())
			{
				Result = MDM_PROCESSING;

				/* Run thread. */
				Resume();
			}
			else
			{
				Result = MDM_FILE_NOT_FOUND;
			}
		}
		else
		{
			Result = MDM_ERROR;
		}
		return Result;
	}

	int GetResult() { return Result; }
	int WaitResult() { e.Wait(); return GetResult(); }

	int rxbyte(int timeout);
	void txbyte(unsigned char b);
	void rxflush(void);

	virtual char *getname() { return ""; }
	virtual int getbyte(void) { return -1; }
	virtual void putbyte(unsigned char b) { }
	virtual bool Init(void) { return false; }
	virtual int Process(void) { return -1; }
};

void XModem::Run()
{
	BOOL bSuccess = true;

	/* Define a processing timer. */
	//Tmr tmr = TMR_New();
	//int Timeout = TMR_TIMEOUT_MAX;
	//TMR_Start(tmr, Timeout);
	printf("%s: Processing\n", getname());

	while (bSuccess)
	{
		if (log)
		{
			log->Flush();
		}
		if (_isDying)
		{
			printf("%s: Aborted!\n", getname());
			Result = MDM_ABORTED;
			break;
		}
		int st = Process();
		if (st < 0)
		{
			printf("%s: Failure, status=%d\n", getname(), st);
			Result = MDM_ERROR;
			bSuccess = false;
		}
		else
		{
			printf("%s: Success: %d bytes\n", getname(), st);
			Result = MDM_SUCCESS;
			bSuccess = false;
		}
	}

	//TMR_Stop(tmr);
	//TMR_Delete(tmr);
	printf("%s: Result=%d\n", getname(), Result);
	e.Release();
}

int XModem::rxbyte(int timeout)
{
	int c = -1;
	for (int count = timeout/10; count > 0 && ((c = getbyte()) == -1); count--)
	{
		Sleep(10);
	}
	return c;
}

void XModem::txbyte(unsigned char b)
{
	putbyte(b);
}

void XModem::rxflush(void)
{
	int c;
	while ((c = rxbyte(((DLY_1S) * 3) >> 1)) >= 0)
	{
		TRACE("%s: <[%02x]\n", getname(), c);
	}
}

/* Circular rx/tx buffers for 'communicating' between the Recv and Send threads. */
circbuf rxbuf(bufsize);
circbuf txbuf(bufsize);

class Recv : public XModem
{
public:
	Recv(char *filename) : XModem()
	{
		TRACE("%s(%s)\n", getname(), filename);
		bf = new BASEFILE(false, filename);
	}

	~Recv()
	{
		TRACE("~%s()\n", getname());
		delete bf;
	}

	char *getname() { return "Recv"; }
	int getbyte(void);
	void putbyte(unsigned char b);
	int Process(void);
};

int Recv::getbyte(void)
{
	int c;
	if (rxbuf.remove(&c) != ERR_BUFFER)
	{
		return c;
	}
	return -1;
}

void Recv::putbyte(unsigned char b)
{
	int c = (b & 0xFF);
	txbuf.insert(c);
}

static Mutex  dump_mutex;

void Packet_Dump(char *name, char *str, unsigned char *data, int len, long base)
{
	Lock lock(dump_mutex);

	int i, j;

	printf("%s: %s @%08x len=%04x\n", name, str, data, len);
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

int Recv::Process(void)
{
	unsigned char *p;
	int bufsz, crc = 0;
	unsigned char trychar = 'C';
	unsigned char packetno = 1;
	int i, c, bufpos = 0;
	int retry, retrans = MAXRETRANS;

	for (;;)
	{
		for (retry = 0; retry < 16; ++retry)
		{
			if (trychar)
			{
				if (trychar == 'C')
					TRACE("%s: >'C'\n", getname());
				else
					TRACE("%s: >NAK\n", getname());
				txbyte(trychar);
			}
			if ((c = rxbyte((DLY_1S) << 1)) >= 0)
			{
				switch(c)
				{
				case SOH:
					bufsz = 128;
					goto start_recv;
				case STX:
					bufsz = 1024;
					goto start_recv;
				case EOT:
					TRACE("%s: <EOT\n", getname());
					//rxflush();	//TODO: ADG! Why flush here?
					txbyte(ACK);
					TRACE("%s: >ACK\n", getname());
					TRACE("%s: file size=%d\n", getname(), bf->Filesize());
					return bufpos; /* normal end */
				case CAN:
					if ((c = rxbyte(DLY_1S)) == CAN)
					{
						//rxflush();	//TODO: ADG! Why flush here?
						txbyte(ACK);
						TRACE("%s: >ACK\n", getname());
						return -1; /* cancelled by remote */
					}
					break;
				default:
					TRACE("%s: <[%02x]\n", getname(), c);
					break;
				}
			}
			else
			{
				TRACE("%s: timeout!\n", getname());
			}
		}
		if (trychar == 'C')
		{
			trychar = NAK; 
			continue;
		}
		//rxflush();	//TODO: ADG! Why flush here?
		txbyte(CAN);
		TRACE("%s: >CAN\n", getname());
		txbyte(CAN);
		TRACE("%s: >CAN\n", getname());
		txbyte(CAN);
		TRACE("%s: >CAN\n", getname());
		return -2; /* sync error */

	start_recv:
		TRACE("%s: bufsz=%d\n", getname(), bufsz);
		if (trychar == 'C')
		{
			crc = 1;
		}
		trychar = 0;
		p = xbuff;
		*p++ = c;
		for (i = 0; i < (bufsz + 3 + (crc ? 1 : 0)); ++i)
		{
			if ((c = rxbyte(DLY_1S)) < 0)
			{
				goto reject;
			}
			*p++ = c;
		}

		if (xbuff[1] == (unsigned char)(~xbuff[2]) &&
			(xbuff[1] == packetno || xbuff[1] == (unsigned char)packetno - 1) &&
			check(crc, &xbuff[3], bufsz))
		{
			if (xbuff[1] == packetno)
			{
				Packet_Dump(getname(), "<PACKET", xbuff, bufsz + 4 + (crc ? 1 : 0), bufpos);
				/*TODO: Handle write errors. */
				bf->WritetoFile((DWORD)bufsz, (BYTE *)&xbuff[3]);
				bufpos += bufsz;
				++packetno;
				retrans = MAXRETRANS + 1;
			}
			if (--retrans <= 0)
			{
				//rxflush();	//TODO: ADG! Why flush here?
				txbyte(CAN);
				TRACE("%s: >CAN\n", getname());
				txbyte(CAN);
				TRACE("%s: >CAN\n", getname());
				txbyte(CAN);
				TRACE("%s: >CAN\n", getname());
				return -3; /* too many retry error */
			}
			TRACE("%s: >ACK\n", getname());
			txbyte(ACK);
			continue;
		}
	reject:
		rxflush();
		txbyte(NAK);
		TRACE("%s: >NAK\n", getname());
	}
}

class Send : public XModem
{
public:
	Send(char *filename) : XModem()
	{
		TRACE("%s(%s)\n", getname(), filename);
		bf = new BASEFILE(true, filename);
	}

	~Send()
	{
		TRACE("~%s()\n", getname());
		delete bf;
	}

	char *getname() { return "Send"; }
	int getbyte(void);
	void putbyte(unsigned char b);
	int Process(void);
};

int Send::getbyte(void)
{
	int c;
	if (txbuf.remove(&c) != ERR_BUFFER)
	{
		return c;
	}
	return -1;
}

void Send::putbyte(unsigned char b)
{
	int c = (b & 0xFF);
	rxbuf.insert(c);
}

int Send::Process(void)
{
	int bufsz, crc = -1;
	unsigned char packetno = 1;
	int i, c, bufpos = 0;
	int retry;
	long fsize = bf->Filesize();
	TRACE("%s: file size=%d\n", getname(), fsize);

	for (;;)
	{
		for (retry = 0; retry < 16; ++retry)
		{
			if ((c = rxbyte((DLY_1S) << 1)) >= 0)
			{
				switch(c)
				{
				case 'C':
					TRACE("%s: <'C'\n", getname());
					crc = 1;
					goto start_trans;
				case NAK:
					TRACE("%s: <NAK\n", getname());
					crc = 0;
					goto start_trans;
				case CAN:
					if ((c = rxbyte(DLY_1S)) == CAN)
					{
						TRACE("%s: <CAN\n", getname());
						txbyte(ACK);
						TRACE("%s: >ACK\n", getname());
						//rxflush();	//TODO: ADG! Why flush here?
						return -1; /* canceled by remote */
					}
					break;
				default:
					TRACE("%s: <[%02x]\n", getname(), c);
					break;
				}
			}
		}
		txbyte(CAN);
		TRACE("%s: >CAN\n", getname());
		txbyte(CAN);
		TRACE("%s: >CAN\n", getname());
		txbyte(CAN);
		TRACE("%s: >CAN\n", getname());
		//rxflush();	//TODO: ADG! Why flush here?
		return -2; /* no sync */

		for (;;)
		{
		start_trans:
#ifdef TRANSMIT_XMODEM_1K
			xbuff[0] = STX; bufsz = 1024;
#else
			xbuff[0] = SOH; bufsz = 128;
#endif
			xbuff[1] = packetno;
			xbuff[2] = ~packetno;
			c = fsize - bufpos;
			if (c > bufsz)
			{
				c = bufsz;
			}
			if (c > 0)
			{
				memset(&xbuff[3], 0, bufsz);
				/*TODO: Handle Read errors. */
				bf->ReadfromFile((DWORD)c, (BYTE *)&xbuff[3]);
				if (c < bufsz)
				{
					/* NOTE mark the end of a text file with '1A'. */
					xbuff[3 + c] = SUB;
				}
				if (crc)
				{
					unsigned short ccrc = crc16_ccitt(&xbuff[3], bufsz);
					xbuff[bufsz + 3] = (ccrc >> 8) & 0xFF;
					xbuff[bufsz + 4] = ccrc & 0xFF;
				}
				else
				{
					unsigned char ccks = 0;
					for (i = 3; i < bufsz + 3; ++i)
					{
						ccks += xbuff[i];
					}
					xbuff[bufsz + 3] = ccks;
				}
				for (retry = 0; retry < MAXRETRANS; ++retry)
				{
					Packet_Dump(getname(), ">PACKET", xbuff, bufsz + 4 + (crc ? 1 : 0), 0);
					for (i = 0; i < bufsz + 4 + (crc ? 1 : 0); ++i)
					{
						txbyte(xbuff[i]);
					}
					if ((c = rxbyte(DLY_1S)) >= 0)
					{
						switch(c)
						{
						case ACK:
							TRACE("%s: <ACK\n", getname());
							++packetno;
							bufpos += bufsz;
							goto start_trans;
						case CAN:
							TRACE("%s: <CAN\n", getname());
							txbyte(ACK);
							TRACE("%s: >ACK\n", getname());
							//rxflush();	//TODO: ADG! Why flush here?
							return -1; /* canceled by remote */
						case NAK:
							TRACE("%s: <NAK\n", getname());
							break; 
						default:
							TRACE("%s: <[%02x]\n", getname(), c);
							break;
						}
					}
				}
				txbyte(CAN);
				TRACE("%s: >CAN\n", getname());
				txbyte(CAN);
				TRACE("%s: >CAN\n", getname());
				txbyte(CAN);
				TRACE("%s: >CAN\n", getname());
				rxflush();
				return -4; /* xmit error */
			}
			else
			{
				for (retry = 0; retry < 10; ++retry)
				{
					txbyte(EOT);
					TRACE("%s: >EOT\n", getname());
					c = rxbyte((DLY_1S) << 1);
					if (c >= 0)
					{
						if (c == ACK)
						{
							TRACE("%s: <ACK\n", getname());
							return bufpos; /* normal end */
						}
						else
						{
							TRACE("%s: <[%02x]\n", getname(), c);
						}
					}
				}
				//rxflush();	//TODO: ADG! Why flush here?
				return -5;
			}
		}
	}
}

char qflag = 0;  						// quiet mode

int main(int argc, char *argv[])
{
	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init("c:\\temp\\winlib.log");

	TMR_Init(100);	// 100ms timebase
	COM_Init(3, 115200);

	/* Initialize the Recv & Send threads. */
	Recv *recv = new Recv("C:\\temp\\recv.txt");
	Send *send = new Send("C:\\temp\\send.txt");
	/* Begin execution of processing. */
	if (send->Begin() == MDM_PROCESSING)
	{
		Sleep(100);

		if (recv->Begin() == MDM_PROCESSING)
		{
			recv->WaitResult();
			send->WaitResult();
		}
	}
	recv->rxflush();
	send->rxflush();

	/* Terminate the Recv & Send thread. */
	delete send;
	delete recv;

	COM_Term();
	TMR_Term();

	// Close capture log file.
	LOG_Term();

	return 0;
}
#endif

#ifdef COM_TESTING
int main(int argc, char *argv[])
{
	TMR_Init(100);	// 100ms timebase
	COM_Init(3,115200);

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
	COM_Term();
	TMR_Term();

	return 0;
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

char qflag = 0;  						// quiet mode

int main(int argc, char *argv[])
{
	TMR_Init(100);	// 100ms timebase

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

	TMR_Term();
	
	return 0;
}
#endif

#ifdef ZAR_TESTING
extern "C" int Cmd_zar(int argc, char *argv[]);

int _main(int argc, char *argv[])
{
	// Capture output from putchar, puts and printf macros.
	LOG_Init("c:\\temp\\winlib.log");
	
	int rc = Cmd_zar(argc, argv);

	// Close capture log file.
	LOG_Term();
	return rc;
}

int main(int argc, char *argv[])
{
#if 0
	argv[0] = "zar";
	argv[1] = "-c";
	argv[2] = "*.txt";
	argv[3] = "-zc:\\temp\\zar.zar";
	argc = 4;
#else
	argv[0] = "zar";
	argc = 1;
#endif
	return _main(argc, argv);
}
#endif

#ifdef FTL_TESTING
char qflag = 0;  						// quiet mode off (output to window)

extern "C" int main_map(void);

int main(int argc, char *argv[])
{
	// Capture output from putchar, puts and printf macros.
	LOG_Init("c:\\temp\\winlib.log");
	printf("\nFTL Testing ");

	main_map();

	// Close capture log file.
	LOG_Term();
	return 0;
}

#endif

#ifdef TED_TESTING
char qflag = 0;  						// quiet mode off (output to window)

#define DEFINE_CONSOLEV2_PROPERTIES

// System headers
#include <windows.h>

// Standard library C-style
#include <wchar.h>
#include <stdlib.h>
#include <stdio.h>

#define ESC "\x1b"
#define CSI "\x1b["

extern "C"
{
HANDLE hOut;
HANDLE hIn;

INPUT_RECORD records[128];

bool GetStdIOHandles()
{
	hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (hIn == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Set output mode to handle virtual terminal sequences
	hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	return true;
}

bool EnableVTMode()
{
	DWORD dwMode = 0;
	if (!GetConsoleMode(hOut, &dwMode))
	{
		return false;
	}

	dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(hOut, dwMode))
	{
		return false;
	}
	return true;
}

#if 0
void PrintVerticalBorder()
{
	printf(ESC "(0"); // Enter Line drawing mode
	printf(CSI "104;93m"); // bright yellow on bright blue
	printf("x"); // in line drawing mode, \x78 -> \u2502 "Vertical Bar"
	printf(CSI "0m"); // restore color
	printf(ESC "(B"); // exit line drawing mode
}

void PrintHorizontalBorder(COORD const Size, bool fIsTop)
{
	printf(ESC "(0"); // Enter Line drawing mode
	printf(CSI "104;93m"); // Make the border bright yellow on bright blue
	printf(fIsTop ? "l" : "m"); // print left corner 

	for (int i = 1; i < Size.X - 1; i++)
		printf("q"); // in line drawing mode, \x71 -> \u2500 "HORIZONTAL SCAN LINE-5"

	printf(fIsTop ? "k" : "j"); // print right corner
	printf(CSI "0m");
	printf(ESC "(B"); // exit line drawing mode
}

void PrintStatusLine(char* const pszMessage, COORD const Size)
{
	printf(CSI "%d;1H", Size.Y);
	printf(CSI "K"); // clear the line
	printf(pszMessage);
}
#endif

int CON_Init(void)
{
	// Get handles for Console I/O
	bool fSuccess = GetStdIOHandles();
	if (!fSuccess)
	{
		return -1;
	}

	// Enable Console VT mode
	fSuccess = EnableVTMode();
	if (!fSuccess)
	{
		//DWORD dw = GetLastError();
		return -1;
	}

	// Get Screen dimensions
	CONSOLE_SCREEN_BUFFER_INFO ScreenBufferInfo;
	GetConsoleScreenBufferInfo(hOut, &ScreenBufferInfo);
	COORD Size;
	Size.X = ScreenBufferInfo.srWindow.Right - ScreenBufferInfo.srWindow.Left + 1;
	Size.Y = ScreenBufferInfo.srWindow.Bottom - ScreenBufferInfo.srWindow.Top + 1;

#if 0
	int ch;

	// Enter the alternate buffer
	printf(CSI "?1049h");

	// Clear screen, tab stops, set, stop at columns 16, 32
	printf(CSI "1;1H");
	printf(CSI "2J"); // Clear screen

	int iNumTabStops = 4; // (0, 20, 40, width)
	printf(CSI "3g"); // clear all tab stops
	printf(CSI "1;20H"); // Move to column 20
	printf(ESC "H"); // set a tab stop

	printf(CSI "1;40H"); // Move to column 40
	printf(ESC "H"); // set a tab stop

					 // Set scrolling margins to 3, h-2
	printf(CSI "3;%dr", Size.Y - 2);
	int iNumLines = Size.Y - 4;

	printf(CSI "1;1H");
	printf(CSI "102;30m");
	printf("Windows 10 Anniversary Update - VT Example");
	printf(CSI "0m");

	// Print a top border - Yellow
	printf(CSI "2;1H");
	PrintHorizontalBorder(Size, true);

	// // Print a bottom border
	printf(CSI "%d;1H", Size.Y - 1);
	PrintHorizontalBorder(Size, false);

	// draw columns
	printf(CSI "3;1H");
	int line = 0;
	for (line = 0; line < iNumLines * iNumTabStops; line++)
	{
		PrintVerticalBorder();
		if (line + 1 != iNumLines * iNumTabStops) // don't advance to next line if this is the last line
			printf("\t"); // advance to next tab stop
	}

	PrintStatusLine("Press any key to see text printed between tab stops.", Size);
	ch = CON_getc();

	// Fill columns with output
	printf(CSI "3;1H");
	for (line = 0; line < iNumLines; line++)
	{
		int tab = 0;
		for (tab = 0; tab < iNumTabStops - 1; tab++)
		{
			PrintVerticalBorder();
			printf("line=%d", line);
			printf("\t"); // advance to next tab stop
		}
		PrintVerticalBorder();// print border at right side
		if (line + 1 != iNumLines)
			printf("\t"); // advance to next tab stop, (on the next line)
	}

	PrintStatusLine("Press any key to demonstrate scroll margins", Size);
	ch = CON_getc();

	printf(CSI "3;1H");
	for (line = 0; line < iNumLines * 2; line++)
	{
		printf(CSI "K"); // clear the line
		int tab = 0;
		for (tab = 0; tab < iNumTabStops - 1; tab++)
		{
			PrintVerticalBorder();
			printf("line=%d", line);
			printf("\t"); // advance to next tab stop
		}
		PrintVerticalBorder(); // print border at right side
		if (line + 1 != iNumLines * 2)
		{
			printf("\n"); //Advance to next line. If we're at the bottom of the margins, the text will scroll.
			printf("\r"); //return to first col in buffer
		}
	}
	PrintStatusLine("Press any key to exit", Size);
	ch = CON_getc(); #endif
#endif
	return 0;
}

int CON_kbhit()
{
	DWORD NumberOfEventsRead;
	BOOL rc = PeekConsoleInput(hIn, records, 128, &NumberOfEventsRead);
	if (rc)
	{
		int count = 0;
		//printf("\nNumberOfEventsRead=%d\n", NumberOfEventsRead);
		for (unsigned int i = 0; i < NumberOfEventsRead; i++)
		{
			WORD type = records[i].EventType;
			if (type == KEY_EVENT)
			{
				if (records[i].Event.KeyEvent.bKeyDown)
				{
					//printf(" %02d: type=%04x Key pressed\n", i, type);
					count++;
				}
			}
		}
		return count;
	}
	return 0;
}

int CON_getc()
{
	DWORD NumberOfEventsTotal;
	DWORD NumberOfEventsRead;
	while (1)
	{
		BOOL rc = GetNumberOfConsoleInputEvents(hIn, &NumberOfEventsTotal);
		if (rc)
		{
			if (NumberOfEventsTotal > 0)
			{
				//printf("\nNumberOfEventsTotal=%d\n", NumberOfEventsTotal);
				BOOL rc = ReadConsoleInput(hIn, records, 1, &NumberOfEventsRead);
				if (rc)
				{
					if (NumberOfEventsRead > 0)
					{
						WORD type = records[0].EventType;
						if (type == KEY_EVENT)
						{
							DWORD state = records[0].Event.KeyEvent.dwControlKeyState;
							WORD kc = records[0].Event.KeyEvent.wVirtualKeyCode;
							WORD sc = records[0].Event.KeyEvent.wVirtualScanCode;
							WORD ch = records[0].Event.KeyEvent.uChar.AsciiChar;
							if (records[0].Event.KeyEvent.bKeyDown)
							{
								//printf(" type=%04x state=%04x sc=%04x kc=%04x ch=%02x ", type, state, sc, kc, ch);
								if (state & ENHANCED_KEY)
								{
									//printf("Enhanced key\n");
									return 0x100 | kc;
								}
								else if (state & SHIFT_PRESSED)
								{
									// a CTRL key pressed
									if (ch != 0x00)
									{
										//printf("Shift\n");
										return ch;
									}
									//printf("Shift?\n");
								}
								else if (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
								{
									// a CTRL key pressed
									if (ch != 0x00)
									{
										//printf("Control\n");
										return ch;
									}
									//printf("Control?\n");
								}
								else if (state & 0x1FF)
								{
									// Ignore unsupported virtual scan key combinations.
									//printf("Unsupported\n");
								}
								else
								{
									//printf("AsciiChar\n");
									return ch;
								}
							}
						}
					}
				}
			}
		}		
	}
	return 0;
}

#include "src/term/bsp.h"
#include "src/term/cfg.h"
#include "src/term/term.h"
#include "src/term/evt.h"
#include "src/term/cli.h"
#include "src/term/pts.hpp"

extern void TRM_Init();
extern void TRM_Term();
extern int Cmd_ted(int argc, char *argv[]);

void MEM_Dump(uint8_t *data, uint16_t len, uint32_t base)
{
	uint16_t i, j;

	//if (!CFG_IsTrace(DFLAG_TRC))
	//	return;

	//CON_printf("MEM: @%08x len=%04x\n",data,len);
	for (i = 0; i < len; i += 16)
	{
		CON_printf(" %06x: ", base + i);
		for (j = 0; j < 16; j++)
		{
			if (j != 0)
			{
				if (!(j % 8))
					CON_printf(" ");
				if (!(j % 1))
					CON_printf(" ");
			}
			if ((i + j) < len)
				CON_printf("%02x", data[i + j]);
			else
				CON_printf("  ");
		}
		CON_printf("  ");
		for (j = 0; j < 16 && (i + j) < len; j++)
		{
			if ((i + j) < len)
			{
				if (isprint(data[i + j]))
					CON_printf("%c", data[i + j]);
				else
					CON_printf(".");
			}
			else
				CON_printf(" ");
		}
		CON_printf("\n");
	}
}

//*****************************************************************************
// This function implements the "help" command.  It prints a list of available
// commands with a brief description.
//*****************************************************************************
int Cmd_help(int argc, char *argv[])
{
	SHELL_COMMAND *psEntry;

	// Point at the beginning of the command table.
	psEntry = &g_psShellCmds[0];

	// Enter a loop to read each entry from the command table.  The end of the
	// table has been reached when the command name is NULL.
	while (psEntry->cmd)
	{
		// Print the command name and the brief description.
		CON_printf("%6s: %s\n", psEntry->cmd, psEntry->help);

		// Advance to the next entry in the table.
		psEntry++;
	}
	return(0);
}

//*****************************************************************************
// This is the table that holds the command names, implementing functions, and
// brief description.
//*****************************************************************************
SHELL_COMMAND g_psShellCmds[] =
{
{ "help",       Cmd_help,       "display list of commands" },
{ "?",          Cmd_help,       "alias for help" },
{ "cfg",        Cmd_cfg,        "configure settings" },
#if HAS_PTS == 1
{ "pts",        Cmd_pts,        "protothread scheduler" },
#endif
#if HAS_LOG == 1
{ "log",        Cmd_log,        "logging options" },
#endif    
#if HAS_CLI == 1
{ "cli",        Cmd_cli,        "command lines" },
#endif
#if HAS_EVT == 1
{ "evt",        Cmd_evt,        "events" },
#endif
{ "ed",         Cmd_ted,        "text editor" },
{ 0, 0, 0 }
};

}

class SysTick : CTimerFunc
{
public:
	SysTick() {}
	void Func(void) { SysTickIntHandler(); }
};

int main(int argc, char *argv[]) 
{
	argc; // unused
	argv; // unused

	// Capture output from putchar, puts and printf macros.
	LOG_Init("c:\\temp\\term.log");
	BSP_Init();
	TMR_Init(100);	// 100ms timebase

	SysTick *Tick = new SysTick();
	TMR_Event(1, (CTimerEvent *)Tick, PERIODIC);
/*
	printf("\nset 1 second timer ");
	Tmr t = TMR_New();
	TMR_Start(t, 10);
	while (!TMR_IsTimeout(t))
	{
		TMR_Delay(1);
		putchar('.');
	}
	printf("done\n");

	uint8_t t1 = BSP_claim_msec_cnt();
	BSP_reset_msec_cnt(t1);
	while (1)
	{
		TMR_Delay(1);
		int count = BSP_get_msec_cnt(t1);
		if (count > 10000)
		{
			break;
		}
		printf("%5d\b\b\b\b\b", count);
	}
	printf("\n");
	BSP_return_msec_cnt(t1);
*/
	if (CON_Init() != 0)
	{
		printf("Unable to enter VT processing mode. Quitting.\n");
		return -1;
	}

#if HAS_PTS == 1
	PTS_Init();
#endif
#if HAS_CLI == 1
	CLI_Init();
#endif

#if HAS_PTS == 1
	while (PTS_GetTaskCnt() > 0)
	{
		PTS_Process();
	}
#endif

#if 0
	// Exit the alternate buffer
	printf(CSI "?1049l");
#endif

	TRM_Term();
	//PTS_Term();
	TMR_Term();
	// Close capture log file.
	LOG_Term();

	return 0;
}
#endif
