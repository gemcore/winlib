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
#define TRACE(fmt,...) do { if (dflag) printf(fmt,__VA_ARGS__); } while(0)
//#define TRACE(...)
#undef DEBUG
#define DEBUG(...)

extern void MEM_Dump(unsigned char* data, int len, long base);

int dflag = false;

#undef MEM_Trace
#define MEM_Trace(data,len,base) do { if (dflag) MEM_Dump(data,len,base); } while(0)
//#define MEM_Trace(fmt,...)

//#define RUN_TESTING
//#define CBOR_TESTING
#define COM_TESTING
//#define TMR_TESTING
//#define MDM_TESTING
//#define FTL_TESTING
#define SEND_IMAGE

/* xScript Rx initial timeout maximum value. */
#define TMR_TIMEOUT_MAX	32000

ComPort *com = NULL;

int rxpacket_len = 0;
int rxpacket_cnt = 0;
byte rxpacket[80];

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
					//MEM_Trace((unsigned char *)d->iArray, r->iLen, 0L);
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
#endif

#ifdef MDM_TESTING
char qflag = 0;  						// quiet mode

int main(int argc, char *argv[])
{
	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init(NULL);

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

#ifdef TMR_TESTING
char qflag = 0;  						// quiet mode off (output to window)

class ATmr : CTimerFunc
{
	void Func(void)
	{
		putchar('.');
	}
};

ATmr atmr;

int main(int argc, char* argv[])
{
	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init(NULL);

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
	TMR_Event(5, (CTimerEvent*)&atmr, PERIODIC);
	TMR_Delay(20);
	printf("2 seconds ");
	TMR_Delay(50);
	printf("5 seconds ");
	TMR_Event(0, (CTimerEvent*)&atmr, PERIODIC);
	putchar('\n');

	TMR_Term();

	// Close capture log file.
	LOG_Term();

	return 0;
}
#endif

#ifdef FTL_TESTING
char qflag = 0;  						// quiet mode off (output to window)

extern "C" int main_map(void);

int main(int argc, char* argv[])
{
	// Capture output from putchar, puts and printf macros.
	LOG_Init(NULL);
	printf("\ntesting ");

	main_map();

	// Close capture log file.
	LOG_Term();
	return 0;
}
#endif

#include "src/cbor/cbor.h"

int token_cnt = 0;
cbor_token_t tokens[16];

cbor_token_t* cbor_get_key(char* s)
{
	bool key = false;
	for (int i = 0; i < token_cnt; i++)
	{
		if (!key && tokens[i].type == CBOR_TOKEN_TYPE_TEXT)
		{
			if (!strncmp(s, tokens[i].text_value, tokens[i].int_value))
			{
				key = true;
			}
		}
		else
		{
			if (key)
			{
				return &tokens[i];
			}
		}
	}
	return NULL;
}

unsigned int cbor_parse(unsigned char* data, int size)
{
	unsigned int offset = 0;
	token_cnt = 0;
	long j = 0;
	while (1)
	{
		// Build up a list of tokens that are contained in a global array.
		cbor_token_t* token = &tokens[token_cnt++];

		offset = cbor_read_token(data, size, offset, token);

		if (token->type == CBOR_TOKEN_TYPE_INCOMPLETE) {
			TRACE(" incomplete\n");
			break;
		}
		if (token->type == CBOR_TOKEN_TYPE_ERROR) {
			TRACE(" error: %s\n", token->error_value);
			break;
		}
		if (token->type == CBOR_TOKEN_TYPE_BREAK) {
			TRACE(" break\n");
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_INT) {
			TRACE(" int(%s%u)\n", token->sign < 0 ? "-" : "", token->int_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_LONG) {
			TRACE(" long(%s%llu)\n", token->sign < 0 ? "-" : "", token->long_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_ARRAY) {
			if (token->int_value == 31)
				TRACE(" array(*)\n");
			else
				TRACE(" array(%u)\n", token->int_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_MAP) {
			if (token->int_value == 31)
				TRACE(" map(*)\n");
			else
				TRACE(" map(%u)\n", token->int_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_TAG) {
			TRACE(" tag(%u)\n", token->int_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_SPECIAL) {
			TRACE(" special(%u)\n", token->int_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_TEXT) {
			TRACE(" text('%.*s')\n", token->int_value, token->text_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_BYTES) {
			TRACE(" bytes([%u]:", token->int_value);
			for (int i=0; i < token->int_value; i++)
			{
				TRACE("%02x ", token->bytes_value[i]);
			}
			TRACE(")\n");
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
	}

	return offset;
}

class CBOR
{
public:
	unsigned char* bfr;
	unsigned char* p;
	unsigned char* q;
	long base;
	int size;

	CBOR(unsigned char* buf, int n)
	{
		bfr = buf;
		base = 0L;
		size = n;
		q = bfr;
		//TRACE("bfr @%xH size=%d\n", bfr, size);
	}

	void put_map(int m)
	{
		TRACE(" map(%d)\n", m);
		p = cbor_write_map(q, size - base, m);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put_text(char* s)
	{
		TRACE(" text('%s')\n", s);
		p = cbor_write_text(q, size - base, s);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}


	void put_array(int m)
	{
		TRACE(" array(%d)", m);
		p = cbor_write_array(q, size - base, m);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put_bytes(unsigned char* bytes, int m)
	{
		TRACE(" bytes([%d]:", m);
		for (int i = 0; i < m; i++)
		{
			TRACE("%02x ", bytes[i]);
		}
		TRACE("\n");
		p = cbor_write_bytes(q, size - base, bytes, m);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put_int(int n)
	{
		TRACE(" int(%d)\n", n);
		p = cbor_write_int(q, size - base, n);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put_long(long long n)
	{
		TRACE(" long(%ld)\n", n);
		p = cbor_write_long(q, size - base, n);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	~CBOR()
	{
		//TRACE("bfr len=%d:\n", base);
	}
};

#ifdef CBOR_TESTING
#include <stdio.h>

char qflag = 0;  						// quiet mode off (output to window)

char* cbor_typetostr(int type)
{
	switch (type)
	{
	case CBOR_TOKEN_TYPE_ERROR:
		return "ERROR";
	case CBOR_TOKEN_TYPE_INCOMPLETE:
		return "INCOMPLETE";
	case CBOR_TOKEN_TYPE_INT:
		return "INT";
	case CBOR_TOKEN_TYPE_LONG:
		return "LONG";
	case CBOR_TOKEN_TYPE_MAP:
		return "MAP";
	case CBOR_TOKEN_TYPE_ARRAY:
		return "ARRAY";
	case CBOR_TOKEN_TYPE_TEXT:
		return "TEXT";
	case CBOR_TOKEN_TYPE_BYTES:
		return "BYTES";
	case CBOR_TOKEN_TYPE_TAG:
		return "TAG";
	case CBOR_TOKEN_TYPE_SPECIAL:
		return "SPECIAL";
	case CBOR_TOKEN_TYPE_BREAK:
		return "BREAK";
	default:
		return "UNKNOWN";
	}
}

void cbor_list_all(void)
{
	printf("%d tokens found:\n", token_cnt);
	cbor_token_t* token = tokens;
	for (int i = 0; i < token_cnt; i++, token++)
	{
		printf("%2d:\t%d=CBOR_TOKEN_TYPE_%s\n", i, token->type, cbor_typetostr(token->type));
	}
}

int main(int argc, char** argv)
{
	static unsigned char buffer[512];
	unsigned int size = sizeof(buffer);
	unsigned char* data = buffer;

	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init(NULL);

	printf("\ncbor put:\n");
	CBOR cbor(data, size);
	cbor.put_map(2);
	cbor.put_text("data");
	unsigned char bytes[] = { 0x01,0x02,0x03,0x04,0x05,0x07 };
	cbor.put_bytes(bytes, sizeof(bytes)); // size of data (in bytes);
	cbor.put_text("foo");
	cbor.put_int(1234);
	cbor.put_array(5);
	cbor.put_int(123);
	cbor.put_text("hello");
	cbor.put_text("world");
	cbor.put_int(1231231231);
	cbor.put_long(12312312311ULL);
	cbor.put_text("f:D");
	int base = cbor.base;

	printf("\ncbor data:\n");
	MEM_Dump(data, base, 0L);

	printf("\ncbor parse:\n");
	unsigned int offset = cbor_parse(data, base);
	printf("offset=%d\n", offset);

	printf("\ncbor list all:\n");
	cbor_list_all();

	// Do key lookups based on a 'text' token paired with the token following it.
	// Find key "data":bytes[].
	printf("\ncbor get key:\n");
	cbor_token_t* token;
	token = cbor_get_key("data");
	unsigned int n = token->int_value;	// total number of bytes of data[] received
	printf("data[%d]:", n);
	MEM_Dump(token->bytes_value, n, 0L);

	// Find key "foo":int.
	token = cbor_get_key("foo");
	printf("foo:%d\n", token->int_value);

	// Close capture log file.
	LOG_Term();

	return 0;
}
#endif

#ifdef COM_TESTING
#include "src/base64/base64.h"
#include "src/cbor/cbor.h"
#include "src/BASEFILE.HPP"

#define BUFSIZE		512

//extern "C" void init_crctable();
//extern "C" unsigned int crc16_buf(unsigned char* bufptr, int buflen, unsigned int crc);
extern "C" uint16_t crc_xmodem(const unsigned char* input_str, size_t num_bytes);

char qflag = 0;  						// quiet mode off (output to window)

unsigned char buf[1024];
unsigned char dec_buf[1024];
char          enc_str[1024];

//unsigned char Cmd_ImageList[] = { 
//	0x00,0x0b,0x00,0x00,0x00,0x01,0x00,0x01, 0x42,0x00,0xa0,0xf5,0x33 };

//unsigned char Cmd_Reset[]     = { 
//	0x00,0x0b,0x02,0x00,0x00,0x01,0x00,0x00, 0x42,0x05,0xa0,0xba,0x15 };

unsigned char bytes_sha[32] = {
	0x6c,0x5a,0x2b,0x11,0x0d,0xdc,0xc0,0xa2, 0x91,0xf0,0xd0,0x6c,0xa0,0x0d,0x2f,0xb0,
	0xe9,0x4c,0x63,0x10,0x0a,0x00,0x72,0x14, 0x12,0xe3,0xff,0xc0,0x35,0xd0,0xbe,0x2f };

int recv_buf(int count, int ch)
{
	int i = 0;
	int delay = 50;
	bool done = false;
	//TRACE("recv_buf(%d, %d){\n", count, ch);
	while (!done && count > 0)
	{
		int n = com->RxCount();
		if (n > 0)
		{
			for (int i=0; i < n && count > 0; i++, count--)
			{
				int c;
				c = com->RxGetch();
				if (isprint(c))
				{
					//TRACE("%c", c);
				}
				else if (c != -1 && c == ch)
				{
					//TRACE("<CR>");
					done = true;
					break;
				}
				else
				{
					//TRACE("[%02x]", c);
				}
				buf[i] = c;
			}
			if (!done && count > 0)
			{
				delay = 5;
				com->Sleep(1);
			}
		}
		else
		{
			com->Sleep(1);
			if (--delay <= 0)
			{
				done = true;
			}
		}
	}
	//TRACE("} return count=%d\n", count);
	return count;
}

void send_buf(unsigned char *buf, int n)
{
	//MEM_Trace(buf, n, 0L);
	if (!com->Write((LPSTR)buf, n))
	{
		printf("send_buf: Write error!\n");
	}
	com->Sleep(1);
}

/* NLIP packets sent over serial are fragmented into frames of 127 bytes or
 * fewer. This 127-byte maximum applies to the entire frame, including header,
 * CRC, and terminating newline.
 */
#define MGMT_NLIP_MAX_FRAME     127

typedef struct nmp_hdr
{
	unsigned char Op;
	unsigned char Flags;
	unsigned short Len;
	unsigned short Group;
	unsigned char Seq;
	unsigned char Id;
} nmp_hdr_t;

typedef struct nmp_packet
{
	nmp_hdr_t hdr;
	unsigned char data[0];
} nmp_packet_t;

#define swapbytes(n)	((n&0xFF)<<8)+(n>>8)

unsigned int total_len = 62956;
unsigned char seq = 0x42;

void TraceNmpHdr(nmp_hdr_t* hdr)
{
	MEM_Trace((unsigned char*)hdr, sizeof(nmp_hdr_t), 2L);
	TRACE(" Op:%d Flags:%d Len:%d Group:%d Seq:%d Id:%d\n",
		hdr->Op, hdr->Flags, swapbytes(hdr->Len), swapbytes(hdr->Group), hdr->Seq, hdr->Id);
}

void recv_nmp_resp(void)
{
#ifdef SEND_IMAGE
	uint16_t len;
	if (recv_buf(2, -1) != 0)
	{
		printf("resp: timeout!");
		return;
	}
	//TRACE("rx: %02x %02x\n", buf[0], buf[1]);
	// Receive remainder of packet:
	len = 50 - recv_buf(50, '\r');
	strncpy(enc_str, (char*)&buf[0], len);
	enc_str[len] = '\0';
	//TRACE("rx: '%s'\n", enc_str);
	int m;
	//m = base64_decode_len(enc_str);
	m = base64_decode(enc_str, dec_buf);
	//MEM_Trace(dec_buf, m, 0L);
	// NMP header (8-bytes):
	len = swapbytes(*(uint16_t*)&dec_buf[0]);
	//TRACE("nmp len=%04x\n", len);
	nmp_hdr_t* hdr = (nmp_hdr_t*)&dec_buf[2];
	len = swapbytes(hdr->Len);
	//TRACE("hdr len=%04x\n", len);
	TRACE("rx: ");
	TraceNmpHdr(hdr);
	unsigned int offset = cbor_parse(&dec_buf[2 + 8], len);
	//unsigned int remaining = len - 8 - offset;
	//TRACE("remaining=%x ", remaining);
	//uint16_t crc = swapbytes(*(uint16_t *)&dec_buf[2 + 8 + offset]);
	//uint16_t crc = swapbytes(*(uint16_t *)&dec_buf[m - 2]);
	//TRACE("offset=%x\n", offset);
	//TRACE("crc=%04x\n", crc);
#endif
}

void InitNmpHdr(nmp_hdr_t* hdr, uint8_t Op, uint8_t Flags, uint16_t Len, uint16_t Group, uint8_t Seq, uint8_t Id)
{
	hdr->Op = Op;
	hdr->Flags = Flags;
	hdr->Len = swapbytes(Len);
	hdr->Group = swapbytes(Group);
	hdr->Seq = Seq;
	hdr->Id = Id;
}

typedef struct nlip_hdr
{
	uint8_t type[2];
} nlip_hdr_t;

// Receive nmp packets
/*
	offset 0:    0x06 0x09
	== = Begin base64 encoding == =
	offset 2 : <16 - bit packet - length>
	offset ? : <body>
	offset ? : <crc16> (if final frame)
	== = End base64 encoding == =
	offset ? : 0x0a (newline)

	All subsequent frames have the following format :

	offset 0:    0x04 0x14
	== = Begin base64 encoding == =
	offset 2 : <body>
	offset ? : <crc16> (if final frame)
	== = End base64 encoding == =
	offset ? : 0x0a (newline)
*/
void recv_packets(unsigned char *data, int size)
{
	long count = 0;		// NMP packet counter
	long j = 0;			// file data[] offset
	unsigned int len = 0;
	BASEFILE bf;

	TRACE("recv_packets()\n");
	bf.InitWriteFile("recv.img");

	while (j <= size)
	{
		if ((j + 515) < size)
		{
			MEM_Trace(&data[j], 515, 0L);
		}
		else
		{
			MEM_Trace(&data[j], size - j, 0L);
		}
		count++;	// increment count of packets
		TRACE("rx packet #%d\n", count);
		int k = 0;		// accumulated decoded base64 data offset in buf[]
		do
		{
			bool flag;
			nlip_hdr_t *nlip_hdr = (nlip_hdr_t*)&data[j];
			TRACE("rx: %02x %02x\n", nlip_hdr->type[0], nlip_hdr->type[1]);
			if (nlip_hdr->type[0] == 0x06 && nlip_hdr->type[1] == 0x09)
			{
				flag = true;	// Initial frame
				TRACE("Initial frame\n");
			}
			else if (nlip_hdr->type[0] == 0x04 && nlip_hdr->type[1] == 0x14)
			{
				flag = false;	// Continuation frame
				TRACE("Continuation frame\n");
			}
			else
			{
				printf("Bad NLIP header type bytes!\n");
			}
			j += sizeof(nlip_hdr_t);
			int i = 0;
			while (data[j] != 0x0a)
			{
				enc_str[i++] = data[j++];
			}
			enc_str[i] = '\0';
			TRACE("rx: '%s'\n", enc_str);
			j += 1;
			TRACE("rx: <LF>\n");
			int m;
			//m = base64_decode_len(enc_str);
			m = base64_decode(enc_str, &dec_buf[k]);
			MEM_Trace(&dec_buf[k], m, 0L);
			if (flag)
			{
				// NMP total length of decoded packet data (in CBOR format) (2-bytes)
				len = swapbytes(*(uint16_t*)&dec_buf[k+0]);
				TRACE("nmp len=%04x\n", len);
				// NMP header (8-bytes)
				nmp_hdr_t *hdr = (nmp_hdr_t *)&dec_buf[k+2];
				TRACE("hdr len=%04x\n", swapbytes(hdr->Len));
				TraceNmpHdr(hdr);
			}
			// Accumulate the decoded base64 data in dec_buf[].
			k += m;
		}
		while (k < len);

		// Parse NMP packet payload as CBOR data map.
		MEM_Trace(dec_buf, len, 0L);
		unsigned int offset = cbor_parse(&dec_buf[10], len-10);
		unsigned int remaining = len-8-offset;
		TRACE("remaining=%x ", remaining);
		uint16_t crc = swapbytes(*(uint16_t*)&dec_buf[len]);
		TRACE("offset=%x crc=%04x\n", offset, crc);

		// Determine if this is the last packet based on cbor map key pairs.
		cbor_token_t* token = cbor_get_key("data");
		unsigned int n = token->int_value;	// total number of bytes of data[] received
		TRACE("data[%d]", n);
		MEM_Trace(token->bytes_value, n, 0L);
		bf.WritetoFile((DWORD)n, (BYTE*)token->bytes_value);
		if (count == 1)
		{
			// First packet has the expected total number of bytes in the image.
			token = cbor_get_key("len");
			total_len = token->int_value;
			TRACE("total_len=%d\n", total_len);
		}
		token = cbor_get_key("off");
		unsigned int off = token->int_value;
		TRACE("off=%d\n", off);
		if ((off + n) == total_len)
		{
			printf("Last packet\n");
			break;
		}
	}

	bf.CloseFile();
	TRACE("recv_packets done\n");
}

// Transmit a nmp packet request
int send_nmp_packet(uint8_t Op, uint16_t Group, uint8_t Id, unsigned int offset, int nbytes, unsigned char* bytes_data)
{
	TRACE("send_nmp_packet Op=%x Group=%x Id=%x offset=%04x nbytes=%x\n", Op, Group, Id, offset, nbytes);
	// Start of CBOR encoded data:
	CBOR cbor(&buf[2 + sizeof(nmp_hdr_t)], sizeof(buf) - 2 - sizeof(nmp_hdr_t));
	cbor.put_map(5);
	cbor.put_text("data");
	cbor.put_bytes(&bytes_data[offset], nbytes); // size of data (in bytes);
	cbor.put_text("image");
	cbor.put_int(0);
	if (offset == 0)
	{
		cbor.put_text("len");
		cbor.put_int(total_len);
	}
	cbor.put_text("off");
	cbor.put_int(offset);
	if (offset == 0)
	{
		cbor.put_text("sha");
		cbor.put_bytes(bytes_sha, sizeof(bytes_sha));
	}
	int base = cbor.base;

	// NMP packet length (2-bytes):
	*(uint16_t*)&buf[0] = swapbytes(base + sizeof(nmp_hdr_t) + 2);
	//MEM_Trace(&buf[0], sizeof(uint16_t), 0L);
	// NMP packet header (8-bytes):
	nmp_hdr_t* hdr = (nmp_hdr_t*)&buf[2];
	InitNmpHdr(hdr, Op, 0x00, base, Group, seq, Id);
	TRACE("tx: ");
	TraceNmpHdr(hdr);
	// NMP packet CRC16 (2-bytes):
	unsigned int crc = 0;
	//crc = crc16_buf(&buf[2],base+sizeof(nmp_hdr_t), crc);
	crc = crc_xmodem(&buf[2], base+sizeof(nmp_hdr_t));
	//TRACE("crc16: %04x\n", crc);
	*(uint16_t *)cbor.p = swapbytes(crc);
	int len = 2 + sizeof(nmp_hdr_t) + base + 2;
	//MEM_Trace(buf, len, 0L);
	// Send NMP packet with NLIP protocol.
	base = base64_encode(buf, len, enc_str, 1);
	//TRACE("enc len=%d:\n", base);
	//MEM_Trace((unsigned char*)enc_str, base, 0L);
	unsigned char *ptr = buf;
	int j = 0;
	for (int i=0,k=MGMT_NLIP_MAX_FRAME-3; i < base; i+=k)
	{
		// Break up packet frames that are <=127 bytes:
		if ((i+k) >= base)
			k = base-i;
		if (i == 0)
		{
			*ptr++ = 0x06;
			*ptr++ = 0x09;
		}
		else
		{
			*ptr++ = 0x04;
			*ptr++ = 0x14;
		}
		strncpy((char*)ptr, &enc_str[i], k);
		ptr += k;
		*ptr++ = 0x0a;
		j += 2 + k + 1;
	}
#ifdef SEND_IMAGE
	send_buf(buf, j);
	//ADG! TMR_Delay(2);
#endif
	//MEM_Trace(buf, j, 0L);
	seq += 1;
	return j;
}

unsigned char *read_image(char* filename)
{
	BASEFILE bf;
	long size;
	BYTE buf[BUFSIZE];
	int buflen;
	bool bDone = false;
	
	printf("read_image(%s) ", filename);
	/* Determine the size of the image file. */
	bf.InitReadFile(filename);
	if (!bf.IsFile())
	{
		printf("\nFile_Not_Found!\n");
		return NULL;
	}
	size = bf.Filesize();
	printf("size=%08x bytes\n", size);
	unsigned char* data = new unsigned char[size];
	if (data == NULL)
	{
		printf("\nOut of memory!\n");
		return NULL;
	}

	long j = 0;
	while (size)
	{
		if (size > BUFSIZE)
		{
			bf.ReadfromFile(BUFSIZE, buf);
			//MEM_Trace(buf, BUFSIZE, j);
			memcpy(&data[j], buf, BUFSIZE);
			size -= BUFSIZE;
			j += BUFSIZE;
		}
		else
		{
			bf.ReadfromFile(size, buf);
			//MEM_Trace(buf, size, j);
			memcpy(&data[j], buf, size);
			j += size;
			size = 0;
		}
	}
	printf("read_image done\n");
	return data;
}

int send_hci_cmd(unsigned char* cmd, int txlen, int rxlen)
{
	TRACE("tx hci cmd:\n");
	memcpy(buf, cmd, txlen);
	MEM_Trace(cmd, txlen, 0L);
	send_buf(buf, txlen);
	int n = rxlen - recv_buf(rxlen, -1);
	TRACE("rx hci resp:");
	if (n == 0)
	{
		printf("HCI Timeout!\n");
	}
	else
	{
		TRACE(" n=%d\n", n);
		MEM_Trace(buf, n, 0L);
		TMR_Delay(1);
	}
	return n;
}

int nmp_format_buf(uint8_t Op, uint16_t Group, uint8_t Id, CBOR* cbor)
{
	int m;
	int len;
	nmp_hdr_t* hdr;

	// Set NMP packet length (2-bytes):
	*(uint16_t*)&buf[0] = swapbytes(cbor->base + sizeof(nmp_hdr_t) + 2);
	MEM_Trace(&buf[0], sizeof(uint16_t), 0L);

	// Set NMP packet header (8-bytes):
	hdr = (nmp_hdr_t*)&buf[2];
	InitNmpHdr(hdr, Op, 0x00, cbor->base, Group, seq, Id);

	// Set NMP packet CRC16 (2-bytes):
	unsigned int crc = 0;
	crc = crc_xmodem(&buf[2], cbor->base + sizeof(nmp_hdr_t));
	TRACE("crc16: %04x\n", crc);
	*(uint16_t*)cbor->p = swapbytes(crc);
	len = 2 + sizeof(nmp_hdr_t) + cbor->base + 2;
	MEM_Trace(buf, len, 0L);

	// Encode NMP packet with base64.
	cbor->base = base64_encode(buf, len, enc_str, 1);
	TRACE("enc len=%d:\n", cbor->base);
	MEM_Trace((unsigned char*)enc_str, cbor->base, 0L);

	// Format packet for NLIP protocol by breaking into frames as needed.
	unsigned char* ptr = buf;
	int j = 0;
	for (int i = 0, k = MGMT_NLIP_MAX_FRAME - 3; i < cbor->base; i += k)
	{
		// Break up packet frames that are <=127 bytes:
		if ((i + k) >= cbor->base)
			k = cbor->base - i;
		if (i == 0)
		{
			*ptr++ = 0x06;
			*ptr++ = 0x09;
		}
		else
		{
			*ptr++ = 0x04;
			*ptr++ = 0x14;
		}
		strncpy((char*)ptr, &enc_str[i], k);
		ptr += k;
		*ptr++ = 0x0a;
		j += 2 + k + 1;
	}
	return j;
}

int nmp_imagelist(void)
{
	printf("\nImage List:\n");
	unsigned char NMP_ImageList[] = {
		0x06,0x09,0x41,0x41,0x73,0x41,0x41,0x41, 0x41,0x42,0x41,0x41,0x46,0x43,0x41,0x4B,
		0x44,0x31,0x4D,0x77,0x3D,0x3D,0x0A };
	int k = base64_decode((char*)&NMP_ImageList[2], dec_buf);
	MEM_Trace(dec_buf, k, 0L);

	// Format the NMP Image List command buf[].
	CBOR cbor(&buf[2 + sizeof(nmp_hdr_t)], sizeof(buf) - 2 - sizeof(nmp_hdr_t));
	cbor.put_map(0);
	int base = cbor.base;
	int j = nmp_format_buf(0x00, 0x0001, 0x00, &cbor);
#ifdef SEND_IMAGE
	// Transmit the command buf[].
	send_buf(buf, j);
	TMR_Delay(1);
#endif
	//MEM_Trace(buf, j, 0L);
	seq += 1;

	// Receive the response buf[].
	int n = 100 - recv_buf(100, '\r');
	//MEM_Trace(buf, n, 0L);

	// Decode the base64 response body.
	int m = base64_decode((char*)&buf[2], dec_buf);
	//MEM_Trace(dec_buf, m, 0L);
	int len = (dec_buf[0] << 8) + dec_buf[1];
	//TRACE("nmp len=%04x\n", len);
	struct nmp_packet* pkt = (struct nmp_packet*)&dec_buf[2];
	nmp_hdr_t *hdr = &pkt->hdr;
	TRACE("rx: ");
	TraceNmpHdr(hdr);
	cbor_parse(pkt->data, swapbytes(hdr->Len));
	//TRACE("crc=%02x%02x\n", dec_buf[m - 2], dec_buf[m - 1]);

	// Find keys pairs describing the image list (ignoring array of maps). 
	cbor_token_t* token;
	if ((token = cbor_get_key("slot")) != NULL)
		printf("slot:%d\n", token->int_value);
	if ((token = cbor_get_key("version")) != NULL)
		printf("version:'%.*s')\n", token->int_value, token->text_value);
	return 0;
}

int nmp_reset(void)
{
	printf("\nReset:\n");
	unsigned char NMP_Reset[] = {
		0x06,0x09,0x41,0x41,0x73,0x43,0x41,0x41, 0x41,0x42,0x41,0x41,0x42,0x43,0x42,0x61,
		0x43,0x36,0x46,0x51,0x3D,0x3D,0x0A };
	int k = base64_decode((char*)&NMP_Reset[2], dec_buf);
	MEM_Trace(dec_buf, k, 0L);

	// Format the NMP Reset command buf[].
	CBOR cbor(&buf[2 + sizeof(nmp_hdr_t)], sizeof(buf) - 2 - sizeof(nmp_hdr_t));
	cbor.put_map(0);
	int base = cbor.base;
	int j = nmp_format_buf(0x02, 0x0000, 0x05, &cbor);
#ifdef SEND_IMAGE
	// Transmit the command buf[].
	send_buf(buf, j);
	TMR_Delay(1);
#endif
	//MEM_Trace(buf, j, 0L);
	seq += 1;
	
	// Receive the response buf[].
	int n = 100 - recv_buf(100, '\r');
	//MEM_Trace(buf, n, 0L);

	// Decode the base64 response body.
	int m = base64_decode((char*)&buf[2], dec_buf);
	//MEM_Trace(dec_buf, m, 0L);
	int len = (dec_buf[0] << 8) + dec_buf[1];
	//TRACE("nmp len=%04x\n", len);
	struct nmp_packet* pkt = (struct nmp_packet*)&dec_buf[2];
	nmp_hdr_t *hdr = &pkt->hdr;
	TRACE("rx: ");
	TraceNmpHdr(hdr);
	cbor_parse(pkt->data, swapbytes(hdr->Len));
	//TRACE("crc=%02x%02x\n", dec_buf[m - 2], dec_buf[m - 1]);

	// Wait 5.5 seconds after a software reset for the Bootloader to startup.
	TMR_Delay(55);

	// Check if the Bootloader signon has been received.
	n = 100 - recv_buf(100, -1);
	buf[n] = '\0';
	printf("%s", (char *)buf);
	return 0;
}

int nmp_upload(char *filename)
{
	/* Dumb resource intensive way is to read the whole file into a large data buffer! */
	unsigned char* data = read_image(filename);
	if (data == NULL)
	{
		return -1;;
	}

	/* Send NMP packets broken up into several NLIP serial chunks that have the NMP packet
	** total length and a header in the first chunk, followed by a CBOR encoded map and a
	** crc16-ccitt value of the unencode packet in the last serial chunk.
	** Note that the first NMP packet is slighly shorter to accommodate some extra CBOR
	** encoded values that describe the total length of the image, etc. Subsequent packets
	** are longer and the final packet is variable based on the exact remainder needed for
	** the last fragment of the image.
	*/
	printf("\nUpload:\n");
	unsigned int offset = 0;
	int size;
	int i;
	long count = 0;
	BASEFILE bf;
	// Capture all serial output to a file to be useverify correct operation.
	bf.InitWriteFile("send.bin");

	size = 0x0129;
	if ((offset + size) <= total_len)
	{
		count++;	// increment count of packets
		printf("tx #%03d off:%x\n", count, offset);
		i = send_nmp_packet(0x02, 0x0001, 0x01, offset, size, data);
		bf.WritetoFile((DWORD)i, (BYTE*)buf);
		recv_nmp_resp();
		offset += size;

		// Change to larger size packet.
		size = 0x0154;
		while ((offset + size) <= total_len)
		{
			count++;	// increment count of packets
			printf("tx #%d off:%x\n", count, offset);
			i = send_nmp_packet(0x02, 0x0001, 0x01, offset, size, data);
			bf.WritetoFile((DWORD)i, (BYTE*)buf);
			recv_nmp_resp();
			offset += size;
		}
	}
	if ((offset + size) > total_len)
	{
		size = total_len - offset;
		count++;	// increment count of packets
		printf("tx #%d off:%x\n", count, offset);
		i = send_nmp_packet(0x02, 0x0001, 0x01, offset, size, data);
		bf.WritetoFile((DWORD)i, (BYTE*)buf);
		recv_nmp_resp();
		offset += size;
	}

	bf.CloseFile();
	delete data;
	return 0;
}

int check_file(char* filename)
{
	TRACE("check_file(%s) ", filename);

	BASEFILE bf;

	bf.InitReadFile(filename);

	if (!bf.IsFile())
	{
		printf("\nFile_Not_Found!\n");
		return -1;
	}
	long size = bf.Filesize();
	printf("size=%08x bytes\n", size);
	unsigned char* data = new unsigned char[size];
	if (data == NULL)
	{
		printf("\nOut of memory!\n");
		return -2;
	}

	long n = size;
	long j = 0;
	while (n)
	{
		if (n > BUFSIZE)
		{
			bf.ReadfromFile(BUFSIZE, buf);
			memcpy(&data[j], buf, BUFSIZE);
			n -= BUFSIZE;
			j += BUFSIZE;
		}
		else
		{
			bf.ReadfromFile(n, buf);
			memcpy(&data[j], buf, n);
			n = 0;
		}
	}

	recv_packets(data, size);

	delete data;

	bf.CloseFile();
	TRACE("check_file done");
	return 0;
}

int main(int argc, char* argv[])
{
	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init("winlib.log");

	TMR_Init(100);	// 10ms timebase
	COM_Init(26, 115200);
	TMR_Delay(5);

#ifdef SEND_IMAGE
	if (com->IsConnected())
	{
		printf("Comport is connected!\n");

		unsigned char HCI_SetUpload[] = {
			0x01,0x04,0xfc,0x01,0x01 };

		unsigned char HCI_Reset[] = {
			0x01,0x02,0xfc,0x00 };

		int n;
		if ((n = send_hci_cmd(HCI_SetUpload, sizeof(HCI_SetUpload), 20)) != 0 &&
			(n = send_hci_cmd(HCI_Reset, sizeof(HCI_Reset), 40)) != 0)
		{
			// No specific response expected from the HCI_Reset, however the 'nmgr>' debug text 
			// is output, once the Bootloader is ready to accept NMP requests.
			if (!strncmp("nmgr>\n", (char*)&buf[n - 6], 6))
			{
				printf("nmp ready\n");
			}
		}
		else
		{
			TMR_Delay(55);
			/* Ignore any spurious data sent from the bootloader. */
			recv_buf(100, -1);
			TRACE("Send <CR>\n");
			/* Sending a <CR> can help in resynchronizing the NMP serial link. */
			buf[0] = 0x0d;
			send_buf(buf, 1);
			TMR_Delay(1);
		}
	}
#endif

	nmp_imagelist();

	nmp_upload("blehci.img");

	//check_file("send.bin");

#ifdef SEND_IMAGE
	nmp_imagelist();
	nmp_reset();
#endif

	COM_Term();
	TMR_Term();

	// Close capture log file.
	LOG_Term();

	return 0;
}
#endif
