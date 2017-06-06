/*0===========================================================================
**
** File:       comport.cpp
**
** Purpose:    Windows serial COM port class methods.
**
**
**
**
** Date:       Dec. 10, 2007
**
** Author:     Alan D. Graves
**
**============================================================================
*/
#include "comport.h"
//#include "canvas.h"
#include "winex.h"
#include "wassert.h"
#include <stdio.h>
#include "ascii.h"
//#include "tty.h"
#include "syserr.h"

#define ASCII_XON		DC1
#define ASCII_XOFF		DC3

#define RXQUEUE         4096
#define TXQUEUE         4096

ComPort::ComPort(int num,DWORD baud,BYTE parity,BYTE stop) :
   bPort(num), 
   dwBaudRate(baud),
   bParity(parity),
   bStopBits(stop),
   fXonXoff(FALSE),
   fDisplayErrors(FALSE),
   bByteSize(8),
   ctxbuf(512),
   crxbuf(512)
{
   // load COM prefix string and append port number
//   char szTemp[ 10 ];
//   LoadString( GETHINST(_hwnd), IDS_COMPREFIX, szTemp, sizeof(szTemp) );
//   wsprintf( szPort, "%s%d", (LPSTR)szTemp, bPort );
   sprintf( szPort, "COM%d", bPort );
   
   osWrite.Offset = 0;
   osWrite.OffsetHigh = 0;
   osRead.Offset = 0;
   osRead.OffsetHigh = 0;

   fConnected = false;

   // Let the caller Resume explicitly
   //Resume ();
}

void ComPort::Run ()
{
   printf("ComPort%d::Run()\n",bPort);
   for (;;)
   {
      if (_isDying)
         return;

      if ( fConnected )
      {
         DWORD dwEvtMask = 0;

         WaitCommEvent( id, &dwEvtMask, NULL );

         if ((dwEvtMask & EV_RXCHAR) == EV_RXCHAR)
         {
            Lock lock(_mutex);

            // do processing for communications thread here...
            Process();
         }
      }
   }
}

void ComPort::FlushThread ()
{
   printf("ComPort%d::FlushThread()\n",bPort);

   // disable event notification and wait for thread to halt
   SetCommMask( id, 0 );

   _event.Release();
}

bool ComPort::Start()
{
   Lock lock(_mutex);

   printf("ComPort%d::Start()\n",bPort);

   MultiByteToWideChar(CP_ACP, 0, szPort, -1, swPort, sizeof(swPort));

   if (fConnected)
   {
      printf("Connection already open!\n");
      return true;
   }

   COMMTIMEOUTS  CommTimeOuts;

   // open COM device
   if ((id = CreateFile( swPort, GENERIC_READ | GENERIC_WRITE,
                              0,                    // exclusive access
                              NULL,                 // no security attrs
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL |
                              FILE_FLAG_OVERLAPPED, // overlapped I/O
                              NULL )) == (HANDLE) -1 )
   {
      printf("Unabled to open %s device!\n",szPort);
      return false;
   }
   
   // get any early notifications
   if (!SetCommMask( id, EV_RXCHAR ))
   {
      printf("Failed to set comm mask!\n");
      return false;
   }

   // setup device buffers
   SetupComm( id, RXQUEUE, TXQUEUE );

   // purge any information in the buffer
   PurgeComm( id, PURGE_TXABORT | PURGE_RXABORT |
                  PURGE_TXCLEAR | PURGE_RXCLEAR );

   // set up for overlapped I/O
   CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF;
   CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
   CommTimeOuts.ReadTotalTimeoutConstant = 1000;
   // CBR_9600 is approximately 1byte/ms. For our purposes, allow
   // x2 the expected time per character for a fudge factor.
   CommTimeOuts.WriteTotalTimeoutMultiplier = 2*CBR_9600/dwBaudRate;
   CommTimeOuts.WriteTotalTimeoutConstant = 5000;
   SetCommTimeouts( id, &CommTimeOuts );

   DCB dcb;

   dcb.DCBlength = sizeof( DCB );
   GetCommState( id, &dcb );
   dcb.BaudRate = dwBaudRate;
   dcb.ByteSize = bByteSize;
   dcb.Parity   = bParity;
   dcb.StopBits = bStopBits;

   // setup hardware flow control
   dcb.fOutxDsrFlow = false;
   dcb.fDtrControl = DTR_CONTROL_ENABLE;
   dcb.fOutxCtsFlow = false;
   dcb.fRtsControl = RTS_CONTROL_ENABLE;

   // setup software flow control
   dcb.fInX = false;
   dcb.fOutX = false;
   dcb.XonChar = ASCII_XON;
   dcb.XoffChar = ASCII_XOFF;
   dcb.XonLim = 100;
   dcb.XoffLim = 100;

   // other various settings
   dcb.fBinary = true;
   dcb.fParity = true;

   if (!SetCommState( id, &dcb ))
   {
      SetCommMask( id, 0 );
      CloseHandle( id );
      printf("Failed to set comm state!\n");
      return false;
   }
   
   EscapeCommFunction( id, SETDTR );   // assert DTR

   // initialize the overlapped Read IO structure
   memset( &osRead, 0, sizeof( OVERLAPPED ) );
   
   // create I/O event used for overlapped read
   osRead.hEvent = CreateEvent( NULL,    // no security
                                TRUE,    // explicit reset req
                                FALSE,   // initial event reset
                                NULL );  // no name
   if (osRead.hEvent == NULL)
   {
      // get rid of event handle
      CloseHandle( osRead.hEvent );

      SetCommMask( id, 0 );
      CloseHandle( id );

      printf("Failed to create event for overlapped read!\n");
      return false;
   }
   // initialize the overlapped Write IO structure
   memset( &osWrite, 0, sizeof( OVERLAPPED ) );

   // create I/O event used for overlapped read
   osWrite.hEvent = CreateEvent( NULL,   // no security
                                 TRUE,   // explicit reset req
                                 FALSE,  // initial event reset
                                 NULL ); // no name
   if (osWrite.hEvent == NULL)
   {
      // get rid of event handles
      CloseHandle( osRead.hEvent );
      CloseHandle( osWrite.hEvent );

      SetCommMask( id, 0 );
      CloseHandle( id );

      printf("Failed to create event for overlapped write!\n");
      return false;
   }
   printf("Connection open\n");
   fConnected = true;
   return true;
}

bool ComPort::Stop()
{
   Lock lock(_mutex);

   printf("ComPort::Stop()\n");

   if (!fConnected)
      return true;
   
   // disable event notification and wait for thread to halt
   SetCommMask( id, 0 );

//   // block until thread has been halted
//   while (dwThreadID != 0)
//      ;

   // drop DTR
   EscapeCommFunction( id, CLRDTR );

   // purge any outstanding reads/writes and close device handle
   PurgeComm( id, PURGE_TXABORT | PURGE_RXABORT |
                  PURGE_TXCLEAR | PURGE_RXCLEAR );
   CloseHandle( id );

   // get rid of event handle for overlapped read
   CloseHandle( osRead.hEvent );

   printf("Connection terminated\n");

   // set connected flag to FALSE
   fConnected = false;
   return true;
}

//---------------------------------------------------------------------------
//  int Read( LPSTR lpszBlock, int nMaxLength )
//
//  Description:
//     Reads a block from the COM port and stuffs it into the provided buffer.
//
//  Parameters:
//     LPSTR lpszBlock
//        block used for storage
//
//     int nMaxLength
//        max length of block to read
//---------------------------------------------------------------------------

int ComPort::Read( LPSTR lpszBlock, int nMaxLength )
{
   COMSTAT    ComStat;
   DWORD      dwErrorFlags;
   DWORD      dwLength;
   DWORD      dwError;

   // only try to read number of bytes in queue
   ClearCommError( id, &dwErrorFlags, &ComStat );
   dwLength = min( (DWORD) nMaxLength, ComStat.cbInQue );

   if (dwLength > 0)
   {
      //printf("ComPort::Read() dwLength=%ld\n",dwLength);

      BOOL       fReadStat;
      fReadStat = ReadFile( id, lpszBlock, dwLength, &dwLength, &osRead );
      //printf("fReadStat=%ld\n",fReadStat);
      if (!fReadStat)
      {
         DWORD LastError = GetLastError();
         if (LastError == ERROR_IO_PENDING)
         {
            //printf("Read IO Pending\n");

            // We have to wait for read to complete. This function will timeout 
            // according to the CommTimeOuts.ReadTotalTimeoutConstant variable
            // Every time it times out, check for port errors.

            while (!GetOverlappedResult( id, &osRead, &dwLength, TRUE ))
            {
               dwError = GetLastError();
               if (dwError != ERROR_IO_INCOMPLETE) // normal result if not finished
               {
                  // IO error occurred, try to recover
   	            printf( "Read IO error=%u\n", dwError );
                  ClearCommError( id, &dwErrorFlags, &ComStat );
                  if ((dwErrorFlags > 0) && fDisplayErrors)
      	            printf( "Read error=%u\n", dwErrorFlags );
                  break;
               }
            }
	      }
         else
         {
            //printf("\nRead error=%ld ",LastError);

            // some other error occurred
            dwLength = 0;
            ClearCommError( id, &dwErrorFlags, &ComStat );
            if ((dwErrorFlags > 0) && fDisplayErrors)
	            printf( "Read error=%u\n", dwErrorFlags );
         }
      }
   }
   return dwLength;
}

//---------------------------------------------------------------------------
//  bool Write( BYTE *pByte, DWORD dwBytesToWrite )
//
//  Description:
//     Writes a block of data to the COM port.
//
//  Parameters:
//
//     BYTE *pByte
//        pointer to block of data
//
//     DWORD dwBytesToWrite
//        length of the block
//---------------------------------------------------------------------------

bool ComPort::Write( LPSTR lpByte , DWORD dwBytesToWrite)
{
   DWORD       dwBytesWritten;
   DWORD       dwErrorFlags;
   DWORD   	   dwError;
   DWORD       dwBytesSent=0;
   COMSTAT     ComStat;

   //printf("ComPort%d::Write()\n",bPort);

   BOOL        fWriteStat;
   fWriteStat = WriteFile( id, lpByte, dwBytesToWrite, &dwBytesWritten, &osWrite );
   //printf("fWriteStat=%ld\n",fWriteStat);

   // NOTE that normally the code will not execute the following because the
   // driver caches write operations. Small I/O requests (up to several thousand 
   // bytes) will normally be accepted immediately and WriteFile will return 
   // true even though an overlapped operation was specified.

   if (!fWriteStat)
   {
      DWORD LastError = GetLastError();
      if (LastError == ERROR_IO_PENDING)
      {
         //printf("Write IO Pending\n");

         // We should wait for the completion of the write operation so we know
         // if it worked or not.

         // This is only one way to do this. It might be beneficial to place the
         // write operation in a separate thread so that blocking on completion 
         // will not negatively affect the responsiveness of the UI.

         // If the write takes too long to complete, this function will timeout 
         // according to the CommTimeOuts.WriteTotalTimeoutMultiplier variable.
         // This code logs the timeout but does not retry the write.

         while (!GetOverlappedResult( id, &osWrite, &dwBytesWritten, TRUE ))
         {
            dwError = GetLastError();
            if (dwError != ERROR_IO_INCOMPLETE) // normal result if not finished
            {
               // IO error occurred, try to recover
               printf( "Write IO error=%u\n", dwError );
               ClearCommError( id, &dwErrorFlags, &ComStat );
               if (dwErrorFlags > 0 && fDisplayErrors)
                  printf( "Write error=%u\n", dwErrorFlags );
               break;
            }
            dwBytesSent += dwBytesWritten;
         }
         dwBytesSent += dwBytesWritten;

         if( dwBytesSent != dwBytesToWrite )
             printf("Probable Write Timeout\n", dwBytesSent);
         //printf("%ld bytes written\n", dwBytesSent);
      }
      else
      {
//ADG!         printf("Write error=%ld\n",LastError);

         // some other error occurred
         ClearCommError( id, &dwErrorFlags, &ComStat );
//ADG!         if (dwErrorFlags > 0 && fDisplayErrors)
//ADG!            printf( "Write error=%u\n", dwErrorFlags );
         return FALSE;
      }
   }
   return TRUE;
}

int ComPort::Write( BYTE c )
{
   if (Write((LPSTR)&c ,1))
	   return c;
   else
      return ERR_BUFFER;
}

int ComPort::TxWrite( int len, char *ptr )
{
	while (len-- > 0)
	{
		ctxbuf.insert(*ptr++);
	}
	return 0;
}

int ComPort::TxPutch( int c )
{
	return ctxbuf.insert(c);
}

int ComPort::TxGetch( void )
{     
	int c;
	if (ctxbuf.remove(&c) != 0)
	{
		c = ERR_BUFFER;
	}
	return c;
}

long ComPort::TxCount( void )
{     
	return ctxbuf.getcount();
}

int ComPort::RxWrite( int len, char *ptr )
{
	while (len-- > 0)
	{
		crxbuf.insert(*ptr++);
	}
	return 0;
}

int ComPort::RxGetch( void )
{     
	int c;
	if (crxbuf.remove(&c) != 0)
	{
		c = ERR_BUFFER;
	}
	return c;
}

long ComPort::RxCount( void )
{     
	return crxbuf.getcount();
}


