/*0===========================================================================
**
** File:       BASEFILE.HPP
**
** Purpose:    Windows file I/O access class definitions.
**
**
**
**
** Date:       Aug. 04, 1999
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#ifndef BASEFILE_HPP
#define BASEFILE_HPP

//---------------------------------------------------------------
// Include Files
//---------------------------------------------------------------
#include <windows.h>
#ifndef _WIN32_WCE
#include <stdio.h>
#endif
#include <tchar.h>

//---------------------------------------------------------------
// Constant & type definitions
//---------------------------------------------------------------
#define NAME_LENGTH	128

//---------------------------------------------------------------
// Class definitions
//---------------------------------------------------------------
class BASEFILE  
{
protected:
	HANDLE hbase;
	BOOL v_bFileExists;
	TCHAR v_szBaseName[NAME_LENGTH];

public:	

	BASEFILE();
    BASEFILE(char *szBaseName);
	BASEFILE(bool b_ReadFile, char *szBaseName);
	
	~BASEFILE();

	BOOL InitFile(char *szBaseName);
	BOOL InitReadFile(char *szBaseName);
	BOOL InitWriteFile(char *szBaseName);

	BOOL ReadfromFile(BYTE* byteptr);
	BOOL ReadfromFile(DWORD length, BYTE* buffer);
	BOOL ReadfromFile(TCHAR c);
	BOOL ReadfromFile(LPCTSTR szInput, DWORD length);

	BOOL WritetoFile(BYTE byte);
	BOOL WritetoFile(DWORD length, BYTE* buffer);
	BOOL WritetoFile(TCHAR c);
	BOOL WritetoFile(LPCTSTR szOutput, DWORD length);
	BOOL WritetoFile(TCHAR *format,...);

	BOOL SetFilePtr(DWORD Position, long Distance);

	BOOL IsFile( void ) { return v_bFileExists; }

	void CloseFile( void );
	void ConvertFiletime(TCHAR *buffer);
	BOOL RenameFile(TCHAR *buffer);
	long Filesize(void);

};

#endif
