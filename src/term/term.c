/*
 * term.c
 *
 *  Created on: Jan 06, 2020
 *     Author: Alan
 * 
 * 	ANSI Terminal sequence support, which makes use of the following sequences:
 * 
 *    Sequence	         Code	    Description	Behavior
 *     ESC [ <n> A	     CUU	    Cursor Up	Cursor up by <n>
 *     ESC [ <n> B	     CUD	    Cursor Down	Cursor down by <n>
 *     ESC [ <n> C	     CUF	    Cursor Forward	Cursor forward (Right) by <n>
 *     ESC [ <n> D	     CUB	    Cursor Backward	Cursor backward (Left) by <n>
 *     ESC [ <n> G	     CHA	    Cursor Horizontal Absolute	Cursor moves to <n>th position horizontally in the current line
 *     ESC [ <n> d	     VPA	    Vertical Line Position Absolute	Cursor moves to the <n>th position vertically in the current column
 *     ESC [ <y> ; <x> H CUP	    Cursor Position	*Cursor moves to <x>; <y> coordinate within the viewport, where <x> is the column of the <y> line
 *     ESC [ 2 J   CLS            Clear Screen
 *     ESC [ <n> K EL             Erase Line
 *                                <n> is 0 or missing, clear from cursor to end of line
 *                                <n> is 1, clear from beginning of line to cursor
 *                                <n> is 2, clear entire line      
 *    Used:
 *    ESC [ <n> E	       CNL      Cursor Next Line	Cursor down <n> lines from current position
 *    ESC [ <n> F	       CPL      Cursor Previous Line	Cursor up <n> lines from current position
 *    ESC [ <y> ; <x> f  HVP      Horizontal Vertical Position
 *                                Cursor moves to <x>; <y> coordinate within the viewport, where <x> is the column of the <y> line
*/

#include <stdarg.h>
#include <string.h>
#include "term.h"
#include "type.h"
#ifndef WIN32
#include "fio.h"
#else
#include <stdio.h>
#define BUFSIZ	512
#endif
#include "platform_conf.h"

//#ifdef BUILD_TERM
#if 1

static void null_out(u8 b)
{
}

static int null_in(int c)
{
    return -1;
}

static int null_translate(int c)
{
    return KC_UNKNOWN;
}

// Local variables
static p_term_out term_out = null_out;
static p_term_in term_in = null_in;
static p_term_translate term_translate = null_translate;
static unsigned term_num_lines, term_num_cols;
static unsigned term_cx, term_cy;

// *****************************************************************************
// Terminal functions

// Helper function: send the requested string to the terminal
void term_ansi( const char* fmt, ... )
{
  char seq[ TERM_MAX_ANSI_SIZE + 1 ];
  int i;
  va_list ap;
    
  seq[ TERM_MAX_ANSI_SIZE ] = '\0';
  seq[ 0 ] = '\x1B';
  seq[ 1 ] = '[';
  va_start( ap, fmt );
  vsnprintf( seq + 2, TERM_MAX_ANSI_SIZE - 2, fmt, ap );
  va_end( ap );
  i = 0;
  while (seq[i] != '\0')
  {
    term_out(seq[i]);
    i++;
  }
}

// Clear the screen
void term_clrscr()
{
  term_ansi( "2J" );
  term_cx = term_cy = 1;
}

// Clear line mode: 0=from cursor position to end of line.
void term_clrline(int mode)
{
  term_ansi( "%dK", mode);
}

// Move cursor to (x, y) 
// If y or x is 0 then use absolute position for the other coordinate.
void term_gotoxy( unsigned x, unsigned y )
{
  if ( y == 0 )
  {
    if ( x > 0 && x <= term_num_cols )
    {
      // Move cursor to absolute horizontal position.
      term_ansi( "%uG", x-1 );
      term_cx = x;
    }
    return;
  }

  if ( y <= term_num_lines )
  {
    if ( x == 0)
    {
      // Move cursor to absolute vertical position.
      term_ansi( "%ud", y-1 );
      term_cy = y;
      return;
    }

    if ( x <= term_num_cols )
    {
      // Move cursor to row and column position.
      term_ansi( "%u;%uH", y, x );
      term_cx = x;
      term_cy = y;
    }
  }
}

// Move cursor up "delta" lines
void term_up( unsigned delta )
{
  term_ansi( "%uA", delta );  
  term_cy -= delta;
}

// Move cursor down "delta" lines
void term_down( unsigned delta )
{
  term_ansi( "%uB", delta );  
  term_cy += delta;
}

// Move cursor forward "delta" chars
void term_right( unsigned delta )
{
  term_ansi( "%uC", delta );  
  term_cx += delta;
}

// Move cursor backwoard "delta" chars
void term_left( unsigned delta )
{
  term_ansi( "%uD", delta );  
  term_cx -= delta;
}

// Return the number of terminal lines
unsigned term_get_lines()
{
  return term_num_lines;
}

// Return the number of terminal columns
unsigned term_get_cols()
{
  return term_num_cols;
}

// Write a character to the terminal
void term_putch( u8 ch )
{
  if( ch == '\n' )
  {
    if( term_cy <= term_num_lines )
      term_cy ++;
    term_cx = 0;
    term_out( ch );
  }
  else
  {
    if( term_cx <= term_num_cols )
    {
      term_cx ++;
      term_out( ch );
    }
  }
}

// Write a string to the terminal
void term_putstr( const char* str, unsigned size )
{
  while( size )
  {
    term_putch( *str++ );
    size--;
  }
}

// Write a formatted string to the terminal
void term_printf( const char *pcFormat, ...)
{
    va_list ap;    
    // Start the varargs processing.
    va_start(ap, pcFormat);
    {
    char buf[BUFSIZ];
    vsprintf(buf, pcFormat, ap);
    term_putstr(buf, strlen(buf));
    }
    // We're finished with the varargs now.
    va_end(ap);
}

void term_printfxy( unsigned x, unsigned y, const char *pcFormat, ...)
{
    va_list ap;    
    // Start the varargs processing.
    va_start(ap, pcFormat);
    {
    char buf[BUFSIZ];
    vsprintf(buf, pcFormat, ap);
    unsigned x0 = term_cx;
    unsigned y0 = term_cy;
    term_gotoxy(x, y);
    term_putstr(buf, strlen(buf));
    term_gotoxy(x0, y0);
    }
    // We're finished with the varargs now.
    va_end(ap);
}

// Return the cursor "x" position
unsigned term_get_cx()
{
  return term_cx;
}

// Return the cursor "y" position
unsigned term_get_cy()
{
  return term_cy;
}

// Return a char read from the terminal
// If "mode" is TERM_INPUT_DONT_WAIT, return the char only if it is available,
// otherwise return -1
// Calls the translate function to translate the terminal's physical key codes
// to logical key codes (defined in the term.h header)
int term_getch( int mode )
{
  int ch;
  
  if( ( ch = term_in( mode ) ) == -1 )
    return -1;
  else
    return term_translate( ch );
}

void term_init( unsigned lines, unsigned cols, p_term_out term_out_func, 
                p_term_in term_in_func, p_term_translate term_translate_func )
{
  term_num_lines = lines;
  term_num_cols = cols;
  term_out = term_out_func;
  term_in = term_in_func;
  term_translate = term_translate_func;
  term_cx = term_cy = 0;
}                

void term_term()
{
}

#else // #ifdef BUILD_TERM

void term_init( unsigned lines, unsigned cols, p_term_out term_out_func, 
                p_term_in term_in_func, p_term_translate term_translate_func )
{
}

void term_term()
{
}

#endif // #ifdef BUILD_TERM
