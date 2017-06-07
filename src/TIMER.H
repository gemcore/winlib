/*0===========================================================================
**
** File:       timer.h
**
** Purpose:    Generic timer class definitions.
**             
**             
**
**
** Date:       June 16, 1993
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#ifndef TIMER_H
#define TIMER_H
#include "generic.hpp"              /* Generic 'template' macros */

/*==========================================================================
|
|  constants
|
===========================================================================*/

#define  MAX_EVENTS  48		  // max. number of timer events

#define  SINGLE      0        // schedule 'single' event
#define  PERIODIC    1        // schedule 'periodic' event

typedef void (* CTimerCallback)(void * p);

typedef void *Tmr;

/*==========================================================================
|
|  class definitions
|
===========================================================================*/

#pragma check_stack(off)                        // turn stack checking off!

class CTimerEvent
{
public:
   CTimerEvent() 
    { }
   virtual ~CTimerEvent() 
    { }
   virtual void Event(void) = 0;
};

class CTimerFlag : public CTimerEvent
{
public:
   CTimerFlag()
    { Flag=0; }
   void Event(void)
    { ++Flag; }
   int Flag;
};

class CTimerFunc : public CTimerEvent
{
public:
   CTimerFunc()
    { }
   void Event(void)
    { Func(); }
   virtual void Func(void) = 0;   // This will be supplied be derived class
};

#define CTimerFlagdeclare2(T,F) \
class name2(T,F) : public CTimerFlag { \
public: \
   name2(T,F)() : CTimerFlag() \
    { } \
   name3(~,T,F)() \
    { Stop(); } \
   void Stop() \
    { Runtime::Event(0,this); } \
   void Set(int ticks,int mode=SINGLE) \
    { Stop(); Runtime::Event(ticks,this,mode); } \
   int Timeout(void) \
    { return Flag > 0; } \
};

#define CTimerFuncdeclare2(T,F) \
class name2(T,F) : public CTimerFunc { \
private: \
public: \
   T *thisp; \
   void Func(void); \
   name2(T,F)() : CTimerFunc() \
    { } \
   name2(T,F)(T *thisp) : CTimerFunc() \
    { this->thisp = thisp; } \
   name3(~,T,F)() \
    { Stop(); } \
   void Stop() \
    { Runtime::Event(0,this); } \
   void Set(int ticks,int mode=SINGLE) \
    { Stop(); Runtime::Event(ticks,this,mode); } \
};

typedef struct _TimerLink
{
   struct _TimerLink *Next;
   unsigned int wDelta;
   unsigned int wPeriod;
   CTimerEvent *pThis;
   char Flags;
} TimerLink, *PTimerLink;

class CTimer
{
protected:
   volatile PTimerLink Last;        // Last->Next is head of list
   PTimerLink Links[MAX_EVENTS];    // pointers to timer links
   void InitData(void);             // initialize data structures

   PTimerLink AllocTimer(unsigned int ticks,CTimerEvent *pThis,char mode);
   void  FreeTimer(struct _TimerLink *t);

   CTimerCallback callfuncptr;
   void   * callparmptr;

public:
   void Dump();

   void ProcessTimers();

   CTimer();
   virtual ~CTimer();

   PTimerLink Schedule(unsigned int ticks,CTimerEvent *pThis,char cmode=SINGLE);

   void Cancel(CTimerEvent *pThis);

   virtual void SetRate(unsigned int freq)
    { }
   
   virtual unsigned int GetRate(void)
    { return 0; }

   void SetCallback(CTimerCallback funcptr,void * parmptr)
    { callfuncptr = funcptr; callparmptr = parmptr; }

   int  IsCallback(void)
    { return callfuncptr != 0; }

   void Callback(void)
    { if (IsCallback()) callfuncptr(callparmptr); }
};

#pragma check_stack( )

#endif

