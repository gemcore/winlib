// winlib.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"
#include "src\comport.h"

ComPort com(3, CBR_115200);

void ComPort::Process(void)
{
}

int main()
{
	printf("winlib\n");
	if (com.Start())
	{
		unsigned char buf[80];
		com.Write("AT\r",3);
		com.Sleep(100);
		int n = com.Read((LPSTR)buf, sizeof(buf));
		if (n > 0)
		{
			for (int i = 0; i < n; i++)
			{
				printf("%c",buf[i]);
			}
			printf("\n");
		}
		com.Stop();
	}
    return 0;
}

