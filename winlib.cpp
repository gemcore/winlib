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
#define MDM_TESTING

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
and input/output functions _inbyte() and _outbyte().

the prototypes of the input/output functions are:
int _inbyte(unsigned short timeout); // msec timeout
void _outbyte(int c);

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
#define TRANSMIT_XMODEM_1K

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

#define bufsize 2000

circbuf rxbuf(bufsize);
circbuf txbuf(bufsize);

/* Recv return result codes. */
#define MDM_PROCESSING       1
#define MDM_SUCCESS          0
#define MDM_EXIT            -1
#define MDM_FILE_NOT_FOUND  -2
#define MDM_ERROR           -3
#define MDM_TIMEOUT         -5
#define MDM_ABORTED        -12

class Recv : public ActiveObject
{
public:
	int Result;
	Event e;

	Recv()
	{
		//TRACE("Recv() this=%08x\n", this);
	}

	~Recv()
	{
		//TRACE("~Recv() this=%08x\n", this);
	}

	char *getname() { return "Recv"; }

	int Begin(void)
	{
		Result = MDM_PROCESSING;
		/* Run thread. */
		Resume();
		return Result;
	}

	int End(void)
	{
		/* Wait for result from processing. */
		return WaitResult();
	}

	int GetResult() { return Result; }
	int WaitResult() { e.Wait(); return GetResult(); }

	int getbyte(void);
	void putbyte(unsigned char b);
	int _inbyte(int timeout);
	void _outbyte(unsigned char b);
	void _flushinput(void);
	int Process(unsigned char *dest, int destsz);

private:
	void InitThread() { }
	void Run();
	void FlushThread() { e.Release(); }
};

void Recv::Run()
{
	BOOL bSuccess = true;

	/* Define a processing timer. */
	Tmr tmr = TMR_New();
	int Timeout = TMR_TIMEOUT_MAX;
	TMR_Start(tmr, Timeout);
	printf("%s: Run\n", getname());

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
		unsigned char *bufptr = (unsigned char *)malloc(bufsize);
		int st = Process(bufptr, bufsize);
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

	printf("%s: Run rc=%d\n", getname(), Result);
	TMR_Stop(tmr);
	TMR_Delete(tmr);
	e.Release();
}

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

int Recv::_inbyte(int timeout)
{
	int c = -1;
	for (int count = timeout; count > 0 && ((c = getbyte()) == -1); count--)
	{
		Sleep(1);
	}
	return c;
}

void Recv::_outbyte(unsigned char b)
{
	putbyte(b);
	Sleep(1);
}

void Recv::_flushinput(void)
{
	while (_inbyte(((DLY_1S) * 3) >> 1) >= 0)
		;
}

int Recv::Process(unsigned char *dest, int destsz)
{
	unsigned char xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	unsigned char *p;
	int bufsz, crc = 0;
	unsigned char trychar = 'C';
	unsigned char packetno = 1;
	int i, c, len = 0;
	int retry, retrans = MAXRETRANS;

	for (;;)
	{
		for (retry = 0; retry < 16; ++retry)
		{
			if (trychar) 
				_outbyte(trychar);
			if ((c = _inbyte((DLY_1S) << 1)) >= 0)
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
					_flushinput();
					_outbyte(ACK);
					TRACE("%s: ACK\n", getname());
					return len; /* normal end */
				case CAN:
					if ((c = _inbyte(DLY_1S)) == CAN)
					{
						_flushinput();
						_outbyte(ACK);
						TRACE("%s: ACK\n", getname());
						return -1; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		if (trychar == 'C')
		{
			trychar = NAK; 
			continue;
		}
		_flushinput();
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		TRACE("%s: CAN\n", getname());
		return -2; /* sync error */

	start_recv:
		TRACE("%s: bufsz=%d\n", getname(), bufsz);
		if (trychar == 'C')
			crc = 1;
		trychar = 0;
		p = xbuff;
		*p++ = c;
		for (i = 0; i < (bufsz + (crc ? 1 : 0) + 3); ++i)
		{
			if ((c = _inbyte(DLY_1S)) < 0)
				goto reject;
			*p++ = c;
		}

		if (xbuff[1] == (unsigned char)(~xbuff[2]) &&
			(xbuff[1] == packetno || xbuff[1] == (unsigned char)packetno - 1) &&
			check(crc, &xbuff[3], bufsz))
		{
			if (xbuff[1] == packetno)
			{
				register int count = destsz - len;
				if (count > bufsz)
					count = bufsz;
				if (count > 0)
				{
					memcpy(&dest[len], &xbuff[3], count);
					len += count;
					TRACE("%s: PACKET\n", getname());
				}
				++packetno;
				retrans = MAXRETRANS + 1;
			}
			if (--retrans <= 0)
			{
				_flushinput();
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				TRACE("%s: CAN\n", getname());
				return -3; /* too many retry error */
			}
			_outbyte(ACK);
			TRACE("%s: ACK\n",getname());
			continue;
		}
	reject:
		_flushinput();
		_outbyte(NAK);
		TRACE("%s: NAK\n", getname());
	}
}

class Send : public ActiveObject
{
public:
	int Result;
	Event e;

	Send()
	{
		//TRACE("Send() this=%08x\n", this);
	}

	~Send()
	{
		//TRACE("~Send() this=%08x\n", this);
	}

	char *getname() { return "Send"; }

	int Begin(void)
	{
		Result = MDM_PROCESSING;
		/* Run thread. */
		Resume();
		return Result;
	}

	int End(void)
	{
		/* Wait for result from processing. */
		return WaitResult();
	}

	int GetResult() { return Result; }
	int WaitResult() { e.Wait(); return GetResult(); }

	int getbyte(void);
	void putbyte(unsigned char b);
	int _inbyte(int timeout);
	void _outbyte(unsigned char b);
	void _flushinput(void);
	int Process(unsigned char *dest, int destsz);

private:
	void InitThread() { }
	void Run();
	void FlushThread() { e.Release(); }
};

void Send::Run()
{
	BOOL bSuccess = true;

	/* Define a processing timer. */
	Tmr tmr = TMR_New();
	int Timeout = TMR_TIMEOUT_MAX;
	TMR_Start(tmr, Timeout);
	printf("%s: Run\n", getname());

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
		unsigned char *bufptr = (unsigned char *)malloc(bufsize);
		memset(bufptr, 0xFF, bufsize);
		int st = Process(bufptr, bufsize);
		if (st < 0)
		{
			printf("%s: Failure, status=%d\n", getname(), st);
			Result = MDM_ERROR;
			bSuccess = false;
		}
		else
		{
			printf("%s: Success, %d bytes\n", getname(), st);
			Result = MDM_SUCCESS;
			bSuccess = false;
		}
	}

	printf("%s: Run rc=%d\n", getname(), Result);
	TMR_Stop(tmr);
	TMR_Delete(tmr);
	e.Release();
}

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

int Send::_inbyte(int timeout)
{
	int c = -1;
	for (int count = timeout; count > 0 && ((c = getbyte()) == -1); count--)
	{
		Sleep(1);
	}
	return c;
}

void Send::_outbyte(unsigned char b)
{
	putbyte(b);
	Sleep(1);
}

void Send::_flushinput(void)
{
	while (_inbyte(((DLY_1S) * 3) >> 1) >= 0)
		;
}

int Send::Process(unsigned char *src, int srcsz)
{
	unsigned char xbuff[1030]; /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	int bufsz, crc = -1;
	unsigned char packetno = 1;
	int i, c, len = 0;
	int retry;

	for (;;)
	{
		for (retry = 0; retry < 16; ++retry)
		{
			if ((c = _inbyte((DLY_1S) << 1)) >= 0)
			{
				switch(c)
				{
				case 'C':
					crc = 1;
					goto start_trans;
				case NAK:
					crc = 0;
					goto start_trans;
				case CAN:
					if ((c = _inbyte(DLY_1S)) == CAN)
					{
						_outbyte(ACK);
						_flushinput();
						return -1; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		_flushinput();
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
			c = srcsz - len;
			if (c > bufsz)
				c = bufsz;
			if (c > 0)
			{
				memset(&xbuff[3], 0, bufsz);
				memcpy(&xbuff[3], &src[len], c);
				if (c < bufsz)
					xbuff[3 + c] = SUB;
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
					for (i = 0; i < bufsz + 4 + (crc ? 1 : 0); ++i)
					{
						_outbyte(xbuff[i]);
					}
					if ((c = _inbyte(DLY_1S)) >= 0)
					{
						switch(c)
						{
						case ACK:
							++packetno;
							len += bufsz;
							goto start_trans;
						case CAN:
							if ((c = _inbyte(DLY_1S)) == CAN)
							{
								_outbyte(ACK);
								_flushinput();
								return -1; /* canceled by remote */
							}
							break;
						case NAK:
						default:
							break;
						}
					}
				}
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				_flushinput();
				return -4; /* xmit error */
			}
			else
			{
				for (retry = 0; retry < 10; ++retry)
				{
					_outbyte(EOT);
					if ((c = _inbyte((DLY_1S) << 1)) == ACK)
						break;
				}
				_flushinput();
				return (c == ACK)? len : -5;
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
	LOG_Init("c:\\temp\\winlib.log");

	TMR_Init(100);	// 100ms timebase

	/* Initialize the Recv & Send thread. */
	Recv *recv = new Recv();
	Send *send = new Send();

	/* Execute the Recv & Send processing. */
	recv->Begin();
	Sleep(500);
	send->Begin();
	Sleep(30000);
	send->End();
	recv->End();

	send->Kill();
	recv->Kill();

	/* Terminate the Recv & Send thread. */
	delete send;
	delete recv;

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
