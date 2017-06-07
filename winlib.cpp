// winlib.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"
#include "src\comport.h"
#include "src\tmr.h"

ComPort com(3, CBR_115200);

void ComPort::Process(void)
{
}

class ATmr : CTimerFunc
{
	void Func(void)
	{
		printf(".");
	}
};

ATmr atmr;

int main()
{
	printf("winlib\n");

	TMR_Init();

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

	printf("\nperiodic timer ");
	TMR_Event(5, (CTimerEvent *)&atmr, PERIODIC);
	TMR_Delay(20);
	printf("2 seconds ");
	TMR_Delay(50);
	printf("5 seconds ");
	TMR_Event(0, (CTimerEvent *)&atmr, PERIODIC);
	printf("\n");
	TMR_Term();

	printf("exiting\n");
	return 0;
}

