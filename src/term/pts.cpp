//#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef WIN32
#include "ustdlib.h"
#else
#include <stdlib.h>
#endif
#include "pts.hpp"          // Scheduler

#define putchar LOG_Putchar
#define puts    LOG_Puts
#define printf  LOG_Printf
#define flush   LOG_Flush

#undef TRACE
//#define TRACE(format,...)   CON_trace(format,##__VA_ARGS__)
//#define TRACEF(format,...)  do{CON_trace(format,##__VA_ARGS__);CON_Flush();}while(0)   // wait for data to be sent
#define TRACE(format,...)
#define TRACEF(format,...)

//#define TESTING_PTS

extern "C"
{
#include "ringbuf.h"
#include "cli.h"            // console handling
#include "cfg.h"            // configuration interface
#include "dbg.h"
#define OK	0

static bool pts_enable = false;

static const char *StateToStr[]=
{
   "IDLE",
   "INIT",
   "RUN ",
   "DEAD"
};

static Tcb *CurTask;
static Tcb Tcbs[MAX_TCBS];

static uint8_t TaskCnt;

const char *PTS_Thread::pts_unnamed = "??? ";

#if HAS_CPU_USAGE == 1
extern uint32_t CPUUsageTick(void);
#endif

//void MSG_Dump(uint8_t *BufferPtr)
//{
//   int len = *BufferPtr;
//   CON_printf("%02x ",len);
//   if (len > 0)
//   {
//      char s[80];
//      CON_printf("%s",bytestoa(BufferPtr+1,s,(len > sizeof(s))? sizeof(s) : len));
//   }
//   CON_putc('\n');
//}

PTS_Thread::PTS_Thread()
{
   TRACE("PTS::PTS() @%x\n", this);
   name = pts_unnamed;
   _ptsLine = 0;
   _enable = true;
   _tmr = BSP_claim_msec_cnt(); _cnt = 0;
}

PTS_Thread::~PTS_Thread()
{
   TRACE("PTS::~PTS() name=%s\n", name);
   BSP_return_msec_cnt(_tmr);
}

/*****************************************************************************
 * Scheduler functions
 *****************************************************************************/
int PTS_Init(void)
{
   TRACE("PTS: Init\n");
   uint8_t i;
   CurTask = NULL;
   TaskCnt = 0;
   for (i=0; i < MAX_TCBS; i++)
   {
      Tcbs[i].state= PTS_IDLE;
      Tcbs[i].id = 0xFF;
      Tcbs[i].mode = 0;
      Tcbs[i].pt = NULL;
   }
   return 0;
}

int PTS_Term(void)
{
   uint8_t i;
   TRACE("PTS: Term\n");
   for (i=0; i < MAX_TCBS; i++)
   {
      if (Tcbs[i].id != 0xFF)
      {
         Tcbs[i].state= PTS_IDLE;
         Tcbs[i].id = 0xFF;
         Tcbs[i].mode = 0;
         if (Tcbs[i].Msg.pui8Buf)
         {
            delete Tcbs[i].Msg.pui8Buf;
            Tcbs[i].Msg.pui8Buf = NULL;
         }
         if (Tcbs[i].pt)
         {
            delete Tcbs[i].pt;
            Tcbs[i].pt = NULL;
         }
      }
   }
   TaskCnt = 0;
   return -1;
}

int PTS_AddTask(PTS_Thread *task, char mode, uint32_t msglen)
{
   uint8_t i;
   for (i=0; i < MAX_TCBS; i++)
   {
      if (Tcbs[i].id == 0xFF)
      {
         Tcbs[i].state = PTS_INIT;
         Tcbs[i].id = i;
         Tcbs[i].mode = mode;
#if HAS_CPU_USAGE == 1
         Tcbs[i].tick.tcnt = 1;
         Tcbs[i].tick.tcur = 0;
         Tcbs[i].tick.tsum = 0;
         Tcbs[i].tick.tavg = 0;
         Tcbs[i].tick.tmin = 0xFFFFFFFF;
         Tcbs[i].tick.tmax = 0x00000000;
#endif
         if (msglen > 0)
         {
            uint8_t *buf = (uint8_t *)malloc(msglen);
            RingBufInit(&Tcbs[i].Msg, buf, msglen);
            Tcbs[i].mode |= 2;
         }
         Tcbs[i].pt = task;
         TaskCnt++;
         return i;
      }
   }
   return -1;
}

int PTS_DeleteTask(uint8_t id)
{
   if (TaskCnt != 0)
   {
      if (id < MAX_TCBS)
      {
         Tcbs[id].state= PTS_IDLE;
         Tcbs[id].id = 0xFF;
         Tcbs[id].mode = 0;
         if (Tcbs[id].Msg.pui8Buf)
         {
            free(Tcbs[id].Msg.pui8Buf);
         }
         if (Tcbs[id].pt)
         {
            delete Tcbs[id].pt;
            Tcbs[id].pt = NULL;
         }
         return(--TaskCnt);
      }
   }
   return -1;
}

void PTS_RestartTask(uint8_t id)
{
   Tcb *tcb = PTS_GetTask(id);
   if (tcb)
   {
      tcb->pt->Restart();
      tcb->state = PTS_INIT;
   }
}

void PTS_SuspendTask(uint8_t id)
{
   Tcb *tcb = PTS_GetTask(id);
   if (tcb)
   {
      tcb->pt->Suspend();
   }
}

void PTS_ResumeTask(uint8_t id)
{
   Tcb *tcb = PTS_GetTask(id);
   if (tcb)
   {
      tcb->pt->Resume();
   }
}

void PTS_KillTask(uint8_t id)
{
   Tcb *tcb = PTS_GetTask(id);
   if (tcb)
   {
      tcb->state = PTS_DEAD;
   }
}

Tcb *PTS_GetTask(uint8_t id)
{
   if (id < MAX_TCBS)
   {
      return &Tcbs[id];
   }
   return NULL;
}

void PTS_RestartCurTask(void)
{
   if (CurTask)
   {
      CurTask->pt->Restart();
      CurTask->state = PTS_INIT;
   }
}

void PTS_KillCurTask(void)
{
   if (CurTask)
   {
      CurTask->state = PTS_DEAD;
   }
}

uint8_t PTS_GetCurTaskId(void)
{
   return CurTask->id;
}

Tcb *PTS_GetCurTask(void)
{
   return CurTask;
}

PTS_Thread *PTS_GetThread(uint8_t id)
{
   if (id < MAX_TCBS)
   {
      return Tcbs[id].pt;
   }
   return NULL;
}

int PTS_CntMsg(uint8_t id)
{
   if (id < MAX_TCBS)
   {
      tRingBufObject *pMsg = &Tcbs[id].Msg;
      uint32_t Index = 0;
      uint32_t Used = RingBufUsed(pMsg);
      int j = 0;
      while (Index < Used)
      {
         // First byte of a Msg is always the length [0-255].
         uint8_t len = RingBufGetAt(pMsg, Index);
         //if (CFG_IsTrace(DFLAG_EVT))
         //{
         //   if (j == 0)
         //   {
         //       TRACE("MSG: id=%d\n", id);
         //   }
         //   TRACE(" %02d: Index=%04x len=%02x ", j, Index, len);
         //}
         if (len > 0 && RingBufGetAt(pMsg, Index + len) < 0)
         {
            // Incomplete Msg in buffer!
            //if (CFG_IsTrace(DFLAG_EVT))
            //{
            //   TRACE("Incomplete\n");
            //}
            break;
         }
         Index = 1 + len + Index;
         //if (CFG_IsTrace(DFLAG_EVT))
         //{
         //   TRACE("\n");
         //}
         // Increment the total Msg count.
         j += 1;
      }
      return j;
   }
   return 0;
}

bool PTS_GetMsg(uint8_t id, uint8_t *BufferPtr)
{
   bool rc = false;
   if (id < MAX_TCBS)
   {
      tRingBufObject *pMsg = &Tcbs[id].Msg;
      // Check for pending event
      rc = PTS_CntMsg(id);
      if (rc)
      {
         // check the event len byte.
         uint32_t len = (RingBufUsed(pMsg) > 0)? RingBufPeek(pMsg) : 0;
         if (RingBufUsed(pMsg) >= (len+1))
         {
            // get the event.
            RingBufRead(pMsg,BufferPtr, len+1);

            //if (CFG_IsTrace(DFLAG_EVT))
            //{
            //   CON_printf("MSG: id=%d Get ",id);
            //   MSG_Dump(BufferPtr);
            //}
         }
      }
   }
   return rc;
}

bool PTS_PutMsg(uint8_t id, uint8_t *BufferPtr)
{
   bool rc = false;
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
      Tcb *tcb = &Tcbs[i];
      if (tcb->state != PTS_RUN)
      {
         continue;
      }
      if (!(tcb->mode & 0x01))
      {
         continue;
      }
      tRingBufObject *pMsg = &tcb->Msg;
      int len = *BufferPtr;

      //if (CFG_IsTrace(DFLAG_EVT))
      //{
      //   CON_printf("MSG: id=%d Put ",i);
      //   MSG_Dump(BufferPtr);
      //}
      if (RingBufFree(pMsg) >= (uint32_t)(len+1))
      {
         RingBufWrite(pMsg, BufferPtr, len+1);
         rc = true;
      }
   }
   return rc;
}

void PTS_WaitMsg(uint8_t id, uint8_t *BufferPtr)
{
    while (PTS_CntMsg(id) == 0)
    {
        PTS_Process();
    }
    PTS_GetMsg(id, BufferPtr);
}

#if HAS_CPU_USAGE == 1
#define N   16

unsigned long Filter(unsigned long sample)
{
   static unsigned long sum;
   
   sum -= sum / N;
   sum += sample;
   return sum / N;
}
#endif

int PTS_ProcessTaskId(uint8_t i)
{
   int rc = 0;

   Tcb *task = &Tcbs[i];

   switch (task->state)
   {
   case PTS_IDLE:
   	break;

   case PTS_INIT:
   	if (task->pt != NULL)
   	{
   		rc = task->pt->Init();
   		TRACEF("%s Init() rc=%d\n", CurTask->pt->Name(), rc);
   		task->state = PTS_RUN;
   	}
   	else
   	{
   		task->state = PTS_IDLE;
   	}
   	break;

   case PTS_RUN:
   	rc = task->pt->Run();
#if HAS_CPU_USAGE == 1
      // Call the CPU usage tick function.  This function will compute the amount
      // of cycles used by the CPU since the last call and return the result in
      // percent in fixed point 16.16 format.
      task->tick.tcur = CPUUsageTick();
      task->tick.tavg = Filter(task->tick.tcur);
      task->tick.tcnt += 1;
      if (task->tick.tcnt > 0)
      {
         if (task->tick.tmin != 0)
         {
            task->tick.tmin = min(task->tick.tmin, task->tick.tcur);
         }
         task->tick.tmax = max(task->tick.tmax, task->tick.tcur);
      }
#endif
   	TRACEF("%s Run() rc=%d\n", CurTask->pt->Name(), rc);
   	break;

   case PTS_DEAD:
   	break;
   }
   return rc;
}

void PTS_Process(void)
{
   uint8_t i;
   uint8_t j;

   for (j=0,i=0; j < TaskCnt && i < MAX_TCBS; i++)
   {
      if (Tcbs[i].id == 0xFF)
      {
          continue;
      }
      j++;

      CurTask = &Tcbs[i];

      TRACEF("CurTask @%x: id=%d state=%s\n", CurTask, CurTask->id, StateToStr[CurTask->state]);
      int rc = PTS_ProcessTaskId(i);
   }
}

/*****************************************************************************
 * 
 *****************************************************************************/
#ifdef TESTING_PTS
class OneShot : public PTS_Thread
{
private:
    uint32_t timeout;

public:
   friend class Flasher;
   OneShot() { Name("OneShot"); }
   OneShot(uint32_t value) { Name("OneShot"); timeout = value; }
   ~OneShot() {}

   virtual int Init() { return 0; }
   virtual bool Run();
};

bool OneShot::Run()
{
   PTS_BEGIN();
   CON_printf("%s: Begin\n",Name());

   PTS_DELAY(timeout);
   
   CON_printf("%s: End\n",Name());
   PTS_END();
}

class Flasher : public PTS_Thread
{
private:
   int _i;
   OneShot oneshot;

public:
   Flasher() { Name("Flasher"); oneshot.timeout = 3000; }
   ~Flasher() { }

   virtual int Init() { return 0; }
   virtual bool Run();

   void SetOneshotTimeout(uint32_t value)
   {
      oneshot.timeout = value;
   }
};

bool Flasher::Run()
{
   PTS_BEGIN();
   CON_printf("%s: Begin\n",Name());
   
   for (_i = 0; _i < 10; _i++)
   {
      PTS_SPAWN(oneshot);
      CON_putc('-');
      PTS_DELAY(250);
      CON_putc('_');
      PTS_DELAY(250);
      CON_putc('_');
      PTS_DELAY(250);
      CON_putc('_');
      PTS_DELAY(250);
      CON_putc('\n');
   }

   CON_printf("%s: End\n",Name());
   PTS_END();
}
#endif

#if 0
static void usage(void)
{
   CON_printf("init   : initialize\n");
   CON_printf("term   : terminate\n");
   CON_printf("delete : delete task id\n");
   CON_printf("restart: restart task id\n");
   CON_printf("kill   : kill task id\n");
   CON_printf("list   : list tcbs\n");
   CON_printf("run    : run all\n");
}

int Cmd_pts(int argc, char *argv[])
{
   int rc = OK;
   char *p;
   int i;
   uint8_t id;

   if (argc > 1)
   {
      p = argv[1];
      if (!strcmp(p,"init"))
      {
         PTS_Init();
      }
      else if (!strcmp(p,"term"))
      {
         PTS_Term();
      }
#ifdef TESTING_PTS
      else if (!strcmp(p,"flasher"))
      {
         i = PTS_AddTask(new Flasher, 0, 32);
         CON_printf("Number of tasks %d\n",i);
      }
      else if (!strcmp(p,"oneshot"))
      {
         if (argc > 2)
         {
            i = PTS_AddTask(new OneShot(atoi(argv[2])), 0, 32);
            CON_printf("Number of tasks %d\n",i);
         }
      }
#endif      
      else if (!strcmp(p,"delete"))
      {
         if (argc > 2)
         {
            id = atoi(argv[2]);
            i = PTS_DeleteTask(id);
            CON_printf("Number of tasks %d\n",i);
         }
      }
      else if (!strcmp(p,"restart"))
      {
         if (argc > 2)
         {
            id = atoi(argv[2]);
            PTS_RestartTask(id);
         }
      }
      else if (!strcmp(p,"suspend"))
      {
         if (argc > 2)
         {
            id = atoi(argv[2]);
            PTS_SuspendTask(id);
         }
      }
      else if (!strcmp(p,"resume"))
      {
         if (argc > 2)
         {
            id = atoi(argv[2]);
            PTS_ResumeTask(id);
         }
      }
      else if (!strcmp(p,"kill"))
      {
         if (argc > 2)
         {
            id = atoi(argv[2]);
            PTS_KillTask(id);
         }
      }
      else if (!strcmp(p,"list"))
      {
         uint8_t i;
         for (i=0; i < MAX_TCBS; i++)
         {
            Tcb *tcb = PTS_GetTask(i);
            if (tcb && (tcb->id != 0xFF))
            {
               CON_printf("Tcb[%02d]=@%x: %s id=%02d mode=%x Msgs=%02d ", i, tcb, tcb->pt->Name(), tcb->id, tcb->mode, PTS_CntMsg(tcb->id));
#if HAS_CPU_USAGE == 1
               CON_printf("t:cur=%08x avg=%08x min=%08x max=%08x ", tcb->tick.tcur, tcb->tick.tavg, tcb->tick.tmin, tcb->tick.tmax);
#endif               
               CON_printf("tmr=%08x/%04x/%d line=%05d state=%s ", tcb->pt->Tmr_Count(),tcb->pt->Tmr_GetTimeout(),tcb->pt->Tmr_IsTimeout(), tcb->pt->AtLine(), StateToStr[tcb->state]);
               if (tcb->pt->IsSuspended())
               {
                  CON_printf("suspended\n");
               }
               else
               {
                  CON_printf("%s\n",(tcb->pt->IsRunning())?"running":"stopped");
               }
            }
         }
      }
      else if (!strcmp(p,"run"))
      {
         PTS_Process();
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
   return rc;
}
#endif
}
