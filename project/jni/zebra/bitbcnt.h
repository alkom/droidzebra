/*
   File:          bitbcnt.h

   Created:       November 22, 1999
   
   Modified:      November 24, 2005

   Authors:       Gunnar Andersson (gunnar@radagast.se)

   Contents:
*/



#ifndef BITBCNT_H
#define BITBCNT_H



#include "bitboard.h"
#include "macros.h"



extern int (REGPARM(2) * const CountFlips_bitboard[78])(unsigned int my_bits_high, unsigned int my_bits_low);



#endif  /* BITBCNT_H */
