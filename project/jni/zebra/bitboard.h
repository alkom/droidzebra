/*
   File:          bitboard.h

   Created:       November 21, 1999
   
   Modified:      November 24, 2005

   Author:        Gunnar Andersson (gunnar@radagast.se)
                  Toshihiko Okuhara

   Contents:
*/



#ifndef BITBOARD_H
#define BITBOARD_H

#include "macros.h"


#define APPLY_NOT( a ) { \
  a.high = ~a.high; \
  a.low = ~a.low; \
}

#define APPLY_XOR( a, b ) { \
  a.high ^= b.high; \
  a.low ^= b.low; \
}

#define APPLY_OR( a, b ) { \
  a.high |= b.high; \
  a.low |= b.low; \
}

#define APPLY_AND( a, b ) { \
  a.high &= b.high; \
  a.low &= b.low; \
}

#define APPLY_ANDNOT( a, b ) { \
  a.high &= ~b.high; \
  a.low &= ~b.low; \
}

#define FULL_XOR( a, b, c ) { \
  a.high = b.high ^ c.high; \
  a.low = b.low ^ c.low; \
}

#define FULL_OR( a, b, c ) { \
  a.high = b.high | c.high; \
  a.low = b.low | c.low; \
}

#define FULL_AND( a, b, c ) { \
  a.high = b.high & c.high; \
  a.low = b.low & c.low; \
}

#define FULL_ANDNOT( a, b, c ) { \
  a.high = b.high & ~c.high; \
  a.low = b.low & ~c.low; \
}

#define CLEAR( a ) { \
  a.high = 0; \
  a.low = 0; \
} 



typedef struct {
  unsigned int high;
  unsigned int low;
} BitBoard;

extern BitBoard square_mask[100];



unsigned int REGPARM(2)
non_iterative_popcount( unsigned int n1, unsigned int n2 );

unsigned int REGPARM(2)
iterative_popcount( unsigned int n1, unsigned int n2 );

unsigned int REGPARM(1)
bit_reverse_32( unsigned int val );

void
set_bitboards( int *board, int side_to_move,
	       BitBoard *my_out, BitBoard *opp_out );

void
init_bitboard( void );



#endif  /* BITBOARD_H */
