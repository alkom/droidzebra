/*
   File:          stable.c

   Created:       March 20, 1999

   Modified:      November 22, 2005

   Authors:       Gunnar Andersson (gunnar@radagast.se)
                  David John Summers
                  Toshihiko Okuhara

   Contents:      Code which conservatively estimates the number of
                  stable (unflippable) discs using the concept
		  "Zardoz stability" along with edge tables.

   This piece of software is released under the GPL.
   See the file COPYING for more information.
*/



#include "porting.h"

#include <stdio.h>

#include "bitboard.h"
#include "bitbtest.h"
#include "constant.h"
#include "end.h"
#include "macros.h"
#include "patterns.h"



/* This constant is used in the DynP stuff for edge stability
   and simply denotes "value not known". */
#define  UNDETERMINED             -1

/* The maximum number of nodes to search when attempting
   a perfect stability assessment */
#define  MAX_STABILITY_NODES      10000

/* When this flag is set, the DynP tables are calculated and
   output and then the program is terminated. */
#define  DEBUG                    0



/* Global variables */

/* All discs determined as stable last time COUNT_STABLE was called
   for the two colors */
BitBoard last_black_stable, last_white_stable;



/* Local variables */

/* For each of the 3^8 edges, edge_stable[] holds an 8-bit mask
   where a bit is set if the corresponding disc can't be changed EVER. */
static short edge_stable[6561];

/* For each edge, *_stable[] holds the number of safe discs counted
   as follows: 1 for a stable corner and 2 for a stable non-corner.
   This to avoid counting corners twice. */
static unsigned char black_stable[6561], white_stable[6561];

/* A conversion table from the 2^8 edge values for one player to
   the corresponding base-3 value. */
static short base_conversion[256];

/* The base-3 indices for the edges */
static int edge_a1h1, edge_a8h8, edge_a1a8, edge_h1h8;


/* Position list used in the complete stability search */

MoveLink stab_move_list[100];

#if 0
INLINE static void
apply_64( BitBoard *target,
	  BitBoard base,
	  unsigned int hi_mask,
	  unsigned int lo_mask ) {
  unsigned int cond_mask = (unsigned int) -(((~base.high & hi_mask) | (~base.low & lo_mask)) == 0);
	/* All 1 if all of hi/lo mask bits are set */
  target->high |= hi_mask & cond_mask;
  target->low |= lo_mask & cond_mask;
}
#endif

INLINE static void
and_line_shift_64( BitBoard *target,
	           BitBoard base,
	           int shift,
	           BitBoard dir_ss ) {
  /* Shift to the left */
  dir_ss.high |= (base.high << shift) | (base.low >> (32 - shift));
  dir_ss.low |= base.low << shift;

  /* Shift to the right */
  dir_ss.high |= base.high >> shift;
  dir_ss.low |= (base.low >> shift) | (base.high << (32 - shift));

  target->high &= dir_ss.high;
  target->low &= dir_ss.low;
}

/*
  EDGE_ZARDOZ_STABLE
  Determines the bit mask for (a subset of) the stable discs in a position.
  Zardoz' algorithm + edge tables is used.
*/

INLINE static void
edge_zardoz_stable( BitBoard *ss,
		    BitBoard dd,
		    BitBoard od ) {
/* dd is the disks of the side we are looking for stable disks for
   od is the opponent
   ss are the stable disks */

  BitBoard ost, fb, lrf, udf, daf, dbf;
  BitBoard expand_ss;
  unsigned int t;

/* ost is a simple test to see if numbers of
   stable disks have stopped increasing.

   fb is the squares which have been played
   ie either by white or black 

   udf are the up-down columns that are filled, and so no vertical flips
   lrf are the left-right
   daf are the NE-SW diags filled
   dbf are the NW-SE diags filled */

/* a stable disk is a disk that has a stable disk on one
   side in each of the 4 directions
   N.B. beyond the edges is of course stable */

  fb.high = dd.high | od.high;
  fb.low = dd.low | od.low;

  t = fb.high;
  t &= (t >> 4);
  t &= (t >> 2);
  t &= (t >> 1);
  lrf.high = ((t & 0x01010101) * 255) | 0x81818181;
  t = fb.low;
  t &= (t >> 4);
  t &= (t >> 2);
  t &= (t >> 1);
  lrf.low = ((t & 0x01010101) * 255) | 0x81818181;

  t = fb.high & fb.low;
  t &= (t >> 16) | (t << 16);
  t &= (t >> 8) | (t << 24);
  udf.high = t | 0xFF000000;
  udf.low = t | 0x000000FF;

  daf.high = 0xFF818181;
  daf.low = 0x818181FF;
  t = ((((fb.high << 4) | 0x0F0F0F0F) & fb.low) | 0xE0C08000) & 0x1FFFFFFE;
  t &= (t >> 14) | (t << 14);	/* rotate within bit 1 and bit 28 */
  t &= (t >> 7) | (t << 21);
  daf.low |= t & 0x1F3F7EFC;
  daf.high |= (t >> 4) & 0x0103070F;
  t = ((((fb.low >> 4) | 0xF0F0F0F0) & fb.high) | 0x00010307) & 0x7FFFFFF8;
  t &= (t >> 14) | (t << 14);	/* rotate within bit 3 and bit 30 */
  t &= (t >> 7) | (t << 21);
  daf.high |= t & 0x3E7CF8F0;
  daf.low |= (t << 4) & 0xE0C08000;

  dbf.high = 0xFF818181;
  dbf.low = 0x818181FF;
  t = ((fb.high >> 4) | 0xF0F0F0F0) & fb.low;
				/* 17 16 15 14 13 12 11 10  9  8 NG  6  5  4  3  2  1  0 */
  t &= (t >> 18) | 0x0003C000;	/*  *  *  *  * 31 30 29 28 27 26 25 NG 23 22 21 20 19 18 */
  t &= (t >> 9) | (t << 9);	/*  8 NG  6  5  4  3  2  1  0 17 16 15 14 13 12 11 10  9 */
  t |= (t << 18);		/* 26 25 NG 23 22 21 20 19 18  *  *  *  * 31 30 29 28 27 */
  dbf.low |= t & 0xF8FC7E3F;
  dbf.high |= (t << 4) & 0x80C0E0F0;
  t = ((fb.low << 4) | 0x0F0F0F0F) & fb.high;
  t &= (t >> 18) | 0x0003C000;
  t &= (t >> 9) | (t << 9);
  t |= (t << 18);
  dbf.high |= t & 0x7C3E1F0F;
  dbf.low |= (t >> 4) & 0x07030100;

  ss->high |= (lrf.high & udf.high & daf.high & dbf.high & dd.high);
  ss->low |= (lrf.low & udf.low & daf.low & dbf.low & dd.low);

  if ((ss->high | ss->low) == 0)
    return;

  do {
    ost = *ss;

    expand_ss.high = lrf.high | (ost.high << 1) | (ost.high >> 1);
    expand_ss.low = lrf.low | (ost.low << 1) | (ost.low >> 1);
    and_line_shift_64( &expand_ss, ost, 8, udf );
    and_line_shift_64( &expand_ss, ost, 7, daf );
    and_line_shift_64( &expand_ss, ost, 9, dbf );

    ss->high = ost.high | (expand_ss.high & dd.high);
    ss->low = ost.low | (expand_ss.low & dd.low);
  } while ( (ost.high ^ ss->high) | (ost.low ^ ss->low) );	/* changing */

  // ss->high &= dd.high;
  // ss->low &= dd.low;
}



/*
  COUNT_EDGE_STABLE
  Returns the number of stable edge discs for COLOR.
  Side effect: The edge indices are calculated. They are needed
  by COUNT_STABLE below.
*/

int
count_edge_stable( int color,
		   BitBoard col_bits,
		   BitBoard opp_bits ) {
  unsigned int col_mask, opp_mask, ix_a1a8, ix_h1h8, ix_a1h1, ix_a8h8;

  col_mask = (((col_bits.low & 0x01010101) + ((col_bits.high & 0x01010101) << 4)) * 0x01020408) >> 24;
  opp_mask = (((opp_bits.low & 0x01010101) + ((opp_bits.high & 0x01010101) << 4)) * 0x01020408) >> 24;
  ix_a1a8 = base_conversion[col_mask] - base_conversion[opp_mask];

  col_mask = ((((col_bits.low & 0x80808080) >> 4) + (col_bits.high & 0x80808080)) * (0x01020408 / 8)) >> 24;
  opp_mask = ((((opp_bits.low & 0x80808080) >> 4) + (opp_bits.high & 0x80808080)) * (0x01020408 / 8)) >> 24;
  ix_h1h8 = base_conversion[col_mask] - base_conversion[opp_mask];

  ix_a1h1 = base_conversion[col_bits.low & 255] - base_conversion[opp_bits.low & 255];

  ix_a8h8 = base_conversion[col_bits.high >> 24] - base_conversion[opp_bits.high >> 24];

  if ( color == BLACKSQ ) {
    edge_a1h1 = 3280 * EMPTY - ix_a1h1;
    edge_a8h8 = 3280 * EMPTY - ix_a8h8;
    edge_a1a8 = 3280 * EMPTY - ix_a1a8;
    edge_h1h8 = 3280 * EMPTY - ix_h1h8;

    return (unsigned char)(black_stable[edge_a1h1] + black_stable[edge_a1a8]
      + black_stable[edge_a8h8] + black_stable[edge_h1h8]) / 2;

  } else {
    edge_a1h1 = 3280 * EMPTY + ix_a1h1;
    edge_a8h8 = 3280 * EMPTY + ix_a8h8;
    edge_a1a8 = 3280 * EMPTY + ix_a1a8;
    edge_h1h8 = 3280 * EMPTY + ix_h1h8;

    return (unsigned char)(white_stable[edge_a1h1] + white_stable[edge_a1a8]
      + white_stable[edge_a8h8] + white_stable[edge_h1h8]) / 2;
  }
}



/*
  COUNT_STABLE
  Returns the number of stable discs for COLOR.
  Side effect: last_black_stable or last_white_stable is modified.
  Note: COUNT_EDGE_STABLE must have been called immediately
        before this function is called *or you lose big*.
*/

int
count_stable( int color,
	      BitBoard col_bits,
	      BitBoard opp_bits ) {
  unsigned int t;
  BitBoard col_stable;
  BitBoard common_stable;

  /* Stable edge discs */

  common_stable.low = edge_stable[edge_a1h1];

  common_stable.high = (edge_stable[edge_a8h8] << 24);

  t = edge_stable[edge_a1a8];
  common_stable.low |= ((t & 0x0F) * 0x00204081) & 0x01010101;
  common_stable.high |= ((t >> 4) * 0x00204081) & 0x01010101;

  t = edge_stable[edge_h1h8];
  common_stable.low |= ((t & 0x0F) * 0x10204080) & 0x80808080;
  common_stable.high |= ((t >> 4) * 0x10204080) & 0x80808080;

  /* Expand the stable edge discs into a full set of stable discs */

  col_stable.high = col_bits.high & common_stable.high;
  col_stable.low = col_bits.low & common_stable.low;
  edge_zardoz_stable( &col_stable, col_bits, opp_bits );
  if ( color == BLACKSQ )
    last_black_stable = col_stable;
  else
    last_white_stable = col_stable;

  if ( col_stable.high | col_stable.low )
    return non_iterative_popcount( col_stable.high, col_stable.low );
  else
    return 0;
}



/*
  STABILITY_SEARCH
  Searches the subtree rooted at the current position and tries to
  find variations in which the discs in CANDIDATE_BITS are
  flipped. Aborts if all those discs are stable in the subtree.
*/

static void
stability_search( BitBoard my_bits,
		  BitBoard opp_bits,
		  int side_to_move,
		  BitBoard *candidate_bits,
		  int max_depth,
		  int last_was_pass,
		  int *stability_nodes ) {
  int sq, old_sq;
  int mobility;
  BitBoard black_bits, white_bits;
  BitBoard new_my_bits, new_opp_bits;
  BitBoard all_stable_bits;

  (*stability_nodes)++;
  if ( *stability_nodes > MAX_STABILITY_NODES )
    return;

  if ( max_depth >= 3 ) {
    if ( side_to_move == BLACKSQ ) {
      black_bits = my_bits;
      white_bits = opp_bits;
    }
    else {
      black_bits = opp_bits;
      white_bits = my_bits;
    }
    CLEAR( all_stable_bits );
    (void) count_edge_stable( BLACKSQ, black_bits, white_bits );
    if ( (candidate_bits->high & black_bits.high) ||
	 (candidate_bits->low  & black_bits.low ) ) {
      (void) count_stable( BLACKSQ, black_bits, white_bits );
      APPLY_OR( all_stable_bits, last_black_stable );
    }
    if ( (candidate_bits->high & white_bits.high) ||
	 (candidate_bits->low  & white_bits.low ) ) {
      (void) count_stable( WHITESQ, white_bits, black_bits );
      APPLY_OR( all_stable_bits, last_white_stable );
    }
    if ( ((candidate_bits->high & ~all_stable_bits.high) == 0) &&
	 ((candidate_bits->low  & ~all_stable_bits.low ) == 0) )
      return;
  }

  mobility = 0;
  for ( old_sq = END_MOVE_LIST_HEAD, sq = stab_move_list[old_sq].succ;
	sq != END_MOVE_LIST_TAIL;
	old_sq = sq, sq = stab_move_list[sq].succ ) {
    if ( TestFlips_bitboard[sq - 11]( my_bits.high, my_bits.low, opp_bits.high, opp_bits.low ) ) {
      new_my_bits = bb_flips;
      APPLY_ANDNOT( bb_flips, my_bits );
      APPLY_ANDNOT( (*candidate_bits), bb_flips );
      if ( max_depth > 1 ) {
        FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
	stab_move_list[old_sq].succ = stab_move_list[sq].succ;
	stability_search( new_opp_bits, new_my_bits, OPP( side_to_move ),
			  candidate_bits, max_depth - 1, FALSE,
			  stability_nodes );
	stab_move_list[old_sq].succ = sq;
      }
      mobility++;
    }
  }

  if ( (mobility == 0) && !last_was_pass )
    stability_search( opp_bits, my_bits, OPP( side_to_move ),
		      candidate_bits, max_depth, TRUE, stability_nodes );
}



/*
  COMPLETE_STABILITY_SEARCH
  Tries to compute all stable discs by search the entire game tree.
  The actual work is performed by STABILITY_SEARCH above.
*/

static void
complete_stability_search( int *board,
			   int side_to_move,
			   BitBoard *stable_bits ) {
  int i, j;
  int empties;
  int shallow_depth;
  int stability_nodes;
  int abort;
  BitBoard my_bits, opp_bits;
  BitBoard all_bits, candidate_bits;
  BitBoard test_bits;

  /* Prepare the move list */

  int last_sq = END_MOVE_LIST_HEAD;
  for ( i = 0; i < 60; i++ ) {
    int sq = position_list[i];
    if ( board[sq] == EMPTY ) {
      stab_move_list[last_sq].succ = sq;
      stab_move_list[sq].pred = last_sq;
      last_sq = sq;
    }
  }
  stab_move_list[last_sq].succ = END_MOVE_LIST_TAIL;

  empties = 0;
  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ )
      if ( board[10 * i + j] == EMPTY )
	empties++;

  /* Prepare the bitmaps for the stability search */

  set_bitboards( board, side_to_move, &my_bits, &opp_bits );

  FULL_OR( all_bits, my_bits, opp_bits );

  FULL_ANDNOT( candidate_bits, all_bits, (*stable_bits) );

  /* Search all potentially stable discs for at most 4 plies
     to weed out those easily flippable */

  stability_nodes = 0;
  shallow_depth = 4;
  stability_search( my_bits, opp_bits, side_to_move, &candidate_bits,
		    MIN( empties, shallow_depth ), FALSE, &stability_nodes );

  /* Scan through the rest of the discs one at a time until the
     maximum number of stability nodes is exceeded. Hopefully
     a subset of the stable discs is found also if this happens. */

  abort = FALSE;
  for ( i = 1; (i <= 8) && !abort; i++ )
    for ( j = 1; (j <= 8) && !abort; j++ ) {
      int sq = 10 * i + j;
      test_bits = square_mask[sq];
      if ( (test_bits.high & candidate_bits.high) |
	   (test_bits.low  & candidate_bits.low ) ) {
	stability_search( my_bits, opp_bits, side_to_move, &test_bits,
			  empties, FALSE, &stability_nodes );
	abort = (stability_nodes > MAX_STABILITY_NODES);
	if ( !abort ) {
	  if ( test_bits.high | test_bits.low ) {
	    stable_bits->high |= test_bits.high;
	    stable_bits->low  |= test_bits.low;
	  }
	}
      }
    }
}



/*
  GET_STABLE
  Determines what discs on BOARD are stable with SIDE_TO_MOVE to play next.
  The stability status of all squares (black, white and empty)
  is returned in the boolean vector IS_STABLE.
*/

void
get_stable( int *board,
	    int side_to_move,
	    int *is_stable ) {
  int i, j;
  unsigned int mask;
  BitBoard black_bits, white_bits, all_stable;

  set_bitboards( board, BLACKSQ, &black_bits, &white_bits );

  for ( i = 0; i < 100; i++ )
    is_stable[i] = FALSE;

  if ( ((black_bits.high | black_bits.low) == 0) ||
       ((white_bits.high | white_bits.low) == 0) )
    for ( i = 1; i <= 8; i++ )
      for ( j = 1; j <= 8; j++ )
	is_stable[10 * i + j] = TRUE;
  else {  /* Nobody wiped out */
    (void) count_edge_stable( BLACKSQ, black_bits, white_bits );
    (void) count_stable( BLACKSQ, black_bits, white_bits );
    (void) count_stable( WHITESQ, white_bits, black_bits );

    FULL_OR( all_stable, last_black_stable, last_white_stable );

    complete_stability_search( board, side_to_move, &all_stable );

    for ( i = 1, mask = 1; i <= 4; i++ )
      for ( j = 1; j <= 8; j++, mask <<= 1 )
	if ( all_stable.low & mask )
	  is_stable[10 * i + j] = TRUE;
    for ( i = 5, mask = 1; i <= 8; i++ )
      for ( j = 1; j <= 8; j++, mask <<= 1 )
	if ( all_stable.high & mask )
	  is_stable[10 * i + j] = TRUE;
  }
}



#if DEBUG
/*
  DISPLAY_ROW
  Display an edge configuration and highlight the stable discs.
*/

static void
display_row( int pattern ) {
  int i;
  int mask = edge_stable[pattern];
  int temp = pattern;

  for ( i = 0; i < 8; i++ ) {
    switch ( temp % 3) {
    case EMPTY:
      putchar( '-' );
      break;
    case BLACKSQ:
      if ( mask & (1 << i) )
	putchar( 'X' );
      else
	putchar( 'x' );
      break;
    case WHITESQ:
      if ( mask & (1 << i) )
	putchar( 'O' );
      else
	putchar( 'o' );
    }
    temp /= 3;
  }
#ifdef TEXT_BASED
  printf( "     pattern %4d   black %2d   white %2d\n", pattern,
	  black_stable[pattern], white_stable[pattern] );
#endif
}
#endif



/*
  RECURSIVE_FIND_STABLE
  Returns a bit mask describing the set of stable discs in the
  edge PATTERN. When a bit mask is calculated, it's stored in
  a table so that any particular bit mask only is generated once.
*/

static int
recursive_find_stable( int pattern ) {
  int i, j;
  int new_pattern;
  int stable;
  int temp;
  int row[8], stored_row[8];

  if ( edge_stable[pattern] != UNDETERMINED )
    return edge_stable[pattern];

  temp = pattern;
  for ( i = 0; i < 8; i++, temp /= 3 )
    row[i] = temp % 3;

  /* All positions stable unless proved otherwise. */

  stable = 255;

  /* Play out the 8 different moves and AND together the stability masks. */

  for ( j = 0; j < 8; j++ )
    stored_row[j] = row[j];

  for ( i = 0; i < 8; i++ ) {

    /* Make sure we work with the original configuration */

    for ( j = 0; j < 8; j++ )
      row[j] = stored_row[j];

    if ( row[i] == EMPTY ) {  /* Empty ==> playable! */

      /* Mark the empty square as unstable and store position */

      stable &= ~(1 << i);

      /* Play out a black move */

      row[i] = BLACKSQ;
      if ( i >= 2 ) {
	j = i - 1;
	while ( (j >= 1) && (row[j] == WHITESQ) )
	  j--;
	if ( row[j] == BLACKSQ )
	  for ( j++; j < i; j++ ) {
	    row[j] = BLACKSQ;
	    stable &= ~(1 << j);
	  }
      }
      if ( i <= 5 ) {
	j = i + 1;
	while ( (j <= 6) && (row[j] == WHITESQ) )
	  j++;
	if ( row[j] == BLACKSQ )
	  for ( j--; j > i; j-- ) {
	    row[j] = BLACKSQ;
	    stable &= ~(1 << j);
	  }
      }
      new_pattern = 0;
      for ( j = 0; j < 8; j++ )
	new_pattern += pow3[j] * row[j];
      stable &= recursive_find_stable( new_pattern );

      /* Restore position */

      for ( j = 0; j < 8; j++ )
	row[j] = stored_row[j];

      /* Play out a white move */

      row[i] = WHITESQ;
      if ( i >= 2 ) {
	j = i - 1;
	while ( (j >= 1) && (row[j] == BLACKSQ) )
	  j--;
	if ( row[j] == WHITESQ )
	  for ( j++; j < i; j++ ) {
	    row[j] = WHITESQ;
	    stable &= ~(1 << j);
	  }
      }
      if ( i <= 5 ) {
	j = i + 1;
	while ( (j <= 6) && (row[j] == BLACKSQ) )
	  j++;
	if ( row[j] == WHITESQ )
	  for ( j--; j > i; j-- ) {
	    row[j] = WHITESQ;
	    stable &= ~(1 << j);
	  }
      }
      new_pattern = 0;
      for ( j = 0; j < 8; j++ )
	new_pattern += pow3[j] * row[j];
      stable &= recursive_find_stable( new_pattern );
    }
  }

  /* Store and return */

  edge_stable[pattern] = stable;

  return stable;
}



/*
  COUNT_COLOR_STABLE
  Determines the number of stable discs for each of the edge configurations
  for the two colors. This is done using the following convention:
  - a stable corner disc gives stability of 1
  - a stable non-corner disc gives stability of 2
  This way the stability values for the four edges can be added together
  without any risk for double-counting.
*/

static void
count_color_stable( void ) {
  int i, j;
  int pattern;
  int row[8];
  static const int stable_incr[8] = { 1, 2, 2, 2, 2, 2, 2, 1};

  for ( i = 0; i < 8; i++ )
    row[i] = 0;

  for ( pattern = 0; pattern < 6561; pattern++ ) {
    black_stable[pattern] = 0;
    white_stable[pattern] = 0;
    for ( j = 0; j < 8; j++ )
      if ( edge_stable[pattern] & (1 << j) ) {
	if ( row[j] == BLACKSQ ) {
	  black_stable[pattern] += stable_incr[j];
	}
	else if ( row[j] == WHITESQ ) {
	  white_stable[pattern] += stable_incr[j];
	}
      }

    /* Next configuration */
    i = 0;
    do {  /* The odometer principle */
      row[i]++;
      if (row[i] == 3)
	row[i] = 0;
      i++;
    } while ( (row[i - 1] == 0) && (i < 8) );
  }
}



/*
  INIT_STABLE
  Build the table containing the stability masks for all edge
  configurations. This is done using dynamic programming.
*/

void
init_stable( void ) {
  int i, j;

  for ( i = 0; i < 256; i++ ) {
    base_conversion[i] = 0;
    for ( j = 0; j < 8; j++ )
      if ( i & (1 << j) )
	base_conversion[i] += pow3[j];
  }

  for ( i = 0; i < 6561; i++ )
    edge_stable[i] = UNDETERMINED;
  for ( i = 0; i < 6561; i++ )
    if ( edge_stable[i] == UNDETERMINED )
      (void) recursive_find_stable( i );
  count_color_stable();
#if DEBUG
  for ( i = 0; i < 6561; i++ )
    display_row( i );
  exit( 1 );
#endif
}
