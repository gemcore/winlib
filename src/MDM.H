/*
 * mdm.h
 *
 *  Created on: Aug 08, 2018
 *      Author: Alan
 */

#ifndef  MDM_H_
#define  MDM_H_

#include <stdint.h>
#include <stdbool.h>
#include "ascii.h"

typedef unsigned char byte;

#if defined __cplusplus
extern "C" {
#endif
//#include "xmodem/xmodem.h"
/*******************************************************************************
* Type definitions
******************************************************************************/
/* Message defines */
#define XMODEM_PACKET_1K 1028       /* Data packet size - xmodem 1k */
#define XMODEM_PACKET    132        /* Data packet size - xmodem standard */
#define XMODEM_HEADER    4          /* Header size of data packets */
#define XMODEM_DATA      3          /* Data index in data packets */

/* Timer values (100ms) */
#define XMODEM_TIME_RX  100         /* Receive timeout (in 100ms ticks) */

#define XMODEM_SUCCESS                 ( 0  )

/* XModem result codes. */
#define XMODEM_ERR_CANCEL              (-301)
#define XMODEM_ERR_SYNC                (-302)
#define XMODEM_ERR_SEND                (-303)
#define XMODEM_ERR_RETRY               (-304)

/*******************************************************************************
 * Type definitions
 ******************************************************************************/
/* Maximum number of retransmissions */
#define XMODEM_MAX_RETRIES 5

enum
{
   MDM_STATE_IDLE=0,
   MDM_STATE_RECV,
   MDM_STATE_SEND
};

//*****************************************************************************
// API functions
//*****************************************************************************
int MDM_Init(void);
void MDM_Timer(void);
void MDM_SetDir(bool dir);

//*****************************************************************************
// Definition of CLI commands
//*****************************************************************************
int Cmd_mdm(int argc, char *argv[]);

#if defined __cplusplus
}
#endif

#endif  /* MDM_H_ */


