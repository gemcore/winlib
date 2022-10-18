/*0===========================================================================
**
** File:       zar.cpp
**
** Purpose:    This file contains source for a utility to archives files.
**
**
**
** Date:       March 8, 1990
**
** Author:     Alan D. Graves
**
** Exports:
**
**
** Rev  Date        Id  Description
** ---  ----------- --- -----------
** 1.00 08-Mar-1990 ADG Created.
** 1.01 28-Apr-1990 ADG Fixed zero file_crc problem.
** 1.02 31-Aug-1994 ADG Compiled with '/G3' option to reduce code size.
** 1.03 26-Aug-2018 ADG Adapted for MOBD device.
** 1.04 03-Sep-2018 ADG Configured for building with WinLib.
**
**============================================================================
*/
#include <stdlib.h>         /* C Standard Library i/f */
#include <string.h>     	/* C string i/f */
#include <limits.h>     	/* C intrinsic type ranges */
//begin #include "fio.h"
#define _CRTDBG_MAP_ALLOC 
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include "LOG.H"		// Capture putchar & printf output to a log file

#define FR_OK				0
#define FIO_FILE			FILE
#define fio_open(A,B)		fopen(A,B)
#define fio_close(F)		fclose(F)
#define fio_read(F,A,B,C)	fread(A,B,C,F)
#define fio_write(F,A,B,C)	fwrite(A,B,C,F)
#define fio_seek(F,A,B)		fseek(F,A,B)
#define fio_tell(F)			ftell(F)
#define fio_getc(F)			fgetc(F)
#define fio_putc(F,A)		fputc(A,F)
#define fio_eof(F)			feof(F)
#define fio_error(F)		ferror(F)
#define fio_size(F)			fsize(F)
#define HAS_STD				1

#define ffind_handle		HANDLE
#define ffind_valid(fh)		(fh == INVALID_HANDLE_VALUE)
#define ffind_data			WIN32_FIND_DATAA

ffind_handle ffind_first(char *pFilename, ffind_data *ffData)
{
	HANDLE fh;
	LPCSTR lpFilename = (LPCSTR)pFilename;
	fh = FindFirstFileA(lpFilename, ffData);
	return fh;
}

int ffind_next(ffind_handle ffh, ffind_data *ffData)
{
	BOOL rc;
	rc = FindNextFileA(ffh, ffData);
	return (rc)? 0 : -1;
}

char *ffind_name(ffind_data *ffData)
{
	return (char *)(ffData->cFileName);
}

long fsize(FILE *fp)
{
	long int len;
	fpos_t pos;
	fgetpos(fp, &pos);
	fseek(fp, 0, SEEK_END);	//ADG! Check for errors!
	len = ftell(fp) + 1;	//ADG! Check for errors!
	fsetpos(fp, &pos);
	return (long)len;
}
//end

#include "coder.h"      	/* De/compression coding methods */
//begin #include "dbg.h"
#define CON_printf	printf
#define	CON_putc	putchar
//end

#undef TRACE
#define TRACE(format,...)
#define TRACEF(format,...)
//#define TRACE(format,...)       printf(format,##__VA_ARGS__)
//#define TRACEF(format,...)      printf(format,##__VA_ARGS__)   // wait for data to be sent

/* Constants */
#define OVERWRITE

#define MAX_FILENAME_LEN    20

#define BUFFERSIZE          128     // store/unstore working buffer

#define DEFAULT_DIR         ""

#define HEADER_ID          	0xEA60
#define HEADER_ID_HI       	0xEA
#define HEADER_ID_LO       	0x60
#define FIRST_HDR_SIZE     	20
#define HEADERSIZE_MAX     	(FIRST_HDR_SIZE + MAX_FILENAME_LEN)
#define HEADERSIZE_MIN     	(FIRST_HDR_SIZE + 1)
#define MAXMETHOD          	1
#define BINARY_TYPE        	0       /* This must line up with binary/text strings */

/* Message text */
const char *M_USAGE[]   ={"ZAR -- archive files\n",
                    "Usage: zar {[-l]|-x|-{c|a} file...} -z[archive]\n",
                    " -l    list archive\n",
                    " -x    extract archive\n",
                    " -c    create archive\n",
                    " -a    append to archive\n",
                    " -z    set achive file name (default is 'ZAR.ZAR')\n",
                    " file  file (wildcards * ? are allowed)\n",
                    " ?     this help\n",
                    NULL};

const char M_CREATZAR[] = "Creating archive: %s\n";
const char M_PROCZAR [] = "Processing archive: %s\n";
const char M_ZARDATE [] = "Archive date      : %s\n";
const char M_COMPR   [] = "Compressing %25s ";
const char M_STORE   [] = "Storing     %25s ";
const char M_ADD     [] = "Adding      %25s ";
const char M_EXTRACT [] = "Extracting  %25s ";
const char M_NBRFILES[] = "%5d file(s)\n";
const char M_ERRORCNT[] = "%sFound %5d error(s)!\n";
const char M_CRCERROR[] = "CRC error!\n";
const char M_CRCOK   [] = "CRC OK\n";
const char M_SKIPPED [] = "Skipped %s";
const char M_BADCOMNT[] = "Invalid comment header\n";
const char M_BADHEADR[] = "Bad header\n";
const char M_FEXISTS [] = "File %s already exists\n";
const char M_CANTOPEN[] = "Can't open %s\n";
const char M_CANTREAD[] = "Can't read file or unexpected end of file\n";
const char M_CANTWRIT[] = "Can't write file. Disk full?\n";
const char M_HEADRCRC[] = "Header CRC error!\n";
const char M_NOMEMORY[] = "Out of memory\n";
const char M_NOTZAR  [] = "%s is not a ZAR archive\n";
const char M_UNKNMETH[] = "Unsupported method: %d, ";
const char M_UNKNTYPE[] = "Unsupported file type: %d, ";

/* Macros */
#define get_crc()       get_word()
#define fget_crc(f)     fget_word(f)

#define setup_get(PTR)  (get_ptr = (PTR))
#define get_byte()      ((unsigned char)(*get_ptr++ & 0xff))

#define put_crc(c)      put_word(c)
#define fput_crc(c,f)   fput_word(c,f)

#define setup_put(PTR)  (put_ptr = (PTR))
#define put_byte(c)     (*(unsigned char *)(put_ptr++) = c&0xff)

#define get_tx(m,d,h,n) (((unsigned long)m<<21)+((unsigned long)d<<16)+((unsigned long)h<<11)+(n<<5))
#define get_tstamp(y,m,d,h,n,s) ((((unsigned long)(y-1980))<<25)+get_tx(m,d,h,n)+(s/2))

#define ts_year(ts)     ((unsigned int)((ts>>25)&0x7f)+1980)
#define ts_month(ts)    ((unsigned int)(ts>>21)&0x0f) /* 1..12 means Jan..Dec */
#define ts_day(ts)      ((unsigned int)(ts>>16)&0x1f) /* 1..31 means 1st..31st */
#define ts_hour(ts)     ((unsigned int)(ts>>11)&0x1f)
#define ts_min(ts)      ((unsigned int)(ts>>5)&0x3f)
#define ts_sec(ts)      ((unsigned int)((ts&0x1f)*2))

/* Functions defined in file 'ECC.C' */
extern "C" void init_crctable(void);
extern "C" int crc16_byte(unsigned int,unsigned int *);
extern "C" unsigned int crc16_buf(unsigned char *,int,unsigned int);

/* Function prototypes */
void usage();
unsigned int ratio(long,long);
int check_flags();
void disp_clock();
void error(char *,char *);
void get_mode_str(char *,unsigned int);
int get_ftime_mode(char *,unsigned long *,unsigned int *);
int set_ftime_mode(char *,unsigned long,unsigned int);
void get_date_str(char *,unsigned long);
int fget_byte(FIO_FILE *);
unsigned int fget_word(FIO_FILE *);
unsigned long fget_longword(FIO_FILE *);
int fread_crc(unsigned char *,int,FIO_FILE *);
unsigned int get_word(void);
unsigned long get_longword(void);
long find_header(FIO_FILE *);
int read_header(int,FIO_FILE *,char *);
void skip(void);
int extract();
int list();
void fput_byte(int,FIO_FILE *);
void fput_word(unsigned int,FIO_FILE *);
void fput_longword(unsigned long,FIO_FILE *);
int fwrite_crc(unsigned char *,int,FIO_FILE *);
void put_word(unsigned int);
void put_longword(unsigned long);
int write_header(int,FIO_FILE *,char *);
int append();

/* Option flags */ 
static char lflag=1,
     aflag=0,
     xflag=0;

/* Local variables */
unsigned int crc;
int file_count;
char filename[MAX_FILENAME_LEN];
char zar_name[MAX_FILENAME_LEN]={"c:\\temp\\ZAR.ZAR"};
char *hdr_filename;
unsigned short headersize;
unsigned char first_hdr_size;
unsigned char zar_flags;
short method;
unsigned int file_mode;
unsigned long time_stamp;
unsigned char *get_ptr;
unsigned char *put_ptr;
unsigned int file_crc;
unsigned int header_crc;
long first_hdr_pos;
int clock_inx;
FIO_FILE *zarfile;
char *writemode[2]={"wb","w"};
FIO_FILE *outfile = stdout;
FIO_FILE *infile = stdin;
unsigned int incrc;
unsigned int outcrc;
long compsize;
long origsize;
unsigned char header[HEADERSIZE_MAX];
int file_type;
int no_output;
int error_count;
char date_str[22];
Coder *coder;

/*
** This virtual method is called prior to writing the next output buffer.
** Thus it is an appropriate time to update the CRC value, also
** the 'rotating clock' indicator is moved to the next position.
*/
void Coder::flushout()
{
//   CON_printf("\nflushout() ");
//   for (int i=0; i < optr; i++)
//   {
//      CON_printf("%02x",obuffer[i]);
//   }
    outcrc = crc16_buf(obuffer,optr,outcrc);
    disp_clock();
}

/*
** This virtual method is called after to reading the next input buffer.
** Thus it is an appropriate time to update the CRC value, also
** the 'rotating clock' indicator is moved to the next position.
*/
void Coder::flushin()
{
    incrc = crc16_buf(ibuffer,icnt,incrc);
}

void usage()
{
    for (int i=0; M_USAGE[i] != NULL; i++)
    {
        CON_printf(M_USAGE[i]);
    }
    //exit(1);
}

void disp_clock()
{
    static char clock_str[4] = { '|', '/', '-', '\\' };

    CON_printf("(%c)\b\b\b", clock_str[clock_inx]);
    clock_inx = (clock_inx + 1) & 0x03;
}

void error(const char *fmt, char *arg)
{
    TRACE("error() ");
    CON_printf(fmt, arg, error_count);
    //exit(-1);
}

void get_date_str(char *str,unsigned long tstamp)
{
    sprintf(str, "%04u-%02u-%02u %02u:%02u:%02u",
            ts_year(tstamp), ts_month(tstamp), ts_day(tstamp),
            ts_hour(tstamp), ts_min(tstamp), ts_sec(tstamp));
}

void get_mode_str(char *str,unsigned int mode)
{
    strcpy(str, "---W");
#ifdef MS_DOS
    if (mode & _A_ARCH)
        str[0] = 'A';
    if (mode & _A_SYSTEM)
        str[1] = 'S';
    if (mode & _A_HIDDEN)
        str[2] = 'H';
    if (mode & _A_RDONLY)
        str[3] = 'R';
#else
#endif
}

int set_ftime_mode(char *name,unsigned long tstamp,unsigned int attribute)
{
    FIO_FILE *fd;
    TRACE("set_ftime_mode(%s,%08x,%04x) access ",name,tstamp,attribute);
    if ((fd = fio_open(name,"r+b")) == NULL)
    {
        TRACE("error\n");
        return -1;
    }
    fio_close(fd);
#if HAS_STD == 1
#else
    FILINFO fi;
    fi.fdate = (unsigned int)(tstamp>>16);
    fi.ftime = (unsigned int)(tstamp&0xFFFF);
    if (f_utime(name,&fi) != FR_OK)
    {
        TRACE("utime error\n");
        return -1;
    }
    // Valid attribute byte bits are: AM_RDO|AM_HID|AM_SYS|AM_ARC
    if (f_chmod(name,(BYTE)attribute,AM_RDO|AM_HID|AM_SYS|AM_ARC) != FR_OK)
    {
        TRACE("chmod error\n");
        return -1;
    }
#endif
    return 0;
}

int get_ftime_mode(char *name,unsigned long *tstamp,unsigned int *attribute)
{
    FIO_FILE *fd;
    TRACE("get ftime_mode %s access ",name);
    if ((fd = fio_open(name,"r+b")) == NULL)
    {
        TRACE("error\n");
        return -1;
    }
    fio_close(fd);
#if HAS_STD == 1
#else
    FILINFO fi;
    if (f_stat(name,&fi) != FR_OK)
    {
        TRACE("info error\n");
        return -1;
    }
    *tstamp = ((unsigned long)fi.fdate<<16) + (unsigned long)fi.ftime;
    TRACE("tstamp=%08x ",*tstamp);
    *attribute = (unsigned int)fi.fattrib;
    TRACE("mode=%04x\n",*attribute);
#endif
    return 0;
}

int check_flags()
{
    if (method < 0 || method > MAXMETHOD)
    {
        CON_printf(M_UNKNMETH, method);
        CON_printf(M_SKIPPED, filename);
        CON_putc('\n');
        skip();
        return -1;
    }
    if (file_type != BINARY_TYPE)
    {
        CON_printf(M_UNKNTYPE, file_type);
        CON_printf(M_SKIPPED, filename);
        CON_putc('\n');
        skip();
        return -1;
    }
    return 0;
}

unsigned int ratio(long a, long b)
{
    int i;

    for (i = 0; i < 3; i++)
    {
        if (a <= LONG_MAX / 10)
            a *= 10;
        else
            b /= 10;
    }
    if ((long) (a + (b >> 1)) < a)
    {
        a >>= 1;
        b >>= 1;
    }
    if (b == 0)
    {
        return 0;
    }
    return (unsigned int) ((a + (b >> 1)) / b);
}

int fget_byte(FIO_FILE *f)
{
    int c;

    if ((c = fio_getc(f)) == EOF)
    {
        error(M_CANTREAD, "");
    }
    //TRACE("%02x",c&0xFF);
    return c & 0xFF;
}

unsigned int fget_word(FIO_FILE *f)
{
    unsigned int b0, b1;

    b0 = fget_byte(f);
    b1 = fget_byte(f);
    return (b1 << 8) + b0;
}

unsigned long fget_longword(FIO_FILE *f)
{
    unsigned long b0, b1, b2, b3;

    b0 = fget_byte(f);
    b1 = fget_byte(f);
    b2 = fget_byte(f);
    b3 = fget_byte(f);
    return (b3 << 24) + (b2 << 16) + (b1 << 8) + b0;
}

int fread_crc(unsigned char *p, int n, FIO_FILE *f)
{
    n = fio_read(f, (char *)p, 1, n);
    crc = crc16_buf(p,n,crc);
    TRACE("fread_crc() crc=%04x\n",crc);
    return n;
}

unsigned int get_word()
{
    unsigned int b0, b1;

    b0 = get_byte();
    b1 = get_byte();
    return (b1 << 8) + b0;
}

unsigned long get_longword()
{
    unsigned long b0, b1, b2, b3;

    b0 = get_byte();
    b1 = get_byte();
    b2 = get_byte();
    b3 = get_byte();
    return (b3 << 24) + (b2 << 16) + (b1 << 8) + b0;
}

long find_header(FIO_FILE *f)
{
    long zarpos, lastpos;
    int c;

    TRACE("find_header() ");
    if (fio_size(f) > 1)
    {
        zarpos = fio_tell(f);
        fio_seek(f, 0L, SEEK_END);
        lastpos = fio_tell(f) - 2;
        for ( ; zarpos < lastpos; zarpos++)
        {
            fio_seek(f, zarpos, SEEK_SET);
            c = fget_byte(f);
            while (zarpos < lastpos)
            {
                if (c != HEADER_ID_LO)  /* low order first */
                {
                    c = fget_byte(f);
                }
                else if ((c = fget_byte(f)) == HEADER_ID_HI)
                {
                    TRACE(" Header found ");
                    break;
                }
                zarpos++;
            }
            if (zarpos >= lastpos)
            {
                break;
            }
            headersize = fget_word(f);
            TRACE(" headersize=%04x ",headersize);
            if (headersize <= HEADERSIZE_MAX)
            {
                crc = 0;
                fread_crc(header, (int) headersize, f);
                if (crc == fget_crc(f))
                {
                    TRACE(" Ok\n");
                    fio_seek(f, zarpos, SEEK_SET);
                    return zarpos;
                }
                TRACE("Failed!\n");
            }
        }
    }
    TRACE("Header not found!\n");
    return -1;        /* could not find a valid header */
}

int read_header(int first, FIO_FILE *f, char *name)
{
    unsigned short header_id;

    TRACE("read_header(%d) ",first);
	if (fio_eof(f))
	{
		TRACE("EOF\n");
		return 0;
	}
    header_id = fget_word(f);
    TRACE(" id=%04x ",header_id);
    if (header_id != HEADER_ID)
    {
        if (first)
            error(M_NOTZAR, name);
        else
            error(M_BADHEADR, "");
    }
    headersize = fget_word(f);
    TRACEF(" size=%04x\n",headersize);
    if (headersize == 0)
    {
        return 0;            /* end of archive */
    }
    if (headersize > HEADERSIZE_MAX)
    {
        error(M_BADHEADR, "");
    }
    crc = 0;
    fread_crc(header, (int) headersize, f);
    header_crc = fget_crc(f);
    TRACE(" crc=%04x header_crc=%04x\n",crc,header_crc);
    if (crc != header_crc)
    {
        error(M_HEADRCRC, "");
    }
    setup_get(header);
    first_hdr_size = get_byte();     TRACE("first_hdr_size=%02x\n",first_hdr_size);
    zar_flags      = get_byte();     TRACE("zar_flags=%02x\n",zar_flags);
    method         = get_byte();     TRACE("method=%d\n",method);
    file_type      = get_byte();     TRACE("file_type=%d\n",file_type);
    file_mode      = get_word();     TRACE("file_mode=%04x\n",file_mode);
    time_stamp     = get_longword(); TRACE("time_stamp=%08x\n",time_stamp);
    compsize       = get_longword(); TRACE("compsize=%08x\n",compsize);
    origsize       = get_longword(); TRACE("origsize=%08x\n",origsize);
    file_crc       = get_crc();      TRACE("file_crc=%04x\n",file_crc);
    hdr_filename = (char *)&header[first_hdr_size];
    TRACEF("filename='%s'\n",hdr_filename);
    strncpy(filename, hdr_filename, sizeof(filename));
    return 1;               /* success */
}

void skip()
{
    fio_seek(zarfile, compsize, SEEK_CUR);
}

void fput_byte(int c,FIO_FILE *f)
{
    //TRACE("%02x",c&0xFF);
    fio_putc(f, c);
    if (fio_error(f))
    {
        error(M_CANTWRIT, "");
    }
}

void fput_word(unsigned int n,FIO_FILE *f)
{
    fput_byte(n&0xFF,f);
    fput_byte(n>>8,f);
}

void fput_longword(unsigned long n,FIO_FILE *f)
{
    fput_byte(n&0xFF,f);
    fput_byte((n>>8)&0xFF,f);
    fput_byte((n>>16)&0xFF,f);
    fput_byte((n>>24)&0xFF,f);
}

int fwrite_crc(unsigned char *p, int n, FIO_FILE *f)
{
    crc = crc16_buf(p,n,crc);
    if (f)
    {
        n = fio_write(f, (char *)p, 1, n);
        if (fio_error(f))
        {
            error(M_CANTWRIT, "");
        }
    }
    TRACEF("fwrite_crc() crc=%04x\n",crc);
    return n;
}

void put_word(unsigned int n)
{
    put_byte(n&0xFF);
    put_byte((n>>8)&0xFF);
}

void put_longword(unsigned long n)
{
    put_byte(n&0xFF);
    put_byte((n>>8)&0xFF);
    put_byte((n>>16)&0xFF);
    put_byte((n>>24)&0xFF);
}

int write_header(int first, FIO_FILE *f,char *name)
{
    TRACE("write_header() ");
    fput_word(HEADER_ID,f);
    fput_word(headersize,f);
    TRACEF(" size=%04x\n",headersize);
    if (headersize == 0)
    {
        return 0;            /* end of archive */
    }
    setup_put(header);
    first_hdr_size = FIRST_HDR_SIZE;
    put_byte(first_hdr_size);     TRACE("first_hdr_size=%02x\n",first_hdr_size);
    put_byte(zar_flags);          TRACE("zar_flags=%02x\n",zar_flags);
    put_byte(method);             TRACE("method=%d\n",method);
    put_byte(file_type);          TRACE("file_type=%d\n",file_type);
    put_word(file_mode);          TRACE("file_mode=%04x\n",file_mode);
    put_longword(time_stamp);     TRACE("time_stamp=%08x\n",time_stamp);
    put_longword(compsize);       TRACE("compsize=%08x\n",compsize);
    put_longword(origsize);       TRACE("origsize=%08x\n",origsize);
    put_word(file_crc);           TRACE("file_crc=%04x\n",file_crc);
    TRACEF("filename='%s'\n",filename);
    strncpy((char *)&header[first_hdr_size],filename,MAX_FILENAME_LEN);
    crc = 0;
    fwrite_crc(header, (int) headersize, zarfile);
    fput_crc(crc,zarfile);
    TRACE("crc=%04x\n",crc);
    return 1;               /* success */
}

int append()
{
    if ((infile = fio_open(filename,"rb")) == NULL)
    {
        CON_printf(M_CANTOPEN,filename);
        CON_putc('\n');
        error_count++;
        return 0;
    }
    CON_printf(M_COMPR,filename);
    incrc = outcrc = 0;
    long zarpos = fio_tell(zarfile);
    method = 1;
    coder = new RLLCoder(infile,zarfile,BUFFERSIZE,BUFFERSIZE);
    coder->process(ENCODER);
    origsize = coder->icount;
    compsize = coder->ocount;
    if (compsize > origsize)
    {
        // RLL compression is no good with this file, so just store it!
        delete coder;
        fio_seek(zarfile,zarpos,SEEK_SET);
        fio_seek(infile,0L,SEEK_SET);
        CON_printf(M_STORE,filename);
        incrc = outcrc = 0;
        method = 0;
        coder = new NoCoder(infile,zarfile,BUFFERSIZE,BUFFERSIZE);
        coder->process(ENCODER);
        origsize = coder->icount;
        compsize = coder->ocount;
    }
    delete coder;
    fio_close(infile);
    file_crc = incrc;
    TRACEF("file_crc=%04x (incrc=%04x outcrc=%04x)\n",file_crc,incrc,outcrc);
    unsigned int r = ratio(compsize, origsize);
    CON_printf("%u.%01u%%\n",r/10,r%10);
    file_count++;
    return 1;
}

int extract()
{
    file_count = 0;

    while (read_header(0,zarfile,zar_name))
    {
        if (check_flags())
        {
            error_count++;
            return 0;
        }

        #ifndef OVERWRITE
        if (_access(filename, 0) == 0)
        {
            CON_printf(M_FEXISTS,filename);
            CON_printf(M_SKIPPED,filename);
            skip();
            error_count++;
            return 0;
        }
        #endif

        if ((outfile = fio_open(filename,writemode[file_type&1])) == NULL)
        {
            CON_printf(M_CANTOPEN,filename);
            CON_putc('\n');
            skip();
            error_count++;
            return 0;
        }
        CON_printf(M_EXTRACT,filename);
        incrc = outcrc = 0;
        if (method == 0)
        {
            coder = new NoCoder(zarfile,outfile,BUFFERSIZE,BUFFERSIZE);
        }
        else if (method == 1)
        {
            coder = new RLLCoder(zarfile,outfile,BUFFERSIZE,BUFFERSIZE);
        }
        coder->seticountmax(compsize);  // don't read beyond the compressed data!
        coder->process(DECODER);
        delete coder;
        fio_close(outfile);
        set_ftime_mode(filename,time_stamp,file_mode);
        TRACEF("incrc=%04x outcrc=%04x file_crc=%04x ",incrc,outcrc,file_crc);
        if (outcrc == file_crc)
        {
            CON_printf(M_CRCOK);
        }
        else
        {
            CON_printf(M_CRCERROR);
            error_count++;
        }
        file_count++;
    }
    return 1;
}

int list()
{
    long tcompsize = 0L;
    long torigsize = 0L;
    unsigned int r;
    int ftype;
    char fmode_str[10];
    static char mode[2] = {'B','?'};

    file_count = 0;

    CON_printf("Filename       Original Compressed Ratio Date last modified  CRC16 AttrTM\n");
    CON_printf("------------ ---------- ---------- ----- ------------------- ----- ------\n");
    while (read_header(0,zarfile,zar_name))
    {
        if (check_flags())
        {
            error_count++;
            return 0;
        }
#if 1
        // To test the archive:
        //CON_printf(M_EXTRACT,filename);
        incrc = outcrc = 0;
        if (method == 0)
        {
            coder = new NoCoder(zarfile,NULL,BUFFERSIZE,BUFFERSIZE);
        }
        else if (method == 1)
        {
            coder = new RLLCoder(zarfile,NULL,BUFFERSIZE,BUFFERSIZE);
        }
        coder->seticountmax(compsize);// don't read beyond the compressed data!
        coder->process(DECODER);
        delete coder;
        TRACEF("incrc=%04x outcrc=%04x file_crc=%04x ",incrc,outcrc,file_crc);
        if (outcrc == file_crc)
        {
            //CON_printf(M_CRCOK);
        }
        else
        {
            //CON_printf(M_CRCERROR);
            error_count++;
        }
#else
        // Else just skip over it:
        skip();
#endif
        r = ratio(compsize, origsize);
        torigsize += origsize;
        tcompsize += compsize;
        ftype = file_type;
        if (ftype != BINARY_TYPE)
        {
            ftype = 1;
        }
        get_date_str(date_str,time_stamp);
        strcpy(fmode_str,"    ");
        get_mode_str(fmode_str,(unsigned int)file_mode);
        CON_printf("%12s %10d %10d %u.%03u %s %04x  %4s%c%u\n",
                    &filename[0],origsize,compsize,r/1000,r%1000,date_str,file_crc,
                    fmode_str,mode[ftype],method);
        file_count++;
    }
    CON_printf("------------ ---------- ---------- ----- -------------------\n");
    r = ratio(tcompsize, torigsize);
    CON_printf(" %5d files %10d %10d %u.%03u %s\n",
                file_count,torigsize,tcompsize,r/1000,r%1000,date_str);
    return 1;
}

extern "C"
{
int Cmd_zar(int argc,char *argv[])
{
    char *p;
    int i;
    ffind_data file;
	ffind_handle fh;

    for (i=1; i < argc; i++)
    {
        p = argv[i];
        if (*p == '-')
        {
			char option;
			if (*++p)
			{
				option = *p++;
				switch (option)
				{
				case 'l':
					lflag = 1;
					xflag = 0;
					aflag = 0;
					break;

				case 'c':
					lflag = 0;
					xflag = 0;
					aflag = 0;
					break;

				case 'x':
					lflag = 0;
					xflag = 1;
					aflag = 0;
					break;

                case 'a':
                    lflag = 0;
                    xflag = 0;
                    aflag = 1;
                    break;

				case 'z':
					strcpy(zar_name, p);
					break;

				default:
					CON_printf("zar: Unknown option \"-%s\".\n", p);
					return -6;
				}
				argv[i] = NULL;
			}
		}
        else if (*p == '?')
        {
            usage();
            return 0;
        }
    }
/*
   //-------------------------------------------------------------------
   // Display signon message.
   //-------------------------------------------------------------------
   CON_printf(M_SIGNON,title,version,copyright);
*/
    /* Generate crc lookup table values */
    init_crctable();

    error_count = 0;
    clock_inx = 0;
    time_stamp = 0;
    first_hdr_size = FIRST_HDR_SIZE;

    if (lflag)
    {
        TRACEF("List archive\n");
        if ((zarfile = fio_open(zar_name, "rb")) == NULL)
        {
            CON_printf(M_CANTOPEN,zar_name);
            return -6;
        }
        CON_printf(M_PROCZAR, zar_name);
        first_hdr_pos  = find_header(zarfile);
        if (first_hdr_pos < 0)
        {
            CON_printf(M_NOTZAR, zar_name);
            goto err;
        }
        fio_seek(zarfile, first_hdr_pos, SEEK_SET);
        if (!read_header(1, zarfile, zar_name))
        {
            CON_printf(M_BADCOMNT);
            goto err;
        }
        get_date_str(date_str, time_stamp);
        CON_printf(M_ZARDATE,date_str);
        list();
    }
    else if (!xflag && !aflag)
    {
        TRACEF("Create archive\n");
        zarfile = fio_open(zar_name, "wb");
        if (zarfile == NULL)
        {
            CON_printf(M_CANTOPEN,zar_name);
            return -6;
        }
        CON_printf(M_CREATZAR, zar_name);
        headersize = HEADERSIZE_MIN;
        zar_flags = 0;
        origsize = 0L;
        compsize = 0L;
        strcpy(filename,zar_name);
        headersize = HEADERSIZE_MIN + strlen(filename);
        get_ftime_mode(filename,&time_stamp,&file_mode);
        get_date_str(date_str, time_stamp);
        CON_printf(M_ZARDATE,date_str);
        write_header(0, zarfile, zar_name);

        file_count = 0;
        for (i=1; i < argc; ++i)
        {
            if ((p = argv[i]) == NULL)
            {
                continue;
            }
			fh = ffind_first(p, &file);
			if (ffind_valid(fh))
            {
                CON_printf("zar: No files match \"%s\".\n",p);
                continue;
            }
            do
            {
                if (strcmp(ffind_name(&file),zar_name) != 0)
                {
                    strcpy(filename,ffind_name(&file));
                    headersize = HEADERSIZE_MIN + strlen(filename);
                    get_ftime_mode(filename,&time_stamp,&file_mode);
                    long zarpos = fio_tell(zarfile);
                    write_header(1, zarfile, zar_name);
                    append();
                    long endpos = fio_tell(zarfile);
                    fio_seek(zarfile,zarpos,SEEK_SET);
                    write_header(1, zarfile, zar_name);
                    fio_seek(zarfile,endpos,SEEK_SET);
                }
            }
            while (ffind_next(fh,&file) == 0);
        }
        headersize = 0;
        write_header(1, zarfile, zar_name);
        CON_printf(M_NBRFILES, file_count);
    }
    else if (xflag && !aflag)
    {
        TRACEF("Extract archive\n");
        if ((zarfile = fio_open(zar_name, "rb")) == NULL)
        {
            CON_printf(M_CANTOPEN,zar_name);
            return -6;
        }
        CON_printf(M_PROCZAR, zar_name);
        first_hdr_pos  = find_header(zarfile);
        if (first_hdr_pos < 0)
        {
            CON_printf(M_NOTZAR, zar_name);
            goto err;
        }
        fio_seek(zarfile, first_hdr_pos, SEEK_SET);
        if (!read_header(1, zarfile, zar_name))
        {
            CON_printf(M_BADCOMNT);
            goto err;
        }
        get_date_str(date_str, time_stamp);
        CON_printf(M_ZARDATE,date_str);
        extract();
        CON_printf(M_NBRFILES, file_count);
    }
    else if (!xflag && aflag)
    {
    }
    
err:
    fio_close(zarfile);

    if (error_count > 0)
    {
        //error(M_ERRORCNT, "");
        CON_printf(M_ERRORCNT,"",error_count);
        return -6;
    }
    return 0;
}
}
