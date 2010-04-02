/*
   doflip.h

   Automatically created by ENDMACRO on Fri Feb 26 20:29:42 1999

   Last modified:   October 25, 2005
*/



#ifndef DOFLIP_H
#define DOFLIP_H

#include "macros.h"


extern unsigned int hash_update1, hash_update2;



int REGPARM(2)
DoFlips_hash( int sqnum, int color );

int REGPARM(2)
DoFlips_no_hash( int sqnum, int color );



#endif  /* DOFLIP_H */
