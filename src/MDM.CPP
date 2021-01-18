/*
 * mdm.c
 *
 *  Created on: Aug 08, 2018
 *   Author: Alan
 *
 * Provides support for the XModem protocol and processes.
 */
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <stdio.h>
#include <windows.h>
#include <process.h>
#ifdef WIN32
#include "active.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
/* Local include files */
//#include "ustdlib.h"
//#include "utils/cmdline.h"
//#include "bsp.h"
//#include "pts.hpp"          // Scheduler
//#include "cli.h"			// console handling
//#include "ffs.h"			// Flash File System
//#include "fio.h"
#include "mdm.h"

#define MDM_STREAM_ID	1
#define CMDLINE_INVALID_ARG	-1
#define CLI_ClrStream	
#define CLI_SetStream	
#define CLI_SetEnable	
#define FIO_FILE	FILE
#define DBGprintf	
#define DBGFlushTx	
#define CON_printf	
#define CON_getc	
#define CON_putc
#define CON_kbhit	
#define PTS_DELAY
#define PTS_BEGIN
#define PTS_END
#define PTS_AddTask
#define PTS_GetCurTaskId
#define Name
#define PTS_GetThread
#define PTS_RESTART
#define PTS_WAIT_UNTIL


bool file_open = false;


#if 1
#if HAS_LOG == 1
#undef TRACE
#define TRACE(format,...)   LOG_Printf(format,##__VA_ARGS__)
#define TRACEF(format,...)  do{LOG_Printf(format,##__VA_ARGS__);LOG_Flush();}while(0)   // wait for data to be sent
#else
#define TRACE(format,...)	DBGprintf(format,##__VA_ARGS__)
#define TRACEF(format,...)  do{DBGprintf(format,##__VA_ARGS__);DBGFlushTx(false);}while(0)
#endif
#else
#define TRACE(...)
#define TRACEF(...)
#endif

/*******************************************************************************
 * Type definitions
 ******************************************************************************/
/* Standard characters */
//  SOH - Ctrl-a Xmodem 128 start of data
//  STX - Ctrl-b Xmodem 1K start of data
//  EOT - Ctrl-d Xmodem end of transmission
//  ACK - Ctrl-f Xmodem acknowledge
//  NAK - Ctrl-u Xmodem negative acknowledge
//  CAN - Ctrl-x Xmodem cancel transfer

/* Xmodem protocol data structure */
typedef struct
{
    uint8_t id;                     /* PTS Thread id */
    volatile bool enable;           /* Enable/Disable */
    int state;                      /* State-machine */
    FIO_FILE *fp = NULL;            /* File pointer */
} tsMdm;

#ifdef WIN32
class MDM_Thread : public ActiveObject
#else
class MDM_Thread : public PTS_Thread
#endif
{
#ifdef WIN32
private:
	void InitThread() { }
	void Run();
	void FlushThread() { }
#endif

public:
   bool bReceive;                   /* Transfer direction (true=Recv, false=Send) */
   bool bComplete;                  /* Complete */
   uint8_t au8Packet[XMODEM_PACKET_1K];/* Data packet */
   uint32_t u32Start;               /* Start of data in flash */
   uint32_t u32End;                 /* End of data in flash */
   uint32_t u16Timer;               /* Receive timer */
   uint32_t u16Duration;            /* Statistics: Duration */
   uint16_t u16Acks;                /* Statistics: Acks */
   uint16_t u16Naks;                /* Statistics: Naks */
   uint16_t u16Packets;             /* Packets total */
   uint16_t u16Size;                /* Xmodem expected packet length */
   uint16_t u16Length;              /* Packet length */
   uint8_t u8Packet;                /* Current packet */
   uint8_t u8Nak;                   /* Nak */
   uint8_t u8Can;                   /* Can */
   uint8_t u8Check;                 /* Checksum */
   uint8_t u8Retries;               /* Number of retries */
   uint8_t u8PacketNo;				/* Next Packet number to send */

   MDM_Thread() { Name("MDM "); }
   ~MDM_Thread() {}

   virtual int Init();
   virtual bool Run();

    void HandleChar(uint8_t u8Char); 
    void TxResp(char c);
    void TxPacket(uint8_t crc, uint16_t Len, uint8_t *Data);
    void RxPacket(uint8_t Check, uint16_t Len, uint8_t *Data);
    void Timeout(void);
    void Start(bool dir);
    void End(void);
    void Timer(void);
};

/*******************************************************************************
 * Variables
 ******************************************************************************/
tsMdm mdm;                  /* Xmodem dat structure */

static char *mdm_state_str[]=
{
    "IDLE",
    "RECV",
    "SEND"
};

/*******************************************************************************
 * Functions
 ******************************************************************************/
void MEM_Trace(uint8_t *data,uint16_t len,uint32_t base)
{
    uint16_t i,j;

    //TRACE("MEM: @%08x len=%04x\n",data,len);
    for (i=0; i < len; i+=16)
    {
        TRACE(" %06x: ",base+i);
        for (j=0; j < 16; j++)
        {
            if (j != 0)
            {
                if (!(j%8))
                    TRACE(" ");
                if (!(j%1))
                    TRACE(" ");
            }
            if ((i+j) < len)
                TRACE("%02x",data[i+j]);
            else
                TRACE("  ");
        }
        TRACE("  ");
        for (j=0; j < 16 && (i+j) < len; j++)
        {
            if ((i+j) < len)
            {
                if (isprint(data[i+j]))
                    TRACE("%c",data[i+j]);
                else
                    TRACE(".");
            }
            else
                TRACE(" ");
        }
        TRACE("\n");
    }
}

void TraceChar(byte b)
{
    if (isprint(b))
        TRACEF("%c",b);
    else if (b == '\b')
        TRACEF("%c",b);
    else if (b == '\r')
        TRACEF("<CR>");
    else if (b == '\n')
        TRACEF("<LF>");
    else if (b == '\0')
        TRACEF("<NUL>");
    else if (b == SOH)
        TRACEF("<SOH>");
    else if (b == STX)
        TRACEF("<STX>");
    else if (b == EOT)
        TRACEF("<EOT>");
    else if (b == CAN)
        TRACEF("<CAN>");
    else if (b == ACK)
        TRACEF("<ACK>");
    else if (b == NAK)
        TRACEF("<NAK>");
    else
        TRACEF("[%02x]",b&0xFF);
}

static uint32_t data_write(uint16_t len, uint8_t *data)
{
    TRACEF("data_write(%04x,%x)\n",len,data);

    // Write a block of data to the file.
    UINT ui32BytesWrite;
    ui32BytesWrite = fio_write(mdm.fp, data, sizeof(uint8_t), len);
    return ui32BytesWrite;
}

static uint32_t data_read(uint16_t len, uint8_t *data)
{
    TRACEF("data_read(%04x,%x)\n",len,data);
    
    // Reade a block of data from the file.
    UINT ui32BytesRead;
    ui32BytesRead = fio_read(mdm.fp, data, sizeof(uint8_t), len);
    return ui32BytesRead;
}

static unsigned short mdm_crc16_ccitt(const unsigned char* data, uint16_t length)
{
    unsigned char x;
    unsigned short crc = 0xFFFF;

    while (length--)
    {
        x = crc >> 8 ^ *data++;
        x ^= x >> 4;
        crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
    }
    return crc;
}

void MDM_SetEnable(bool flag)
{
	mdm.enable = flag;
}

bool MDM_GetEnable(void)
{
	return mdm.enable;
}

/*******************************************************************************
 * MDM_Init    Initialise Xmodem.
 ******************************************************************************/
int MDM_Thread::Init(void)
{
    mdm.id = PTS_GetCurTaskId();
    TRACE("MDM: Init id=%d\n", mdm.id);

    /* Nothing happening */
    mdm.state = MDM_STATE_IDLE; 

    /* Stop timer for xmodem */
    u16Timer = 0;

    /* Initialise nak reply timer function needs a pointer to this */

    /* Note size of data packets */

	mdm.enable = true;

    return 0;
}

bool MDM_Thread::Run()
{
    PTS_BEGIN();

    PTS_WAIT_UNTIL(MDM_GetEnable());

    mdm.state = MDM_STATE_IDLE;
    PTS_WAIT_UNTIL(mdm.state != MDM_STATE_IDLE);
    TRACEF("MDM: Run() state=%s\n", mdm_state_str[mdm.state]);

    while (1)
    {
        PTS_DELAY(3);

        if (mdm.state == MDM_STATE_IDLE)
        {
            CLI_ClrStream(MDM_STREAM_ID);

            /* Enable command line processing */
            CLI_SetEnable(true);
            PTS_RESTART();
        }

        while (CON_kbhit())
        {
            uint8_t c = CON_getc();
            HandleChar(c);
        }
    }

    PTS_END();
}

/*******************************************************************************
 * HandleChar - Handle Xmodem mode processing of characters.
 ******************************************************************************/
void MDM_Thread::HandleChar(uint8_t u8Char)
{
    if (mdm.state == MDM_STATE_RECV)
    {
        TraceChar(u8Char);

        /* If first character of packet ? */
        if (u16Length == 0)
        {
            switch (u8Char)
            {
            /* Start of header (xmodem standard packet) ? */
            case SOH:
                TRACE("\nRx 128 packet\n");
                au8Packet[u16Length++] = u8Char;
                /* Expect a standard packet length */
                u16Size = XMODEM_PACKET;
                /* Start checksum */
                u8Check = SOH;
                break;

            /* Start of text (xmodem 1k packet) ? */
            case STX:
                TRACE("\nRx 1K packet\n");
                au8Packet[u16Length++] = u8Char;
                /* Expect a 1K packet length */
                u16Size = XMODEM_PACKET_1K;
                /* Start checksum (xmodem 1k pretends STX is SOH for checksum purposes)*/
                u8Check = SOH;
                break;

            /* End of transmission, cancel, ack or nak character ? */
            case EOT:
            case CAN:
                TRACE("\nRx no packet\n");
                au8Packet[u16Length++] = u8Char;
                RxPacket(0, u16Length, au8Packet);
                /* Reset receive command */
                u16Length = 0;
                break;
            }
        }
        /* If in the data packet and still room available ? */
        else if (u16Length > 0 && u16Length < u16Size)
        {
            au8Packet[u16Length++] = u8Char;

            /* Check if that was the last character ? */
            if (u16Length == u16Size)
            {
                RxPacket(u8Check, u16Length, au8Packet);
                /* Reset receive command */
                u16Length = 0;
            }
            else
            {
                /* Update checksum */
                u8Check += u8Char;
            }
        }
    }
    else if (mdm.state == MDM_STATE_SEND)
    {
        TraceChar(u8Char);

        /* Start of header (xmodem standard packet) ? */
        if (u8Char == 'C')
        {
            // 1K packets
            TRACE("\nTx 1K packet\n");
            TxPacket(1, 1024, au8Packet);
        }
        else if (u8Char == NAK)
        {
            // 128byte packets
            TRACE("\nTx 128 packet\n");
            TxPacket(0, 128, au8Packet);
        }
        else if (u8Char == CAN)
        {
            TRACE("\nTx no packet\n");
            CON_putc(ACK);

            End();
        }
    }
}

/*******************************************************************************
 * RxPacket - Process an Xmodem received packet
 ******************************************************************************/
void MDM_Thread::RxPacket(uint8_t Check, uint16_t Len, uint8_t *Data)
{
    char c = NAK;     /* Assume NAK */

    TRACE("MDM: RxPacket Check=%02x Len=%d\n", Check, Len);
    MEM_Trace(Data, Len, 0);

    /* Is this a data packet ? */
    if (Data[0] == SOH || Data[0] == STX)
    {
        /* Length valid */
        if (Len != u16Size)
        {
            TRACE(" Invalid Length, (Size==%d)\n",u16Size);
        }
        /* Sequence valid ? */
        else if (Data[1] != (255-Data[2]))
        {
            TRACE(" Invalid Sequence, (Data[1]=%02x!=%02x)\n", Data[1], 255 - Data[2]);
        }
        /* Checksum valid ? */
        else if (Data[Len-1] != Check)
        {
            TRACE(" Invalid Check, (%02x)\n", Data[Len-1]);
        }
        else
        {
            /* Is the packet a resend of the previous packet ? */
            if (Data[1] == (u8Packet-1))
            {
                TRACE(" Resent");
                /* Form ACK reply to get next packet */
                c = ACK;
            }
            /* Is this the packet we were expecting ? */
            else if (Data[1] == u8Packet)
            {
                uint32_t n;
                TRACE(" Accept ");

                /* Increment the packet counter */
                u16Packets++;
               
                /* Set the next packet we are expecting */
                u8Packet++;
               
                /* Write to file */
                n = data_write(Len-XMODEM_HEADER, &Data[XMODEM_DATA]);
                TRACE("nbytes=%d", n);
                u32End += n;

                /* Form ACK reply to get next packet */
                c = ACK;
            }
            TRACE("\n");
        }
    }
    /* Is this an end of transmission packet ? */
    else if (Data[0] == EOT)
    {
        TRACE(" End of transmission\n");
        /* Did we write any data ? */
        if (u32End > u32Start)
        {
            /* We completed the transfer */
            bComplete = true;
        }
        /* Form ACK reply to end transfer */
        c = ACK;
    }
    /* Is this an cancel packet ? */
    else if (Data[0] == CAN)
    {
        TRACE(" Cancel\n");
        /* No reply required */
        c = 0;
    }
    else
    {
        TRACE(" Data[0]=%02x expected SOH or STX\n", Data[0]);
    }

    /* Got a reply to output - output it  */
    if (c != 0)
    {
        TxResp(c);
    }

    /* End of transmission or cancel character ? */
    if (Data[0] == EOT || Data[0] == CAN)
    {
       /* End xmodem mode */
       End();
    }
}

/*******************************************************************************
 * TxPacket - Process an Xmodem transmit packet
 ******************************************************************************/
void MDM_Thread::TxPacket(uint8_t crc, uint16_t Len, uint8_t *Data)
{
    uint32_t n;

    TRACE("\nMDM: TxPacket crc=%x Length=%d ", crc, u16Length);
    /* Initialize the packet 3-byte header */
    if (Len == 1024)
    {
        Data[0] = STX;
        u16Size = 1024;
    }
    else
    {
        Data[0] = SOH;
        u16Size = 128;
    }
    Data[1] = u8PacketNo;
    Data[2] = ~u8PacketNo;

    /* Determine the number of bytes to Read from the file. */
    n = u32End - u32Start;
    if (n > u16Size)
    {
        n = u16Size;
    }
    if (n > 0)
    {
        memset(&Data[3], 0, u16Size);

        /*TODO: Handle Read errors. */
        n = data_read(n, (BYTE *)&Data[3]);
        if (n < u16Size)
        {
            /* NOTE mark the end of a text file with '1A'. */
            Data[3 + n] = SUB;
        }

        /* Append either 2-byte CRC or 1-byte CheckSum value */
        if (crc)
        {
            unsigned short ccrc = mdm_crc16_ccitt(&Data[3], u16Size);
            Data[u16Size + 3] = ((ccrc >> 8) & 0xFF);
            Data[u16Size + 4] = (ccrc & 0xFF);
        }
        else
        {
            int i;
            unsigned char ccks = 0;
            for (i = 3; i < u16Size + 3; ++i)
            {
                ccks += Data[i];
            }
            Data[u16Size + 3] = ccks;
        }
    }
    TRACE("nbytes=%d\n", n);
    MEM_Trace(Data, Len, 0);
}

/*******************************************************************************
 * TxResp - Transmit an xmodem response (single chars only)
 ******************************************************************************/
 void MDM_Thread::TxResp(char c)
{
    TRACE("MDM: TxResp(");
    TraceChar(c);
    TRACE(")\n");
    /* Want to output a NAK ? */
    if (c == NAK || c == 'C')
    {
        /* Allowed to retry ? */
        if (u8Retries > 0)
        {
            TRACEF(" Retry packet\n");
            /* Decrement retries */
            u8Retries--;

            /* Statistics: Handled some data - increment nak counter */
            if (u16Packets > 0)
            {
                u16Naks++;
            }
        }
        else
        {
            if (c == 'C')
            {
                /* Switch to NAKs */
                TRACEF(" Switch to Naks\n");
                c = u8Nak = NAK;
            }
            else
            {
                TRACEF(" Cancel transfer\n");
                /* Send a CAN to cancel instead */
                c = CAN;
            }
        }
    }
    /* Want to output an ACK ? */
    else if (c == ACK)
    {
        TRACEF(" Acknowledge packet\n");
        /* Reset retries */
        u8Retries = XMODEM_MAX_RETRIES;

        /* Statistics: Handled some data - increment ACK counter */
        if (u16Packets > 0)
        {
            u16Acks++;
        }
    }
    else
    {
        TRACEF(" Unknown response!\n");
    }

    /* Redirect to output stream circular buffer */
    CON_putc(c);

    if (c == CAN)
    {
        End();   /* Cancelled - end transfer */
        return;
    } 

    /* Restart timer */
    u16Timer = XMODEM_TIME_RX;
}

/*******************************************************************************
 * Timeout - Handle Xmodem timeouts.
 ******************************************************************************/
void MDM_Thread::Timeout(void)
{
    TRACE("MDM: Timeout() state=%s Packets=%d Timer=%d\n", mdm_state_str[mdm.state], u16Packets, u16Timer);
    /* If there has been packets - increment duration timer */
    if (u16Packets > 0)
    {
        u16Duration++;
    }

    /* Xmodem timer running */
    if (u16Timer > 0)
    {
        if (((--u16Timer) == 0) && (bReceive == true))
        {
            TxResp(u8Nak);
        }
    }
}

void MDM_Thread::Start(bool dir)            
{   
    TRACE("MDM: Start(%d) ", dir);
    /* Initialize start address */
    u32Start = 0L;
    u32End = 0L;
    /* No longer have a complete image */
    bComplete = false;
    /* Reset counters and timers */
    u16Length = 0;
    /* Reset Statistics */
    u16Duration = 0;
    u16Acks = 0;
    u16Naks = 0;
    /* Default size of data packets */
    u16Size = XMODEM_PACKET;
    /* Start with packet one */
    u8Packet = 1;
    /* Allow maximum retrys */
    u8Retries = XMODEM_MAX_RETRIES;
    /* Initialise nak reply timer function needs a pointer to this */
    u8Nak = 'C'; /* set timeout response (switch to NAK after max. retries) */
    u8Can = CAN;
    /* Set timer for xmodem */
    u16Timer = XMODEM_TIME_RX;

    if (!file_open)
    {
        mdm.fp = fio_open("t", (dir)? "w" : "r");
        if (mdm.fp == NULL)
        {
            TRACEF("Can't open file!\n");
            return;
        }
        else
        {
            file_open = true;
        }
    }
    bReceive = dir;

    if (dir)
    {
        mdm.state = MDM_STATE_RECV;
        TRACEF("Recv\n");
    }
    else
    {
        mdm.state = MDM_STATE_SEND;
        u32End = fio_size(mdm.fp);
        TRACEF("Send End=%d\n", u32End);
    }        
}

/*******************************************************************************
 * End - End xmodem mode.
 ******************************************************************************/
void MDM_Thread::End(void)
{
    TRACE("MDM: End() ");
    /* End Xmodem transfer */
    mdm.state = MDM_STATE_IDLE;

    /* Cancel timer */
    u16Timer = 0;

    uint16_t u32Count = u32End - u32Start;
    uint16_t u16Bps = (u16Packets*(u16Size) - XMODEM_HEADER);

    if (file_open)
    {
        fio_close(mdm.fp);
        file_open = false;
        TRACE("file closed\n");
    }

    if (bReceive)
    {
        TRACEF("MDM: Recv Complete=%d Start=%x Count=%x %dms %dbps Packets=%d Acks=%d Naks=%d\n",
                        bComplete, u32Start, u32Count, u16Duration*10, u16Bps,
                        u16Packets, u16Acks, u16Naks);
    }
    else
    {
        TRACEF("MDM: Send Complete=%d\n",
                        bComplete);
    }
}

/*******************************************************************************
 * Timer - 1 msec timer.
 ******************************************************************************/
void MDM_Thread::Timer(void)        
{
    if (mdm.state != MDM_STATE_IDLE)
    {
        /* If there has been packets - increment duration timer */
        if (u16Packets > 0)
        {
            u16Duration++;
        }

        /* Xmodem timer running */
        if (u16Timer > 0)
        {
            if ((--u16Timer) == 0)
            {
                Timeout();
            }
        }
    }
}


extern "C"
{

int MDM_Init(void)
{
	return PTS_AddTask(new MDM_Thread, 0);
}

void MDM_SetDir(bool dir)
{
    TRACEF("MDM: SetDir(%d) state=%s\n", dir, mdm_state_str[mdm.state]);
    if (mdm.state == MDM_STATE_IDLE)
    {
        CLI_SetEnable(false);
        CLI_SetStream(MDM_STREAM_ID);
        ((MDM_Thread *)PTS_GetThread(mdm.id))->Start(dir);
    }
}

/*******************************************************************************
 * MDM_Timer   Timer for Xmodem timing.
 ******************************************************************************/
void MDM_Timer(void)
{
    ((MDM_Thread *)PTS_GetThread(mdm.id))->Timer();
}

static void usage(void)
{
    CON_printf("recv  : receive file\n");
    CON_printf("send  : transmit file\n");
}

int Cmd_mdm(int argc, char *argv[])
{
    if (argc > 1)
    {
        char *p = argv[1];

        if (!strcmp(p,"on"))
        {
            MDM_SetEnable(true);
        }
        else if (!strcmp(p,"off"))
        {
            MDM_SetEnable(false);
        }
        else if (!strcmp(p,"recv"))
        {
            MDM_SetDir(true);   // Recv
        }
        else if (!strcmp(p,"send"))
        {
            MDM_SetDir(false);  // Send
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
        CON_printf("MDM:[%d,%s]\n", mdm.enable, mdm_state_str[mdm.state]);
    }
    return(0);
}

}
