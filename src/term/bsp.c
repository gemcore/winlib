#include <stdlib.h>
#include <stdio.h>
#include "bsp.h"

volatile uint32_t sys_seconds_cnt;
volatile uint16_t sys_millisec_cnt;
uint32_t bsp_millisec_cnt[NR_OF_MSEC_COUNTERS];
uint8_t bsp_millisec_cnt_flag[NR_OF_MSEC_COUNTERS];
static void(*systick_hook)(uint32_t);
static void(*timer_hook)(void);

//*****************************************************************************
//
// The interrupt handler for the for Systick interrupt.
//
//*****************************************************************************
/*!
\brief          System TIck interrupt handler
\param[in]      none
\return         none
\note
\warning
*/
void SysTickIntHandler(void)
{
	uint32_t i;
	uint32_t millisec_increment = 1000 / SYSTICK_RATE_HZ;

	// Update the Systick interrupt counters.
	sys_millisec_cnt += millisec_increment;
	if (sys_millisec_cnt >= 1000)
	{
		sys_millisec_cnt -= 1000;
		sys_seconds_cnt++;
		//if (led_heartbeat_enable)
		//{
		//	BSP_toggle_heartbeat_led();
		//}
	}
	if (systick_hook != NULL)
	{
		systick_hook(millisec_increment);
	}
	for (i = 0; i<NR_OF_MSEC_COUNTERS; i++)
	{
		if ((bsp_millisec_cnt_flag[i] & 0x03) == 0x02)
		{
			bsp_millisec_cnt[i] += millisec_increment;
		}
	}
}

int BSP_Init(void)
{
	uint32_t i;

	/* Init global variables */
	sys_seconds_cnt = 0;
	sys_millisec_cnt = 0;
	for (i = 0; i < NR_OF_MSEC_COUNTERS; i++)
	{
		bsp_millisec_cnt_flag[i] = 0x01;
	}
	systick_hook = NULL;
	timer_hook = NULL;

	return 0;
}

uint8_t BSP_claim_msec_cnt(void)
{
	uint8_t counterId = 0;

	while (counterId < NR_OF_MSEC_COUNTERS)
	{
		if (bsp_millisec_cnt_flag[counterId] & 0x01)
		{
			bsp_millisec_cnt[counterId] = 0;
			bsp_millisec_cnt_flag[counterId] &= ~(0x01);
			bsp_millisec_cnt_flag[counterId] |= 0x02;
			break;
		}
		counterId++;
	}
	return counterId;
}

void BSP_return_msec_cnt(uint8_t counterId)
{
	if (counterId < NR_OF_MSEC_COUNTERS)
	{
		bsp_millisec_cnt_flag[counterId] = 0x01;
	}
}

void BSP_reset_msec_cnt(uint8_t counterId)
{
	if (counterId < NR_OF_MSEC_COUNTERS)
	{
		bsp_millisec_cnt[counterId] = 0;
	}
}

uint32_t BSP_get_msec_cnt(uint8_t counterId)
{
	if (counterId < NR_OF_MSEC_COUNTERS)
	{
		// Convert count to millisecs.
		return bsp_millisec_cnt[counterId] * (1000 / SYSTICK_RATE_HZ);
	}
	return 0;
}

void BSP_enable_msec_cnt(uint8_t counterId, bool flag)
{
	if (flag)
	{
		bsp_millisec_cnt_flag[counterId] |= 0x02;
	}
	else
	{
		bsp_millisec_cnt_flag[counterId] &= ~(0x02);
	}
}

