/*
   File:       safemem.h

   Created:    August 30, 1998
   
   Modified:   January 25, 2000

   Author:     Gunnar Andersson (gunnar@radagast.se)

   Contents:   The interface to the safer version of malloc.
*/



#ifndef SAFEMEM_H
#define SAFEMEM_H



#include <stdlib.h>



void *
safe_malloc( size_t size );

void *
safe_realloc( void *ptr, size_t size );



#endif  /* __SAFEMEM_H */
