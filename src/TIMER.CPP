/*0===========================================================================
**
** File:       timer.cpp
**
** Purpose:    Generic timer class methods definitions.
**
**             Implements a generic timer ISR and allows timing tasks to be 
**             accomplished while other processing occurs.
**
** Date:       December 10, 1990
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#include <stdio.h>                  /* C standard I/O includes */
#include <stddef.h>                 /* C standard definitions */
#include "timer.h"                  /* Generic timer class definition */

/*==========================================================================
|
|  constants
|
===========================================================================*/

CTimer *timer;

/*==========================================================================
|
|  functions definitions
|
===========================================================================*/

/**********************************************************************/
/* CTimer - constructor                                    */
/*--------------------------------------------------------------------*/
/* Description: 1. save this pointer                                  */
/*              2. call InitData to initialize data structures        */
/*--------------------------------------------------------------------*/
/* Parameters: none                                                   */
/*--------------------------------------------------------------------*/
/* Returns: nothing                                                   */
/**********************************************************************/

CTimer::CTimer()
{
//   trace_ch('T');

   SetCallback(NULL,NULL);

	InitData();                      // initialize data structures
}

/**********************************************************************/
/* CTimer - destructor                                                */
/*--------------------------------------------------------------------*/
/* Description: 1. deallocate data structures                         */
/*--------------------------------------------------------------------*/
/* Parameters: none                                                   */
/*--------------------------------------------------------------------*/
/* Returns: nothing                                                   */
/**********************************************************************/

CTimer::~CTimer()
{
	if (Last)                        // deallocate data structures
   {
      for (int i=0; i < MAX_EVENTS; i++)
      {
         if (Links[i] != NULL)
         {
            delete Links[i];
         }
      }
      delete (void *)Last;
   }
}

/**********************************************************************/
/* InitData                                                           */
/*--------------------------------------------------------------------*/
/* Description:                                                       */
/*                                                                    */
/*--------------------------------------------------------------------*/
/* Parameters:                                                        */
/*                                                                    */
/*--------------------------------------------------------------------*/
/* Returns:                                                           */
/*                                                                    */
/**********************************************************************/

void CTimer::InitData(void)
{
//   printf("\nCTimer::InitData() called");
#ifndef WIN32
	short sreg = int_disable();      // disable interupts while in here
#endif
   if ((Last = new TimerLink[1]) != NULL)
   {
      Last->Next = NULL;
      Last->pThis = NULL;
      Last->wDelta = 0;
      Last->wPeriod = 0;
      Last->Flags = 0;

   	for (int i=0; i < MAX_EVENTS; i++)  // allocate timer links
      {
         Links[i] = new TimerLink[1];
         Links[i]->Next = NULL;
         Links[i]->pThis = NULL;
         Links[i]->wDelta = 0;
         Links[i]->wPeriod = 0;
         Links[i]->Flags = 0;
      }
   }
#ifndef WIN32
	int_enable(sreg);			         // enable interupts if already enabled
#endif
}

void CTimer::Dump()
{
   for (int i=0; i < MAX_EVENTS; i++)  // allocate timer links
   {
      if (Links[i] != NULL)
      {
         TimerLink *tp = Links[i];

         #ifdef _MSC_VER
         #if defined(_M_I86LM)
         printf("\n%02d:%08lx Next=%08lx pThis=%08lx Delta=%05u T=%05u Flags=%x ",
                  i,tp,tp->Next,tp->pThis,tp->wDelta,tp->wPeriod,tp->Flags);
         #else
         printf("\n%02d:%04x Next=%04x pThis=%04x Delta=%05u T=%05u Flags=%x ",
                  i,tp,tp->Next,tp->pThis,tp->wDelta,tp->wPeriod,tp->Flags);
         #endif
         #else
         printf("\n%02d:%04x Next=%04x pThis=%04x Delta=%05u T=%05u Flags=%x ",
                  i,tp,tp->Next,tp->pThis,tp->wDelta,tp->wPeriod,tp->Flags);
         #endif
      }
   }
}

#pragma check_stack(off)						// turn stack checking off!

/**********************************************************************/
/* ProcessTimers                                                      */
/*--------------------------------------------------------------------*/
/* Description:                                                       */
/*                                                                    */
/*--------------------------------------------------------------------*/
/* Parameters: none                                                   */
/*--------------------------------------------------------------------*/
/* Returns: nothing                                                   */
/**********************************************************************/

void CTimer::ProcessTimers()
{
#ifndef WIN32
//	CPU_SEN_OFF(1);						  // for debug
	short sreg = int_disable();		// disable interupts while in here
#endif
   if (Last->Next != NULL)
   {
      if (Last->Next->wDelta <= 1)
      {
         struct _TimerLink *t = Last->Next;
         t->wDelta = 0;
         do
         {
				if (t->pThis != NULL)
            {

               t->pThis->Event();
            }
//				LED_ERROR_ON();			  // for debug
            if (t->Flags&PERIODIC)
            {
               Schedule(t->wPeriod,t->pThis,t->Flags);
            }
//				LED_ERROR_OFF();			  // for debug
														
            Cancel(t->pThis);
            t = Last->Next;
         }
         while (t != NULL && t->wDelta == 0);
      }
      else
         Last->Next->wDelta -= 1;
   }
#ifndef WIN32
	int_enable(sreg);			         // enable interupts if already enabled
//	CPU_SEN_ON(1);							  // for debug
#endif
}

/**********************************************************************/
/* AllocTimer                                                         */
/*--------------------------------------------------------------------*/
/* Description: Allocate timer event which either raises a flag,      */
/*              calls a C function or calls a C++ member function     */
/*              when done.                                            */
/*--------------------------------------------------------------------*/
/* Parameters: 1. wTicks - no. of ticks to activation                 */
/*             2. pEvent - pointer to event                           */
/*             3. pThis - member this pointer (or NULL)               */
/*             4. cMode - timer mode [SINGLE                          */
/*--------------------------------------------------------------------*/
/* Returns: pointer to newly allocated timer                          */
/*          NULL if unable to allocate pointer                        */
/**********************************************************************/

PTimerLink CTimer::AllocTimer(unsigned int ticks,CTimerEvent *pThis,char mode)
{
   struct _TimerLink *t = NULL;

   for (int i=0; i < MAX_EVENTS; i++)
   {
      if (Links[i]->pThis == NULL)
      {
         t = Links[i];              // allocate timer link
         t->Next = NULL;
         t->pThis = pThis;
         t->wDelta = ticks; 
         t->wPeriod = ticks;
         t->Flags = mode;
         break;
      }
   }
   return t;
}

/**********************************************************************/
/* FreeTimer                                                          */
/*--------------------------------------------------------------------*/
/* Description: free an allocated timer                               */
/*--------------------------------------------------------------------*/
/* Parameters: pTimer - timer to be freed                             */
/*--------------------------------------------------------------------*/
/* Returns: nothing                                                   */
/**********************************************************************/

void CTimer::FreeTimer(struct _TimerLink *t)
{
   for (int i=0; i < MAX_EVENTS; i++)
   {
      if (Links[i] == t)
      {
         Links[i]->pThis = NULL;    // deallocate timer link
         break;
      }
   }
}

/**********************************************************************/
/* Schedule                                                           */
/*--------------------------------------------------------------------*/
/* Description: Schedule an event.                                    */
/*--------------------------------------------------------------------*/
/* Parameters: 1. wTicks - no. of ticks to activation                 */
/*             2. pThis - member this pointer                         */
/*             3. mode - activation mode                              */
/*--------------------------------------------------------------------*/
/* Returns: pointer to newly allocated timer                          */
/*          NULL if unable to allocate pointer                        */
/**********************************************************************/

PTimerLink CTimer::Schedule(unsigned int ticks,CTimerEvent *pThis,char mode)
{ 
   struct _TimerLink *t;
#ifndef WIN32
	short sreg = int_disable();      // disable interupts while in here
#endif
   if ((t = AllocTimer(ticks,pThis,mode)) == NULL)
   {
#ifndef WIN32
   	int_enable(sreg);			      // enable interupts if already enabled
#endif
      printf("\nSchedule(%04x,%04x,%02x) failed! ",ticks,pThis,mode);   //DBG_Printf("\nSchedule(%04x,%04x,%02x) failed! ",ticks,pThis,mode);
      Dump();
      return NULL;
   }
   volatile struct _TimerLink *tp = Last;
   while (tp->Next != NULL)
   {
      if (tp->Next->wDelta > t->wDelta)
      {
         tp->Next->wDelta -= t->wDelta;
         break;
      }
      t->wDelta -= tp->Next->wDelta;
      tp = tp->Next;
   }
   t->Next = tp->Next;
   tp->Next = t;
#ifndef WIN32
	int_enable(sreg);			         // enable interupts if already enabled
#endif
   return t;
}

/**********************************************************************/
/* Cancel                                                             */
/*--------------------------------------------------------------------*/
/* Description: Cancel an event.                                      */
/*--------------------------------------------------------------------*/
/* Parameters: 1. pEvent - pointer to event                           */
/*             2. pThis - member this pointer                         */
/*--------------------------------------------------------------------*/
/* Returns: nothing                                                   */
/**********************************************************************/

void CTimer::Cancel(CTimerEvent *pThis)
{  
   volatile struct _TimerLink *tp = Last;
#ifndef WIN32
	short sreg = int_disable();      // disable interupts while in here
#endif
   while (tp->Next != NULL)
   {
      if (tp->Next->pThis == pThis)
      {
         struct _TimerLink *t = tp->Next;
         if (t->Next != NULL)
         {
            t->Next->wDelta += tp->Next->wDelta;
         }
         tp->Next = t->Next;
         FreeTimer(t);
         break;
      }
      tp = tp->Next;
   }
#ifndef WIN32
	int_enable(sreg);			         // enable interupts if already enabled
#endif
}

#pragma check_stack( )			      // resume stack checking


#ifdef __TESTING_TIMER__
/*==========================================================================
|
|  Conformance Testing
|
===========================================================================*/
#include <stdlib.h>                 /* C standard library includes */
#include "runtime.hpp"              /* static Runtime class definitions */
#include "getkey.h"                 /* BIOS getkey() function definition */

/* External definitions in file 'DELAYMS.C'. */

extern "C" void delay_msec(unsigned int);

#define BREAKPOINT   __asm INT 3

class ATMR;

class ATMRFunc : public CTimerFunc
{
public:
   ATMR *tmr;

   void Func(void);                 // Override base virtual function

   ATMRFunc() : CTimerFunc()
    { }

   ATMRFunc(ATMR *tmr) : CTimerFunc()
    { this->tmr = tmr; }
};

class ATMRFlag : public CTimerFlag
{
public:
   ATMR *tmr;

   ATMRFlag() : CTimerFlag()
    { }

   ATMRFlag(ATMR *tmr) : CTimerFlag()
    { this->tmr = tmr; }
};

class ATMR : public CTimer
{
private:
   int maxtimeout;
   volatile unsigned int counter;

   friend class ATMRFunc;
   friend class ATMRFlag;

public:
   ATMRFunc *t1;
   ATMRFlag *t2;
   void stop();
   void set();
   unsigned int count()
    { return counter; }

   ATMR(int maxtimeout) : CTimer()
    { this->maxtimeout=maxtimeout; counter=0; t1=new ATMRFunc(this); t2=new ATMRFlag(this); }

   ~ATMR()
    { delete t2; delete t1; }
};

void ATMR::stop()
{
   Cancel(t1);
}

void ATMR::set()
{
   printf("\nmaxtimeout=%d ",maxtimeout);
   stop();
   Schedule(maxtimeout,t1,PERIODIC);
   Schedule(maxtimeout,t2,PERIODIC);
}

#pragma check_stack(off)				// turn stack checking off!

void ATMRFunc::Func(void)
{
   tmr->counter++;
}

#pragma check_stack( )

void main()
{
	printf("TIMER - a program to test CTimer Class\n");

   ATMR *tmr = new ATMR(1200);
 
   tmr->set();

   tmr->Dump();
   printf("\nFlag=%d counter=%d\n",tmr->t2->Flag,tmr->count());

   while(!kbhit())
   {
      tmr->ProcessTimers();
      printf("\rFlag=%d counter=%d",tmr->t2->Flag,tmr->count());
//      ERR_Delay(DELAY_HALFSECOND);
   }
   _getch();

   tmr->stop();

   tmr->Dump();
   _getch();

   delete tmr;
}
#endif


