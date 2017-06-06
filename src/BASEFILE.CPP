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
#include "BASEFILE.hpp"

//---------------------------------------------------------------
// Construction/Destruction
//---------------------------------------------------------------
BASEFILE::BASEFILE()
{
	hbase = INVALID_HANDLE_VALUE;
}

BASEFILE::BASEFILE(char *szBaseName)
{
	hbase = INVALID_HANDLE_VALUE;
	InitFile(szBaseName);
}

BASEFILE::BASEFILE(BOOL b_ReadFile, char *szBaseName)
{
	hbase = INVALID_HANDLE_VALUE;
	if (b_ReadFile)
		InitReadFile(szBaseName);
	else
		InitWriteFile(szBaseName);
}

BASEFILE::~BASEFILE()
{
	CloseFile();
}

//---------------------------------------------------------------

BOOL BASEFILE::InitFile(char *szBaseName)
{	
	MultiByteToWideChar(CP_ACP, 0, szBaseName, -1, v_szBaseName, sizeof(v_szBaseName));

	if ((IsFile())||(hbase != INVALID_HANDLE_VALUE)) 
	{
		CloseFile();
	}
	if (INVALID_HANDLE_VALUE == (hbase = CreateFile(v_szBaseName,
		GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)))
	{
		v_bFileExists = FALSE;
	}
	else
	{
		v_bFileExists = TRUE;
	}
	return v_bFileExists;
}

BOOL BASEFILE::InitReadFile(char *szBaseName)
{
	MultiByteToWideChar(CP_ACP, 0, szBaseName, -1, v_szBaseName, sizeof(v_szBaseName));

	if ((IsFile()) || (hbase != INVALID_HANDLE_VALUE))
	{
		CloseFile();
	}

	if (INVALID_HANDLE_VALUE == (hbase = CreateFile(v_szBaseName,
		GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)))
	{
		v_bFileExists = FALSE;
	}
	else
	{
		v_bFileExists = TRUE;
	}
	return v_bFileExists;
}

BOOL BASEFILE::InitWriteFile(char *szBaseName)
{	
	MultiByteToWideChar(CP_ACP, 0, szBaseName, -1, v_szBaseName, sizeof(v_szBaseName));

	if ((IsFile())||(hbase != INVALID_HANDLE_VALUE)) 
	{
		CloseFile(); //reinit
	}
	if (INVALID_HANDLE_VALUE == (hbase = CreateFile(v_szBaseName,
		GENERIC_WRITE, FILE_SHARE_READ, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0)))
	{
		v_bFileExists = FALSE;
	}
	else
	{
		v_bFileExists = TRUE;
	}
	return v_bFileExists;
}
//---------------------------------------------------------------

BOOL BASEFILE::ReadfromFile(BYTE *byteptr)
{
	if (IsFile())
	{
		DWORD len;
		BOOL retval;

		retval = ReadFile(hbase,byteptr,1,&len,0);
		return ((retval)&&(len == 1));
	}
	return FALSE;
}

BOOL BASEFILE::ReadfromFile(DWORD length, BYTE *buffer)
{
	if (IsFile())
	{
		DWORD len;
		BOOL retval;

		retval = ReadFile(hbase,buffer,length,&len,0);
		return ((retval)&&(len == length));
	}
	return FALSE;
}

#if 0
BOOL BASEFILE::ReadfromFile(TCHAR c)
{
	if (IsFile())
	{
		DWORD len;
		BOOL retval;

		retval = ReadFile(hbase,&c,sizeof(TCHAR),&len,0);
		return ((retval)&&(len>0));
	}
	return FALSE;
}

BOOL BASEFILE::ReadfromFile(LPCTSTR szInput, DWORD length)
{
	if (IsFile())
	{
		DWORD len;
		BOOL retval;

		retval = ReadFile(hbase,&szInput,length,&len,0);
		return ((retval)&&(len>0));
	}
	return FALSE;
}
#endif

BOOL BASEFILE::SetFilePtr(DWORD Position, long Distance)
{
	if (IsFile())
	{
		if (SetFilePointer(hbase,Distance,0,Position) == 0xFFFFFFFF)
		{
		return FALSE;
		}
		else
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL BASEFILE::WritetoFile(BYTE byte)
{
	if (IsFile())
	{
		DWORD len;
    
		return WriteFile(hbase, &byte, 1, &len, 0);
	}
	return FALSE;
}

BOOL BASEFILE::WritetoFile(DWORD length, BYTE* buffer)
{
	if (IsFile())
	{
		DWORD len;

		return WriteFile(hbase, buffer, length, &len, 0);	
	}
	return FALSE;
}

#if 0
BOOL BASEFILE::WritetoFile(TCHAR c)
{
	if (IsFile())
	{
		DWORD len;

		return WriteFile(hbase, &c, 1, &len, 0);
	}
	return FALSE;
}

BOOL BASEFILE::WritetoFile(LPCTSTR szOutput, DWORD length)
{
	if (IsFile())
	{
		DWORD len;

		return WriteFile(hbase, szOutput, length, &len, 0);
	}
	return FALSE;
}

BOOL BASEFILE::WritetoFile(TCHAR *format,...)
{
	if (IsFile())
	{
		va_list arg_ptr;
		va_start(arg_ptr,format);
		TCHAR s[512];               // formatted output buffer 

		int num_chars = _vstprintf(s,format,arg_ptr);
#ifdef _WIN32_WCE
		for( int i=0; i<num_chars; i++ ) 
			WritetoFile(s[i]);
		return(TRUE);
#else
		DWORD len;
		return WriteFile(hbase, s, num_chars, &len, 0);
#endif
	}
	return FALSE;
}

void BASEFILE::ConvertFiletime(TCHAR *buffer)
{
	FILETIME CreateTime, LastAccessTime, LastWriteTime, LocalFileTime;
    SYSTEMTIME SysTime;
	if (IsFile())
	{
		GetFileTime( hbase, &CreateTime, &LastAccessTime, &LastWriteTime );
		FileTimeToLocalFileTime(&LastWriteTime, &LocalFileTime);
		FileTimeToSystemTime(&LocalFileTime, &SysTime);

		wsprintf(buffer, TEXT("%04d%02d%02d_%02d%02d%02d"),
			SysTime.wYear,
			SysTime.wMonth,
			SysTime.wDay,
			SysTime.wHour,
			SysTime.wMinute,
			SysTime.wSecond);
	}
}

BOOL BASEFILE::RenameFile(TCHAR *buffer)
{
	BOOL wasfile;
	wasfile = IsFile();
	if (wasfile)
	{
		CloseFile();
	}
	if (MoveFile(v_szBaseName,buffer))
	{
		if (wasfile)
			InitFile(buffer);
		return TRUE;
	}
	else
	{
		if (DeleteFile(buffer))
		{
			if (MoveFile(v_szBaseName,buffer))
			{
				if (wasfile)
					InitFile(buffer);
				return TRUE;
			}
		}
		if (wasfile)
			InitFile(v_szBaseName);
		return FALSE;
	}
}
#endif

void BASEFILE::CloseFile( void ) 
{
	if (IsFile())
	{
		CloseHandle(hbase); 
		hbase = INVALID_HANDLE_VALUE; 
		v_bFileExists = FALSE;
	}
}

long BASEFILE::Filesize( void ) 
{
	DWORD dw1,dw2;

	if (IsFile())
	{
		dw1 = GetFileSize(hbase,&dw2);
		return((dw2<<16)+dw1);
	}
	return(0);
}	