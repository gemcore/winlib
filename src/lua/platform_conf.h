// eLua platform configuration

#ifndef __PLATFORM_CONF_H__
#define __PLATFORM_CONF_H__

#include "modules/auxmods.h"
#include "type.h"
#include "stacks.h"

// *****************************************************************************
// Define here what components you want for this platform

//ADG!
//#define BUILD_SHELL
//#define BUILD_ROMFS
#define BUILD_CON_GENERIC
//#define BUILD_TERM

#define TERM_LINES    25
#define TERM_COLS     80

// *****************************************************************************
// Auxiliary libraries that will be compiled for this platform

#define LUA_PLATFORM_LIBS_ROM\
  _ROM( AUXLIB_PD, luaopen_pd, pd_map )\
  _ROM( AUXLIB_CPU, luaopen_cpu, cpu_map )\
  _ROM( LUA_MATHLIBNAME, luaopen_math, math_map )
//  _ROM( AUXLIB_TMR, luaopen_tmr, tmr_map )\
//  _ROM( LUA_MATHLIBNAME, luaopen_math, math_map )\
//  _ROM( AUXLIB_TERM, luaopen_term, term_map )

// Bogus defines for common.c
#define CON_UART_ID           0
#define CON_TIMER_ID          0

// *****************************************************************************
// Configuration data

// Virtual timers (0 if not used)
#define VTMR_NUM_TIMERS       0
#define VTMR_FREQ_HZ          10 // the base clock for the virtual timers is 10Hz

// Number of resources (0 if not available/not implemented)
#define NUM_PIO               0
#define NUM_SPI               0
#define NUM_UART              0
#define NUM_TIMER             1
#define NUM_PWM               0
#define NUM_ADC               0
#define NUM_CAN               0

// CPU frequency (needed by the CPU module, 0 if not used)
#define CPU_FREQUENCY         50000000L

// PIO prefix ('0' for P0, P1, ... or 'A' for PA, PB, ...)
#define PIO_PREFIX            'A'
// Pins per port configuration:
// #define PIO_PINS_PER_PORT (n) if each port has the same number of pins, or
// #define PIO_PIN_ARRAY { n1, n2, ... } to define pins per port in an array
// Use #define PIO_PINS_PER_PORT 0 if this isn't needed
#define PIO_PINS_PER_PORT     0

// Allocator data: define your free memory zones here in two arrays
// (start address and end address)
#define MEM_START_ADDRESS     { ( void* )&heap_start }
#define MEM_END_ADDRESS       { ( void* )&heap_end }

#endif // #ifndef __PLATFORM_CONF_H__
