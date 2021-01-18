/*
 * cfg.h
 *
 *  Created on: Dec 9, 2014
 *      Author: Alan
 */

#ifndef CFG_H_
#define CFG_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "bsp.h"            // board support functions

//*****************************************************************************
// Definition of types
//*****************************************************************************
#define CFG_DISABLE     0
#define CFG_ENABLE      1

#pragma pack (push,1)
typedef struct
{
    byte pad[4];        // 00-03: FF, FF, FF, FF
    byte sig[2];        // 04-05: 55, AA
    byte len;           // 06   :
    word crc;           // 07-08:
    word dflag;         // 09-10: debug output flags
    byte sap;           // 11   : 0=STA, 1=APT, FF=OFF
    byte hib;           // 12   : 0=disable, 1=enable
    byte rtc;           // 13   : 0=disable, 1=enable
    byte ffs;           // 14   : 0=disable, 1=enable
    byte rfs;           // 15   : 0=disable, 1=enable
    byte result;        // 16   : 0=disable, 1=enable
    byte verb;          // 17   : 0=off, 1=low, 2=medium, 3=high
    byte conid;			// 18   : console id
    byte sbl;           // 19   : 0=MCU, 1=FFS, FF=OFF
} tsSettings;
#pragma pack (pop)

//*****************************************************************************
// Definition of global variables
//*****************************************************************************
extern tsSettings sSettings;

//*****************************************************************************
// Definition of interface functions
//*****************************************************************************
bool CFG_IsSblMCU(void);
bool CFG_IsSblFFS(void);
bool CFG_IsSblOFF(void);
void CFG_SetSbl(byte mode);
byte CFG_GetSbl(void);
void CFG_SetRfs(byte mode);
byte CFG_GetRfs(void);
void CFG_SetFfs(byte mode);
byte CFG_GetFfs(void);
void CFG_SetHib(byte mode);
byte CFG_GetHib(void);
void CFG_SetRtc(byte mode);
byte CFG_GetRtc(void);
void CFG_SetCliResult(byte mode);
byte CFG_GetCliResult(void);
void CFG_SetCliVerbosity(byte level);
byte CFG_GetCliVerbosity(void);
bool CFG_IsTrace(word mask);
void CFG_ToggleTrace(word mask);
word CFG_GetTrace(void);
bool CFG_IsSapSTA(void);
bool CFG_IsSapAPT(void);
bool CFG_IsSapOFF(void);
void CFG_SetSap(byte mode);
byte CFG_GetSap(void);
void CFG_SetConId(byte id);
byte CFG_GetConId(void);
void CFG_Init(void);
void CFG_Save(bool flag);
bool CFG_Load(void);

//*****************************************************************************
// Definition of CLI commands
//*****************************************************************************
int Cmd_cfg(int argc, char *argv[]);

#ifdef    __cplusplus
}
#endif // __cplusplus

#endif /* CFG_H_ */
