/*
 * evt.cpp
 *
 *  Created on: Oct 31, 2014
 *      Author: Alan
 */

#ifndef __EVT_H__
#define __EVT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Type definitions
 ******************************************************************************/
// Events types
typedef enum
{
	EVT_NUL=0,
	EVT_RST,
	EVT_BTN,
	EVT_MNU,
	EVT_WDT,
	EVT_CAN,
	EVT_CFG,
	EVT_USB,
	EVT_OBD,
	EVT_HIB,
	EVT_NET,
	EVT_TCP,
	EVT_SPT,
	EVT_SSI,
	EVT_WAP,
	EVT_UIP,
	EVT_LIN,
} teEvent;

/*******************************************************************************
 * Exported functions
 ******************************************************************************/
void EVT_SetEnable(bool flag);
bool EVT_GetEnable(void);
int	 EVT_Init(void);
uint8_t EVT_GetId(void);
void EVT_Make(uint8_t *evt, int code, uint8_t *bufptr, int buflen);
void EVT_Process(void);

//*****************************************************************************
// Definition of CLI commands
//*****************************************************************************
int Cmd_evt(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* __EVT_H__ */
