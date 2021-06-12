/*
 *  pts.hpp
 *
 * Description: Protothreads extensions.
 * 
 *  Created on: Jul 8, 2018
 *      Author: Alan Grave <agraves@gemcore.com>
 */

#ifndef PTS_H_
#define PTS_H_

#include <stdint.h>
#include <stdbool.h>
#include "bsp.h"            // board support functions

#ifdef __cplusplus
extern "C" {
#endif

struct pt_sem {
  unsigned int count;
};

/*****************************************************************************
 * Protothread definitions
 *****************************************************************************/
class PTS_Thread
{
   static const char *pts_unnamed;

public:
   const char *name;

   // Construct a new protothread that will start from the beginning
   // of its Run() function.
   PTS_Thread();
   virtual ~PTS_Thread();

   inline const char *Name(void)  { return name; }

   inline void Name(const char *name)  { PTS_Thread::name = name; }

   // Restart protothread.
   inline void Restart() { _ptsLine = 0; }

   // Stop the protothread from running. Happens automatically at PTS_END.
   // Note: this differs from the Dunkels' original protothread behaviour
   // (his restart automatically, which is usually not what you want).
   inline void Stop() { _ptsLine = LineNumberInvalid; }

   // Return true if the protothread is running or waiting, false if it has
   // ended or exited.
   inline bool IsRunning() { return _ptsLine != LineNumberInvalid; }

   // Suspend protothread.
   inline void Suspend() { _enable = false; }

   // Resume protothread.
   inline void Resume() { _enable = true; }

   // Return true if the protothread is suspended, else false.
   inline bool IsSuspended() { return !_enable; }

   // Run next part of protothread or return immediately if it's still
   // waiting. Return true if protothread is still running, false if it
   // has finished. Implement this method in your Protothread subclass.
   virtual int Init() = 0;
   virtual bool Run() = 0;

   /*
    * Timer related hooks into the BSP. 
    */
   inline bool Tmr_IsTimeout()
   {
      return BSP_get_msec_cnt(_tmr) >= _cnt;
   }

   inline void Tmr_Begin(uint32_t ticks)
   {
      _cnt = ticks;
      BSP_reset_msec_cnt(_tmr);
   }

   inline unsigned int Tmr_GetTimeout()
   {
      return _cnt;
   }

   inline void Tmr_Suspend()
   {
      BSP_enable_msec_cnt(_tmr, false);
   }

   inline void Tmr_Resume()
   {
      BSP_enable_msec_cnt(_tmr, true);
   }

   inline uint32_t Tmr_Count()
   {
      return BSP_get_msec_cnt(_tmr);
   }

   inline void Tmr_Reset()
   {
      BSP_reset_msec_cnt(_tmr);      
   }

   inline unsigned long AtLine()
   {
      return _ptsLine;
   }

protected:
   // Used to store a protothread's position (what Dunkels calls a
   // "local continuation").
   typedef unsigned long LineNumber;

   // An invalid line number, used to mark the protothread has ended.
   static const LineNumber LineNumberInvalid = (LineNumber)(-1);

   // Stores the protothread's position (by storing the line number of
   // the last PTS_Wait, which is then switched on at the next Run).
   LineNumber _ptsLine;

   // Timer support.
   unsigned int _cnt;
   uint8_t _tmr;

   // Event support.
   struct pt_sem _evt;

   bool _enable;
};

// Declare start of protothread (use at start of Run() implementation).
#define PTS_BEGIN() \
   bool ptYielded = true; \
   (void) ptYielded; \
   if (!_enable) \
      return true; \
   switch (_ptsLine) { \
   case 0:

// Stop protothread and end it (use at end of Run() implementation).
#define PTS_END() \
   default: ; \
   } \
   Stop(); \
   return false;

// Cause protothread to wait until given condition is true.
#define PTS_WAIT_UNTIL(condition) do { \
   _ptsLine = __LINE__; case __LINE__: \
   if (!(condition)) \
      return true; \
} while (0)

// Cause protothread to wait while given condition is true.
#define PTS_WAIT_WHILE(condition) \
   PTS_WAIT_UNTIL(!(condition))

// Cause protothread to wait until given child protothread completes.
#define PTS_WAIT_THREAD(child) \
   PTS_WAIT_WHILE((child).Run())

// Restart and spawn given child protothread and wait until it completes.
#define PTS_SPAWN(child) do { \
   (child).Restart(); \
   PTS_WAIT_THREAD(child); \
} while (0)

// Restart protothread's execution at its PTS_BEGIN.
#define PTS_RESTART() do { \
   Restart(); \
   return true; \
} while (0)

// Stop and exit from protothread.
#define PTS_EXIT() do { \
   Stop(); \
   return false; \
} while (0)

// Yield protothread till next call to its Run().
#define PTS_YIELD() do { \
   ptYielded = false; \
   _ptsLine = __LINE__; case __LINE__: \
   if (!ptYielded) \
      return true; \
} while (0)

// Yield protothread until given condition is true.
#define PTS_YIELD_UNTIL(condition) do { \
   ptYielded = false; \
   _ptsLine = __LINE__; case __LINE__: \
   if (!ptYielded || !(condition)) \
      return true; \
} while (0)

// Delay protothread until the timer has expired.
#define PTS_DELAY(t) do { \
   Tmr_Begin(t); \
   PTS_WAIT_UNTIL(Tmr_IsTimeout()); \
} while(0)

// Initializes a semaphore with a value for the counter. 
#define PTS_SEM_INIT(s, c) (s)->count = c

// Wait for a semaphore counter to be greater than zero.
#define PTS_SEM_WAIT(s) do { \
      PTS_WAIT_UNTIL((s)->count > 0); \
      --(s)->count; \
} while(0)

// Signal a semaphore by incrementing the counter.
#define PTS_SEM_SIGNAL(s) ++(s)->count

/*****************************************************************************
 * Scheduler definitions
 *****************************************************************************/
#include "utils\ringbuf.h"

#define PTS_IDLE    0
#define PTS_INIT    1
#define PTS_RUN     2
#define PTS_DEAD    3

#define MAX_TCBS	32

#define MAX_MSGLEN  256


typedef struct
{
   int state;
   uint8_t id;
   uint8_t mode;
#if HAS_CPU_USAGE == 1
   CPUUsageData_t tick;
#endif
   tRingBufObject Msg;
   PTS_Thread *pt;
} Tcb;

//*****************************************************************************
// Definition of interface functions
//*****************************************************************************
int PTS_Init(void);
int PTS_Term(void);
int PTS_AddTask(PTS_Thread *pt, char mode, uint32_t msglen);
int PTS_DeleteTask(uint8_t id);
void PTS_RestartTask(uint8_t id);
void PTS_SuspendTask(uint8_t id);
void PTS_ResumeTask(uint8_t id);
void PTS_KillTask(uint8_t id);
uint8_t PTS_GetCurTaskId(void);
Tcb *PTS_GetCurTask(void);
uint8_t PTS_GetTaskCnt(void);
Tcb *PTS_GetTask(uint8_t id);
PTS_Thread *PTS_GetThread(uint8_t id);
void PTS_RestartCurTask(void);
void PTS_KillCurTask(void);
int PTS_CntMsg(uint8_t id);
bool PTS_GetMsg(uint8_t id,uint8_t *BufferPtr);
bool PTS_PutMsg(uint8_t id,uint8_t *BufferPtr);
void PTS_WaitMsg(uint8_t id, uint8_t *BufferPtr);
int PTS_ProcessTaskId(uint8_t i);
void PTS_Process(void);

//*****************************************************************************
// Definition of CLI commands
//*****************************************************************************
int Cmd_pts(int argc, char *argv[]);

#ifdef    __cplusplus
}
#endif // __cplusplus

#endif /* PTS_HPP_ */
