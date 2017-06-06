/*0===========================================================================
**
** File:       tmr.cpp
**
** Purpose:    Timer processing system i/f.
**
**             Notes: Runtime target specific.
**
**
** Date:       October 16, 1997
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#include "TIMER.H"

#ifdef WIN32
/* Not strictly necessary if TMR_ functions are used by a single Windows Thread. */
#include "Thread.h"

static Mutex TMR_Mutex;
#endif

class ATmr : public CTimerFunc
{
	void Func(void);
public:
   ATmr()
      { TMR_Event(1000,this,PERIODIC); }
   ~ATmr()
     { TMR_Event(0,this,PERIODIC); }
};

ATmr *atmr;

void ATmr::Func(void)
{
   // Toggle a 'heartbeat' LED.
   //DEBUG("+");
}

void TMR_Init(void)
{
   atmr = new ATmr();	/* Acquire a periodic 'heartbeat' timer. */
}

void TMR_Term(void)
{
   delete atmr;
}

Tmr  TMR_New( void )
{
   return (Tmr)new CTimerFlag;
}

void TMR_Delete( Tmr t )
{
   delete (CTimerFlag *)t;
}

void TMR_Start( Tmr t,int ticks )
{
   ((CTimerFlag *)t)->Flag = 0;
   TMR_Event( ticks,(CTimerFlag *)t,SINGLE );
}

void TMR_Stop( Tmr t )
{
   TMR_Event( 0, (CTimerFlag *)t, SINGLE );
   ((CTimerFlag *)t)->Flag = 0;
}

int TMR_IsTimeout( Tmr t )
{
   return ((CTimerFlag*)t)->Flag;
}

void TMR_Delay(int ticks)
{
#ifdef WIN32
	Lock lock(TMR_Mutex);
#endif
	if (ticks == 0)
	{
		//Sleep(ticks);
	}
	else
	{
		CTimerFlag d;
		timer->Schedule(ticks, &d);
		while (!d.Flag)
			;
	}
}

void TMR_Event(int ticks, CTimerEvent *e, char mode)
{
#ifdef WIN32
	Lock lock(TMR_Mutex);
#endif
	if (ticks)
		timer->Schedule(ticks, e, mode);
	else
		timer->Cancel(e);
}

void TMR_Process(void)
{
	timer->ProcessTimers();
}


