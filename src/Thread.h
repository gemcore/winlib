#if !defined THREAD_H
#define THREAD_H
//------------------------------------
//  thread.h
//  (c) Bartosz Milewski, 1996
//------------------------------------

#include <windows.h>

class Thread
{
public:
    Thread (DWORD (WINAPI * pFun) (void* arg), void* pArg)
    {
        _handle = CreateThread (
            0, // Security attributes
            0, // Stack size
            pFun, 
            pArg, 
            CREATE_SUSPENDED, 
            &_tid);
    }
    ~Thread () { CloseHandle (_handle); }
    void Resume () { ResumeThread (_handle); }
    void Suspend () { SuspendThread (_handle); }
	void WaitForDeath () { WaitForSingleObject (_handle, 2000); }
	int Sleep(int ticks);
private:
    HANDLE _handle;
    DWORD  _tid;     // thread id
};

class Mutex
{
    friend class Lock;
public:
    Mutex () { InitializeCriticalSection (&_critSection); }
    ~Mutex () { DeleteCriticalSection (&_critSection); }
private:
    void Acquire () 
    { 
        EnterCriticalSection (&_critSection);
    }
    void Release () 
    { 
        LeaveCriticalSection (&_critSection);
    }

    CRITICAL_SECTION _critSection;
};

class Lock 
{
public:
	// Acquire the state of the semaphore
	Lock ( Mutex& mutex ) 
		: _mutex(mutex) 
	{
		_mutex.Acquire();
	}
	// Release the state of the semaphore
	~Lock ()
	{
		_mutex.Release();
	}
private:
	Mutex& _mutex;
};

class Event
{
public:
    Event ()
    {
        // start in non-signaled state (red light)
        // auto reset after every Wait
        _handle = CreateEvent (0, FALSE, FALSE, 0);
    }

    ~Event ()
    {
        CloseHandle (_handle);
    }

    // put into signaled state
    void Release () { SetEvent (_handle); }
    void Wait ()
    {
        // Wait until event is in signaled (green) state
        WaitForSingleObject(_handle, INFINITE);
    }
    operator HANDLE () { return _handle; }
private:
    HANDLE _handle;
};

class TrafficLight
{
public:
    TrafficLight ()
    {
        // Start in non-signaled state (red light)
        // Manual reset
        _handle = CreateEvent (0, TRUE, FALSE, 0);
    }

    ~TrafficLight ()
    {
        CloseHandle (_handle);
    }

    // put into signaled state
    void GreenLight () { SetEvent (_handle); }

    // put into non-signaled state
    void RedLight () { ResetEvent (_handle); }

    void Wait ()
    {
        // Wait until event is in signaled (green) state
        WaitForSingleObject(_handle, INFINITE);
    }

private:
    HANDLE _handle;
};

#endif