#if !defined WINEX_H
#define WINEX_H

#include <windows.h>

class WinException
{
public:
    WinException (char* msg)
    : _err (GetLastError()), _msg(msg)
    {}
    DWORD GetError() const { return _err; }
    char* GetMessage () const { return _msg; }
private:
    DWORD _err;
    char* _msg;
};

#endif