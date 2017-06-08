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
#include "TMR.H"

#define TRACE	//printf

#ifdef WIN32
#include "active.h"

static Mutex TMR_Mutex;
#endif

/*
** Simple Timer support functions adapted to WIN32 thread API.
*/
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <stdio.h>
#include <windows.h>
#include <process.h>

class TMR : CTimer, ActiveObject
{
private:
	char *getname() { return "TMR"; }
	int timebase;
	void InitThread() { }
	void Run();
	void FlushThread() { }

public:
	TMR(int timebase=100)  
		: timebase(timebase), CTimer(), ActiveObject()
	{
		TRACE("%s(%d) called\n", getname(), timebase);
		Resume();
	}

	Tmr  New(void)
	{
		return (Tmr) new CTimerFlag();
	}

	void Delete(Tmr t)
	{
		Cancel((CTimerEvent *)t);
		delete (CTimerEvent *)t;
	}

	void Start(Tmr t, int ticks)
	{
		Schedule(ticks, (CTimerEvent *)t, SINGLE);
	}

	void Stop(Tmr t)
	{
		Cancel((CTimerEvent *)t);
	}

	int IsTimeout(Tmr t)
	{
		return ((CTimerFlag *)t)->Flag == 0;
	}
};

void TMR::Run(void)
{
	TRACE("%s::Run()\n", getname());
	for (;;)
	{
		if (_isDying)
			return;

		// Do processing for timer thread here...
		ProcessTimers();

		// Use a tick in milli-seconds.
		Sleep(timebase);
	}
}

TMR *tmr;

extern "C"
{
	void TMR_Init(int timebase)
	{
		tmr = new TMR(timebase);
	}

	void TMR_Term(void)
	{
		delete tmr;
		tmr = NULL;
	}

	Tmr  TMR_New(void)
	{
		return tmr->New();
	}

	void TMR_Delete(Tmr t)
	{
#ifdef WIN32
		Lock lock(TMR_Mutex);
#endif
		tmr->Delete(t);
	}

	void TMR_Start(Tmr t, int ticks)
	{
#ifdef WIN32
		Lock lock(TMR_Mutex);
#endif
		tmr->Start(t, ticks);
	}

	void TMR_Stop(Tmr t)
	{
#ifdef WIN32
		Lock lock(TMR_Mutex);
#endif
		tmr->Stop(t);
	}

	int  TMR_IsTimeout(Tmr t)
	{
		return tmr->IsTimeout(t);
	}

	void TMR_Delay(int ticks)
	{
		if (ticks == 0)
		{
			//Sleep(ticks);
		}
		else
		{
#ifdef WIN32
			Lock lock(TMR_Mutex);
#endif
			CTimerFlag d;
			((CTimer *)tmr)->Schedule(ticks, &d);
			while (!d.Flag)
				;
		}
	}

	void TMR_Event(int ticks, CTimerEvent *e, char mode=SINGLE)
	{
#ifdef WIN32
		Lock lock(TMR_Mutex);
#endif
		if (ticks)
			((CTimer *)tmr)->Schedule(ticks, e, mode);
		else
			((CTimer *)tmr)->Cancel(e);
	}
}

