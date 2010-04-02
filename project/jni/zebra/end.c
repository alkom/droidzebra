/*
   File:          end.c

   Created:       1994
   
   Modified:      December 19, 2005

   Authors:       Gunnar Andersson (gunnar@radagast.se)

   Contents:      The fast endgame solver.
*/



#include "porting.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "autoplay.h"
#include "bitbcnt.h"
#include "bitbmob.h"
#include "bitboard.h"
#include "bitbtest.h"
#include "cntflip.h"
#include "counter.h"
#include "display.h"
#include "doflip.h"
#include "end.h"
#include "epcstat.h"
#include "eval.h"
#include "getcoeff.h"
#include "globals.h"
#include "hash.h"
#include "macros.h"
#include "midgame.h"
#include "moves.h"
#include "osfbook.h"
#include "probcut.h"
#include "search.h"
#include "stable.h"
#include "texts.h"
#include "timer.h"
#include "unflip.h"



#define USE_MPC                      1
#define MAX_SELECTIVITY              9
#define DISABLE_SELECTIVITY          18

#define PV_EXPANSION                 16

#define DEPTH_TWO_SEARCH             15
#define DEPTH_THREE_SEARCH           20
#define DEPTH_FOUR_SEARCH            24
#define DEPTH_SIX_SEARCH             30
#define EXTRA_ROOT_SEARCH            2

#ifdef _WIN32_WCE
#define EVENT_CHECK_INTERVAL         25000.0
#else
#define EVENT_CHECK_INTERVAL         250000.0
#endif

#define LOW_LEVEL_DEPTH              8
#define FASTEST_FIRST_DEPTH          12
#define HASH_DEPTH                   (LOW_LEVEL_DEPTH + 1)

#define VERY_HIGH_EVAL               1000000

#define GOOD_TRANSPOSITION_EVAL      10000000

/* Parameters for the fastest-first algorithm. The performance does
   not seem to depend a lot on the precise values. */
#define FAST_FIRST_FACTOR            0.45
#define MOB_FACTOR                   460

/* The disc difference when special wipeout move ordering is tried.
   This means more aggressive use of fastest first. */
#define WIPEOUT_THRESHOLD            60

/* Use stability pruning? */
#define USE_STABILITY                TRUE



#if 0

// Profiling code

static long long int
rdtsc( void ) {
#if defined(__GNUC__)
  long long a;
  asm volatile("rdtsc":"=A" (a));
  return a;
#else
  return 0;
#endif
}

#endif



typedef enum {
  NOTHING,
  SELECTIVE_SCORE,
  WLD_SCORE,
  EXACT_SCORE
} SearchStatus;



MoveLink end_move_list[100];



/* The parities of the regions are in the region_parity bit vector. */

static unsigned int region_parity;

/* Pseudo-probabilities corresponding to the percentiles.
   These are taken from the normal distribution; to the percentile
   x corresponds the probability Pr(-x <= Y <= x) where Y is a N(0,1)
   variable. */

static const double confidence[MAX_SELECTIVITY + 1] =
{ 1.000, 0.99, 0.98, 0.954, 0.911, 0.838, 0.729, 0.576, 0.383, 0.197 };

/* Percentiles used in the endgame MPC */
static const double end_percentile[MAX_SELECTIVITY + 1] =
{ 100.0, 4.0, 3.0, 2.0, 1.7, 1.4, 1.1, 0.8, 0.5, 0.25 };

#if USE_STABILITY
#define  HIGH_STABILITY_THRESHOLD     24
static const int stability_threshold[] = { 65, 65, 65, 65, 65, 46, 38, 30, 24,
				           24, 24, 24, 0, 0, 0, 0, 0, 0, 0 };
#endif



static double fast_first_mean[61][64];
static double fast_first_sigma[61][64];
static int best_move, best_end_root_move;
static int true_found, true_val;
static int full_output_mode;
static int earliest_wld_solve, earliest_full_solve;
static int fast_first_threshold[61][64];
static int ff_mob_factor[61];

static BitBoard neighborhood_mask[100];
const unsigned int quadrant_mask[100] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 1, 1, 1, 1, 2, 2, 2, 2, 0,
  0, 1, 1, 1, 1, 2, 2, 2, 2, 0,
  0, 1, 1, 1, 1, 2, 2, 2, 2, 0,
  0, 1, 1, 1, 1, 2, 2, 2, 2, 0,
  0, 4, 4, 4, 4, 8, 8, 8, 8, 0,
  0, 4, 4, 4, 4, 8, 8, 8, 8, 0,
  0, 4, 4, 4, 4, 8, 8, 8, 8, 0,
  0, 4, 4, 4, 4, 8, 8, 8, 8, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Number of discs that the side to move at the root has to win with. */
static int komi_shift;



#if 1

/*
  TESTFLIPS_WRAPPER
  Checks if SQ is a valid move by
  (1) verifying that there exists a neighboring opponent disc,
  (2) verifying that the move flips some disc.
*/

INLINE static int
TestFlips_wrapper( int sq,
		   BitBoard my_bits,
		   BitBoard opp_bits ) {
  int flipped;

  if ( ((neighborhood_mask[sq].high & opp_bits.high) |
       (neighborhood_mask[sq].low & opp_bits.low)) != 0 )
    flipped = TestFlips_bitboard[sq - 11]( my_bits.high, my_bits.low, opp_bits.high, opp_bits.low );
  else
    flipped = 0;

  return flipped;
}

#else
#define TestFlips_wrapper( sq, my_bits, opp_bits ) \
  TestFlips_bitboard[sq - 11]( my_bits.high, my_bits.low, opp_bits.high, opp_bits.low )


#endif



/*
  PREPARE_TO_SOLVE
  Create the list of empty squares.
*/

static void
prepare_to_solve( const int *board ) {
  /* fixed square ordering: */
  /* jcw's order, which is the best of 4 tried (according to Warren Smith) */
  static const unsigned char worst2best[64] = {
    /*B2*/      22 , 27 , 72 , 77 ,
    /*B1*/      12 , 17 , 21 , 28 , 71 , 78 , 82,  87 ,
    /*C2*/      23 , 26 , 32 , 37 , 62 , 67 , 73 , 76 ,
    /*D2*/      24 , 25 , 42 , 47 , 52 , 57 , 74 , 75 ,
    /*D3*/      34 , 35 , 43 , 46 , 53 , 56 , 64 , 65 ,
    /*C1*/      13 , 16 , 31 , 38 , 61 , 68 , 83 , 86 ,
    /*D1*/      14 , 15 , 41 , 48 , 51 , 58 , 84 , 85 ,
    /*C3*/      33 , 36 , 63 , 66 ,
    /*A1*/      11 , 18 , 81 , 88 , 
    /*D4*/      44 , 45 , 54 , 45
  };
  int i;
  int last_sq;

  region_parity = 0;

  last_sq = END_MOVE_LIST_HEAD;
  for ( i = 59; i >=0; i-- ) {
    int sq = worst2best[i];
    if ( board[sq] == EMPTY ) {
      end_move_list[last_sq].succ = sq;
      end_move_list[sq].pred = last_sq;
      region_parity ^= quadrant_mask[sq];
      last_sq = sq;
    }
  }
  end_move_list[last_sq].succ = END_MOVE_LIST_TAIL;
}



#if 0

/*
  CHECK_LIST
  Performs a minimal sanity check of the move list: That it contains
  the same number of moves as there are empty squares on the board.
*/

static void
check_list( int empties ) {
  int links = 0;
  int sq = end_move_list[END_MOVE_LIST_HEAD].succ;

  while ( sq != END_MOVE_LIST_TAIL ) {
    links++;
    sq = end_move_list[sq].succ;
  }

  if ( links != empties )
    printf( "%d links, %d empties\n", links, empties );
}

#endif




/*
  SOLVE_TWO_EMPTY
  SOLVE_THREE_EMPTY
  SOLVE_FOUR_EMPTY
  SOLVE_PARITY
  SOLVE_PARITY_HASH
  SOLVE_PARITY_HASH_HIGH
  These are the core routines of the low level endgame code.
  They all perform the same task: Return the score for the side to move.
  Structural differences:
  * SOLVE_TWO_EMPTY may only be called for *exactly* two empty
  * SOLVE_THREE_EMPTY may only be called for *exactly* three empty
  * SOLVE_FOUR_EMPTY may only be called for *exactly* four empty
  * SOLVE_PARITY uses stability, parity and fixed move ordering
  * SOLVE_PARITY_HASH uses stability, hash table and fixed move ordering
  * SOLVE_PARITY_HASH_HIGH uses stability, hash table and (non-thresholded)
    fastest first
*/

static int
solve_two_empty( BitBoard my_bits,
		 BitBoard opp_bits,
		 int sq1,
		 int sq2,
		 int alpha,
		 int beta,
		 int disc_diff,
		 int pass_legal ) {
  // BitBoard new_opp_bits;
  int score = -INFINITE_EVAL;
  int flipped;
  int ev;

  INCREMENT_COUNTER( nodes );

  /* Overall strategy: Lazy evaluation whenever possible, i.e., don't
     update bitboards until they are used. Also look at alpha and beta
     in order to perform strength reduction: Feasibility testing is
     faster than counting number of flips. */

  /* Try the first of the two empty squares... */

  flipped = TestFlips_wrapper( sq1, my_bits, opp_bits );
  if ( flipped != 0 ) {  /* SQ1 feasible for me */
    INCREMENT_COUNTER( nodes );

    ev = disc_diff + 2 * flipped;

#if 0
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    if ( ev - 2 <= alpha ) { /* Fail-low if he can play SQ2 */
      if ( ValidOneEmpty_bitboard[sq2]( new_opp_bits ) != 0 )
	ev = alpha;
      else {  /* He passes, check if SQ2 is feasible for me */
	if ( ev >= 0 ) {  /* I'm ahead, so EV will increase by at least 2 */
	  ev += 2;
	  if ( ev < beta )  /* Only bother if not certain fail-high */
	    ev += 2 * CountFlips_bitboard[sq2 - 11]( bb_flips.high, bb_flips.low );
	}
	else {
	  if ( ev < beta ) {  /* Only bother if not fail-high already */
	    flipped = CountFlips_bitboard[sq2 - 11]( bb_flips.high, bb_flips.low );
	    if ( flipped != 0 )  /* SQ2 feasible for me, game over */
	      ev += 2 * (flipped + 1);
	    /* ELSE: SQ2 will end up empty, game over */
	  }
	}
      }
    }
    else {
#endif
      flipped = CountFlips_bitboard[sq2 - 11]( opp_bits.high & ~bb_flips.high, opp_bits.low & ~bb_flips.low );
      if ( flipped != 0 )
	ev -= 2 * flipped;
      else {  /* He passes, check if SQ2 is feasible for me */
	if ( ev >= 0 ) {  /* I'm ahead, so EV will increase by at least 2 */
	  ev += 2;
	  if ( ev < beta )  /* Only bother if not certain fail-high */
	    ev += 2 * CountFlips_bitboard[sq2 - 11]( bb_flips.high, bb_flips.low );
	}
	else {
	  if ( ev < beta ) {  /* Only bother if not fail-high already */
	    flipped = CountFlips_bitboard[sq2 - 11]( bb_flips.high, bb_flips.low );
	    if ( flipped != 0 )  /* SQ2 feasible for me, game over */
	      ev += 2 * (flipped + 1);
	    /* ELSE: SQ2 will end up empty, game over */
	  }
	}
      }
#if 0
    }
#endif

    /* Being legal, the first move is the best so far */
    score = ev;
    if ( score > alpha ) {
      if ( score >= beta )
	return score;
      alpha = score;
    }
  }

  /* ...and then the second */

  flipped = TestFlips_wrapper( sq2, my_bits, opp_bits );
  if ( flipped != 0 ) {  /* SQ2 feasible for me */
    INCREMENT_COUNTER( nodes );

    ev = disc_diff + 2 * flipped;
#if 0
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    if ( ev - 2 <= alpha ) {  /* Fail-low if he can play SQ1 */
      if ( ValidOneEmpty_bitboard[sq1]( new_opp_bits ) != 0 )
	ev = alpha;
      else {  /* He passes, check if SQ1 is feasible for me */
	if ( ev >= 0 ) {  /* I'm ahead, so EV will increase by at least 2 */
	  ev += 2;
	  if ( ev < beta )  /* Only bother if not certain fail-high */
	    ev += 2 * CountFlips_bitboard[sq1 - 11]( bb_flips.high, bb_flips.low );
	}
	else {
	  if ( ev < beta ) {  /* Only bother if not fail-high already */
	    flipped = CountFlips_bitboard[sq1 - 11]( bb_flips.high, bb_flips.low );
	    if ( flipped != 0 )  /* SQ1 feasible for me, game over */
	      ev += 2 * (flipped + 1);
	    /* ELSE: SQ1 will end up empty, game over */
	  }
	}
      }
    }
    else {
#endif
      flipped = CountFlips_bitboard[sq1 - 11]( opp_bits.high & ~bb_flips.high, opp_bits.low & ~bb_flips.low );
      if ( flipped != 0 )  /* SQ1 feasible for him, game over */
	ev -= 2 * flipped;
      else {  /* He passes, check if SQ1 is feasible for me */
	if ( ev >= 0 ) {  /* I'm ahead, so EV will increase by at least 2 */
	  ev += 2;
	  if ( ev < beta )  /* Only bother if not certain fail-high */
	    ev += 2 * CountFlips_bitboard[sq1 - 11]( bb_flips.high, bb_flips.low );
	}
	else {
	  if ( ev < beta ) {  /* Only bother if not fail-high already */
	    flipped = CountFlips_bitboard[sq1 - 11]( bb_flips.high, bb_flips.low );
	    if ( flipped != 0 )  /* SQ1 feasible for me, game over */
	      ev += 2 * (flipped + 1);
	    /* ELSE: SQ1 will end up empty, game over */
	  }
	}
      }
#if 0
    }
#endif

    /* If the second move is better than the first (if that move was legal),
       its score is the score of the position */
    if ( ev >= score )
      return ev;
  }

  /* If both SQ1 and SQ2 are illegal I have to pass,
     otherwise return the best score. */

  if ( score == -INFINITE_EVAL ) {
    if ( !pass_legal ) {  /* Two empty squares */
      if ( disc_diff > 0 )
	return disc_diff + 2;
      if ( disc_diff < 0 )
	return disc_diff - 2;
      return 0;
    }
    else
      return -solve_two_empty( opp_bits, my_bits, sq1, sq2, -beta,
			       -alpha, -disc_diff, FALSE );
  }
  else
    return score;
}


static int
solve_three_empty( BitBoard my_bits,
		   BitBoard opp_bits,
		   int sq1,
		   int sq2,
		   int sq3,
		   int alpha,
		   int beta,
		   int disc_diff,
		   int pass_legal ) {
  BitBoard new_opp_bits;
  int score = -INFINITE_EVAL;
  int flipped;
  int new_disc_diff;
  int ev;

  INCREMENT_COUNTER( nodes );

  flipped = TestFlips_wrapper( sq1, my_bits, opp_bits );
  if ( flipped != 0 ) {
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    new_disc_diff = -disc_diff - 2 * flipped - 1;
    score = -solve_two_empty( new_opp_bits, bb_flips, sq2, sq3,
			      -beta, -alpha, new_disc_diff, TRUE );
    if ( score >= beta )
      return score;
    else if ( score > alpha )
      alpha = score;
  }

  flipped = TestFlips_wrapper( sq2, my_bits, opp_bits );
  if ( flipped != 0 ) {
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    new_disc_diff = -disc_diff - 2 * flipped - 1;
    ev = -solve_two_empty( new_opp_bits, bb_flips, sq1, sq3,
			   -beta, -alpha, new_disc_diff, TRUE );
    if ( ev >= beta )
      return ev;
    else if ( ev > score ) {
      score = ev;
      if ( score > alpha )
	alpha = score;
    }
  }

  flipped = TestFlips_wrapper( sq3, my_bits, opp_bits );
  if ( flipped != 0 ) {
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    new_disc_diff = -disc_diff - 2 * flipped - 1;
    ev = -solve_two_empty( new_opp_bits, bb_flips, sq1, sq2,
			   -beta, -alpha, new_disc_diff, TRUE );
    if ( ev >= score )
      return ev;
  }

  if ( score == -INFINITE_EVAL ) {
    if ( !pass_legal ) {  /* Three empty squares */
      if ( disc_diff > 0 )
	return disc_diff + 3;
      if ( disc_diff < 0 )
	return disc_diff - 3;
      return 0;  /* Can't reach this code, only keep it for symmetry */
    }
    else
      return -solve_three_empty( opp_bits, my_bits, sq1, sq2, sq3,
				 -beta, -alpha, -disc_diff, FALSE );
  }

  return score;
}



static int
solve_four_empty( BitBoard my_bits,
		  BitBoard opp_bits,
		  int sq1,
		  int sq2,
		  int sq3,
		  int sq4,
		  int alpha,
		  int beta,
		  int disc_diff,
		  int pass_legal ) {
  BitBoard new_opp_bits;
  int score = -INFINITE_EVAL;
  int flipped;
  int new_disc_diff;
  int ev;

  INCREMENT_COUNTER( nodes );

  flipped = TestFlips_wrapper( sq1, my_bits, opp_bits );
  if ( flipped != 0 ) {
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    new_disc_diff = -disc_diff - 2 * flipped - 1;
    score = -solve_three_empty( new_opp_bits, bb_flips, sq2, sq3, sq4,
				-beta, -alpha, new_disc_diff, TRUE );
    if ( score >= beta )
      return score;
    else if ( score > alpha )
      alpha = score;
  }

  flipped = TestFlips_wrapper( sq2, my_bits, opp_bits );
  if ( flipped != 0 ) {
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    new_disc_diff = -disc_diff - 2 * flipped - 1;
    ev = -solve_three_empty( new_opp_bits, bb_flips, sq1, sq3, sq4,
			     -beta, -alpha, new_disc_diff, TRUE );
    if ( ev >= beta )
      return ev;
    else if ( ev > score ) {
      score = ev;
      if ( score > alpha )
	alpha = score;
    }
  }

  flipped = TestFlips_wrapper( sq3, my_bits, opp_bits );
  if ( flipped != 0 ) {
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    new_disc_diff = -disc_diff - 2 * flipped - 1;
    ev = -solve_three_empty( new_opp_bits, bb_flips, sq1, sq2, sq4,
			     -beta, -alpha, new_disc_diff, TRUE );
    if ( ev >= beta )
      return ev;
    else if ( ev > score ) {
      score = ev;
      if ( score > alpha )
	alpha = score;
    }
  }

  flipped = TestFlips_wrapper( sq4, my_bits, opp_bits );
  if ( flipped != 0 ) {
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
    new_disc_diff = -disc_diff - 2 * flipped - 1;
    ev = -solve_three_empty( new_opp_bits, bb_flips, sq1, sq2, sq3,
			     -beta, -alpha, new_disc_diff, TRUE );
    if ( ev >= score )
      return ev;
  }

  if ( score == -INFINITE_EVAL ) {
    if ( !pass_legal ) {  /* Four empty squares */
      if ( disc_diff > 0 )
	return disc_diff + 4;
      if ( disc_diff < 0 )
	return disc_diff - 4;
      return 0;
    }
    else
      return -solve_four_empty( opp_bits, my_bits, sq1, sq2, sq3, sq4,
				-beta, -alpha, -disc_diff, FALSE );
  }

  return score;
}



static int
solve_parity( BitBoard my_bits,
	      BitBoard opp_bits,
	      int alpha,
	      int beta, 
	      int color,
	      int empties,
	      int disc_diff,
	      int pass_legal ) {
  BitBoard new_opp_bits;
  int score = -INFINITE_EVAL;
  int oppcol = OPP( color );
  int ev;
  int flipped;
  int new_disc_diff;
  int sq, old_sq, best_sq = 0;
  unsigned int parity_mask;

  INCREMENT_COUNTER( nodes );

  /* Check for stability cutoff */

#if USE_STABILITY
  if ( alpha >= stability_threshold[empties] ) {
    int stability_bound;
    stability_bound = 64 - 2 * count_edge_stable( oppcol, opp_bits, my_bits );
    if ( stability_bound <= alpha )
      return alpha;
    stability_bound = 64 - 2 * count_stable( oppcol, opp_bits, my_bits );
    if ( stability_bound < beta )
      beta = stability_bound + 1;
    if ( stability_bound <= alpha )
      return alpha;
  }
#endif

  /* Odd parity */

  parity_mask = region_parity;

  if ( region_parity != 0 )  /* Is there any region with odd parity? */
    for ( old_sq = END_MOVE_LIST_HEAD, sq = end_move_list[old_sq].succ;
	  sq != END_MOVE_LIST_TAIL;
	  old_sq = sq, sq = end_move_list[sq].succ ) {
      unsigned int holepar = quadrant_mask[sq];
      if ( holepar & parity_mask ) {
	flipped = TestFlips_wrapper( sq, my_bits, opp_bits );
	if ( flipped != 0 ) {
	  FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );

	  end_move_list[old_sq].succ = end_move_list[sq].succ;
	  new_disc_diff = -disc_diff - 2 * flipped - 1;
	  if ( empties == 5 ) {
	    int sq1 = end_move_list[END_MOVE_LIST_HEAD].succ;
	    int sq2 = end_move_list[sq1].succ;
	    int sq3 = end_move_list[sq2].succ;
	    int sq4 = end_move_list[sq3].succ;
	    ev = -solve_four_empty( new_opp_bits, bb_flips, sq1, sq2, sq3, sq4,
				     -beta, -alpha, new_disc_diff, TRUE );
	  }
	  else {
	    region_parity ^= holepar;
	    ev = -solve_parity( new_opp_bits, bb_flips, -beta, -alpha,
				oppcol, empties - 1, new_disc_diff, TRUE );
	    region_parity ^= holepar;
	  }
	  end_move_list[old_sq].succ = sq;

	  if ( ev > score ) {
	    if ( ev > alpha ) {
	      if ( ev >= beta ) {
		best_move = sq;
		return ev;
	      }
	      alpha = ev;
	    }
	    score = ev;
	    best_sq = sq;
	  }
	}
      }
    }

  /* Even parity */

  parity_mask = ~parity_mask;
  for ( old_sq = END_MOVE_LIST_HEAD, sq = end_move_list[old_sq].succ;
	sq != END_MOVE_LIST_TAIL;
	old_sq = sq, sq = end_move_list[sq].succ ) {
    unsigned int holepar = quadrant_mask[sq];
    if ( holepar & parity_mask ) {
      flipped = TestFlips_wrapper( sq, my_bits, opp_bits );
      if ( flipped != 0 ) {
	FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );

	end_move_list[old_sq].succ = end_move_list[sq].succ;
	new_disc_diff = -disc_diff - 2 * flipped - 1;
	if ( empties == 5 ) {
	  int sq1 = end_move_list[END_MOVE_LIST_HEAD].succ;
	  int sq2 = end_move_list[sq1].succ;
	  int sq3 = end_move_list[sq2].succ;
	  int sq4 = end_move_list[sq3].succ;
	  ev = -solve_four_empty( new_opp_bits, bb_flips, sq1, sq2, sq3, sq4,
				  -beta, -alpha, new_disc_diff, TRUE );
	}
	else {
	  region_parity ^= holepar;
	  ev = -solve_parity( new_opp_bits, bb_flips, -beta, -alpha,
			      oppcol, empties - 1, new_disc_diff, TRUE );
	  region_parity ^= holepar;
	}
	end_move_list[old_sq].succ = sq;

	if ( ev > score ) {
	  if ( ev > alpha ) {
	    if ( ev >= beta ) {
	      best_move = sq;
	      return ev;
	    }
	    alpha = ev;
	  }
	  score = ev;
	  best_sq = sq;
	}
      }
    }
  }

  if ( score == -INFINITE_EVAL ) {
    if ( !pass_legal ) {
      if ( disc_diff > 0 )
	return disc_diff + empties;
      if ( disc_diff < 0 )
	return disc_diff - empties;
      return 0;
    }
    else
      return -solve_parity( opp_bits, my_bits, -beta, -alpha, oppcol,
			    empties, -disc_diff, FALSE );
  }
  best_move = best_sq;

  return score;
}



static int
solve_parity_hash( BitBoard my_bits,
		   BitBoard opp_bits,
		   int alpha,
		   int beta,
		   int color,
		   int empties,
		   int disc_diff,
		   int pass_legal ) {
  BitBoard new_opp_bits;
  int score = -INFINITE_EVAL;
  int oppcol = OPP( color );
  int in_alpha = alpha;
  int ev;
  int flipped;
  int new_disc_diff;
  int sq, old_sq, best_sq = 0;
  unsigned int parity_mask;
  HashEntry entry;

  INCREMENT_COUNTER( nodes );

  find_hash( &entry, ENDGAME_MODE );
  if ( (entry.draft == empties) &&
       (entry.selectivity == 0) &&
       valid_move( entry.move[0], color ) &&
       (entry.flags & ENDGAME_SCORE) &&
       ((entry.flags & EXACT_VALUE) ||
	((entry.flags & LOWER_BOUND) && entry.eval >= beta) ||
	((entry.flags & UPPER_BOUND) && entry.eval <= alpha)) ) {
    best_move = entry.move[0];
    return entry.eval;
  }

  /* Check for stability cutoff */

#if USE_STABILITY
  if ( alpha >= stability_threshold[empties] ) {
    int stability_bound;

    stability_bound = 64 - 2 * count_edge_stable( oppcol, opp_bits, my_bits );
    if ( stability_bound <= alpha )
      return alpha;
    stability_bound = 64 - 2 * count_stable( oppcol, opp_bits, my_bits );
    if ( stability_bound < beta )
       beta = stability_bound + 1;
    if ( stability_bound <= alpha )
      return alpha;
  }
#endif

  /* Odd parity. */

  parity_mask = region_parity;

  if ( region_parity != 0 )  /* Is there any region with odd parity? */
    for ( old_sq = END_MOVE_LIST_HEAD, sq = end_move_list[old_sq].succ;
	  sq != END_MOVE_LIST_TAIL;
	  old_sq = sq, sq = end_move_list[sq].succ ) {
      unsigned int holepar = quadrant_mask[sq];
      if ( holepar & parity_mask ) {
	flipped = TestFlips_wrapper( sq, my_bits, opp_bits );
	if ( flipped != 0 ) {
	  FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );

	  region_parity ^= holepar;
	  end_move_list[old_sq].succ = end_move_list[sq].succ;
	  new_disc_diff = -disc_diff - 2 * flipped - 1;
	  ev = -solve_parity( new_opp_bits, bb_flips, -beta, -alpha, oppcol,
			      empties - 1, new_disc_diff, TRUE );
	  region_parity ^= holepar;
	  end_move_list[old_sq].succ = sq;
	      
	  if ( ev > score ) {
	    score = ev;
	    if ( ev > alpha ) {
	      if ( ev >= beta ) { 
		best_move = sq;
		add_hash( ENDGAME_MODE, score, best_move,
			  ENDGAME_SCORE | LOWER_BOUND, empties, 0 );
		return score;
	      }
	      alpha = ev;
	    }
	    best_sq = sq;
	  }
	}
      }
    }

  /* Even parity. */

  parity_mask = ~parity_mask;

  for ( old_sq = END_MOVE_LIST_HEAD, sq = end_move_list[old_sq].succ;
	sq != END_MOVE_LIST_TAIL;
	old_sq = sq, sq = end_move_list[sq].succ ) {
    unsigned int holepar = quadrant_mask[sq];
    if ( holepar & parity_mask ) {
      flipped = TestFlips_wrapper( sq, my_bits, opp_bits );
      if ( flipped != 0 ) {
	FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );

	region_parity ^= holepar;
	end_move_list[old_sq].succ = end_move_list[sq].succ;
	new_disc_diff = -disc_diff - 2 * flipped - 1;
	ev = -solve_parity( new_opp_bits, bb_flips, -beta, -alpha, oppcol,
			    empties - 1, new_disc_diff, TRUE );
	region_parity ^= holepar;
	end_move_list[old_sq].succ = sq;
	      
	if ( ev > score ) {
	  score = ev;
	  if ( ev > alpha ) {
	    if ( ev >= beta ) { 
	      best_move = sq;
	      add_hash( ENDGAME_MODE, score, best_move,
			ENDGAME_SCORE | LOWER_BOUND, empties, 0 );
	      return score;
	    }
	    alpha = ev;
	  }
	  best_sq = sq;
	}
      }
    }
  }

  if ( score == -INFINITE_EVAL ) {
    if ( !pass_legal ) {
      if ( disc_diff > 0 )
	return disc_diff + empties;
      if ( disc_diff < 0 )
	return disc_diff - empties;
      return 0;
    }
    else {
      hash1 ^= hash_flip_color1;
      hash2 ^= hash_flip_color2;
      score = -solve_parity_hash( opp_bits, my_bits, -beta, -alpha, oppcol,
				  empties, -disc_diff, FALSE );
      hash1 ^= hash_flip_color1;
      hash2 ^= hash_flip_color2;
    }
  }
  else {
    best_move = best_sq;
    if ( score > in_alpha)
      add_hash( ENDGAME_MODE, score, best_move, ENDGAME_SCORE | EXACT_VALUE,
		empties, 0 );
    else
      add_hash( ENDGAME_MODE, score, best_move, ENDGAME_SCORE | UPPER_BOUND,
		empties, 0 );
  }

  return score;
}



static int
solve_parity_hash_high( BitBoard my_bits,
			BitBoard opp_bits,
			int alpha,
			int beta,
			int color,
			int empties,
			int disc_diff,
			int pass_legal ) {
  /* Move bonuses without and with parity for the squares.
     These are only used when sorting moves in the 9-12 empties
     range and were automatically tuned by OPTIMIZE. */
  static const unsigned char move_bonus[2][128] = {  /* 2 * 100 used */
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0,  24,   1,   0,  25,  25,   0,   1,  24,   0,
	0,   1,   0,   0,   0,   0,   0,   0,   1,   0,
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0,  25,   0,   0,   0,   0,   0,   0,  25,   0,
	0,  25,   0,   0,   0,   0,   0,   0,  25,   0,
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0,   1,   0,   0,   0,   0,   0,   0,   1,   0,
	0,  24,   1,   0,  25,  25,   0,   1,  24,   0,
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
    {   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0, 128,  86, 122, 125, 125, 122,  86, 128,   0,
	0,  86, 117, 128, 128, 128, 128, 117,  86,   0,
	0, 122, 128, 128, 128, 128, 128, 128, 122,   0,
	0, 125, 128, 128, 128, 128, 128, 128, 125,   0,
	0, 125, 128, 128, 128, 128, 128, 128, 125,   0,
	0, 122, 128, 128, 128, 128, 128, 128, 122,   0,
	0,  86, 117, 128, 128, 128, 128, 117,  86,   0,
	0, 128,  86, 122, 125, 125, 122,  86, 128,   0,
	0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
  };
  BitBoard new_opp_bits;
  BitBoard best_new_my_bits, best_new_opp_bits;
  int i;
  int score;
  int in_alpha = alpha;
  int oppcol = OPP( color );
  int flipped, best_flipped;
  int new_disc_diff;
  int ev;
  int hash_move;
  int moves;
  int parity;
  int best_value, best_index;
  int pred, succ;
  int sq, old_sq, best_sq = 0;
  int move_order[64];
  int goodness[64];
  unsigned int diff1, diff2;
  HashEntry entry;

  INCREMENT_COUNTER( nodes );

  hash_move = -1;
  find_hash( &entry, ENDGAME_MODE );
  if ( entry.draft == empties ) {
    if ( (entry.selectivity == 0) &&
	 (entry.flags & ENDGAME_SCORE) &&
	 valid_move( entry.move[0], color ) &&
	 ((entry.flags & EXACT_VALUE) ||
	  ((entry.flags & LOWER_BOUND) && entry.eval >= beta) ||
	  ((entry.flags & UPPER_BOUND) && entry.eval <= alpha)) ) {
      best_move = entry.move[0];
      return entry.eval;
    }
  }

  /* Check for stability cutoff */

#if USE_STABILITY
  if ( alpha >= stability_threshold[empties] ) {
    int stability_bound;

    stability_bound = 64 - 2 * count_edge_stable( oppcol, opp_bits, my_bits );
    if ( stability_bound <= alpha )
      return alpha;
    stability_bound = 64 - 2 * count_stable( oppcol, opp_bits, my_bits );
    if ( stability_bound < beta )
      beta = stability_bound + 1;
    if ( stability_bound <= alpha )
      return alpha;
  }
#endif

  /* Calculate goodness values for all moves */

  moves = 0;
  best_value = -INFINITE_EVAL;
  best_index = 0;
  best_flipped = 0;

  for ( old_sq = END_MOVE_LIST_HEAD, sq = end_move_list[old_sq].succ;
	sq != END_MOVE_LIST_TAIL;
	old_sq = sq, sq = end_move_list[sq].succ ) {
    flipped = TestFlips_wrapper( sq, my_bits, opp_bits );
    if ( flipped != 0 ) {
      INCREMENT_COUNTER( nodes );

      FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );
      end_move_list[old_sq].succ = end_move_list[sq].succ;

      if ( quadrant_mask[sq] & region_parity )
	parity = 1;
      else
	parity = 0;
      goodness[moves] = move_bonus[parity][sq];
      if ( sq == hash_move )
	goodness[moves] += 128;

      goodness[moves] -= weighted_mobility( new_opp_bits, bb_flips );

      if ( goodness[moves] > best_value ) {
	best_value = goodness[moves];
	best_index = moves;
	best_new_my_bits = bb_flips;
	best_new_opp_bits = new_opp_bits;
	best_flipped = flipped;
      }

      end_move_list[old_sq].succ = sq;
      move_order[moves] = sq;
      moves++;
    }
  }

  /* Maybe there aren't any legal moves */

  if ( moves == 0 ) {  /* I have to pass */
    if ( !pass_legal ) {  /* Last move also pass, game over */
      if ( disc_diff > 0 )
	return disc_diff + empties;
      if ( disc_diff < 0 )
	return disc_diff - empties;
      return 0;
    }
    else {  /* Opponent gets the chance to play */
      hash1 ^= hash_flip_color1;
      hash2 ^= hash_flip_color2;
      score = -solve_parity_hash_high( opp_bits, my_bits, -beta, -alpha,
				       oppcol, empties, -disc_diff, FALSE );
      hash1 ^= hash_flip_color1;
      hash2 ^= hash_flip_color2;
      return score;
    }
  }

  /* Try move with highest goodness value */

  sq = move_order[best_index];

  (void) DoFlips_hash( sq, color );

  board[sq] = color;
  diff1 = hash_update1 ^ hash_put_value1[color][sq];
  diff2 = hash_update2 ^ hash_put_value2[color][sq];
  hash1 ^= diff1;
  hash2 ^= diff2;

  region_parity ^= quadrant_mask[sq];

  pred = end_move_list[sq].pred;
  succ = end_move_list[sq].succ;
  end_move_list[pred].succ = succ;
  end_move_list[succ].pred = pred;

  new_disc_diff = -disc_diff - 2 * best_flipped - 1;
  if ( empties <= LOW_LEVEL_DEPTH + 1 )
    score = -solve_parity_hash( best_new_opp_bits, best_new_my_bits,
				-beta, -alpha, oppcol, empties - 1,
				new_disc_diff, TRUE );
  else
    score = -solve_parity_hash_high( best_new_opp_bits, best_new_my_bits,
				     -beta, -alpha, oppcol, empties - 1,
				     new_disc_diff, TRUE );

  UndoFlips( best_flipped, oppcol );
  hash1 ^= diff1;
  hash2 ^= diff2;
  board[sq] = EMPTY;

  region_parity ^= quadrant_mask[sq];

  end_move_list[pred].succ = sq;
  end_move_list[succ].pred = sq;

  best_sq = sq;
  if ( score > alpha ) {
    if ( score >= beta ) { 
      best_move = best_sq;
      add_hash( ENDGAME_MODE, score, best_move,
		ENDGAME_SCORE | LOWER_BOUND, empties, 0 );
      return score;
    }
    alpha = score;
  }

  /* Play through the rest of the moves */

  move_order[best_index] = move_order[0];
  goodness[best_index] = goodness[0];

  for ( i = 1; i < moves; i++ ) {
    int j;

    best_value = goodness[i];
    best_index = i;
    for ( j = i + 1; j < moves; j++ )
      if ( goodness[j] > best_value ) {
	best_value = goodness[j];
	best_index = j;
      }
    sq = move_order[best_index];
    move_order[best_index] = move_order[i];
    goodness[best_index] = goodness[i];

    flipped = TestFlips_wrapper( sq, my_bits, opp_bits );
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );

    (void) DoFlips_hash( sq, color );
    board[sq] = color;
    diff1 = hash_update1 ^ hash_put_value1[color][sq];
    diff2 = hash_update2 ^ hash_put_value2[color][sq];
    hash1 ^= diff1;
    hash2 ^= diff2;

    region_parity ^= quadrant_mask[sq];

    pred = end_move_list[sq].pred;
    succ = end_move_list[sq].succ;
    end_move_list[pred].succ = succ;
    end_move_list[succ].pred = pred;

    new_disc_diff = -disc_diff - 2 * flipped - 1;

    if ( empties <= LOW_LEVEL_DEPTH )  /* Fail-high for opp is likely. */
      ev = -solve_parity_hash( new_opp_bits, bb_flips, -beta, -alpha,
			       oppcol, empties - 1, new_disc_diff, TRUE );
    else
      ev = -solve_parity_hash_high( new_opp_bits, bb_flips, -beta, -alpha,
				    oppcol, empties - 1, new_disc_diff, TRUE );

    region_parity ^= quadrant_mask[sq];

    UndoFlips( flipped, oppcol );
    hash1 ^= diff1;
    hash2 ^= diff2;
    board[sq] = EMPTY;

    end_move_list[pred].succ = sq;
    end_move_list[succ].pred = sq;

    if ( ev > score ) {
      score = ev;
      if ( ev > alpha ) {
	if ( ev >= beta ) { 
	  best_move = sq;
	  add_hash( ENDGAME_MODE, score, best_move,
		    ENDGAME_SCORE | LOWER_BOUND, empties, 0 );
	  return score;
	}
	alpha = ev;
      }
      best_sq = sq;
    }
  }

  best_move = best_sq;
  if ( score > in_alpha )
    add_hash( ENDGAME_MODE, score, best_move,
	      ENDGAME_SCORE | EXACT_VALUE, empties, 0 );
  else
    add_hash( ENDGAME_MODE, score, best_move,
	      ENDGAME_SCORE | UPPER_BOUND, empties, 0 );

  return score;
}



/*
  END_SOLVE
  The search itself. Assumes relevant data structures have been set up with
  PREPARE_TO_SOLVE(). Returns difference between disc count for
  COLOR and disc count for the opponent of COLOR.
*/

static int
end_solve( BitBoard my_bits,
	   BitBoard opp_bits,
	   int alpha,
	   int beta, 
	   int color,
	   int empties,
	   int discdiff,
	   int prevmove ) {
  int result;

  if ( empties <= LOW_LEVEL_DEPTH )
    result = solve_parity( my_bits, opp_bits, alpha, beta, color, empties,
			   discdiff, prevmove );
  else
    result = solve_parity_hash_high( my_bits, opp_bits, alpha, beta, color,
				     empties, discdiff, prevmove );

  return result;
}



/*
  UPDATE_BEST_LIST
*/

static void
update_best_list( int *best_list, int move, int best_list_index,
		  int *best_list_length, int verbose ) {
  int i;

  verbose = FALSE;

  if ( verbose ) {
    printf( "move=%2d  index=%d  length=%d      ", move, best_list_index,
	    *best_list_length );
    printf( "Before:  " );
    for ( i = 0; i < 4; i++ )
      printf( "%2d ", best_list[i] );
  }

  if ( best_list_index < *best_list_length )
    for ( i = best_list_index; i >= 1; i-- )
      best_list[i] = best_list[i - 1];
  else {
    for ( i = 3; i >= 1; i-- )
      best_list[i] = best_list[i - 1];
    if ( *best_list_length < 4 )
      (*best_list_length)++;
  }
  best_list[0] = move;

  if ( verbose ) {
    printf( "      After:  " );
    for ( i = 0; i < 4; i++ )
      printf( "%2d ", best_list[i] );
    puts( "" );
  }
}



/*
  END_TREE_SEARCH
  Plain nega-scout with fastest-first move ordering.
*/

static int
end_tree_search( int level,
		 int max_depth,
		 BitBoard my_bits,
		 BitBoard opp_bits,
		 int side_to_move,
		 int alpha,
		 int beta,
		 int selectivity,
		 int *selective_cutoff,
		 int void_legal ) {
  static char buffer[16];
  double node_val;
  int i, j;
  int empties;
  int disk_diff;
  int previous_move;
  int result;
  int curr_val, best;
  int move;
  int hash_hit;
  int move_index;
  int remains, exp_depth, pre_depth;
  int update_pv, first, use_hash;
  int my_discs, opp_discs;
  int curr_alpha;
  int pre_search_done;
  int mobility;
  int threshold;
  int best_list_index, best_list_length;
  int best_list[4];
  HashEntry entry, mid_entry;
#if CHECK_HASH_CODES
  unsigned int h1, h2;
#endif
#if USE_STABILITY
  int stability_bound;
#endif

  if ( level == 0 ) {
    sprintf( buffer, "[%d,%d]:", alpha, beta );
    clear_sweep();
  }
  remains = max_depth - level;
  *selective_cutoff = FALSE;

  /* Always (almost) check for stability cutoff in this region of search */

#if USE_STABILITY
  if ( alpha >= HIGH_STABILITY_THRESHOLD ) {
    stability_bound = 64 -
      2 * count_edge_stable( OPP( side_to_move ), opp_bits, my_bits );
    if ( stability_bound <= alpha ) {
      pv_depth[level] = level;
      return alpha;
    }
    stability_bound = 64 -
      2 * count_stable( OPP( side_to_move ), opp_bits, my_bits );
    if ( stability_bound < beta )
      beta = stability_bound + 1;
    if ( stability_bound <= alpha ) {
      pv_depth[level] = level;
      return alpha;
    }
  }
#endif

  /* Check if the low-level code is to be invoked */

  my_discs = piece_count[side_to_move][disks_played];
  opp_discs = piece_count[OPP( side_to_move )][disks_played];
  empties = 64 - my_discs - opp_discs;
  if ( remains <= FASTEST_FIRST_DEPTH ) {
    disk_diff = my_discs - opp_discs;
    if ( void_legal )  /* Is PASS legal or was last move a pass? */
      previous_move = 44;  /* d4, of course impossible */
    else
      previous_move = 0;

    prepare_to_solve( board );
    result = end_solve( my_bits, opp_bits, alpha, beta, side_to_move,
			empties, disk_diff, previous_move );

    pv_depth[level] = level + 1;
    pv[level][level] = best_move;
      
    if ( (level == 0) && !get_ponder_move() ) {
      send_sweep( "%-10s ", buffer );
      send_sweep( "%c%c", TO_SQUARE( best_move ) );
      if ( result <= alpha )
	send_sweep( "<%d", result + 1 );
      else if ( result >= beta )
	send_sweep( ">%d", result - 1 );
      else
	send_sweep( "=%d", result );
    }
    return result;
  }

  /* Otherwise normal search */

  INCREMENT_COUNTER( nodes );

  use_hash = USE_HASH_TABLE;
  if ( use_hash ) {
#if CHECK_HASH_CODES
    h1 = hash1;
    h2 = hash2;
#endif

    /* Check for endgame hash table move */

    find_hash( &entry, ENDGAME_MODE );
    if ( (entry.draft == remains) &&
	 (entry.selectivity <= selectivity) &&
	 valid_move( entry.move[0], side_to_move ) &&
	 (entry.flags & ENDGAME_SCORE) &&
	 ((entry.flags & EXACT_VALUE) ||
	  ((entry.flags & LOWER_BOUND) && entry.eval >= beta) ||
	  ((entry.flags & UPPER_BOUND) && entry.eval <= alpha)) ) {
      pv[level][level] = entry.move[0];
      pv_depth[level] = level + 1;
      if ( (level == 0) && !get_ponder_move() ) {  /* Output some stats */
	send_sweep( "%c%c", TO_SQUARE( entry.move[0] ) );
	if ( (entry.flags & ENDGAME_SCORE) &&
	     (entry.flags & EXACT_VALUE) )
	  send_sweep( "=%d", entry.eval );
	else if ( (entry.flags & ENDGAME_SCORE) &&
		  (entry.flags & LOWER_BOUND) )
	  send_sweep( ">%d", entry.eval - 1 );
	else
	  send_sweep( "<%d", entry.eval + 1 );
#ifdef TEXT_BASED
	fflush( stdout );
#endif
      }
      if ( entry.selectivity > 0 )
	*selective_cutoff = TRUE;
      return entry.eval;
    }

    hash_hit = (entry.draft != NO_HASH_MOVE);

    /* If not any such found, check for a midgame hash move */

    find_hash( &mid_entry, MIDGAME_MODE );
    if ( (mid_entry.draft != NO_HASH_MOVE) &&
	 (mid_entry.flags & MIDGAME_SCORE) ) {
      if ( (level <= 4) || (mid_entry.flags & (EXACT_VALUE | LOWER_BOUND)) ) {
	/* Give the midgame move full priority if we're are the root
	   of the tree, no endgame hash move was found and the position
	   isn't in the wipeout zone. */

	if ( (level == 0) && !hash_hit &&
	     (mid_entry.eval < WIPEOUT_THRESHOLD * 128) ) {
	  entry = mid_entry;
	  hash_hit = TRUE;
	}
      }
    }
  }

  /* Use endgame multi-prob-cut to selectively prune the tree */

  if ( USE_MPC && (level > 2) && (selectivity > 0) ) {
    int cut;
    for ( cut = 0; cut < use_end_cut[disks_played]; cut++ ) {
      int shallow_remains = end_mpc_depth[disks_played][cut];
      int mpc_bias = ceil( end_mean[disks_played][shallow_remains] * 128.0 );
      int mpc_window = ceil( end_sigma[disks_played][shallow_remains] *
			     end_percentile[selectivity] * 128.0 );
      int beta_bound = 128 * beta + mpc_bias + mpc_window;
      int alpha_bound = 128 * alpha + mpc_bias - mpc_window;
      int shallow_val =
	tree_search( level, level + shallow_remains, side_to_move,
		     alpha_bound, beta_bound, use_hash, FALSE, void_legal );
      if ( shallow_val >= beta_bound ) {
	if ( use_hash )
	  add_hash( ENDGAME_MODE, alpha, pv[level][level],
		    ENDGAME_SCORE | LOWER_BOUND, remains, selectivity );
	*selective_cutoff = TRUE;
	return beta;
      }
      if ( shallow_val <= alpha_bound ) {
	if ( use_hash )
	  add_hash( ENDGAME_MODE, beta, pv[level][level],
		    ENDGAME_SCORE | UPPER_BOUND, remains, selectivity );
	*selective_cutoff = TRUE;
	return alpha;
      }
    }
  }

  /* Determine the depth of the shallow search used to find
     achieve good move sorting */

  if ( remains >= DEPTH_TWO_SEARCH ) {
    if ( remains >= DEPTH_THREE_SEARCH )
      if ( remains >= DEPTH_FOUR_SEARCH ) {
	if ( remains >= DEPTH_SIX_SEARCH )
	  pre_depth = 6;
	else
	  pre_depth = 4;
      }
      else
	pre_depth = 3;
    else
      pre_depth = 2;
  }
  else
    pre_depth = 1;
  if ( level == 0 ) {  /* Deeper pre-search from the root */
    pre_depth += EXTRA_ROOT_SEARCH;
    if ( (pre_depth % 2) == 1)  /* Avoid odd depths from the root */
      pre_depth++;
  }
         
  /* The nega-scout search */

  exp_depth = remains;
  first = TRUE;
  best = -INFINITE_EVAL;
  pre_search_done = FALSE;
  curr_alpha = alpha;

  /* Initialize the move list and check the hash table move list */

  move_count[disks_played] = 0;
  best_list_length = 0;
  for ( i = 0; i < 4; i++ )
    best_list[i] = 0;
  if ( hash_hit )
    for ( i = 0; i < 4; i++ )
      if ( valid_move( entry.move[i], side_to_move ) ) {
	best_list[best_list_length++] = entry.move[i];

	/* Check for ETC among the hash table moves */

	if ( use_hash &&
	     (make_move( side_to_move, entry.move[i], TRUE ) != 0) ) {
	  HashEntry etc_entry;

          find_hash( &etc_entry, ENDGAME_MODE );
	  if ( (etc_entry.flags & ENDGAME_SCORE) &&
	       (etc_entry.draft == empties - 1) &&
	       (etc_entry.selectivity <= selectivity) &&
	       (etc_entry.flags & (UPPER_BOUND | EXACT_VALUE)) &&
	       (etc_entry.eval <= -beta) ) {

	    /* Immediate cutoff from this move, move it up front */

	    for ( j = best_list_length - 1; j >= 1; j-- )
	      best_list[j] = best_list[j - 1];
	    best_list[0] = entry.move[i];
	  }
	  unmake_move( side_to_move, entry.move[i] );
	}
      }

  for ( move_index = 0, best_list_index = 0; TRUE;
	move_index++, best_list_index++ ) {
    int child_selective_cutoff;
    BitBoard new_my_bits;
    BitBoard new_opp_bits;

    /* Use results of shallow searches to determine the move order */

    if ( (best_list_index < best_list_length) ) {
      move = best_list[best_list_index];
      move_count[disks_played]++;
    }
    else {
      if ( !pre_search_done ) {
	int shallow_index;

	pre_search_done = TRUE;

	threshold =
	  MIN( WIPEOUT_THRESHOLD * 128,
	       128 * alpha + fast_first_threshold[disks_played][pre_depth] );

	for ( shallow_index = 0; shallow_index < MOVE_ORDER_SIZE;
	      shallow_index++ ) {
	  int already_checked;

	  move = sorted_move_order[disks_played][shallow_index];
	  already_checked = FALSE;
	  for ( j = 0; j < best_list_length; j++ )
	    if ( move == best_list[j] )
	      already_checked = TRUE;

	  if ( !already_checked && (board[move] == EMPTY) &&
	       (TestFlips_wrapper( move, my_bits, opp_bits ) > 0) ) {
	    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );

	    (void) make_move( side_to_move, move, TRUE );
	    curr_val = 0;

	    /* Enhanced Transposition Cutoff: It's a good idea to
	       transpose back into a position in the hash table. */

	    if ( use_hash ) {
	      HashEntry etc_entry;

              find_hash( &etc_entry, ENDGAME_MODE );
	      if ( (etc_entry.flags & ENDGAME_SCORE) &&
		   (etc_entry.draft == empties - 1) ) {
		curr_val += 384;
		if ( etc_entry.selectivity <= selectivity ) {
		  if ( (etc_entry.flags & (UPPER_BOUND | EXACT_VALUE)) &&
		       (etc_entry.eval <= -beta) )
		    curr_val = GOOD_TRANSPOSITION_EVAL;
		  if ( (etc_entry.flags & LOWER_BOUND) &&
		       (etc_entry.eval >= -alpha) )
		    curr_val -= 640;
		}
	      }
	    }

	    /* Determine the midgame score. If it is worse than
	       alpha-8, a fail-high is likely so precision in that
	       range is not worth the extra nodes required. */

	    if ( curr_val != GOOD_TRANSPOSITION_EVAL )
	      curr_val -=
		tree_search( level + 1, level + pre_depth,
			     OPP( side_to_move ), -INFINITE_EVAL,
			     (-alpha + 8) * 128, TRUE, TRUE, TRUE );

	    /* Make the moves which are highly likely to result in
	       fail-high in decreasing order of mobility for the
	       opponent. */

	    if ( (curr_val > threshold) || (move == mid_entry.move[0]) ) {
	      if ( curr_val > WIPEOUT_THRESHOLD * 128 )
		curr_val += 2 * VERY_HIGH_EVAL;
	      else
		curr_val += VERY_HIGH_EVAL;
	      if ( curr_val < GOOD_TRANSPOSITION_EVAL ) {
		mobility = bitboard_mobility( new_opp_bits, bb_flips );
		if ( curr_val > 2 * VERY_HIGH_EVAL )
		  curr_val -= 2 * ff_mob_factor[disks_played - 1] * mobility;
		else
		  curr_val -= ff_mob_factor[disks_played - 1] * mobility;
	      }
	    }

	    unmake_move( side_to_move, move );
	    evals[disks_played][move] = curr_val;
	    move_list[disks_played][move_count[disks_played]] = move;
	    move_count[disks_played]++;
	  }
	}
      }

      if ( move_index == move_count[disks_played] )
	break;
      move = select_move( move_index, move_count[disks_played] );
    }

    node_val = counter_value( &nodes );
    if ( node_val - last_panic_check >= EVENT_CHECK_INTERVAL) {
      /* Check for time abort */
      last_panic_check = node_val;
      check_panic_abort();

      /* Output status buffers if in interactive mode */
      if ( echo )
	display_buffers();

      /* Check for events */
      handle_event( TRUE, FALSE, TRUE );
      if ( is_panic_abort() || force_return )
	return SEARCH_ABORT;
    }

    if ( (level == 0) && !get_ponder_move() ) {
      if ( first )
	send_sweep( "%-10s ", buffer );
      send_sweep( "%c%c", TO_SQUARE( move ) );
    }

    (void) make_move( side_to_move, move, use_hash );
    (void) TestFlips_wrapper( move, my_bits, opp_bits );
    new_my_bits = bb_flips;
    FULL_ANDNOT( new_opp_bits, opp_bits, bb_flips );

    update_pv = FALSE;
    if ( first ) {
      best = curr_val =
	-end_tree_search( level + 1, level + exp_depth,
			  new_opp_bits, new_my_bits, OPP( side_to_move ),
			  -beta, -curr_alpha, selectivity,
			  &child_selective_cutoff, TRUE );
      update_pv = TRUE;
      if ( level == 0 )
	best_end_root_move = move;
    }
    else {
      curr_alpha = MAX( best, curr_alpha );
      curr_val =
	-end_tree_search( level + 1, level + exp_depth,
			  new_opp_bits, new_my_bits, OPP( side_to_move ),
			  -(curr_alpha + 1), -curr_alpha,
			  selectivity, &child_selective_cutoff, TRUE );
      if ( (curr_val > curr_alpha) && (curr_val < beta) ) {
	if ( selectivity > 0 )
	  curr_val =
	    -end_tree_search( level + 1, level + exp_depth,
			      new_opp_bits, new_my_bits, OPP( side_to_move ),
			      -beta, INFINITE_EVAL,
			      selectivity, &child_selective_cutoff,  TRUE );
	else
	  curr_val =
	    -end_tree_search( level + 1, level + exp_depth,
			      new_opp_bits, new_my_bits, OPP( side_to_move ),
			      -beta, -curr_val,
			      selectivity, &child_selective_cutoff, TRUE );
	if ( curr_val > best ) {
	  best = curr_val;
	  update_pv = TRUE;
	  if ( (level == 0) && !is_panic_abort() && !force_return )
	    best_end_root_move = move;
	}
      }
      else if ( curr_val > best ) {
	best = curr_val;
	update_pv = TRUE;
	if ( (level == 0) && !is_panic_abort() && !force_return )
	  best_end_root_move = move;
      }
    }

    if ( best >= beta )  /* The other children don't matter in this case. */
      *selective_cutoff = child_selective_cutoff;
    else if ( child_selective_cutoff )
      *selective_cutoff = TRUE;

    unmake_move( side_to_move, move );

    if ( is_panic_abort() || force_return )
      return SEARCH_ABORT;

    if ( (level == 0) && !get_ponder_move() ) {  /* Output some stats */
      if ( update_pv ) {
	if ( curr_val <= alpha )
	  send_sweep( "<%d", curr_val + 1 );
	else {
	  if ( curr_val >= beta )
	    send_sweep( ">%d", curr_val - 1 );
	  else {
	    send_sweep( "=%d", curr_val );
	    true_found = TRUE;
	    true_val = curr_val;
	  }
	}
      }
      send_sweep( " " );
      if ( update_pv && (move_index > 0) && echo)
	display_sweep( stdout );
    }

    if ( update_pv ) {
      update_best_list( best_list, move, best_list_index, &best_list_length,
			level == 0 );
      pv[level][level] = move;
      pv_depth[level] = pv_depth[level + 1];
      for ( i = level + 1; i < pv_depth[level + 1]; i++ )
	pv[level][i] = pv[level + 1][i];
    }
    if ( best >= beta ) {  /* Fail high */
      if ( use_hash )
	add_hash_extended( ENDGAME_MODE, best, best_list,
			   ENDGAME_SCORE | LOWER_BOUND, remains,
			   *selective_cutoff ? selectivity : 0 );
      return best;
    }

    if ( (best_list_index >= best_list_length) && !update_pv &&
	 (best_list_length < 4) )
      best_list[best_list_length++] = move;

    first = FALSE;
  }

#if CHECK_HASH_CODES && defined( TEXT_BASED )
  if ( use_hash )
    if ( (h1 != hash1) || (h2 != hash2) )
      printf( "%s: %x%x    %s: %x%x", HASH_BEFORE, h1, h2,
	      HASH_AFTER, hash1, hash2 );
#endif
  if ( !first ) {
    if ( use_hash ) {
      int flags = ENDGAME_SCORE;
      if ( best > alpha )
	flags |= EXACT_VALUE;
      else
	flags |= UPPER_BOUND;
      add_hash_extended( ENDGAME_MODE, best, best_list, flags, remains,
			 *selective_cutoff ? selectivity : 0 );
    }
    return best;
  }
  else if ( void_legal ) {
    if ( use_hash ) {
      hash1 ^= hash_flip_color1;
      hash2 ^= hash_flip_color2;
    }
    curr_val = -end_tree_search( level, max_depth,
				 opp_bits, my_bits, OPP( side_to_move ),
				 -beta, -alpha,
				 selectivity, selective_cutoff, FALSE );

    if ( use_hash ) {
      hash1 ^= hash_flip_color1;
      hash2 ^= hash_flip_color2;
    }
    return curr_val;
  }
  else {
    pv_depth[level] = level;
    my_discs = piece_count[side_to_move][disks_played];
    opp_discs = piece_count[OPP( side_to_move )][disks_played];
    disk_diff = my_discs - opp_discs;
    if ( my_discs > opp_discs )
      return 64 - 2 * opp_discs;
    else if ( my_discs == opp_discs )
      return 0;
    else
      return -(64 - 2 * my_discs);
  }
}



/*
  END_TREE_WRAPPER
  Wrapper onto END_TREE_SEARCH which applies the knowledge that
  the range of valid scores is [-64,+64].  Komi, if any, is accounted for.
*/

static int
end_tree_wrapper( int level,
		  int max_depth,
		  int side_to_move,
		  int alpha,
		  int beta,
		  int selectivity,
		  int void_legal ) {
  int selective_cutoff;
  BitBoard my_bits, opp_bits;

  init_mmx();
  set_bitboards( board, side_to_move, &my_bits, &opp_bits );

  return end_tree_search( level, max_depth,
			  my_bits, opp_bits, side_to_move,
			  MAX( alpha - komi_shift, -64 ),
			  MIN( beta - komi_shift, 64 ),
			  selectivity, &selective_cutoff, void_legal ) +
    komi_shift;
}



/*
   FULL_EXPAND_PV
   Pad the PV with optimal moves in the low-level phase.
*/

static void
full_expand_pv( int side_to_move,
		int selectivity ) {
  int i;
  int pass_count;
  int new_pv_depth;
  int new_pv[61];
  int new_side_to_move[61];

  new_pv_depth = 0;
  pass_count = 0;
  while ( pass_count < 2 ) {
    int move;

    generate_all( side_to_move );
    if ( move_count[disks_played] > 0 ) {
      int empties = 64 - disc_count( BLACKSQ ) - disc_count( WHITESQ );

      (void) end_tree_wrapper( new_pv_depth, empties, side_to_move,
			       -64, 64, selectivity, TRUE );
      move = pv[new_pv_depth][new_pv_depth];
      new_pv[new_pv_depth] = move;
      new_side_to_move[new_pv_depth] = side_to_move;
      (void) make_move( side_to_move, move, TRUE );
      new_pv_depth++;
    }
    else {
      hash1 ^= hash_flip_color1;
      hash2 ^= hash_flip_color2;
      pass_count++;
    }
    side_to_move = OPP( side_to_move );
  }
  for ( i = new_pv_depth - 1; i >= 0; i-- )
    unmake_move( new_side_to_move[i], new_pv[i] );
  for ( i = 0; i < new_pv_depth; i++ )
    pv[0][i] = new_pv[i];
  pv_depth[0] = new_pv_depth;
}



/*
  SEND_SOLVE_STATUS
  Displays endgame results - partial or full.
*/

static void
send_solve_status( int empties,
		   int side_to_move,
		   EvaluationType *eval_info ) {
  char *eval_str;
  double node_val;

  set_current_eval( *eval_info );
  clear_status();
  send_status( "-->  %2d  ", empties );
  eval_str = produce_eval_text( *eval_info, TRUE );
  send_status( "%-10s  ", eval_str );
  free( eval_str );
  node_val = counter_value( &nodes );
  send_status_nodes( node_val );
  if ( get_ponder_move() )
    send_status( "{%c%c} ", TO_SQUARE( get_ponder_move() ) );
  send_status_pv( pv[0], empties );
  send_status_time( get_elapsed_time() );
  if ( get_elapsed_time() > 0.0001 )
    send_status( "%6.0f %s  ", node_val / (get_elapsed_time() + 0.0001),
		 NPS_ABBREV);
}


/*
  END_GAME
  Provides an interface to the fast endgame solver.
*/

int
end_game( int side_to_move,
	  int wld,
	  int force_echo,
	  int allow_book,
	  int komi,
	  EvaluationType *eval_info ) {
  double current_confidence;
  enum { WIN, LOSS, DRAW, UNKNOWN } solve_status;
  int book_move;
  int empties;
  int selectivity;
  int alpha, beta;
  int any_search_result, exact_score_failed;
  int incomplete_search;
  int long_selective_search;
  int old_depth, old_eval;
  int last_window_center;
  int old_pv[MAX_SEARCH_DEPTH];
  EvaluationType book_eval_info;

  empties = 64 - disc_count( BLACKSQ ) - disc_count( WHITESQ );

  /* In komi games, the WLD window is adjusted. */

  if ( side_to_move == BLACKSQ )
    komi_shift = komi;
  else
    komi_shift = -komi;

  /* Check if the position is solved (WLD or exact) in the book. */

  book_move = PASS;
  if ( allow_book ) {
    /* Is the exact score known? */

    fill_move_alternatives( side_to_move, FULL_SOLVED );
    book_move = get_book_move( side_to_move, FALSE, eval_info );
    if ( book_move != PASS ) {
      root_eval = eval_info->score / 128;
      hash_expand_pv( side_to_move, ENDGAME_MODE, EXACT_VALUE, 0 );
      send_solve_status( empties, side_to_move, eval_info );
      return book_move;
    }

    /* Is the WLD status known? */

    fill_move_alternatives( side_to_move, WLD_SOLVED );
    if ( komi_shift == 0 ) {
      book_move = get_book_move( side_to_move, FALSE, eval_info );
      if ( book_move != PASS ) {
	if ( wld ) {
	  root_eval = eval_info->score / 128;
	  hash_expand_pv( side_to_move, ENDGAME_MODE,
			  EXACT_VALUE | UPPER_BOUND | LOWER_BOUND, 0  );
	  send_solve_status( empties, side_to_move, eval_info );
	  return book_move;
	}
	else
	  book_eval_info = *eval_info;
      }
    }

    fill_endgame_hash( HASH_DEPTH, 0 );
  }

  last_panic_check = 0.0;
  solve_status = UNKNOWN;
  old_eval = 0;

  /* Prepare for the shallow searches using the midgame eval */

  piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
  piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );

  if ( empties > 32 )
    set_panic_threshold( 0.20 );
  else if ( empties < 22 )
    set_panic_threshold( 0.50 );
  else
    set_panic_threshold( 0.50 - (empties - 22) * 0.03 );

  reset_buffer_display();

  /* Make sure the pre-searches don't mess up the hash table */

  toggle_midgame_hash_usage( TRUE, FALSE );

  incomplete_search = FALSE;
  any_search_result = FALSE;

  /* Start off by selective endgame search */

  last_window_center = 0;

  if ( empties > DISABLE_SELECTIVITY ) {
    if ( wld )
      for ( selectivity = MAX_SELECTIVITY; (selectivity > 0) &&
	      !is_panic_abort() && !force_return; selectivity-- ) {
	unsigned int flags;
	EvalResult res;

	alpha = -1;
	beta = +1;
	root_eval = end_tree_wrapper( 0, empties, side_to_move,
				      alpha, beta, selectivity, TRUE );

	adjust_counter( &nodes );

	if ( is_panic_abort() || force_return )
	  break;

	any_search_result = TRUE;
	old_eval = root_eval;
	store_pv( old_pv, &old_depth );
	current_confidence = confidence[selectivity];

	flags = EXACT_VALUE;
	if ( root_eval == 0 )
	  res = DRAWN_POSITION;
	else {
	  flags |= (UPPER_BOUND | LOWER_BOUND);
	  if ( root_eval > 0 )
	    res = WON_POSITION;
	  else
	    res = LOST_POSITION;
	}
	*eval_info =
	  create_eval_info( SELECTIVE_EVAL, res, root_eval * 128,
			    current_confidence, empties, FALSE );
	if ( full_output_mode ) {
	  hash_expand_pv( side_to_move, ENDGAME_MODE, flags, selectivity );
	  send_solve_status( empties, side_to_move, eval_info );
	}
      }
    else
      for ( selectivity = MAX_SELECTIVITY; (selectivity > 0) &&
	      !is_panic_abort() && !force_return; selectivity-- ) {
	alpha = last_window_center - 1;
	beta = last_window_center + 1;

	root_eval = end_tree_wrapper( 0, empties, side_to_move,
				      alpha, beta, selectivity, TRUE );

	if ( root_eval <= alpha ) {
	  do {
	    last_window_center -= 2;
	    alpha = last_window_center - 1;
	    beta = last_window_center + 1;
	    if ( is_panic_abort() || force_return )
	      break;
	    root_eval = end_tree_wrapper( 0, empties, side_to_move,
					  alpha, beta, selectivity, TRUE );
	  } while ( root_eval <= alpha );
	  root_eval = last_window_center;
	}
	else if ( root_eval >= beta ) {
	  do {
	    last_window_center += 2;
	    alpha = last_window_center - 1;
	    beta = last_window_center + 1;
	    if ( is_panic_abort() || force_return )
	      break;
	    root_eval = end_tree_wrapper( 0, empties, side_to_move,
					  alpha, beta, selectivity, TRUE );
	  } while ( root_eval >= beta );
	  root_eval = last_window_center;
	}

	adjust_counter( &nodes );

	if ( is_panic_abort() || force_return )
	  break;

	last_window_center = root_eval;

	if ( !is_panic_abort() && !force_return ) {
	  any_search_result = TRUE;
	  old_eval = root_eval;
	  store_pv( old_pv, &old_depth );
	  current_confidence = confidence[selectivity];

	  *eval_info =
	    create_eval_info( SELECTIVE_EVAL, UNSOLVED_POSITION,
			      root_eval * 128, current_confidence,
			      empties, FALSE );
	  if ( full_output_mode ) {
	    hash_expand_pv( side_to_move, ENDGAME_MODE, EXACT_VALUE, selectivity );
	    send_solve_status( empties, side_to_move, eval_info );
	  }
	}
      }
  }
  else
    selectivity = 0;

  /* Check if the selective search took more than 40% of the allocated
       time. If this is the case, there is no point attempting WLD. */

  long_selective_search = check_threshold( 0.35 );

  /* Make sure the panic abort flag is set properly; it must match
     the status of long_selective_search. This is not automatic as
     it is not guaranteed that any selective search was performed. */

  check_panic_abort();

  if ( is_panic_abort() || force_return || long_selective_search ) {

    /* Don't try non-selective solve. */

    if ( any_search_result ) {
      if ( echo && (is_panic_abort() || force_return) ) {
#ifdef TEXT_BASED
	printf( "%s %.1f %c %s\n", SEMI_PANIC_ABORT_TEXT, get_elapsed_time(),
		SECOND_ABBREV, SEL_SEARCH_TEXT );
#endif
	if ( full_output_mode ) {
	  unsigned int flags;

	  flags = EXACT_VALUE;
	  if ( solve_status != DRAW )
	    flags |= (UPPER_BOUND | LOWER_BOUND);
	  hash_expand_pv( side_to_move, ENDGAME_MODE, flags, selectivity );
	  send_solve_status( empties, side_to_move, eval_info );
	}
      }
      pv[0][0] = best_end_root_move;
      pv_depth[0] = 1;
      root_eval = old_eval;
      clear_panic_abort();
    }
    else {
#ifdef TEXT_BASED
      if ( echo )
	printf( "%s %.1f %c %s\n", PANIC_ABORT_TEXT, get_elapsed_time(),
		SECOND_ABBREV, SEL_SEARCH_TEXT );
#endif
      root_eval = SEARCH_ABORT;                   
    }

    if ( echo || force_echo )
      display_status( stdout, FALSE );

    if ( (book_move != PASS) &&
	 ((book_eval_info.res == WON_POSITION) ||
	  (book_eval_info.res == DRAWN_POSITION)) ) {
      /* If there is a known win (or mismarked draw) available,
	   always play it upon timeout. */
      *eval_info = book_eval_info;
      root_eval = eval_info->score / 128;
      return book_move;
    }
    else
      return pv[0][0];
  }

  /* Start non-selective solve */

  if ( wld ) {
    alpha = -1;
    beta = +1;
  }
  else {
    alpha = last_window_center - 1;
    beta = last_window_center + 1;
  }

  root_eval = end_tree_wrapper( 0, empties, side_to_move,
				alpha, beta, 0, TRUE );

  adjust_counter( &nodes );

  if ( !is_panic_abort() && !force_return ) {
    if ( !wld ) {
      if ( root_eval <= alpha ) {
	int ceiling_value = last_window_center - 2;
	while ( 1 ) {
	  alpha = ceiling_value - 1;
	  beta = ceiling_value;
	  root_eval = end_tree_wrapper( 0, empties, side_to_move,
					alpha, beta, 0, TRUE );
	  if ( is_panic_abort() || force_return )
	    break;
	  if ( root_eval > alpha )
	    break;
	  else
	    ceiling_value -= 2;
	}
      }
      else if ( root_eval >= beta ) {
	int floor_value = last_window_center + 2;
	while ( 1 ) {
	  alpha = floor_value - 1;
	  beta = floor_value + 1;
	  root_eval = end_tree_wrapper( 0, empties, side_to_move,
					alpha, beta, 0, TRUE );
	  if ( is_panic_abort() || force_return )
	    break;
	  assert( root_eval > alpha );
	  if ( root_eval < beta )
	    break;
	  else
	    floor_value += 2;
	}
      }
    }
    if ( !is_panic_abort() && !force_return ) {
      EvalResult res;
      if ( root_eval < 0 )
	res = LOST_POSITION;
      else if ( root_eval == 0 )
	res = DRAWN_POSITION;
      else
	res = WON_POSITION;
      if ( wld ) {
	unsigned int flags;

	if ( root_eval == 0 )
	  flags = EXACT_VALUE;
	else
	  flags = UPPER_BOUND | LOWER_BOUND;
	*eval_info =
	  create_eval_info( WLD_EVAL, res, root_eval * 128, 0.0, empties, FALSE );
	if ( full_output_mode ) {
	  hash_expand_pv( side_to_move, ENDGAME_MODE, flags, 0 );
	  send_solve_status( empties, side_to_move, eval_info );
	}
      }
      else {
	*eval_info =
	  create_eval_info( EXACT_EVAL, res, root_eval * 128, 0.0, 
			    empties, FALSE );
	if ( full_output_mode ) {
	  hash_expand_pv( side_to_move, ENDGAME_MODE, EXACT_VALUE, 0 );
	  send_solve_status( empties, side_to_move, eval_info );
	}
      }
    }
  }

  adjust_counter( &nodes );

  /* Check for abort. */

  if ( is_panic_abort() || force_return ) {
    if ( any_search_result ) {
      if ( echo ) {
#ifdef TEXT_BASED
	printf( "%s %.1f %c %s\n", SEMI_PANIC_ABORT_TEXT,
		get_elapsed_time(), SECOND_ABBREV, WLD_SEARCH_TEXT );
#endif
	if ( full_output_mode ) {
	  unsigned int flags;

	  flags = EXACT_VALUE;
	  if ( root_eval != 0 )
	    flags |= (UPPER_BOUND | LOWER_BOUND);
	  hash_expand_pv( side_to_move, ENDGAME_MODE, flags, 0 );
	  send_solve_status( empties, side_to_move, eval_info );
	}
	if ( echo || force_echo )
	  display_status( stdout, FALSE );
      }
      restore_pv( old_pv, old_depth );
      root_eval = old_eval;
      clear_panic_abort();
    }
    else {
#ifdef TEXT_BASED
      if ( echo )
	printf( "%s %.1f %c %s\n", PANIC_ABORT_TEXT,
		get_elapsed_time(), SECOND_ABBREV, WLD_SEARCH_TEXT );
#endif
      root_eval = SEARCH_ABORT;
    }

    return pv[0][0];
  }

  /* Update solve info. */

  store_pv( old_pv, &old_depth );
  old_eval = root_eval;

  if ( !is_panic_abort() && !force_return && (empties > earliest_wld_solve) )
    earliest_wld_solve = empties;


  /* Check for aborted search. */

  exact_score_failed = FALSE;
  if ( incomplete_search ) {
    if ( echo ) {
#ifdef TEXT_BASED
      printf( "%s %.1f %c %s\n", SEMI_PANIC_ABORT_TEXT,
	      get_elapsed_time(), SECOND_ABBREV, EXACT_SEARCH_TEXT );
#endif
      if ( full_output_mode ) {
	hash_expand_pv( side_to_move, ENDGAME_MODE, EXACT_VALUE, 0 );
	send_solve_status( empties, side_to_move, eval_info );
      }
      if ( echo || force_echo )
	display_status( stdout, FALSE );
    }
    pv[0][0] = best_end_root_move;
    pv_depth[0] = 1;
    root_eval = old_eval;
    exact_score_failed = TRUE;
    clear_panic_abort();
  }
      
  if ( abs( root_eval ) % 2 == 1 ) {
    if ( root_eval > 0 )
      root_eval++;
    else
      root_eval--;
  }

  if ( !exact_score_failed && !wld && (empties > earliest_full_solve) )
    earliest_full_solve = empties;

  if ( !wld && !exact_score_failed ) {
    eval_info->type = EXACT_EVAL;
    eval_info->score = root_eval * 128;
  }

  if ( !wld && !exact_score_failed ) {
    hash_expand_pv( side_to_move, ENDGAME_MODE, EXACT_VALUE, 0 );
    send_solve_status( empties, side_to_move, eval_info );
  }

  if ( echo || force_echo )
    display_status( stdout, FALSE );

  /* For shallow endgames, we can afford to compute the entire PV
     move by move. */

  if ( !wld && !incomplete_search && !force_return &&
       (empties <= PV_EXPANSION) )
    full_expand_pv( side_to_move, 0 );

  return pv[0][0];
}



/*
   SETUP_END
   Prepares the endgame solver for a new game.
   This means clearing a few status fields.   
*/

void
setup_end( void ) {
  double last_mean, last_sigma;
  double ff_threshold[61];
  double prelim_threshold[61][64];
  int i, j;
  static const int dir_shift[8] = {1, -1, 7, -7, 8, -8, 9, -9};

  earliest_wld_solve = 0;
  earliest_full_solve = 0;
  full_output_mode = TRUE;

  /* Calculate the neighborhood masks */

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      /* Create the neighborhood mask for the square POS */

      int pos = 10 * i + j;
      int shift = 8 * (i - 1) + (j - 1);
      unsigned int k;

      neighborhood_mask[pos].low = 0;
      neighborhood_mask[pos].high = 0;

      for ( k = 0; k < 8; k++ )
	if ( dir_mask[pos] & (1 << k) ) {
	  unsigned int neighbor = shift + dir_shift[k];
	  if ( neighbor < 32 )
	    neighborhood_mask[pos].low |= (1 << neighbor);
	  else
	    neighborhood_mask[pos].high |= (1 << (neighbor - 32));
	}
    }

  /* Set the fastest-first mobility encouragements and thresholds */

  for ( i = 0; i <= 60; i++ )
    ff_mob_factor[i] = MOB_FACTOR;
  for ( i = 0; i <= 60; i++ )
    ff_threshold[i] = FAST_FIRST_FACTOR;

  /* Calculate the alpha thresholds for using fastest-first for
     each #empty and shallow search depth. */

  for ( j = 0; j <= MAX_END_CORR_DEPTH; j++ ) {
    last_sigma = 100.0;  /* Infinity in disc difference */
    last_mean = 0.0;
    for ( i = 60; i >= 0; i-- ) {
      if ( end_stats_available[i][j] ) {
	last_mean = end_mean[i][j];
	last_sigma = ff_threshold[i] * end_sigma[i][j];
      }
      fast_first_mean[i][j] = last_mean;
      fast_first_sigma[i][j] = last_sigma;
      prelim_threshold[i][j] = last_mean + last_sigma;
    }
  }
  for ( j = MAX_END_CORR_DEPTH + 1; j < 64; j++ )
    for ( i = 0; i <= 60; i++ )
      prelim_threshold[i][j] = prelim_threshold[i][MAX_END_CORR_DEPTH];
  for ( i = 0; i <= 60; i++ )
    for ( j = 0; j < 64; j++ )
      fast_first_threshold[i][j] =
	(int) ceil( prelim_threshold[i][j] * 128.0 );
}



/*
  GET_EARLIEST_WLD_SOLVE
  GET_EARLIEST_FULL_SOLVE
  Return the highest #empty when WLD and full solve respectively
  were completed (not initiated).
*/

int
get_earliest_wld_solve( void ) {
  return earliest_wld_solve;
}

int
get_earliest_full_solve( void ) {
  return earliest_full_solve;
}



/*
  SET_OUTPUT_MODE
  Toggles output of intermediate search status on/off.
*/

void
set_output_mode( int full ) {
  full_output_mode = full;
}
