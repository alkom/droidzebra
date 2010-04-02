/*
   File:          doflip.c

   Modified:      November 15, 2005

   Authors:       Gunnar Andersson (gunnar@radagast.se)
	          Toshihiko Okuhara

   Contents:      Low-level code which flips the discs (if any) affected
                  by a potential move, with or without updating the
		  hash code.

   This piece of software is released under the GPL.
   See the file COPYING for more information.
*/



#include <stdio.h>
#include <stdlib.h>
#include "doflip.h"
#include "error.h"
#include "globals.h"
#include "hash.h"
#include "macros.h"
#include "moves.h"
#include "patterns.h"
#include "texts.h"
#include "unflip.h"



/* Global variables */

unsigned int hash_update1, hash_update2;



/* The board split into nine regions. */

static const char board_region[100] = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   1,   1,   2,   2,   2,   2,   3,   3,   0,
  0,   1,   1,   2,   2,   2,   2,   3,   3,   0,
  0,   4,   4,   5,   5,   5,   5,   6,   6,   0,
  0,   4,   4,   5,   5,   5,   5,   6,   6,   0,
  0,   4,   4,   5,   5,   5,   5,   6,   6,   0,
  0,   4,   4,   5,   5,   5,   5,   6,   6,   0,
  0,   7,   7,   8,   8,   8,   8,   9,   9,   0,
  0,   7,   7,   8,   8,   8,   8,   9,   9,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};



#define DrctnlFlips_six( sq, inc, color, oppcol ) {      \
  int *pt = sq + inc;                                    \
                                                         \
  if ( *pt == oppcol ) {                                 \
    pt += inc;                                           \
    if ( *pt == oppcol ) {                               \
      pt += inc;                                         \
      if ( *pt == oppcol ) {                             \
	pt += inc;                                       \
	if ( *pt == oppcol ) {                           \
	  pt += inc;                                     \
	  if ( *pt == oppcol ) {                         \
	    pt += inc;                                   \
	    if ( *pt == oppcol )                         \
	      pt += inc;                                 \
	  }                                              \
	}                                                \
      }                                                  \
    }                                                    \
    if ( *pt == color ) {                                \
      pt -= inc;                                         \
      do {                                               \
	*pt = color;                                     \
	*(t_flip_stack++) = pt;                          \
	pt -= inc;                                       \
      } while ( pt != sq );                              \
    }                                                    \
  }                                                      \
}


#define DrctnlFlips_four( sq, inc, color, oppcol ) {     \
  int *pt = sq + inc;                                    \
                                                         \
  if ( *pt == oppcol ) {                                 \
    pt += inc;                                           \
    if ( *pt == oppcol ) {                               \
      pt += inc;                                         \
      if ( *pt == oppcol ) {                             \
	pt += inc;                                       \
	if ( *pt == oppcol )                             \
	  pt += inc;                                     \
      }                                                  \
    }                                                    \
    if ( *pt == color ) {                                \
      pt -= inc;                                         \
      do {                                               \
	*pt = color;                                     \
	*(t_flip_stack++) = pt;                          \
	pt -= inc;                                       \
      } while( pt != sq );                               \
    }                                                    \
  }                                                      \
}


INLINE int REGPARM(2)
DoFlips_no_hash( int sqnum, int color ) {
  int opp_color = OPP( color );
  int *sq;
  int **old_flip_stack, **t_flip_stack;

  old_flip_stack = t_flip_stack = flip_stack;
  sq = &board[sqnum];

  switch ( board_region[sqnum] ) {
  case 1:
    DrctnlFlips_six( sq, 1, color, opp_color );
    DrctnlFlips_six( sq, 11, color, opp_color );
    DrctnlFlips_six( sq, 10, color, opp_color );
    break;
  case 2:
    DrctnlFlips_four( sq, 1, color, opp_color );
    DrctnlFlips_four( sq, 11, color, opp_color );
    DrctnlFlips_six( sq, 10, color, opp_color );
    DrctnlFlips_four( sq, 9, color, opp_color );
    DrctnlFlips_four( sq, -1, color, opp_color );
    break;
  case 3:
    DrctnlFlips_six( sq, 10, color, opp_color );
    DrctnlFlips_six( sq, 9, color, opp_color );
    DrctnlFlips_six( sq, -1, color, opp_color );
    break;
  case 4:
    DrctnlFlips_four( sq, -10, color, opp_color );
    DrctnlFlips_four( sq, -9, color, opp_color );
    DrctnlFlips_six( sq, 1, color, opp_color );
    DrctnlFlips_four( sq, 11, color, opp_color );
    DrctnlFlips_four( sq, 10, color, opp_color );
    break;
  case 5:
    DrctnlFlips_four( sq, -11, color, opp_color );
    DrctnlFlips_four( sq, -10, color, opp_color );
    DrctnlFlips_four( sq, -9, color, opp_color );
    DrctnlFlips_four( sq, 1, color, opp_color );
    DrctnlFlips_four( sq, 11, color, opp_color );
    DrctnlFlips_four( sq, 10, color, opp_color );
    DrctnlFlips_four( sq, 9, color, opp_color );
    DrctnlFlips_four( sq, -1, color, opp_color );
    break;
  case 6:
    DrctnlFlips_four( sq, -10, color, opp_color );
    DrctnlFlips_four( sq, -11, color, opp_color );
    DrctnlFlips_six( sq, -1, color, opp_color );
    DrctnlFlips_four( sq, 9, color, opp_color );
    DrctnlFlips_four( sq, 10, color, opp_color );
    break;
  case 7:
    DrctnlFlips_six( sq, -10, color, opp_color );
    DrctnlFlips_six( sq, -9, color, opp_color );
    DrctnlFlips_six( sq, 1, color, opp_color );
    break;
  case 8:
    DrctnlFlips_four( sq, -1, color, opp_color );
    DrctnlFlips_four( sq, -11, color, opp_color );
    DrctnlFlips_six( sq, -10, color, opp_color );
    DrctnlFlips_four( sq, -9, color, opp_color );
    DrctnlFlips_four( sq, 1, color, opp_color );
    break;
  case 9:
    DrctnlFlips_six( sq, -10, color, opp_color );
    DrctnlFlips_six( sq, -11, color, opp_color );
    DrctnlFlips_six( sq, -1, color, opp_color );
    break;
  default:
    break;
  }

  flip_stack = t_flip_stack;
  return t_flip_stack - old_flip_stack;
}


#define DrctnlFlipsHash_four( sq, inc, color, oppcol ) { \
  int *pt = sq + inc;                                    \
                                                         \
  if ( *pt == oppcol ) {                                 \
    pt += inc;                                           \
    if ( *pt == oppcol ) {                               \
      pt += inc;                                         \
      if ( *pt == oppcol ) {                             \
	pt += inc;                                       \
	if ( *pt == oppcol )                             \
	  pt += inc;                                     \
      }                                                  \
    }                                                    \
    if ( *pt == color ) {                                \
      pt -= inc;                                         \
      do {                                               \
        t_hash_update1 ^= hash_flip1[pt - board];        \
        t_hash_update2 ^= hash_flip2[pt - board];        \
	*pt = color;                                     \
	*(t_flip_stack++) = pt;                          \
	pt -= inc;                                       \
      } while ( pt != sq );                              \
    }                                                    \
  }                                                      \
}


#define DrctnlFlipsHash_six( sq, inc, color, oppcol ) {  \
  int *pt = sq + inc;                                    \
                                                         \
  if ( *pt == oppcol ) {                                 \
    pt += inc;                                           \
    if ( *pt == oppcol ) {                               \
      pt += inc;                                         \
      if ( *pt == oppcol ) {                             \
	pt += inc;                                       \
	if ( *pt == oppcol ) {                           \
	  pt += inc;                                     \
	  if ( *pt == oppcol ) {                         \
	    pt += inc;                                   \
	    if ( *pt == oppcol )                         \
	      pt += inc;                                 \
	  }                                              \
	}                                                \
      }                                                  \
    }                                                    \
    if ( *pt == color ) {                                \
      pt -= inc;                                         \
      do {                                               \
        t_hash_update1 ^= hash_flip1[pt - board];        \
        t_hash_update2 ^= hash_flip2[pt - board];        \
	*pt = color;                                     \
	*(t_flip_stack++) = pt;                          \
	pt -= inc;                                       \
      } while ( pt != sq );                              \
    }                                                    \
  }                                                      \
}


INLINE int REGPARM(2)
DoFlips_hash( int sqnum, int color ) {
  int opp_color = OPP( color );
  int *sq;
  int **old_flip_stack, **t_flip_stack;
  int t_hash_update1, t_hash_update2;

  old_flip_stack = t_flip_stack = flip_stack;
  t_hash_update1 = t_hash_update2 = 0;
  sq = &board[sqnum];

  switch ( board_region[sqnum] ) {
  case 1:
    DrctnlFlipsHash_six( sq, 1, color, opp_color );
    DrctnlFlipsHash_six( sq, 11, color, opp_color );
    DrctnlFlipsHash_six( sq, 10, color, opp_color );
    break;
  case 2:
    DrctnlFlipsHash_four( sq, 1, color, opp_color );
    DrctnlFlipsHash_four( sq, 11, color, opp_color );
    DrctnlFlipsHash_six( sq, 10, color, opp_color );
    DrctnlFlipsHash_four( sq, 9, color, opp_color );
    DrctnlFlipsHash_four( sq, -1, color, opp_color );
    break;
  case 3:
    DrctnlFlipsHash_six( sq, 10, color, opp_color );
    DrctnlFlipsHash_six( sq, 9, color, opp_color );
    DrctnlFlipsHash_six( sq, -1, color, opp_color );
    break;
  case 4:
    DrctnlFlipsHash_four( sq, -10, color, opp_color );
    DrctnlFlipsHash_four( sq, -9, color, opp_color );
    DrctnlFlipsHash_six( sq, 1, color, opp_color );
    DrctnlFlipsHash_four( sq, 11, color, opp_color );
    DrctnlFlipsHash_four( sq, 10, color, opp_color );
    break;
  case 5:
    DrctnlFlipsHash_four( sq, -11, color, opp_color );
    DrctnlFlipsHash_four( sq, -10, color, opp_color );
    DrctnlFlipsHash_four( sq, -9, color, opp_color );
    DrctnlFlipsHash_four( sq, 1, color, opp_color );
    DrctnlFlipsHash_four( sq, 11, color, opp_color );
    DrctnlFlipsHash_four( sq, 10, color, opp_color );
    DrctnlFlipsHash_four( sq, 9, color, opp_color );
    DrctnlFlipsHash_four( sq, -1, color, opp_color );
    break;
  case 6:
    DrctnlFlipsHash_four( sq, -10, color, opp_color );
    DrctnlFlipsHash_four( sq, -11, color, opp_color );
    DrctnlFlipsHash_six( sq, -1, color, opp_color );
    DrctnlFlipsHash_four( sq, 9, color, opp_color );
    DrctnlFlipsHash_four( sq, 10, color, opp_color );
    break;
  case 7:
    DrctnlFlipsHash_six( sq, -10, color, opp_color );
    DrctnlFlipsHash_six( sq, -9, color, opp_color );
    DrctnlFlipsHash_six( sq, 1, color, opp_color );
    break;
  case 8:
    DrctnlFlipsHash_four( sq, -1, color, opp_color );
    DrctnlFlipsHash_four( sq, -11, color, opp_color );
    DrctnlFlipsHash_six( sq, -10, color, opp_color );
    DrctnlFlipsHash_four( sq, -9, color, opp_color );
    DrctnlFlipsHash_four( sq, 1, color, opp_color );
    break;
  case 9:
    DrctnlFlipsHash_six( sq, -10, color, opp_color );
    DrctnlFlipsHash_six( sq, -11, color, opp_color );
    DrctnlFlipsHash_six( sq, -1, color, opp_color );
    break;
  default:
    break;
  }

#if 0
  pfs = old_flip_stack;
  while (pfs != t_flip_stack) {
    t_hash_update1 ^= hash_flip1[*pfs - board];
    t_hash_update2 ^= hash_flip2[*pfs - board];
    ++pfs;
  }
#endif

  hash_update1 = t_hash_update1;
  hash_update2 = t_hash_update2;
  flip_stack = t_flip_stack;
  return t_flip_stack - old_flip_stack;
}
