// winlib.cpp : Defines the entry point for the console application.
//

#define _CRTDBG_MAP_ALLOC  
#include "stdint.h"
#include "stdafx.h"
#include "windows.h"
#include "LOG.H"		// Capture putchar & printf output to a log file
#include "TMR.H"
#include "comport.h"
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
//#define COM_TESTING
//#define TMR_TESTING
//#define MDM_TESTING
//#define ZAR_TESTING	// not working
//#define LUA_TESTING	// not working
//#define FTL_TESTING
//#define TED_TESTING
//#define B64_TESTING	// not working

#if 1
/* xScript Rx initial timeout maximum value. */
#define TMR_TIMEOUT_MAX	32000

ComPort *com = NULL;

/*
** ComPort virtual thread processing function.
*/
void ComPort::Process(void)
{
	//static int rxpacket_len = 0;
	//static int rxpacket_cnt = 0;
	static byte rxpacket[512];

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
	void MEM_Dump(uint8_t *data, uint16_t len, uint32_t base)
	{
		uint16_t i, j;

		//if (!CFG_IsTrace(DFLAG_TRC))
		//	return;

		//CON_printf("MEM: @%08x len=%04x\n",data,len);
		for (i = 0; i < len; i += 16)
		{
			printf(" %06x: ", base + i);
			for (j = 0; j < 16; j++)
			{
				if (j != 0)
				{
					if (!(j % 8))
						printf(" ");
					if (!(j % 1))
						printf(" ");
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
					if (isprint(data[i + j]))
						printf("%c", data[i + j]);
					else
						printf(".");
				}
				else
					printf(" ");
			}
			printf("\n");
		}
	}

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

	bool COM_connected(void)
	{
		return com->IsConnected();
	}

	int COM_recv_char(void)
	{
		return com->RxGetch();
	}

	long COM_recv_count(void)
	{
		return com->RxCount();
	}

	int COM_send_char(byte c)
	{
		return com->Write(c);
	}

	bool COM_send_buf(char *buf, int len)
	{
		return com->Write(buf, len);
	}

	void COM_Sleep(int ticks)
	{
		com->Sleep(ticks);
	}
}
#endif 

#ifndef TED_TESTING	//#if HAS_CLI == 0
extern "C"
{
#include "cli.h"

	//*****************************************************************************
	// This is the table that holds the command names, implementing functions, and
	// brief description.
	//*****************************************************************************
	SHELL_COMMAND g_psShellCmds[] =
	{
	{ 0, 0, 0 }
	};

	int CON_kbhit()
	{
		return 0;
	}

	int CON_getc()
	{
		return -1;
	}
}
#endif

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
		if (logger)
		{
			logger->Flush();
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
					logger->Flush();
				}
				else
				{
					printf("\n> ...\n");
					logger->Flush();
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
			COM_send_buf(pRecData->cArray, rec.iLen);
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
					COM_send_buf((char *)&buf[0], BUFSIZE);
					size -= BUFSIZE;
				}
				else
				{
					bf.ReadfromFile(size, buf);
					COM_send_buf((char *)&buf[0], size);
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
		if (logger)
		{
			logger->Flush();
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

char qflag = 0;  						// quiet mode

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

int main(int argc, char* argv[])
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

#include "bsp.h"
#include "cfg.h"
#include "term.h"
#include "evt.h"
#include "cli.h"
#include "pts.hpp"

	extern void TRM_Init();
	extern void TRM_Term();
	extern int Cmd_ted(int argc, char* argv[]);
	extern int Cmd_pic(int argc, char* argv[]);

	//*****************************************************************************
	// This function implements the "help" command.  It prints a list of available
	// commands with a brief description.
	//*****************************************************************************
	int Cmd_help(int argc, char* argv[])
	{
		SHELL_COMMAND* psEntry;

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

	bool exit_flag = false;

	int Cmd_exit(int argc, char* argv[])
	{
		exit_flag = true;
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
	{ "pic",        Cmd_pic,        "pico C" },
	{ "exit",       Cmd_exit,       "exit" },
	{ 0, 0, 0 }
	};

}

class SysTick : CTimerFunc
{
public:
	SysTick() {}
	void Func(void) { SysTickIntHandler(); }
};

int main(int argc, char* argv[])
{
	argc; // unused
	argv; // unused

	// Capture output from putchar, puts and printf macros.
	LOG_Init("c:\\temp\\term.log");
	BSP_Init();
	TMR_Init(SYSTICK_RATE_HZ);	// 100Hz timebase

	SysTick* Tick = new SysTick();
	TMR_Event(1, (CTimerEvent*)Tick, PERIODIC);
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

		if (exit_flag)
			break;
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

#if 1
#include "cbor/cbor.h"

class CBOR
{
public:
	unsigned char* data;
	unsigned char* p;
	unsigned char* q;
	long base;
	int size;

	cbor_token_t* tokens;
	int tokens_size;
	int token_cnt;

	CBOR()
	{
		TRACE("CBOR()\n");
	}

	CBOR(int n)
	{
		TRACE("CBOR(%d) ", n);
		tokens = new cbor_token_t[n];
		tokens_size = n;
		TRACE("tokens[%d] @%xH\n", n, tokens);
	}

	void Init(unsigned char* data, int size)
	{
		TRACE("Init data @%xH size=%d\n", data, size);
		this->data = data;
		this->size = size;
		token_cnt = 0;
		base = 0L;
		q = data;
		p = q;
	}

	void def_map(int m)
	{
		TRACE(" map(%d)\n", m);
		p = cbor_write_map(q, size - base, m);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void def_array(int m)
	{
		TRACE(" array(%d)\n", m);
		p = cbor_write_array(q, size - base, m);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put()
	{
		TRACE(" break\n");
		p = cbor_write_special(q, size - base, 31);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put(int n)
	{
		TRACE(" int(%d)\n", n);
		p = cbor_write_int(q, size - base, n);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put(unsigned int n)
	{
		TRACE(" uint(%u)\n", n);
		p = cbor_write_uint(q, size - base, n);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put(long long n)
	{
		TRACE(" long(%lld)\n", n);
		p = cbor_write_long(q, size - base, n);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put(unsigned long long n)
	{
		TRACE(" ulong(%llu)\n", n);
		p = cbor_write_ulong(q, size - base, n);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put(char* s)
	{
		TRACE(" text('%s')\n", s);
		p = cbor_write_text(q, size - base, s);
		MEM_Trace(q, p - q, base);
		base += p - q;
		q = p;
	}

	void put(unsigned char* bytes, int m)
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

	char* typetostr(cbor_token_type_e type);

	cbor_token_t* token;

	cbor_token_t* get_first(cbor_token_type_e type);
	cbor_token_t* get_next(void);
	cbor_token_t* get_key(char* s);
	bool istype(cbor_token_type_e type);
	bool expected_next_type(cbor_token_type_e type);

	void list(void);

	virtual int parse(void);

	~CBOR()
	{
		TRACE("~CBOR() tokens @%xH size=%d:\n", tokens, tokens_size);
		if (tokens_size > 0)
		{
			delete tokens;
		}
	}
};

//#define CBOR_PARSE_MAX_TOKENS	ARRAYSIZE(CBOR_Parser::tokens)

// Find the text key value in the array of parsed tokens and return a pointer to the next token.
cbor_token_t* CBOR::get_key(char* s)
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

cbor_token_t* CBOR::get_first(cbor_token_type_e type)
{
	for (int i = 0; i < token_cnt; i++)
	{
		if ((cbor_token_type_e)tokens[i].type == type)
		{
			return &tokens[i];
		}
	}
	return NULL;
}

cbor_token_t* CBOR::get_next(void)
{
	if (token_cnt == 0)
	{
		TRACE("get_next: No tokens[]!\n");
		return NULL;
	}
	if (token == NULL)
	{
		TRACE("get_next: NULL pointer!\n");
		return NULL;
	}
	if ((token+1) > &tokens[token_cnt-1])
	{
		TRACE("get_next: EOF\n");
		return NULL;
	}
	token += 1;
	//TRACE("get_next: %s\n", typetostr((cbor_token_type_e)token->type));
	return token;
}

char* CBOR::typetostr(cbor_token_type_e type)
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

void CBOR::list(void)
{
	printf("%d tokens found:\n", token_cnt);
	cbor_token_t* token = tokens;
	for (int i = 0; i < token_cnt; i++, token++)
	{
		printf("%2d:\t(%d)%s\n", i, token->type, typetostr((cbor_token_type_e)token->type));
	}
}

int CBOR::parse(void)
{
	unsigned int offset = 0;
	token_cnt = 0;
	long j = 0;
	while (1)
	{
		//TRACE("cbor_parse data size=%d token_cnt=%d ", size, token_cnt);
		// Build up a list of tokens that are contained in a global array.
		if ((token_cnt + 1) > tokens_size)
		{
			printf("Out of token space!\n");
			MEM_Dump(data, size, 0L);
			return 0;
		}
		cbor_token_t* token = &tokens[token_cnt++];

		offset = cbor_read_token(data, size, offset, token);
		TRACE("cbor_read_token() offset=%d token->type=%d\n", offset, token->type);
		if (token->type == CBOR_TOKEN_TYPE_INCOMPLETE) {
			TRACE(" incomplete\n");
			break;
		}
		if (token->type == CBOR_TOKEN_TYPE_ERROR) {
			printf(" error: %s\n", token->error_value);
			MEM_Dump(data, size, 0L);
			break;
		}
		if (token->type == CBOR_TOKEN_TYPE_BREAK) {
			TRACE(" break\n");
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_INT) {
			TRACE(" int(%s%d)\n", token->sign < 0 ? "-" : "", (int)token->int_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_UINT) {
			TRACE(" uint(%s%u)\n", token->sign < 0 ? "-" : "", token->int_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_LONG) {
			TRACE(" long(%s%lld)\n", token->sign < 0 ? "-" : "", (long long)token->long_value);
			MEM_Trace(&data[j], offset - j, j);
			j = offset;
			continue;
		}
		if (token->type == CBOR_TOKEN_TYPE_ULONG) {
			TRACE(" ulong(%s%llu)\n", token->sign < 0 ? "-" : "", token->long_value);
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

bool CBOR::istype(cbor_token_type_e type)
{
	return token->type == type;
}

bool CBOR::expected_next_type(cbor_token_type_e type)
{
	return (token = get_next()) != NULL && istype(type);
}


#ifdef CBOR_TESTING
#include <stdio.h>

char qflag = 0;  						// quiet mode off (output to window)

class myCBOR : public CBOR
{
public:
	myCBOR()
	{
		TRACE("myCBOR()\n");
	}

	myCBOR(int n) : CBOR(n)
	{
		TRACE("myCBOR(%d)\n", n);
	}

	~myCBOR()
	{
		TRACE("~myCBOR()\n");
	}

	int parse(void);
};

int myCBOR::parse()
{
	int offset = CBOR::parse();
	printf("parse() offset=%d\n", offset);

	// Expecting map of key pairs (text:array).
	if ((token=get_first(CBOR_TOKEN_TYPE_MAP)) == NULL)
	{
		printf(" Expected %s!\n", typetostr(CBOR_TOKEN_TYPE_MAP));
		return -1;
	}
	printf("%s[", typetostr((cbor_token_type_e)token->type));

	while (!expected_next_type(CBOR_TOKEN_TYPE_BREAK))
	{
		// Expecting text key.
		if (!istype(CBOR_TOKEN_TYPE_TEXT))
		{
			printf(" Expected key %s!\n", typetostr(CBOR_TOKEN_TYPE_TEXT));
			return -1;
		}
		printf("%s:", typetostr((cbor_token_type_e)token->type));

		// Expecting array.
		if (!expected_next_type(CBOR_TOKEN_TYPE_ARRAY))
		{
			printf(" Expected %s!\n", typetostr(CBOR_TOKEN_TYPE_ARRAY));
			return -1;
		}
		printf("%s[", typetostr((cbor_token_type_e)token->type));
		while (!expected_next_type(CBOR_TOKEN_TYPE_BREAK))
		{
			// Expecting map.
			if (!istype(CBOR_TOKEN_TYPE_MAP))
			{
				printf(" Expected map!\n");
				return -1;
			}
			printf("%s[", typetostr((cbor_token_type_e)token->type));
			while (!expected_next_type(CBOR_TOKEN_TYPE_BREAK))
			{
				// Expecting text key.
				if (!istype(CBOR_TOKEN_TYPE_TEXT))
				{
					printf(" Expected key %s!\n", typetostr(CBOR_TOKEN_TYPE_TEXT));
					return -1;
				}
				printf("%s:", typetostr((cbor_token_type_e)token->type));

				// Expecting anything (except break).
				if (expected_next_type(CBOR_TOKEN_TYPE_BREAK))
				{
					printf(" Expected key pair!\n");
					return -1;
				}
				printf("%s,", typetostr((cbor_token_type_e)token->type));
			}
			printf("]");
		}
		printf("]");
	}
	printf("]\n");
	return 0;
}

int main(int argc, char** argv)
{
	static unsigned char buffer[512];
	unsigned int size = sizeof(buffer);
	unsigned char* data = buffer;
	int base;
	unsigned int offset;
	cbor_token_t* token;

	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init(NULL);

	/* Basic parser test. */
	CBOR cbor(32);

	printf("\ncbor put:\n");
	cbor.Init(data, size);
	cbor.def_map(2);
	cbor.put("data");
	unsigned char bytes[] = { 0x01,0x02,0x03,0x04,0x05,0x07 };
	cbor.put(bytes, sizeof(bytes)); // size of data (in bytes);
	cbor.put("foo");
	cbor.put(1234);
	cbor.def_array(5);
	cbor.put(123);
	cbor.put(5);
	cbor.put((unsigned int)65535U);
	cbor.put("hello");
	cbor.put("world");
	cbor.put((long long)1231231231LL);
	cbor.put((unsigned long long)12312312311ULL);
	cbor.put("f:D");
	base = cbor.base;

	printf("\ncbor data:\n");
	MEM_Dump(data, base, 0L);

	printf("\ncbor parse:\n");
	cbor.Init(data, base);
	offset = cbor.parse();
	printf("offset=%d\n", offset);

	printf("\ncbor list:\n");
	cbor.list();

	// Do key lookups based on a 'text' token paired with the token following it.
	// Find key "data":bytes[].
	printf("\ncbor get_key:\n");
	token = cbor.get_key("data");
	unsigned int n = token->int_value;	// total number of bytes of data[] received
	printf("data[%d]:", n);
	MEM_Dump(token->bytes_value, n, 0L);

	// Find key "foo":int.
	token = cbor.get_key("foo");
	printf("foo:%d\n", token->int_value);

	/* Custom parser test. */
	myCBOR *mycbor;
	printf("\ncbor put:\n");
	mycbor = new myCBOR(32);
	mycbor->Init(data, size);
	mycbor->def_map(0);		// map
	mycbor->put("images");	//  text
	mycbor->def_array(0);	//  array
	mycbor->def_map(0);		//   map
	mycbor->put("slot");	//    text
	mycbor->put(0);			//    int
	mycbor->put("version");	//    text
	mycbor->put("1.0.7.0");	//    text
	mycbor->put("slot");	//    text
	mycbor->put(1);			//    int
	mycbor->put("version");	//    text
	mycbor->put("1.0.8.0");	//    text
	mycbor->put();			//   break
	mycbor->put();			//  break
	mycbor->put();			// break
	base = mycbor->base;

	printf("\ncbor data:\n");
	MEM_Dump(data, base, 0L);

	printf("\ncbor parse:\n");
	mycbor->Init(data, base);
	offset = mycbor->parse();
	printf("offset=%d\n", offset);

	printf("\ncbor list:\n");
	mycbor->list();
	delete mycbor;

	// Close capture log file.
	LOG_Term();

	return 0;
}
#endif

#ifdef COM_TESTING
#include "base64/base64.h"
#include "cbor/cbor.h"
#include "BASEFILE.HPP"

#define ERR_COM_TIMEOUT			-1
#define ERR_HCI_TIMEOUT			-2
#define ERR_NMP_DECODE			-3
#define ERR_NMP_HDR_BAD			-4
#define ERR_NMP_CRC_BAD			-5
#define ERR_FILE_NOT_FOUND		-6
#define ERR_LOAD_FAILURE		-7
#define ERR_NLIP_HDR_BAD		-8
#define ERR_NLIP_NO_LF			-9

char* ifile = NULL;						// upload image input filename
char ifname[80];

char oflag = 0;  						// capture send output 
char* ofile = NULL;						// capture send output filename
char ofname[80];

char tflag = 0;  						// load send input

char lflag = 0;  						// log text output 
char* lfile = NULL;						// log text output filename
char lfname[80];

int port = 0;							// COM port number

char nflag = 0;							// send HCI commands to enable NMP mode
char rflag = 0;							// send NMP reset command

BASEFILE bf;

#define BUFSIZE		512

extern "C" uint16_t crc_xmodem(const unsigned char* input_str, size_t num_bytes);

char qflag = 0;  						// quiet mode off (output to window)

unsigned char bytes_sha[32] = {
	0x6c,0x5a,0x2b,0x11,0x0d,0xdc,0xc0,0xa2, 0x91,0xf0,0xd0,0x6c,0xa0,0x0d,0x2f,0xb0,
	0xe9,0x4c,0x63,0x10,0x0a,0x00,0x72,0x14, 0x12,0xe3,0xff,0xc0,0x35,0xd0,0xbe,0x2f };

void TraceChar(int c)
{
	if (isprint(c))
	{
		TRACE("%c", c);
	}
	else if (c == '\n')
	{
		TRACE("<LF>\n");
	}
	else if (c == '\r')
	{
		TRACE("<CR>");
	}
	else
	{
		TRACE("[%02x]", c);
	}
}

/*********************************************/
/* Serial communications interface functions */
/*********************************************/
/* Receive a buffer[] of serial data bytes. */
int recv_buf_start(unsigned char* buf, int size, int count, unsigned char* pat)
{
	int len = 0;
	int delay = 50;

	TRACE("recv_buf_start(@%08x, %d, %d, [", buf, size, count);
	for (int i = 0; i < count; i++)
	{
		TRACE("%02x ", pat[i]);
	}
	TRACE("]){\n");
	while (len < count)
	{
		if (COM_recv_count() > 0)
		{
			int c;
			c = COM_recv_char();
			TraceChar(c);
			buf[len] = c;
			int ch = pat[len];
			len++;
			if (c != ch)
				len = 0;
			delay = 10;
		}
		else
		{
			if (--delay >= 0)
			{
				COM_Sleep(1);
			}
			else
			{
				printf("rx: Timeout!");
				return ERR_COM_TIMEOUT;
			}
		}
	}
	TRACE("} len=%d\n", len);
	return len;
}

int recv_buf(unsigned char *buf, int size, int count, int ch)
{
	int len = 0;
	int delay = 50;

	int m = count;
	int i = 0;
	bool done = false;
	TRACE("recv_buf(@%08x, %d, %d, %d){", buf, size, count, ch);
	while (len < count)
	{
		if (COM_recv_count() > 0)
		{
			int c;
			c = COM_recv_char();
			TraceChar(c);
			buf[len] = c;
			len++;
			if (ch != -1 && c == ch)
			{
				break;
			}
			delay = 10;
		}
		else
		{
			if (--delay >= 0)
			{
				COM_Sleep(1);
			}
			else
			{
				if (ch == -1)
				{
					break;
				}
				printf("rx: Timeout!");
				return ERR_COM_TIMEOUT;
			}
		}
	}
	TRACE("} len=%d\n", len);
	return len;
}

/* Send a buffer[] of data bytes over serial. */
void send_buf(unsigned char *buf, int n)
{
	//MEM_Trace(buf, n, 0L);
	if (!COM_connected())
	{
		return;
	}
	if (!COM_send_buf((LPSTR)buf, n))
	{
		printf("send_buf: Write error!\n");
	}
	COM_Sleep(1);
}

/* Send a pre-formatted HCI command buffer[] of data bytes over serial. */
int send_hci_cmd(unsigned char *buf, int size, unsigned char* cmd, int txlen, int rxlen)
{
	TRACE("tx hci cmd:\n");
	memcpy(buf, cmd, txlen);
	MEM_Trace(cmd, txlen, 0L);
	send_buf(buf, txlen);
	int n = recv_buf(buf, sizeof(buf), rxlen, -1);
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

/*********************************************/
/* NLIP serial type definitions.             */
/*********************************************/

/* NLIP packets sent over serial are fragmented into frames of 127 bytes or
 * fewer. This 127-byte maximum applies to the entire frame, including header,
 * CRC, and terminating newline.
 */
#define MGMT_NLIP_MAX_FRAME     127

typedef struct nlip_hdr
{
	uint8_t type[2];
} nlip_hdr_t;

typedef struct nlip_pkt_t
{
	nlip_hdr hdr;
	unsigned char data[0];
} nlip_pkt_t;

/*********************************************/
/* NMP type definitions.                     */
/*********************************************/
// Mcu Manager operation codes
typedef enum
{
	OP_READ = 0,
	OP_READ_RSP = 1,
	OP_WRITE = 2,
	OP_WRITE_RSP = 3
} nmp_op_codes_t;

// Mcu Manager groups
typedef enum
{
	GROUP_DEFAULT = 0,
	GROUP_IMAGE = 1,
	//GROUP_STATS = 2,
	//GROUP_CONFIG = 3,
	//GROUP_LOGS = 4,
	//GROUP_CRASH = 5,
	//GROUP_SPLIT = 6,
	//GROUP_RUN = 7,
	GROUP_FS = 8
	//GROUP_PERUSER = 64,
} nmp_groups_t;

// Default manager command IDs
typedef enum
{
	//DEFAULT_ID_ECHO = 0,
	//DEFAULT_ID_CONS_ECHO_CTRL = 1,
	//DEFAULT_ID_TASKSTATS = 2,
	//DEFAULT_ID_MPSTATS = 3,
	//DEFAULT_ID_DATETIME_STR = 4,
	DEFAULT_ID_RESET = 5
} nmp_default_ids_t;

// Image manager command IDs
typedef enum
{
    IMAGE_ID_STATE = 0,
	IMAGE_ID_UPLOAD = 1,
	//IMAGE_ID_FILE = 2
	//IMAGE_ID_CORELIST = 3,
	//IMAGE_ID_CORELOAD = 4,
	//IMAGE_ID_ERASE = 5,
	//IMAGE_ID_ERASE_STATE = 6
} nmp_image_ids_t;

// Stats manager command IDs
//typedef enum
//{
//    STATS_ID_READ = 0,
//    STATS_ID_LIST = 1
//} nmp_stats_ids_t;

// Config manager command IDs
//typedef enum
//{
//	CONFIG_ID_CONFIG = 0
//} nmp_config_ids_t;

// Logs manager command IDs
//typedef enum
//{
//    LOGS_ID_READ = 0,
//	LOGS_ID_CLEAR = 1,
//	LOGS_ID_APPEND = 2,
//	LOGS_ID_MODULE_LIST = 3,
//	LOGS_ID_LEVEL_LIST = 4,
//	LOGS_ID_LOGS_LIST = 5
//} nmp_logs_ids_t;

// FS manager command IDs
typedef enum
{
	FS_ID_FILE = 0
} nmp_fs_ids_t;

typedef struct nmp_hdr
{
	unsigned char Op;
	unsigned char Flags;
	unsigned short Len;
	unsigned short Group;
	unsigned char Seq;
	unsigned char Id;
} nmp_hdr_t;

typedef struct nmp_pkt
{
	nmp_hdr_t hdr;
	unsigned char data[0];
} nmp_pkt_t;

#define swapbytes(n)	((n&0xFF)<<8)+(n>>8)

long total_len = 0;				// total length of image file
unsigned char seq = 0x01;		// next nmp sequence number to send

CBOR cbor(16);					// CBOR token parsing storage array

void TraceNmpHdr(nmp_hdr_t* hdr)
{
	MEM_Trace((unsigned char*)hdr, sizeof(nmp_hdr_t), 2L);
	TRACE(" Op:%d Flags:%d Len:%d Group:%d Seq:%d Id:%d\n",
		hdr->Op, hdr->Flags, swapbytes(hdr->Len), swapbytes(hdr->Group), hdr->Seq, hdr->Id);
}


int recv_nmp_rsp(unsigned char *dec, int size, uint8_t Op, uint16_t Group, uint8_t Id)
{
	if (!COM_connected())
	{
		return 0;
	}

	unsigned char enc[256];
	unsigned char pat[2] = { 0x06, 0x09 };

	// Find the start of the NLIP packet:
	int len = recv_buf_start(enc, sizeof(enc), sizeof(pat), pat);
	if (len < 0)
	{
		return len;
	}

	// Receive body of the NLIP packet (ie. ascii text that is terminated with a <LF>):
	len = recv_buf(enc, sizeof(enc), sizeof(enc)-1, '\r');
	if (len < 1)
	{
		return -1;
	}
	enc[len-1] = '\0';	// remove anything after the trailing <LF>
	TRACE("rx: '%s'\n", &enc[0]);
	int m;
	//m = base64_decode_len((char*)enc);
	m = base64_decode((char*)enc, dec);
	if (m <= 0)
	{
		printf("rx: Decode failure!\n");
		return ERR_NMP_DECODE;
	}
	// NMP header (8-bytes):
	int k = swapbytes(*(uint16_t*)&dec[0]);
	MEM_Trace(&dec[2], k, 0L);
	TRACE("nmp len=%04x ", k);
	struct nmp_pkt* pkt = (struct nmp_pkt*)&dec[2];
	nmp_hdr_t* hdr = &pkt->hdr;
	len = swapbytes(hdr->Len);
	TRACE("hdr len=%04x\n", len);
	TRACE("rx: ");
	TraceNmpHdr(hdr);
	if (hdr->Flags != 0x00 || hdr->Op != Op || swapbytes(hdr->Group) != Group || hdr->Id != Id)
	{
		printf("rx: Bad hdr! Op:%d Flags:%d Len:%d Group:%d Seq:%d Id:%d\n",
			hdr->Op, hdr->Flags, len, swapbytes(hdr->Group), hdr->Seq, hdr->Id);
		return ERR_NMP_HDR_BAD;
	}
	uint16_t crc = crc_xmodem(&dec[2], k-2);
	if (swapbytes(*(uint16_t*)&dec[2 + k-2]) != crc)
	{
		printf("rx: Bad crc!\n");
		return ERR_NMP_CRC_BAD;
	}
	cbor.Init(pkt->data, len);
	int remaining = len - cbor.parse();
	TRACE("remaining=%d\n", remaining);
	return remaining;
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

unsigned char* load_file(char* filename, long* size)
{
	BASEFILE bf;

	printf("Loading bytes from file '%s'", filename);
	if (size != NULL)
	{
		*size = 0L;
	}
	bf.InitReadFile(filename);
	if (!bf.IsFile())
	{
		printf("\nFile Not Found!\n");
		return NULL;
	}
	long m = bf.Filesize();
	printf(" size=%d bytes\n", m);
	unsigned char* data = new unsigned char[m];
	if (data == NULL)
	{
		printf("\nOut of memory!\n");
		return NULL;
	}

	long n = m;
	long j = 0;
	while (n)
	{
		unsigned char buf[BUFSIZE];
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
	bf.CloseFile();

	if (size != NULL)
	{
		*size = m;
	}
	return data;
}

// Simulated receiving of the saved data stream that was uploaded previously.
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
int download(char *fndata)
{
	unsigned char dec[520];
	long count = 0;		// NMP packet counter
	long j = 0;			// file data[] offset
	unsigned int len = 0;
	char fnimage[80];

	TRACE("download(%s)\n", fndata);
	strcpy(fnimage, fndata);
	char *p = strchr(fnimage, '.');
	if (p)
	{
		p++;
		strcpy(p, "img");
	}
	printf("Output image to file '%s'\n", fnimage);
	BASEFILE bf;
	bf.InitWriteFile(fnimage);
	if (!bf.IsFile())
	{
		printf("\nFile Not Found!\n");
		return ERR_FILE_NOT_FOUND;
	}

	long size;
	unsigned char* data = load_file(fndata, &size);
	if (data == NULL)
	{
		printf("\nLoad data failed!\n");
		return ERR_LOAD_FAILURE;
	}
	printf("Download:");

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
		printf(!(count++ % 32) ? "\n." : ".");
		int k = 0;		// accumulated decoded base64 data offset in buf[]
		do
		{
			bool flag;
			nlip_hdr_t *nlip_hdr = (nlip_hdr_t*)&data[j];
			if (nlip_hdr->type[0] == 0x06 && nlip_hdr->type[1] == 0x09)
			{
				flag = true;	// Initial frame
			}
			else if (nlip_hdr->type[0] == 0x04 && nlip_hdr->type[1] == 0x14)
			{
				flag = false;	// Continuation frame
			}
			else
			{
				printf("Bad NLIP header type bytes!\n");
				MEM_Dump((unsigned char*)nlip_hdr, sizeof(nlip_hdr_t), j);
				delete data;
				bf.CloseFile();
				return ERR_NLIP_HDR_BAD;
			}
			j += sizeof(nlip_hdr_t);
			char enc[520];
			int i = 0;
			while (i < (sizeof(enc)-1) && data[j] != 0x0a)
			{
				enc[i++] = data[j++];
			}
			if (i >= (sizeof(enc) - 1))
			{
				printf("No <LF> found!\n");
				delete data;
				return ERR_NLIP_NO_LF;
			}
			enc[i] = '\0';
			TRACE("rx: %02x %02x '%s'<LF>\n", nlip_hdr->type[0], nlip_hdr->type[1], enc);
			j += 1;
			int m;
			//m = base64_decode_len(enc);
			m = base64_decode(enc, &dec[k]);
			MEM_Trace(&dec[k], m, 0L);
			if (flag)
			{
				// NMP total length of decoded packet data (in CBOR format) (2-bytes)
				len = swapbytes(*(uint16_t*)&dec[k+0]);
				TRACE("nmp len=%04x\n", len);
				// NMP header (8-bytes)
				nmp_hdr_t *hdr = (nmp_hdr_t *)&dec[k+2];
				TRACE("hdr len=%04x\n", swapbytes(hdr->Len));
				TraceNmpHdr(hdr);
			}
			// Accumulate the decoded base64 data in dec[].
			k += m;
		}
		while (k < len);

		// Parse NMP packet payload as CBOR data map.
		MEM_Trace(dec, len, 0L);
		cbor.Init(&dec[10], len-10);
		int offset = cbor.parse();
		unsigned int remaining = len-8-offset;
		TRACE("remaining=%x ", remaining);
		uint16_t crc = swapbytes(*(uint16_t*)&dec[len]);
		TRACE("offset=%x crc=%04x\n", offset, crc);

		// Determine if this is the last packet based on cbor map key pairs.
		cbor_token_t* token = cbor.get_key("data");
		unsigned int n = token->int_value;	// total number of bytes of data[] received
		TRACE("data[%d]:\n", n);
		MEM_Trace(token->bytes_value, n, 0L);
		bf.WritetoFile((DWORD)n, (BYTE*)token->bytes_value);
		if (count == 1)
		{
			// First packet has the expected total number of bytes in the image.
			token = cbor.get_key("len");
			total_len = token->int_value;
			TRACE("total_len=%d ", total_len);
		}
		token = cbor.get_key("off");
		unsigned int off = token->int_value;
		TRACE("off=%d\n", off);
		if ((off + n) == total_len)
		{
			//TRACE("Last packet\n");
			break;
		}
	}
	printf("EOF\n");
	delete data;
	return 0;
}

int nmp_format_buf(unsigned char* buf, int size, uint8_t Op, uint16_t Group, uint8_t Id, CBOR* cbor)
{
	int len;
	nmp_hdr_t* hdr;

	// Set NMP packet length (2-bytes):
	*(uint16_t*)&buf[0] = swapbytes(cbor->base + sizeof(nmp_hdr_t) + 2);
	MEM_Trace(&buf[0], sizeof(uint16_t), 0L);

	// Set NMP packet header (8-bytes):
	hdr = (nmp_hdr_t*)&buf[2];
	InitNmpHdr(hdr, Op, 0x00, cbor->base, Group, seq, Id);
	TRACE("tx: ");
	TraceNmpHdr(hdr);

	// Set NMP packet CRC16 (2-bytes):
	uint16_t crc = crc_xmodem(&buf[2], cbor->base + sizeof(nmp_hdr_t));
	TRACE("crc16: %04x\n", crc);
	*(uint16_t*)cbor->p = swapbytes(crc);
	len = 2 + sizeof(nmp_hdr_t) + cbor->base + 2;
	MEM_Trace(buf, len, 0L);

	// Encode NMP packet with base64.
	char enc[520];
	cbor->base = base64_encode(buf, len, enc, 1);
	TRACE("enc len=%d:\n", cbor->base);
	MEM_Trace((unsigned char*)enc, cbor->base, 0L);

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
		strncpy((char*)ptr, &enc[i], k);
		ptr += k;
		*ptr++ = 0x0a;
		j += 2 + k + 1;
	}
	return j;
}

int send_nmp_req(unsigned char *buf, int size, uint8_t Op, uint16_t Group, uint8_t Id, CBOR *cbor)
{
	//TRACE("send_nmp_req Op=%x Group=%x Id=%x\n", Op, Group, Id);
	int j = nmp_format_buf(buf, sizeof(buf), Op, Group, Id, cbor);

	send_buf(buf, j);

	if (!tflag && oflag) 
	{
		// Capture all serial output to a file to be used to verify correct operation.
		bf.WritetoFile((DWORD)j, (BYTE*)buf);
	}
	//MEM_Trace(buf, j, 0L);
	seq += 1;
	return j;
}

// Transmit a nmp imagelist request
static int send_imagelist(unsigned char *buf, int size)
{
	// Format the NMP Image List command buf[].
	cbor.Init(&buf[2 + sizeof(nmp_hdr_t)], sizeof(buf) - 2 - sizeof(nmp_hdr_t));
	cbor.def_map(0);
	send_nmp_req(buf, sizeof(buf), OP_READ, GROUP_IMAGE, IMAGE_ID_STATE, &cbor);
	int rc = recv_nmp_rsp(buf, sizeof(buf), OP_READ_RSP, GROUP_IMAGE, IMAGE_ID_STATE);
	if (rc < 0)
	{
		return rc;
	}
	return 0;
}

int imagelist(void)
{
	unsigned char buf[256];

	printf("Image List:\n");
	if (send_imagelist(buf, sizeof(buf)) != 0)
	{
		return -1;
	}
	// Find keys pairs describing the image list (ignoring array of maps). 
	cbor_token_t* images = cbor.get_key("images");
	if (images == NULL)
	{
		TRACE("rx: No images!\n");
		//return -2;
	}
	cbor_token_t* slot = cbor.get_key("slot");
	cbor_token_t* version = cbor.get_key("version");
	printf("images:[", images->int_value);
	if (slot != NULL)
		printf("slot:%d", slot->int_value);
	if (version != NULL)
		printf(",version:'%.*s'", version->int_value, version->text_value);
	printf("]\n");
	return 0;
}

// Transmit a nmp reset request
static int send_reset(unsigned char *buf, int size)
{
	// Format the NMP Reset command buf[].
	cbor.Init(&buf[2 + sizeof(nmp_hdr_t)], sizeof(buf) - 2 - sizeof(nmp_hdr_t));
	cbor.def_map(0);
	send_nmp_req(buf, size, OP_WRITE, GROUP_DEFAULT, DEFAULT_ID_RESET, &cbor);

	// Receive the response buf[].
	int rc = recv_nmp_rsp(buf, size, OP_WRITE_RSP, GROUP_DEFAULT, DEFAULT_ID_RESET);
	if (rc < 0)
	{
		return rc;
	}
	return 0;
}

int reset(void)
{
	unsigned char buf[256];
	int n;

	printf("Reset:\n");
	if (send_reset(buf, sizeof(buf)) != 0)
	{
		return -1;
	}
	char pat1[] = { "Boot detect" };
	n = recv_buf_start(buf, sizeof(buf), sizeof(pat1) - 1, (unsigned char*)pat1);
	if (n < 0)
	{
		printf("<nil>\n");
		return n;
	}
	else
	{
		buf[n] = '\0';
		printf("%s\n", (char*)buf);
	}

	// Wait >5 seconds after a software reset for the Bootloader to startup.
	TMR_Delay(55);

	// Check if the Bootloader signon has been received.
	char pat2[] = { "Boot valid" };
	n = recv_buf_start(buf, sizeof(buf), sizeof(pat2)-1, (unsigned char *)pat2);
	if (n < 0)
	{
		printf("<nil>\n");
		return n;
	}
	else
	{
		buf[n] = '\0';
		printf("%s\n", (char*)buf);
	}
	return 0;
}

// Transmit a nmp upload request
static int send_upload(unsigned char *buf, int size, unsigned int offset, int nbytes, unsigned char* bytes_data)
{
	cbor.Init(&buf[2 + sizeof(nmp_hdr_t)], sizeof(buf) - 2 - sizeof(nmp_hdr_t));
	cbor.def_map(5);
	cbor.put("data");
	cbor.put(&bytes_data[offset], nbytes); // size of data (in bytes);
	cbor.put("image");
	cbor.put(0);
	if (offset == 0)
	{
		cbor.put("len");
		cbor.put(total_len);
	}
	cbor.put("off");
	cbor.put((int)offset);
	if (offset == 0)
	{
		cbor.put("sha");
		cbor.put(bytes_sha, sizeof(bytes_sha));
	}
	send_nmp_req(buf, size, OP_WRITE, GROUP_IMAGE, IMAGE_ID_UPLOAD, &cbor);

	// Receive the response buf[].
	int rc = recv_nmp_rsp(buf, sizeof(buf), OP_WRITE_RSP, GROUP_IMAGE, IMAGE_ID_UPLOAD);
	if (rc < 0)
	{
		return rc;
	}
	return 0;
}

typedef struct
{
	int rc;
	int off;
} upload_rsp_t;

static int recv_upload(upload_rsp_t *rsp)
{
	// Find keys pair describing the return code. 
	cbor_token_t* rc = cbor.get_key("rc");
	if (rc == NULL)
	{
		printf("rx: No return code!\n");
		return -2;
	}
	if (rc->int_value != 0)
	{
		printf("rx: Error rc=%d\n", rc->int_value);
		return -3;
	}
	cbor_token_t* off = cbor.get_key("off");
	if (rc == NULL)
	{
		printf("rx: No offset!\n");
		return -4;
	}
	rsp->rc = rc->int_value;
	rsp->off = off->int_value;
	TRACE("rc:%d off:%d\n", rsp->rc, rsp->off);
	return 0;
}

int send_upload_next(unsigned int offset, int *size, unsigned char *data)
{
	unsigned char buf[520];
	upload_rsp_t rsp;
	int rc;
	*size = (*size == 0)? 0x0129 : 0x0154;
	//TRACE("offset=%d size=%d %d > %d\n", offset, *size, offset + *size, total_len);
	if ((offset + *size) > total_len)
	{
		*size = total_len - offset;
	}
	if (send_upload(buf, sizeof(buf), offset, *size, data) != 0)
	{
		return -1;
	}
	rc = recv_upload(&rsp);
	if (rc < 0)
	{
		return rc;
	}
	return 0;
}

int upload(char *fnimage)
{
	/* Dumb resource intensive way is to read the whole file into a large data buffer! */
	unsigned char* data = load_file(fnimage, &total_len);
	if (data == NULL)
	{
		return ERR_LOAD_FAILURE;
	}

	/* Send NMP packets broken up into several NLIP serial chunks that have the NMP packet
	** total length and a header in the first chunk, followed by a CBOR encoded map and a
	** crc16-ccitt value of the unencode packet in the last serial chunk.
	** Note that the first NMP packet is slighly shorter to accommodate some extra CBOR
	** encoded values that describe the total length of the image, etc. Subsequent packets
	** are longer and the final packet is variable based on the exact remainder needed for
	** the last fragment of the image.
	*/
	printf("Upload:");
	long count = 0;
	unsigned int offset = 0;
	int size = 0;
	do
	{
		printf(!(count++ % 32) ? "\n." : ".");
		send_upload_next(offset, &size, data);
		offset += size;
	} 
	while (offset < total_len);
	printf("EOF\n");
	delete data;
	return 0;
}

#if 0
int _main(int argc, char* argv[]);

int main(int argc, char* argv[])
{
	argc = 0;
	argv[argc++] = "winlib";
	//argv[argc++] = "-d";
	argv[argc++] = "-p26";
	argv[argc++] = "blehci";			// ifile
	argv[argc++] = "-l";				//-"lwinlib.log";
	argv[argc++] = "-n";
	argv[argc++] = "-r";
	//argv[argc++] = "-t";
	argv[argc++] = "-ocapture";
	//argv[argc++] = "-q";
	//argv[argc++] = "-h";
	return _main(argc, argv);
}

int _main(int argc, char* argv[])
{
#else

int main(int argc, char* argv[])
{
#endif

	char* p;
	int i;
	int rc = 0;

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
					lflag++;
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

				case 'p':
					if (strlen(p + 1))
					{
						port = atoi(p + 1);
					}
					p = " ";
					break;

				case 't':
					tflag++;
					break;

				case 'n':
					nflag++;
					break;

				case 'r':
					rflag++;
					break;

				case 'd':
					dflag++;
					break;

				case 'q':
					qflag++;
					break;

				default:
					fprintf(stderr, "Usage: %s [-i][file] [-o][file] [-l][file] [-c] [-e] [-d]\n", argv[0]);
					fprintf(stderr, "[-i][file] image upload\n");
					fprintf(stderr, "-o[file]   capture output\n");
					fprintf(stderr, "-l[file]   log debug output\n");
					fprintf(stderr, "-pn        COM port number\n");
					fprintf(stderr, "-t         test capture\n");
					fprintf(stderr, "-d         dump data output\n");
					fprintf(stderr, "-n         send HCI commands to enable NMP mode\n");
					fprintf(stderr, "-r         send NMP reset command\n");
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
			strcat(ifname, ".img");
		}
		ifile = ifname;
		fprintf(stdout, "ifile='%s'\n", ifile);
	}
	else
		fprintf(stdout, "ifile=stdin\n");

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
			fprintf(stdout, "ofile='%s'\n", ofile);
		}
		else
			fprintf(stdout, "ofile=stdout\n");
	}

	if (lflag)
	{
		if (!lfile)
		{
			lfile = argv[0];
		}
		strcpy(lfname, lfile);
		if (!strchr(lfile, '.'))
		{
			strcat(lfname, ".log");
		}
		lfile = lfname;
		fprintf(stdout, "lfile='%s'\n", lfile);
	}
	else
		fprintf(stdout, "lfile=NULL\n");

	// Open log file to capture output from putchar, puts and printf macros.
	LOG_Init(lfile);

	TMR_Init(100);	// 100ms timebase

	if (oflag)
	{
		if (!tflag)
		{
			printf("Capture upload to file '%s'\n", ofile);
			bf.InitWriteFile(ofile);
			if (!bf.IsFile())
			{
				printf("File Not Found!\n");
				goto err;
			}
		}
		else
		{
			printf("Read download from file '%s'\n", ofile);
			bf.InitReadFile(ofile);
			if (!bf.IsFile())
			{
				printf("File Not Found!\n");
				goto err;
			}
		}
	}

	if (port != 0)
	{
		COM_Init(port, 115200);
		TMR_Delay(5);

		if (COM_connected())
		{
			printf("Comport is connected!\n");

			if (nflag)
			{
				unsigned char HCI_SetUpload[] = {
					0x01,0x04,0xfc,0x01,0x01 };

				unsigned char HCI_Reset[] = {
					0x01,0x02,0xfc,0x00 };

				unsigned char buf[128];
				int n;
				if ((n = send_hci_cmd(buf, sizeof(buf), HCI_SetUpload, sizeof(HCI_SetUpload), 20)) == 0 ||
					(n = send_hci_cmd(buf, sizeof(buf), HCI_Reset, sizeof(HCI_Reset), 40)) == 0)
				{
					unsigned char buf[256];
					/* Ignore any spurious data sent from the bootloader. */
					recv_buf(buf, sizeof(buf), 100, -1);
					TRACE("Send <CR>\n");
					/* Sending a <CR> can help in resynchronizing the NMP serial link. */
					buf[0] = 0x0d;
					send_buf(buf, 1);
					TMR_Delay(1);

					COM_Term();
					rc = ERR_HCI_TIMEOUT;
					goto err;
				}
				// No specific response expected from the HCI_Reset, however the 'nmgr>' debug text 
				// is output, once the Bootloader is ready to accept NMP requests.
				if (!strncmp("nmgr>\n", (char*)&buf[n - 6], 6))
				{
					printf("nmgr>\n");
				}
			}
			rc = imagelist();
			if (rc < 0)
			{
				COM_Term();
				goto err;
			}
#if 0
			else
			{
				unsigned char buf[256];
				TMR_Delay(55);
				/* Ignore any spurious data sent from the bootloader. */
				recv_buf(buf, sizeof(buf), 100, -1);
				TRACE("Send <CR>\n");
				/* Sending a <CR> can help in resynchronizing the NMP serial link. */
				buf[0] = 0x0d;
				send_buf(buf, 1);
				TMR_Delay(1);
			}
#endif

			if (ifile)
			{
				rc = upload(ifile);
				if (rc < 0)
				{
					COM_Term();
					goto err;
				}
				rc = imagelist();
				if (rc < 0)
				{
					COM_Term();
					goto err;
				}
			}
			if (rflag)
			{
				rc = reset();
				if (rc < 0)
				{
					COM_Term();
					goto err;
				}
			}
		}
		COM_Term();
	}
	if (tflag && oflag)
	{
		rc = download(ofile);
		if (rc < 0)
		{
			goto err;
		}
	}

err:
	if (oflag)
	{
		bf.CloseFile();
	}
	TMR_Term();

	// Close capture log file.
	LOG_Term();

	return rc;
}
#endif

#endif