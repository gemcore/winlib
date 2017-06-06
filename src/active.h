#if !defined ACTIVE_H
#define ACTIVE_H
//------------------------------------
//  active.h
//  Active object framework
//  (c) Bartosz Milewski, 1996
//------------------------------------

#include "thread.h"
#include <windows.h>

class ActiveObject
{
public:
    ActiveObject ();
    virtual ~ActiveObject () {}
    void Kill ();
    void Resume () { _thread.Resume (); }
    void Suspend () { _thread.Suspend (); }
	int  Sleep(int ticks) { return _thread.Sleep(ticks); }

protected:
    virtual void InitThread () = 0;
    virtual void Run () = 0;
    virtual void FlushThread () = 0;

    int             _isDying;

    static DWORD WINAPI ThreadEntry (void *pArg);
    Thread          _thread;
};

// Last thing in the constructor of a class derived from 
// ActiveObject you must call
//    _thread.Resume ();
// Inside the loop the Run method you must keep checking _isDying
//    if (_isDying)
//         return;



#endif