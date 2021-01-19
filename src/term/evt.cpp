/*
 * evt.cpp
 *
 *  Created on: Oct 31, 2014
 *      Author: Alan
 */

#include <string.h>
#ifndef WIN32
#include "ustdlib.h"
#else
#include <stdlib.h>
#endif
#include "bsp.h"           // board support functions
#include "cli.h"            // console handling
#include "dbg.h"            // debug console functions
#include "cfg.h"
#include "ringbuf.h" // Ring buffer
#if HAS_MNU == 1
#include "menu.hpp"			// Menu definitions
#endif
#include "pts.hpp"         // Scheduler
#include "evt.h"           // event handling

#undef TRACE
#define TRACE(format,...)   do{if (CFG_IsTrace(DFLAG_EVT)) CON_trace(format,##__VA_ARGS__);CON_Flush();}while(0)
//#define TRACE(format,...)

/*******************************************************************************
 * Type definitions
 ******************************************************************************/

extern "C"
{
void MEM_Dump(uint8_t *data,uint16_t len,uint32_t base);

/*******************************************************************************
 * Local Variables
 ******************************************************************************/
bool evt_enable = true;

}

class EVT_Thread : public PTS_Thread
{
public:
   EVT_Thread() { Name("EVT "); }
   ~EVT_Thread() {}

   virtual int Init();
   virtual bool Run();
};

int EVT_Thread::Init(void)
{
   TRACE("EVT: Init\n");
   return 0;
}

bool EVT_Thread::Run()
{
   PTS_BEGIN();

   PTS_WAIT_UNTIL(EVT_GetEnable());

   EVT_Process();

   PTS_RESTART();
   PTS_END();
}

extern "C"
{
static uint8_t evt_id;

int EVT_Init(void)
{
   evt_id = PTS_AddTask(new EVT_Thread, 0, 256);
   return evt_id;
}

uint8_t EVT_GetId(void)
{
    return evt_id;
}

void EVT_SetEnable(bool state)
{
   evt_enable = state;
}

bool EVT_GetEnable(void)
{
   return evt_enable;
}

void EVT_Make(uint8_t *evt, int code, uint8_t *bufptr, int buflen)
{
   evt[0] = 1 + buflen;
   evt[1] = code;
   memcpy(&evt[2], bufptr, buflen);
}

/*******************************************************************************
 * EVT Processing
 ******************************************************************************/
#include <ctype.h>

void DumpChar(byte b)
{
	if (isprint(b))
		CON_printf("%c",b);
    else if (b == '\b')
    	CON_printf("%c",b);
    else if (b == '\r')
        ;
    else if (b == '\n')
    	CON_printf("\n");
    else if (b == '\0')
        CON_printf("<NUL>");
    else
    	CON_printf("[%02x]",b&0xFF);
}

void EVT_Process(void)
{
    uint8_t evt[MAX_MSGLEN];

    // Check for pending event
    if (PTS_GetMsg(evt_id, &evt[0]))
    {
        if (evt[0] >= 1)
        {
            TRACE("EVT: %02x=",evt[1]);

            /* Process the events */
            switch(evt[1])
            {
#if HAS_SSI == 1
            case EVT_SSI:
                TRACE("SSI ");
                if (CFG_IsTrace(DFLAG_SSI))
                {
                    int i;
                    int len = evt[0] - 1;
                    CON_printf("SSI Rx '");
                    for (i=0; i < len; i++)
                    {
                        DumpChar(evt[2+i]);
                    }
                    CON_printf("\n");
                }
                else
                    TRACE("\n");
                break;
#endif
#if HAS_WAP == 1
            case EVT_WAP:
                TRACE("WAP status=%02x\n",evt[2]);
#if HAS_MNU == 1
                switch(evt[2])
                {
                case 0x00:
                    MNU_Status("WAP CLOSED     ");
                    break;
                case 0x01:
                    MNU_Status("WAP ACCEPT     ");
                    break;
                }
#endif
                break;
#endif
            case EVT_RST:
                TRACE("RST\n");
                break;
#if HAS_BTN == 1
            case EVT_BTN:
                TRACE("BTN Id=%02x\n",evt[2]);
#if HAS_MNU == 1
                MNU_ProcessEvent(evt);
#endif
                break;
#endif
#if HAS_MNU == 1
            case EVT_MNU:
                TRACE("MNU\n");
                MNU_ProcessEvent(evt);
                break;
#endif
#if HAS_USB == 1
            case EVT_USB:
                if (CFG_IsTrace(DFLAG_USB))
                {
                    CON_printf("USB status=%02x\n",evt[2]);
                }
#if HAS_MNU == 1
                switch(evt[2])
                {
                case 0x01:
                    MNU_Status("CDC CONNECT    ");
                    break;
                case 0x00:
                    MNU_Status("CDC DISCONNECT ");
                    DBGStdioConfig(6, 115200, g_ui32SysClock);
                    break;
                }
#endif
                break;
#endif
#if HAS_OBD == 1
            case EVT_OBD:
                if (CFG_IsTrace(DFLAG_OBD))
                {
                    CON_printf("OBD status=%02x\n",evt[2]);
                }
#if HAS_MNU == 1
                switch(evt[2])
                {
                case 0x00:
                    MNU_Status("OBD DOWN       ");
                    break;
                case 0x01:
                    MNU_Status("OBD UP         ");
                    break;
                case 0x02:
                    MNU_Status("OBD ERROR      ");
                    break;
                }
#endif
                break;
#endif
#if HAS_CAN == 1
                case EVT_CAN:
                TRACE("CAN ");
                if (CFG_IsTrace(DFLAG_CAN))
                {
                    CON_printf("Can Rx ");
                    for (int i=0,len=evt[0]-1; i < len; i++)
                    {
                        DumpChar(evt[2+i]);
                    }
                    CON_putc('\n');
                }
                else
                    TRACE("\n");
                break;
#endif
#if HAS_HIB == 1
            case EVT_HIB:
                TRACE("HIB ");
                switch(evt[2])
                {
                case 0x00:
                    TRACE("Set Time\n");
                    break;
                case 0x01:
                    TRACE("The Time ");
                    for (int i=0, len=evt[0]-1; i < len; i++)
                    {
                        DumpChar(evt[3+i]);
                    }
                    CON_putc('\n');
                    break;
                case 0x02:
                    TRACE("Get DateTime Error\n");
                    break;
                case 0x03:
                    TRACE("Get DateTime Invalid\n");
                    break;
                }
                break;
#endif
            case EVT_NET:
                TRACE("NET status=%02x\n",evt[2]);
#if HAS_MNU == 1
                switch(evt[2])
                {
                case 0x00:
                    MNU_Status("NET CLOSE      ");
                    break;
                case 0x01:
                    MNU_Status("NET OPEN       ");
                    break;
                case 0x02:
                    MNU_Status("NET ERROR      ");
                    break;
                }
#endif
                break;
            case EVT_TCP:
                TRACE("TCP status=%02x\n",evt[2]);
#if HAS_MNU == 1
                switch(evt[2])
                {
                case 0x01:
                    MNU_Status("TCP CONNECT    ");
                    break;
                case 0x00:
                    MNU_Status("TCP DISCONNECT ");
                    break;
                }
#endif
                break;
#if HAS_UIP == 1
            case EVT_UIP:
                TRACE("UIP %d.%d.%d.%d\n", evt[2], evt[3], evt[4], evt[5]);
                break;
#endif
#if HAS_TRM == 1
            case EVT_LIN:
                TRACE("LIN ");
                if (CFG_IsTrace(DFLAG_TRC))
                {
                    int i;
                    int len = evt[0] - 1;
                    CON_printf("Line len=%02x '", len);
                    for (i=0; i < len; i++)
                    {
                        DumpChar(evt[2+i]);
                    }
                    CON_printf("'\n");
                }
                else
                    TRACE("\n");
                break;
#endif
            case EVT_NUL:
                TRACE("NUL\n");
                break;
            default:
                TRACE("Len=%02x Unknown Code=%02x\n", evt[0], evt[1]);
            }
        }
    }
}

static void usage(void)
{
   CON_printf("on    : enable\n");
   CON_printf("off   : disable\n");
   CON_printf("id    : tcb id\n");
   CON_printf("cnt   : number of events\n");
   CON_printf("peek  : peek next char\n");
   CON_printf("getat : peek char at\n");
   CON_printf("dump  : peek data\n");
   CON_printf("view  : peek events\n");
   CON_printf("recv  : set recv\n");
   CON_printf("get   : get event\n");
   CON_printf("put   : put event\n");
}

static void DumpMsgs(uint8_t id)
{
   int n;
   int i = 0;
   if (id == 0xFF)
   {
      // Broadcast destination.
      n = MAX_TCBS;
      i = 0;
   }
   else
   {
      n = (id >= MAX_TCBS)? MAX_TCBS-1 : id;
      i = id;
   }
   for (; i <= n; i++)
   {
      Tcb *tcb = PTS_GetTask(i);
      if (tcb && (tcb->id != 0xFF))
      {
         uint8_t buf[MAX_MSGLEN];
         tRingBufObject *pMsg = &(tcb->Msg);
         int k = 0;
         CON_printf("Tcb[%02d]:\n", tcb->id);
         for (int n=RingBufUsed(pMsg); n > 0; n-=(1))
         {
            int c = RingBufGetAt(pMsg, k);
            if (c < 0)
            {
               break;
            }
            buf[k] = c;
            k += 1;
         }
         MEM_Dump(buf, k, 0);
      }
   }
}

static void ViewMsgs(uint8_t id)
{
   int n;
   int i = 0;
   if (id == 0xFF)
   {
      // Broadcast destination.
      n = MAX_TCBS;
      i = 0;
   }
   else
   {
      n = (id >= MAX_TCBS)? MAX_TCBS-1 : id;
      i = id;
   }
   for (; i <= n; i++)
   {
      Tcb *tcb = PTS_GetTask(i);
      if (tcb && (tcb->id != 0xFF))
      {
         tRingBufObject *pMsg = &(tcb->Msg);
         int k = 0;
         CON_printf("Tcb[%02d]:\n", tcb->id);
         for (int len, n=RingBufUsed(pMsg); n > 0; n-=(len+1))
         {
            len = RingBufGetAt(pMsg, k);
            CON_printf(" Index=%04x len=%02x ", k, len);
            k += 1;
            for (int j=0; j < len; j++)
            {
               int c = RingBufGetAt(pMsg, k);
               if (c < 0)
               {
                  break;
               }
               CON_printf("%02x ", c&0xFF);
               k += 1;
            }
            CON_printf("\n");
         }
      }
   }
}

int Cmd_evt(int argc, char *argv[])
{
   static uint8_t id = 0xFF;
   uint8_t evt[MAX_MSGLEN];
   tRingBufObject *pMsg;

   if (argc > 1)
   {
      char *p = argv[1];

      if (!strcmp(p,"on"))
      {
           EVT_SetEnable(true);
      }
      else if (!strcmp(p,"off"))
      {
         EVT_SetEnable(false);
      }
      else if (!strcmp(p,"id"))
      {
         if (argc > 2)
         {
            id = atoi(argv[2]);
         }
         CON_printf("%d\n", id);
      }
      else if (!strcmp(p,"cnt"))
      {
         CON_printf("%d\n", PTS_CntMsg(id));
      }
      else if (!strcmp(p,"peek"))
      {
         if (id >= MAX_TCBS)
         {
             return(0);
         }
         pMsg = &(PTS_GetTask(id)->Msg);
         CON_printf("%02x\n", RingBufPeek(pMsg));
      }
      else if (!strcmp(p,"getat"))
      {
         if (argc > 2)
         {
            if (id >= MAX_TCBS)
            {
                return(0);
            }
            pMsg = &(PTS_GetTask(id)->Msg);
            int i = atoi(argv[2]);
            CON_printf("%02x\n", RingBufGetAt(pMsg, i));
         }
      }
      else if (!strcmp(p,"dump"))
      {
         DumpMsgs(id);
      }
      else if (!strcmp(p,"view"))
      {
         ViewMsgs(id);
      }
      else if (!strcmp(p,"put"))
      {
         const char *msg = "TESTING 1 2 3";
         EVT_Make(evt, EVT_NUL, (uint8_t *)msg, strlen(msg)+1);
         if (PTS_PutMsg(id, evt))
         {
            MEM_Dump(evt,1+evt[0],0);
         }
      }
      else if (!strcmp(p,"get"))
      {
         if (PTS_GetMsg(id, evt))
         {
             MEM_Dump(evt,1+evt[0],0);
         }
      }
      else if (!strcmp(p,"recv"))
      {
         if (id >= MAX_TCBS)
         {
            return(0);
         }
         Tcb *tcb = PTS_GetTask(id);
         if (argc > 2)
         {
            if (atoi(argv[2]) == 0)
            {
               tcb->mode &= ~0x01;
            }
            else
            {
               tcb->mode |= 0x01;
            }
         }
         CON_printf("%d\n", tcb->mode);
      }
      else if (!strcmp(p,"?"))
      {
         usage();
      }
      else
      {
         return(CMDLINE_INVALID_ARG);
      }
   }
   return(0);
}

}
/* End Of File */
