/*
 * cfg.c
 *
 *  Created on: Dec 9, 2014
 *      Author: Alan
 */

#include <string.h>
#include <stdbool.h>
#ifndef WIN32
#include "ustdlib.h"
#else
#include <stdlib.h>
#endif
#include "cli.h"            // Console handling
#ifndef WIN32
#if HAS_PBS == 1
#include "utils/flash_pb.h"     // Parameter Block settings
#else
#include "driverlib/eeprom.h"   // EEPROM On-Chip settings
#endif
#endif
#include "dbg.h"
#if HAS_CLI == 1
#include "cli.h"
#endif
#include "cfg.h"

#define HAS_CFG_SAVE

#undef TRACE
//#define TRACE(format,...)    CON_printf(format,##__VA_ARGS__)
//#define TRACEF(format,...)  do{CON_printf(format,##__VA_ARGS__);CON_Flush();}while(0)   // wait for data to be sent
#define TRACE(format,...)
#define TRACEF(format,...)

//#define DFLAG_DEFAULTS  (DFLAG_TRC | DFLAG_EVT | DFLAG_RTC | DFLAG_CAN | DFLAG_USB | DFLAG_CAP | DFLAG_ECU)
#define DFLAG_DEFAULTS  (DFLAG_TRC)

/****************************************************************************
** Define configuration settings
*****************************************************************************/
tsSettings sSettings;

/****************************************************************************
** Define configuration functions
*****************************************************************************/
bool CFG_IsSblMCU(void)
{
    return (sSettings.sbl==SBL_BOOT_MCU)? true : false;
}

bool CFG_IsSblFFS(void)
{
    return (sSettings.sbl==SBL_BOOT_FFS)? true : false;
}

bool CFG_IsSblOFF(void)
{
    return (sSettings.sbl==SBL_BOOT_OFF)? true : false;
}

void CFG_SetSbl(byte boot)
{
    sSettings.sbl = boot;
}

byte CFG_GetSbl(void)
{
    return sSettings.sbl;
}

void CFG_SetRfs(byte state)
{
    sSettings.rfs = state;
}

byte CFG_GetRfs(void)
{
    return sSettings.rfs;
}

void CFG_SetFfs(byte state)
{
    sSettings.ffs = state;
}

byte CFG_GetFfs(void)
{
    return sSettings.ffs;
}

void CFG_SetHib(byte state)
{
    sSettings.hib = state;
}

byte CFG_GetHib(void)
{
    return sSettings.hib;
}

void CFG_SetRtc(byte state)
{
    sSettings.rtc = state;
}

byte CFG_GetRtc(void)
{
    return sSettings.rtc;
}

byte CFG_GetCliResult(void)
{
    return sSettings.result;
}

void CFG_SetCliResult(byte state)
{
    sSettings.result = state;
}

byte CFG_GetCliVerbosity(void)
{
    return sSettings.verb;
}

void CFG_SetCliVerbosity(byte level)
{
    sSettings.verb = level;
}

bool CFG_IsTrace(word mask)
{
    return (sSettings.dflag&mask)? true : false;
}

void CFG_ToggleTrace(word mask)
{
    sSettings.dflag ^= mask;
}

word CFG_GetTrace(void)
{
    return sSettings.dflag;
}

bool CFG_IsSapSTA(void)
{
    return (sSettings.sap==SAP_MODE_STA)? true : false;
}

bool CFG_IsSapAPT(void)
{
    return (sSettings.sap==SAP_MODE_APT)? true : false;
}

bool CFG_IsSapOFF(void)
{
    return (sSettings.sap==SAP_MODE_OFF)? true : false;
}

void CFG_SetSap(byte mode)
{
    sSettings.sap = mode;
}

byte CFG_GetSap(void)
{
    return sSettings.sap;
}

void CFG_SetConId(byte id)
{
    sSettings.conid = id;
}

byte CFG_GetConId(void)
{
    return sSettings.conid;
}

const char *SblToStr(byte m)
{
    static const char *valtostr[]={"off", "ffs", "mcu"};
    if (m > 2)
    {
        return "Unknown";
    }
    return valtostr[m];
}

const char *SapToStr(byte m)
{
    static const char *valtostr[]={"off", "apt", "sta"};
    if (m > 2)
    {
        return "Unknown";
    }
    return valtostr[m];
}

const char *VerbToStr(byte m)
{
    static const char *valtostr[]={"off", "low", "medium", "high"};
    if (m > 3)
    {
        return "Unknown";
    }
    return valtostr[m];
}

const char *ConIdToStr(byte m)
{
    static const char *valtostr[]={"dbg", "usb", "net"};
    if (m > 2)
    {
        return "Unknown";
    }
    return valtostr[m];
}

void CFG_Init(void)
{
#ifndef WIN32
#if HAS_PBS == 1
    // Initialize the Flash Parameter Blocks
    // Reserve the last two pages (2x2048 byte) of the 512Kbyte flash
    const uint32_t size = 4096;
    const uint32_t blocks = 2;   // min. # of blocks is 2
    FlashPBInit(0x80000-size, 0x80000, sizeof(tsSettings));
    TRACE("NVM: Init PB block: count=%d size=%d\n", blocks, size/blocks);
#else
    SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0); // EEPROM activate
    EEPROMInit(); // EEPROM start
    uint32_t size = EEPROMSizeGet();
    uint32_t blocks = EEPROMBlockCountGet();
    TRACE("NVM: Init EE block: count=%d size=%d\n", blocks, size/blocks);
#endif
#endif
    if (!CFG_Load())
    {
        // Restore to default settings
        CFG_Save(false);
    }
}

void CFG_Save(bool flag)
{
    byte aucBuffer[sizeof(tsSettings)];
   
    TRACEF("CFG: Save(%d) ",flag);
    sSettings.pad[0] = 0xFF;
    sSettings.pad[1] = 0xFF;
    sSettings.pad[2] = 0xFF;
    sSettings.pad[3] = 0xFF;
    sSettings.sig[0] = 0x55;
    sSettings.sig[1] = 0xAA;
    sSettings.len = 0xFF;
    sSettings.crc = 0xFFFF;
    if (!flag)
    {
        TRACEF("restore defaults ");
        // Initialize default settings
        sSettings.dflag = DFLAG_DEFAULTS;
        sSettings.sap = SAP_MODE_OFF;   // SAP OFF mode
        sSettings.rfs = CFG_ENABLE;     // Enable RFS initialization
        sSettings.ffs = CFG_ENABLE;     // Enable FFS initialization
        sSettings.hib = CFG_DISABLE;    // Disable Hibernation mode
        sSettings.rtc = CFG_DISABLE;    // Disable RTC display
        sSettings.result = CFG_ENABLE;  // Enable CLI result string mode
        sSettings.verb = CLI_VERB_HIGH; // CLI HIGN verbosity
        sSettings.conid = CONID_DBG;    // DBG Console
        sSettings.sbl = SBL_BOOT_OFF;   // SBL OFF boot
    }
    memcpy(&aucBuffer[0], (byte *)&sSettings, sizeof(sSettings));
    int i;
    for (i=0; i < sizeof(aucBuffer); i++)
    {
      TRACE("%02x", aucBuffer[i]&0xFF);
    }
    TRACE("\n");
#ifdef HAS_CFG_SAVE
#ifndef WIN32
#if HAS_PBS == 1
    FlashPBSave(aucBuffer);
#else
    // Write struct to EEPROM start from 0x0000
    EEPROMProgram((uint32_t *)&aucBuffer[0], 0x0000, sizeof(sSettings));
#endif
#endif
#endif
    TRACEF("Ok\n");
}

bool CFG_Load(void)
{
    byte aucBuffer[sizeof(tsSettings)];
    tsSettings *tp = (tsSettings *)aucBuffer;

    TRACEF("CFG: Load ");
#ifndef WIN32
#if HAS_PBS == 1
    byte *pucPB;

    // Read the current parameter block.
    pucPB = FlashPBGet();
    TRACEF("PB=%x ",pucPB);

    if (pucPB)
    {
        memcpy(aucBuffer, pucPB, sizeof(aucBuffer));
#else
        // Read from struct at EEPROM start from 0x0000
        EEPROMRead((uint32_t *)&aucBuffer[0], 0x0000, sizeof(sSettings));
#endif
#endif
        int i;
        for (i=0; i < sizeof(aucBuffer); i++)
        {
          TRACE("%02x", aucBuffer[i]&0xFF);
        }
        TRACE("\n");
        TRACE("len=%02x crc=%04x sig[0]=%02x sig[1]=%02x\n", tp->len, tp->crc, tp->sig[0], tp->sig[1]);
        if (tp->sig[0] == 0x55 && tp->sig[1] == 0xAA)
        {
            memcpy((byte *)&sSettings, &aucBuffer[0], sizeof(sSettings));
            TRACEF("dbg=%04x sap=%s rfs=%x ffs=%x hib=%x rtc=%x result=%x verb=%s con=%s sbl=%s\n", \
                sSettings.dflag, \
                SapToStr(sSettings.sap), \
                sSettings.rfs, \
                sSettings.ffs, \
                sSettings.hib, \
                sSettings.rtc, \
                sSettings.result, \
                VerbToStr(sSettings.verb), \
                ConIdToStr(sSettings.conid), \
                SblToStr(sSettings.sbl));
            return true;
        }
        else
        {
            TRACEF("invalid signature\n");
            return false;
        }
#ifndef WIN32
#if HAS_PBS == 1
    }
    else
    {
        TRACEF("invalid block\n");
        return false;
    }
#endif
#endif
}

static void usage(void)
{
    CON_printf("load  : load configuration\n");
    CON_printf("save  : save configuration\n");
    CON_printf("dbg   : toggle debug flag\n");
//#if HAS_SAP == 1
    CON_printf("sap   : set sap mode\n");
//#endif
//#if HAS_RFS == 1
    CON_printf("rfs   : set ram file system\n");
//#endif
//#if HAS_FFS == 1
    CON_printf("ffs   : set flash file system\n");
//#endif
//#if HAS_HIB == 1
    CON_printf("hib   : set hibernation\n");
    CON_printf("rtc   : set display\n");
//#endif
//#if HAS_CLI == 1
    CON_printf("result: set enable/disable\n");
    CON_printf("verb  : set verbostiy 0-3\n");
//#endif
    CON_printf("con   : set console id\n");
    CON_printf("sbl   : set sbl boot\n");
}

int Cmd_cfg(int argc, char *argv[])
{
    if (argc > 1)
    {
        char *p = argv[1];
        if (!strcmp(p,"load"))
        {
            // Load and print the configuration to the console.
            CFG_Load();
        }
        else if (!strcmp(p,"save"))
        {
            bool save = true;
            // Save the current configuration to flash.
            if (argc > 2)
            {
                if (!strcmp(argv[2],"ff"))
                {
                    // Reset settings to 'Factory Fresh'.
                    save = false;
                }
            }
               CFG_Save(save);
        }
        else if (!strcmp(p,"dbg"))
        {
            word mask = 0x0000;
            byte bit = 0;
            if (argc > 2)
            {
                if (!strcmp(argv[2],"?"))
                {
                    CON_printf("bit: "
                        "0=TRC "
                        "1=EVT "
                        "2=RTC "
                        "3=CAN "
                        "4=USB "
                        "5=OBD "
                        "6=CAP "
                        "7=SSI "
                        "8=SPF "
                        "9=FTL "
                        "10=RDB "
                        "\n");
                }
                else
                {
                    mask = 0x0001;
                }
                bit = atoi(argv[2]);
                mask <<= bit;
            }
            CFG_ToggleTrace(mask);
            CON_printf("dbg=%04x\n",CFG_GetTrace());
        }
        else if (!strcmp(p,"sbl"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"mcu"))
                {
                    CFG_SetSbl(SBL_BOOT_MCU);
                }
                else if (!strcmp(p,"ffs"))
                {
                    CFG_SetSbl(SBL_BOOT_FFS);
                }
                else if (!strcmp(p,"off"))
                {
                    CFG_SetSbl(SBL_BOOT_OFF);
                }
                else
                {
                    CON_printf("flash: set sbl=mcu\n");
                    CON_printf("ffs:   set sbl=ffs\n");
                    CON_printf("off:   set sbl=off\n");
                }

            }
            CON_printf("sbl=%s\n",SblToStr(CFG_GetSbl()));
        }
        else if (!strcmp(p,"sap"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"sta"))
                {
                    CFG_SetSap(SAP_MODE_STA);
                }
                else if (!strcmp(p,"apt"))
                {
                    CFG_SetSap(SAP_MODE_APT);
                }
                else if (!strcmp(p,"off"))
                {
                    CFG_SetSap(SAP_MODE_OFF);
                }
                else
                {
                    CON_printf("sta: set sap=station\n");
                    CON_printf("apt: set sap=access point\n");
                    CON_printf("off: set sap=off\n");
                }
            }
            CON_printf("sap=%s\n",SapToStr(CFG_GetSap()));
        }
        else if (!strcmp(p,"rfs"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"1") || !strcmp(p,"enable"))
                {
                    CFG_SetRfs(CFG_ENABLE);
                }
                else if (!strcmp(p,"0") || !strcmp(p,"disable"))
                {
                    CFG_SetRfs(CFG_DISABLE);
                }
                else
                {
                    CON_printf("disable: set rfs=0\n");
                    CON_printf("enable : set rfs=1\n");
                }

            }
            CON_printf("rfs=%s\n",(CFG_GetRfs() == CFG_ENABLE)?"enable":"disable");
        }
        else if (!strcmp(p,"ffs"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"1") || !strcmp(p,"enable"))
                {
                    CFG_SetFfs(CFG_ENABLE);
                }
                else if (!strcmp(p,"0") || !strcmp(p,"disable"))
                {
                    CFG_SetFfs(CFG_DISABLE);
                }
                else
                {
                    CON_printf("disable: set ffs=0\n");
                    CON_printf("enable : set ffs=1\n");
                }

            }
            CON_printf("ffs=%s\n",(CFG_GetFfs() == CFG_ENABLE)?"enable":"disable");
        }
        else if (!strcmp(p,"hib"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"1") || !strcmp(p,"enable"))
                {
                    CFG_SetHib(CFG_ENABLE);
                }
                else if (!strcmp(p,"0") || !strcmp(p,"disable"))
                {
                    CFG_SetHib(CFG_DISABLE);
                }
                else
                {
                    CON_printf("disable: set hib=0\n");
                    CON_printf("enable : set hib=1\n");
                }

            }
            CON_printf("hib=%s\n",(CFG_GetHib() == CFG_ENABLE)?"enable":"disable");
        }
        else if (!strcmp(p,"rtc"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"1") || !strcmp(p,"enable"))
                {
                    CFG_SetRtc(CFG_ENABLE);
                }
                else if (!strcmp(p,"0") || !strcmp(p,"disable"))
                {
                    CFG_SetRtc(CFG_DISABLE);
                }
                else
                {
                    CON_printf("disable: set rtc=0\n");
                    CON_printf("enable : set rtc=1\n");
                }

            }
            CON_printf("rtc=%s\n",(CFG_GetRtc() == CFG_ENABLE)?"enable":"disable");
        }
        else if (!strcmp(p,"result"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"1") || !strcmp(p,"enable"))
                {
                    CFG_SetCliResult(CFG_ENABLE);
                }
                else if (!strcmp(p,"0") || !strcmp(p,"disable"))
                {
                    CFG_SetCliResult(CFG_DISABLE);
                }
                else
                {
                    CON_printf("disable: set result=0\n");
                    CON_printf("enable : set result=1\n");
                }
            }
            CON_printf("result=%s\n",(CFG_GetCliResult() == CFG_ENABLE)?"enable":"disable");
        }
        else if (!strcmp(p,"verb"))
        {
            // Set verbosity (0: RC, 1: [RC]/[Ok], 2: [RC, #RC]/[Ok], 3: CLI:[RC, #RC]/[Ok])
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"3") || !strcmp(p,"high"))
                {
                    // Enable high cli result verbosity
                    //  CLI:[RC, #RC]
                    CFG_SetCliVerbosity(3);
                } 
                else if (!strcmp(p,"2") || !strcmp(p,"medium"))
                {
                    // Enable medium cli result verbosity
                    //  [RC, #RC]
                    CFG_SetCliVerbosity(2);
                }
                else if (!strcmp(p,"1") || !strcmp(p,"low"))
                {
                    // Enable low cli result verbosity
                    //  [#RC]
                    CFG_SetCliVerbosity(1);
                }
                else if (!strcmp(p,"0") || !strcmp(p,"off"))
                {
                    // Disable command line result processing.
                    CFG_SetCliVerbosity(0);
                }
                else
                {
                    CON_printf("high  : set verbosity=3\n");
                    CON_printf("medium: set verbosity=2\n");
                    CON_printf("low   : set verbosity=1\n");
                    CON_printf("off   : set verbosity=0\n");
                }
            }
            CON_printf("verb=%s\n",VerbToStr(CFG_GetCliVerbosity()));
        }
        else if (!strcmp(p,"con"))
        {
            if (argc > 2)
            {
                char *p = argv[2];
                if (!strcmp(p,"0") || !strcmp(p,"dbg"))
                {
                    CFG_SetConId(0);
                }
#if HAS_USB == 1
#if HAS_CDC == 1
                else if (!strcmp(p,"1") || !strcmp(p,"usb"))
                {
                    CFG_SetConId(1);
                }
#endif
#endif
#if HAS_NET == 1
                else if (!strcmp(p,"2") || !strcmp(p,"net"))
                {
                    CFG_SetConId(2);
                }
#endif
                else
                {
                    CON_printf("dbg   : set con=0\n");
#if HAS_USB == 1
#if HAS_CDC == 1
                    CON_printf("usb   : set con=1\n");
#endif
#endif
#if HAS_NET == 1
                    CON_printf("net   : set con=2\n");
#endif
                }

            }
            CON_printf("con=%s\n",ConIdToStr(CFG_GetConId()));
        }
        else if (!strcmp(p,"?"))
        {
            usage();
        }
        else
        {
            return(CMDLINE_INVALID_ARG);
        }
    }
    else
    {
        CON_printf("dbg=%04x sap=%s rfs=%x ffs=%x hib=%x rtc=%x result=%x verb=%s con=%s sbl=%s\n", \
            CFG_GetTrace(), \
            SapToStr(CFG_GetSap()), \
            CFG_GetRfs(), \
            CFG_GetFfs(), \
            CFG_GetHib(), CFG_GetRtc(), \
            CFG_GetCliResult(), \
            VerbToStr(CFG_GetCliVerbosity()), \
            ConIdToStr(CFG_GetConId()), \
            SblToStr(CFG_GetSbl()));
    }
    return(0);
}
