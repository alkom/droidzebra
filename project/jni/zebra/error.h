/*
   File:       error.h

   Created:    June 13, 1998
   
   Modified:   August 1, 2002

   Author:     Gunnar Andersson (gunnar@radagast.se)

   Contents:   The interface to the error handler.
*/



#ifndef ERROR_H
#define ERROR_H



#ifdef __cplusplus
extern "C" {
#endif



void
fatal_error( const char *format, ... );



#ifdef __cplusplus
}
#endif



#endif  /* ERROR_H */
