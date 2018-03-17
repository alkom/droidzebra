/*
   File:       error.c

   Created:    June 13, 1998
   
   Modified:   November 12, 2001

   Author:     Gunnar Andersson (gunnar@radagast.se)

   Contents:   The text-based error handler.
*/



#include "porting.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32_WCE
#include <time.h>
#endif

#include "error.h"
#include "texts.h"

#if defined(_WIN32_WCE) || defined(_MSC_VER)

#include <windows.h>


void
fatal_error( const char *format, ... ) {
  va_list arg_ptr;
  char sError[128];
  WCHAR wcsError[128];

  va_start( arg_ptr, format );

  vsprintf(sError, format, arg_ptr);
  mbstowcs(wcsError, sError, 128);
  OutputDebugString(wcsError);
  MessageBox(NULL, wcsError, L"Fatal Error", MB_OK);

  exit( EXIT_FAILURE );
}

#else	/* not Windows CE */


void
fatal_error( const char *format, ... ) {
  FILE *stream;
  time_t timer;
  va_list arg_ptr;

  va_start( arg_ptr, format );
  fprintf( stderr, "\n%s: ", FATAL_ERROR_TEXT );
  vfprintf( stderr, format, arg_ptr );

  va_end( arg_ptr );

  stream = fopen( "zebra.err", "a" );
  if ( stream != NULL ) {
    time( &timer );
    fprintf( stream, "%s @ %s\n  ", FATAL_ERROR_TEXT, ctime( &timer ) );
    va_start( arg_ptr, format );
    vfprintf( stream, format, arg_ptr );
    va_end( arg_ptr );
  }

  exit( EXIT_FAILURE );
}


#endif
