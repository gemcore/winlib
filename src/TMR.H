#pragma once

#if !defined TMR_H
#define TMR_H
#include "TIMER.H"

extern "C"
{
	void TMR_Init(void);
	void TMR_Term(void);
	Tmr  TMR_New(void);
	void TMR_Delete(Tmr t);
	void TMR_Start(Tmr t, int ticks);
	void TMR_Stop(Tmr t);
	int  TMR_IsTimeout(Tmr t);
	void TMR_Delay(int ticks);
	void TMR_Event(int ticks, CTimerEvent *e, char mode);
}

#endif
