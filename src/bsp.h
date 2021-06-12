#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "dbg.h"

// Note these compile opetions are normally configured in the .cproject properties file
//#define HAS_FFS       1   // has SD Fat32 Flash File System
//#define HAS_CAN       1   // has CAN testing code
//#define HAS_OLED		1   // has a graphics OLED display
//#define HAS_LED		1   // has a heartbeat LED like on the DK-TM4C123G board
//#define HAS_MDM       1   // has serial XMODEM file transfer code (experimental)
#define HAS_PTS       1   // has Protothreads
//#define HAS_BUZ       1   // has a PWM buzzer output driven by Timer0
//#define HAS_BTN       1   // has a user push button
//#define HAS_WDT		1	// has Watchdog timer enabled
//#define HAS_TFT		1	// has a graphics TFT display
//#define HAS_TMR		1	// has Timer interrupts
//#define HAS_SEE		1	// has I2C EEPROM
//#define HAS_USB		1	// has USB
//#define HAS_TMRS      1   // has Application Timer callbacks (experimental)
//#define HAS_LFS       1   // has Little File System
#define HAS_LOG       1   // has system logging
#define HAS_CLI       1   // Console handling
#define HAS_EVT       1   // Event handling
#define HAS_TRM       1   // ANSI terminal

#define	SYSTICK_RATE_HZ			100

#define NR_OF_MSEC_COUNTERS		32

#define SAP_MODE_STA            2
#define SAP_MODE_APT            1
#define SAP_MODE_OFF            0

#define SBL_BOOT_MCU            2
#define SBL_BOOT_FFS            1
#define SBL_BOOT_OFF            0

#define CLI_VERB_HIGH           3
#define CLI_VERB_MEDIUM         2
#define CLI_VERB_LOW            1
#define CLI_VERB_OFF            0


#define byte		uint8_t
#define word		uint16_t

extern void SysTickIntHandler(void);
extern void TMR_Delay(int ticks);

int BSP_Init(void);
uint8_t BSP_claim_msec_cnt(void);
void BSP_return_msec_cnt(uint8_t counterId);
void BSP_reset_msec_cnt(uint8_t counterId);
uint32_t BSP_get_msec_cnt(uint8_t counterId);
void BSP_enable_msec_cnt(uint8_t counterId, bool flag);

void BSP_enable_heartbeat_led(bool flag);
bool BSP_set_heartbeat_led(bool state);
bool BSP_toggle_heartbeat_led(void);
void BSP_pulse_heartbeat_led(void);
void BSP_msec_delay(uint32_t interval);


#ifdef    __cplusplus
}
#endif // __cplusplus

