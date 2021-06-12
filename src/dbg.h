#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Type definitions
******************************************************************************/
#define DFLAG_TRC   0x0001
#define DFLAG_EVT	0x0002
#define DFLAG_RTC	0x0004
#define DFLAG_CAN	0x0008
#define DFLAG_USB	0x0010
#define DFLAG_OBD	0x0020
#define DFLAG_CAP	0x0040
#define DFLAG_SSI   0x0080
#define DFLAG_SPF   0x0100
#define DFLAG_FTL   0x0200
#define DFLAG_RDB   0x0400

#define CONID_DBG       0
#define CONID_USB       1
#define CONID_NET       2

int CON_Init(void);
int CON_kbhit();
int CON_getc();
int CON_putc(char c);
void CON_ungetc(char c);
void CON_printf(const char *format, ...);
void CON_trace(const char *pcFormat, ...);
void CON_Flush(void);
void CON_EchoSet(bool state);
bool CON_EchoGet(void);
void CON_NLSet(bool state);
bool CON_NLGet(void);

#ifdef    __cplusplus
}
#endif // __cplusplus
