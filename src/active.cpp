#include <stdio.h>
#include "active.h"
//------------------------------------
//  active.cpp
//  Active object framework
//  (c) Bartosz Milewski, 1996
//------------------------------------

// The constructor of the derived class
// should call
//    _thread.Resume ();
// at the end of construction

HANDLE hTimer = NULL;

ActiveObject::ActiveObject ()
: _isDying (0),
#pragma warning(disable: 4355) // 'this' used before initialized
  _thread (ThreadEntry, this)
#pragma warning(default: 4355)
{
	// Create a waitable timer.
#ifdef _CONSOLE
	hTimer = CreateWaitableTimer(NULL, TRUE, (WCHAR *)"WaitableTimer");
#else
	hTimer = CreateWaitableTimer(NULL, TRUE, "WaitableTimer");
#endif
	if (NULL == hTimer)
	{
		printf("CreateWaitableTimer failed (%d)\n", GetLastError());
	}
}

// FlushThread must reset all the events
// on which the thread might be waiting.

void ActiveObject::Kill ()
{
    _isDying++;
    FlushThread ();
    // Let's make sure it's gone
    _thread.WaitForDeath ();
}

DWORD WINAPI ActiveObject::ThreadEntry (void* pArg)
{
    ActiveObject * pActive = (ActiveObject *) pArg;
    pActive->InitThread ();
    pActive->Run ();
    return 0;
}

#include <windows.h>
#include <stdio.h>

int Thread::Sleep(int ms)
{
	LARGE_INTEGER liDueTime;

	liDueTime.QuadPart = -10000 * ms;

	//printf("Waiting for %d ticks...\n",ms);

	// Set a timer to wait for specified milli-seconds.
	if (!SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0))
	{
		printf("SetWaitableTimer failed (%d)\n", GetLastError());
		return -2;
	}

	// Wait for the timer.
	if (WaitForSingleObject(hTimer, INFINITE) != WAIT_OBJECT_0)
		printf("WaitForSingleObject failed (%d)\n", GetLastError());
	//else 
	//	printf("Timer was signaled.\n");
	return 0;
}

#if 0
// TimerTest.cpp : Defines the entry point for the console application.
//

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <stdio.h>
#include <windows.h>
#include <process.h>

unsigned int __stdcall TimerThread(void *p_thread_data);
void DoSomethingOnTimer(void);
void DoRegularStuff(void);

bool is_running = true;

int main(int argc, char* argv[])
{
	unsigned int thread_id = 0;

	_beginthreadex(NULL, 0, TimerThread, NULL, 0, &thread_id);

	DoRegularStuff();

	return 0;
}

unsigned int __stdcall TimerThread(void *p_thread_data)
{
	HANDLE event_handle = CreateEvent(NULL, FALSE, FALSE, "my_handle");

	while (is_running)
	{
		switch (WaitForSingleObject(event_handle, 2000)) // do something every 2 seconds
		{
		case WAIT_TIMEOUT:
			DoSomethingOnTimer();
		}
	}

	return(0);
}

void DoSomethingOnTimer(void)
{
	::MessageBox(NULL, "timer went off", "test", MB_OK);
}

void DoRegularStuff(void)
{
	while (is_running)
	{
		// do regular stuff here
	}
}
#endif