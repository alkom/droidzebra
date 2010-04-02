/*
   File:          midgame.c

   Created:       July 1, 1997

   Modified:      November 23, 2002
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      Search routines designated to be used in the
                  midgame phase of the game.
*/



#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "autoplay.h"
#include "constant.h"
#include "display.h"
#include "eval.h"
#include "getcoeff.h"
#include "globals.h"
#include "hash.h"
#include "macros.h"
#include "midgame.h"
#include "moves.h"
#include "myrandom.h"
#include "patterns.h"
#include "pcstat.h"
#include "probcut.h"
#include "search.h"
#include "texts.h"
#include "timer.h"



/* Enable/disable Multi-ProbCut */
#define USE_MPC                  1

/* Number of nodes searched between consecutive time checks */
#ifdef _WIN32_WCE
#define EVENT_CHECK_INTERVAL     10000
#else
#define EVENT_CHECK_INTERVAL     100000
#endif

/* The eval bonus given if the hash table move is found during the
   shallow search. */
#define HASH_MOVE_BONUS          10000

/* The depth from which where hash tables lookups are performed. */
#define HASH_THRESHOLD           2

/* The depth where shallow searches are first performed. */
#define PRE_SEARCH_THRESHOLD     3

/* The depth to which all variations must be searched */
#define FULL_WIDTH_DEPTH         8

/* The depth where shallow searches are performed to depth 2. */
#define DEPTH_TWO_DEPTH          10

/* Default aspiration window parameters. These values are currently
   really huge as usage of a small windows tends to slow down
   the search. */
#define USE_WINDOW               FALSE
#define ALPHA_WINDOW             2048
#define BETA_WINDOW              2048

#define WIPEOUT_THRESHOLD        60



static int allow_midgame_hash_probe;
static int allow_midgame_hash_update;
static int best_mid_move, best_mid_root_move;
static int midgame_abort;
static int do_check_midgame_abort = TRUE;
static int counter_phase;
static int apply_perturbation = TRUE;
static int perturbation_amplitude = 0;
static int stage_reached[61], stage_score[61];
static int score_perturbation[100];
static int feas_index_list[64][64];



/*
   SETUP_MIDGAME
   Sets up some search parameters.
*/

void
setup_midgame( void ) {
  int i;

  allow_midgame_hash_probe = TRUE;
  allow_midgame_hash_update = TRUE;
  for ( i = 0; i <= 60; i++ )
    stage_reached[i] = FALSE;

  calculate_perturbation();
}



/*
  CLEAR_MIDGAME_ABORT
  IS_MIDGAME_ABORT
  SET_MIDGAME_ABORT
  TOGGLE_MIDGAME_ABORT_CHECK
  These functions handle the midgame abort system which kicks in
  when it is estimated that the next iteration in the iterative
  deepening would take too long.
*/

void
clear_midgame_abort( void ) {
  midgame_abort = FALSE;
}

int
is_midgame_abort( void ) {
  return midgame_abort;
}

void
set_midgame_abort( void ) {
  midgame_abort = do_check_midgame_abort;
}

void
toggle_midgame_abort_check( int toggle ) {
  do_check_midgame_abort = toggle;
}



/*
   TOGGLE_MIDGAME_HASH_USAGE
   Toggles hash table access in the midgame search on/off.
*/   

void
toggle_midgame_hash_usage( int allow_read, int allow_write ) {
  allow_midgame_hash_probe = allow_read;
  allow_midgame_hash_update = allow_write;
}



/*
  CALCULATE_PERTURBATION
  Determines the score perturbations (if any) to the root moves.
*/

void
calculate_perturbation( void ) {
  int i;
  int shift;

  if ( !apply_perturbation || (perturbation_amplitude == 0) )
    for ( i = 0; i < 100; i++ )
      score_perturbation[i] = 0;
  else {
    shift = perturbation_amplitude / 2;
    for ( i = 0; i < 100; i++ )
      score_perturbation[i] =
	(abs( my_random()) % perturbation_amplitude) - shift;
  }
}



/*
  SET_PERTURBATION
  Set the amplitude of the score perturbation applied by
  CALCULATE_PERTURBATION.
*/

void
set_perturbation( int amplitude ) {
  perturbation_amplitude = amplitude;
}



/*
  TOGGLE_PERTURBATION_USAGE
  Toggle usage of score perturbations on/off.
*/

void
toggle_perturbation_usage( int toggle ) {
  apply_perturbation = toggle;
}



/*
  ADVANCE_MOVE
  Swaps a move and its predecessor in the move list if it's
  not already first in the list.
*/

INLINE static
void
advance_move( int index ) {
  int temp_move;

  if ( index > 0 ) {
    temp_move = sorted_move_order[disks_played][index];
    sorted_move_order[disks_played][index] =
      sorted_move_order[disks_played][index - 1];
    sorted_move_order[disks_played][index - 1] = temp_move;
  }
}



/*
  STATIC_OR_TERMINAL_EVALUATION
  Invokes the proper evaluation function depending on whether the
  board is filled or not.
*/

INLINE static int
static_or_terminal_evaluation( int side_to_move ) {
  if ( disks_played == 60 )
    return terminal_evaluation( side_to_move );
  else
    return static_evaluation( side_to_move );
}



/*
   FAST_TREE_SEARCH
   The recursive tree search function. It uses negascout for
   tree pruning.
*/

static int
fast_tree_search( int level, int max_depth, int side_to_move, int alpha,
		  int beta, int allow_hash, int void_legal ) {
  int curr_val, best;
  int move_index, move;
  int best_move_index, best_move;
  int first;
  int remains;
  int use_hash, new_use_hash;
  int curr_alpha;
  int empties_remaining;
  HashEntry entry;

  INCREMENT_COUNTER( nodes );

  if ( level >= max_depth )
    return static_or_terminal_evaluation( side_to_move );

  /* Check the hash table */

  remains = max_depth - level;

  use_hash = (remains >= HASH_THRESHOLD) && USE_HASH_TABLE && allow_hash;
  if ( use_hash && allow_midgame_hash_probe ) {
    find_hash( &entry, MIDGAME_MODE );
    if ( (entry.draft >= remains) &&
	 (entry.selectivity == 0) &&
	 valid_move( entry.move[0], side_to_move ) &&
	 (entry.flags & MIDGAME_SCORE) &&
	 ((entry.flags & EXACT_VALUE) ||
	  ((entry.flags & LOWER_BOUND) && entry.eval >= beta) ||
	  ((entry.flags & UPPER_BOUND) && entry.eval <= alpha)) ) {
      best_mid_move = entry.move[0];
      return entry.eval;
    }
  }

  /* Reorder the move lists now and then to keep the empty squares up front */

  if ( (nodes.lo & 4095) == 0 )
    reorder_move_list( disks_played );

  /* Search */

  first = TRUE;

  best_move = PASS;
  best_move_index = -1;
  best = -INFINITE_EVAL;

  if ( remains == 1 ) {  /* Plain alpha-beta last ply */
    empties_remaining = 60 - disks_played;
    for ( move_index = 0; move_index < MOVE_ORDER_SIZE; move_index++ ) {
      move = sorted_move_order[disks_played][move_index];
      if ( board[move] == EMPTY ) {
	if ( make_move_no_hash( side_to_move, move ) != 0 ) {
	  curr_val = -static_or_terminal_evaluation( OPP( side_to_move ) );
	  unmake_move_no_hash( side_to_move, move );
	  INCREMENT_COUNTER( nodes );
	  if ( curr_val > best ) {
	    best = curr_val;
	    best_move_index = move_index;
	    best_move = move;
	    if ( curr_val >= beta ) {
	      advance_move( move_index );
	      best_mid_move = best_move;
	      if ( use_hash && allow_midgame_hash_update )
		add_hash( MIDGAME_MODE, best, best_move,
			  MIDGAME_SCORE | LOWER_BOUND, remains, 0 );
	      return best;
	    }
	  }
	  first = FALSE;
	}
	empties_remaining--;
	if ( empties_remaining == 0 )
	  break;
      }
    }
  }
  else {  /* Principal variation search for deeper searches */
    new_use_hash = (remains >= HASH_THRESHOLD + 1) && use_hash;
    curr_alpha = alpha;
    empties_remaining = 60 - disks_played;
    for ( move_index = 0; move_index < MOVE_ORDER_SIZE; move_index++ ) {
      move = sorted_move_order[disks_played][move_index];
      if ( board[move] == EMPTY ) {
	if ( make_move( side_to_move, move, new_use_hash ) != 0 ) {
	  if ( first ) {
	    best = curr_val =
	      -fast_tree_search( level + 1, max_depth, OPP( side_to_move ),
				 -beta, -curr_alpha, allow_hash, TRUE );
	    best_move = move;
	    best_move_index = move_index;
	  }
	  else {
	    curr_alpha = MAX( best, curr_alpha );
	    curr_val =
	      -fast_tree_search( level + 1, max_depth, OPP( side_to_move ),
				 -(curr_alpha + 1), -curr_alpha, allow_hash,
				 TRUE );
	    if ( (curr_val > curr_alpha) && (curr_val < beta) )
	      curr_val =
		-fast_tree_search( level + 1, max_depth, OPP( side_to_move ),
				   -beta, INFINITE_EVAL, allow_hash, TRUE );
	    if ( curr_val > best ) {
	      best_move = move;
	      best_move_index = move_index;
	      best = curr_val;
	    }
	  }
	  unmake_move( side_to_move, move );
	  if ( best >= beta ) {
	    advance_move( move_index );
	    best_mid_move = best_move;
	    if ( use_hash && allow_midgame_hash_update )
	      add_hash( MIDGAME_MODE, best, best_move,
			MIDGAME_SCORE | LOWER_BOUND, remains, 0 );
	    return best;
	  }
	  first = FALSE;
	}
	empties_remaining--;
	if ( empties_remaining == 0 )
	  break;
      }
    }
  }
  if ( !first ) {
    advance_move( best_move_index );
    best_mid_move = best_move;
    if ( use_hash && allow_midgame_hash_update ) {
      if ( best > alpha )
	add_hash( MIDGAME_MODE, best, best_move,
		  MIDGAME_SCORE | EXACT_VALUE, remains, 0 );
      else
	add_hash( MIDGAME_MODE, best, best_move,
		  MIDGAME_SCORE | UPPER_BOUND, remains, 0 );
    }
    return best;
  }
  else if ( void_legal ) {  /* I pass, other player's turn now */
    hash1 ^= hash_flip_color1;
    hash2 ^= hash_flip_color2;
    curr_val = -fast_tree_search( level, max_depth, OPP( side_to_move ),
				  -beta, -alpha, allow_hash, FALSE );
    hash1 ^= hash_flip_color1;
    hash2 ^= hash_flip_color2;
    return curr_val;
  }
  else {  /* Both players had to pass ==> evaluate board as final */
    curr_val = terminal_evaluation( side_to_move );
    return curr_val;
  }
}



#define VERBOSE 0

/*
  UPDATE_BEST_LIST
*/

static void
update_best_list( int *best_list, int move, int best_list_index,
		  int best_list_length ) {
  int i;

#if VERBOSE
  printf( "move=%2d  index=%d  length=%d      ", move, best_list_index,
	  best_list_length );
  printf( "Before:  " );
  for ( i = 0; i < 4; i++ )
    printf( "%2d ", best_list[i] );
#endif
  if ( best_list_index < best_list_length )
    for ( i = best_list_index; i >= 1; i-- )
      best_list[i] = best_list[i - 1];
  else
    for ( i = 3; i >= 1; i-- )
      best_list[i] = best_list[i - 1];
  best_list[0] = move;
#if VERBOSE
  printf( "      After:  " );
  for ( i = 0; i < 4; i++ )
    printf( "%2d ", best_list[i] );
  puts( "" );
#endif
}



/*
   TREE_SEARCH
   The recursive tree search function. It uses negascout for
   tree pruning.
*/

int
tree_search( int level,
	     int max_depth,
	     int side_to_move,
	     int alpha,
	     int beta,
	     int allow_hash,
	     int allow_mpc,
	     int void_legal ) {
  int i, j;
  int curr_val, best, pre_best;
  int searched;
  int move, hash_move;
  int move_index, best_move_index;
  int empties_remaining;
  int hash_hit;
  int pre_depth;
  int update_pv;
  int remains, shallow_remains;
  int use_hash, pre_search_done;
  int curr_alpha;
  int best_index, best_score;
  int best_list_index, best_list_length;
  int selectivity, cut;
  int best_list[4];
  HashEntry entry;
#if CHECK_HASH_CODES && defined( TEXT_BASED )
  unsigned int h1, h2;
#endif

  if ( level >= max_depth ) {
    INCREMENT_COUNTER( nodes );
    return static_or_terminal_evaluation( side_to_move );
  }

  remains = max_depth - level;

  if ( remains < PRE_SEARCH_THRESHOLD ) {
    curr_val = fast_tree_search( level, max_depth, side_to_move, alpha,
				 beta, allow_hash, void_legal );
    pv_depth[level] = level + 1;
    pv[level][level] = best_mid_move;
    return curr_val;
  }

  INCREMENT_COUNTER( nodes );

  /* Check the hash table */

  use_hash = (remains >= HASH_THRESHOLD) && USE_HASH_TABLE && allow_hash;
  if ( USE_MPC && allow_mpc )
    selectivity = 1;
  else
    selectivity = 0;
#if CHECK_HASH_CODES && defined( TEXT_BASED )
  if ( use_hash ) {
    h1 = hash1;
    h2 = hash2;
  }
#endif
  if ( use_hash && allow_midgame_hash_probe ) {
    find_hash( &entry, MIDGAME_MODE );
    if ( (entry.draft >= remains) &&
	 (entry.selectivity <= selectivity) &&
	 valid_move( entry.move[0], side_to_move ) &&
	 (entry.flags & MIDGAME_SCORE) &&
	 ((entry.flags & EXACT_VALUE) ||
	  ((entry.flags & LOWER_BOUND) && entry.eval >= beta) ||
	  ((entry.flags & UPPER_BOUND) && entry.eval <= alpha)) ) {
      pv[level][level] = entry.move[0];
      pv_depth[level] = level + 1;
      return entry.eval;
    }
  }

  hash_hit = (use_hash && allow_midgame_hash_probe);
  if ( hash_hit )
    hash_move = entry.move[0];
  else
    hash_move = 44;

  pre_search_done = FALSE;

  /* Use multi-prob-cut to selectively prune the tree */

  if ( USE_MPC && allow_mpc && (remains <= MAX_CUT_DEPTH) ) {
    int alpha_test = TRUE;
    int beta_test = TRUE;

    for ( cut = 0; cut < mpc_cut[remains].cut_tries; cut++ ) {

      /* Determine the fail-high and fail-low bounds */

      int bias = mpc_cut[remains].bias[cut][disks_played];
      int window = mpc_cut[remains].window[cut][disks_played];
      int alpha_bound = alpha + bias - window;
      int beta_bound = beta + bias + window;

      /* Don't use an MPC cut which results in the full-width depth
	 being less than some predefined constant */

      shallow_remains = mpc_cut[remains].cut_depth[cut];
      if ( level + shallow_remains < FULL_WIDTH_DEPTH )
	continue;

      if  ( shallow_remains > 1 ) {  /* "Deep" shallow search */
	if ( cut == 0 ) {

	  /* Use static eval to decide if a one- or two-sided
	     MPC test is to be performed. */

	  int static_eval = static_evaluation( side_to_move );
	  if ( static_eval <= alpha_bound )
	    beta_test = FALSE;
	  else if ( static_eval >= beta_bound )
	    alpha_test = FALSE;
	}

	assert( alpha_test || beta_test );

	if ( alpha_test && beta_test ) {
	  /* Test for likely fail-low or likely fail-high. */
	  int shallow_val =
	    tree_search( level, level + shallow_remains, side_to_move,
			 alpha_bound, beta_bound, allow_hash, FALSE,
			 void_legal );
	  if ( shallow_val >= beta_bound  ) {
	    if ( use_hash && allow_midgame_hash_update )
	      add_hash( MIDGAME_MODE, beta, pv[level][level],
			MIDGAME_SCORE | LOWER_BOUND, remains, selectivity );
	    return beta;
	  }
	  else if ( shallow_val <= alpha_bound ) {
	    if ( use_hash && allow_midgame_hash_update )
	      add_hash( MIDGAME_MODE, alpha, pv[level][level],
			MIDGAME_SCORE | UPPER_BOUND, remains, selectivity );
	    return alpha;
	  }
	  else {
	    /* Use information learned from the failed cut test to decide
	       if a one or a two-sided test is to be performed next. */
	    int mid = (alpha_bound + beta_bound) / 2;

#if 0
	    if ( abs( shallow_val - alpha_bound ) < abs( shallow_val - mid ) )
	      beta_test = FALSE;
	    if ( abs( shallow_val - beta_bound ) < abs( shallow_val - mid ) )
	      alpha_test = FALSE;
#elif 1
	    if ( shallow_val < mid )
	      beta_test = FALSE;
	    else
	      alpha_test = FALSE;
#else
	    int low_threshold = (2 * mid + alpha_bound) / 3;
	    int high_threshold = (2 * mid + beta_bound) / 3;

	    if ( shallow_val <= low_threshold )
	      beta_test = FALSE;
	    else if ( shallow_val >= high_threshold )
	      alpha_test = FALSE;
	    else
	      break;  /* Unlikely that there is any selective cutoff. */
#endif	    
	  }
	}
	else if ( beta_test ) {
	  /* Fail-high with high probability? */
	  if ( tree_search( level, level + shallow_remains, side_to_move,
			    beta_bound - 1, beta_bound, allow_hash, FALSE,
			    void_legal ) >= beta_bound ) {
	    if ( use_hash && allow_midgame_hash_update )
	      add_hash( MIDGAME_MODE, beta, pv[level][level],
			MIDGAME_SCORE | LOWER_BOUND, remains, selectivity );
	    return beta;
	  }
	}
	else if ( alpha_test ) {
	  /* Fail-low with high probability? */
	  if ( tree_search( level, level + shallow_remains, side_to_move,
			    alpha_bound, alpha_bound + 1, allow_hash, FALSE,
			    void_legal ) <= alpha_bound ) {
	    if ( use_hash && allow_midgame_hash_update )
	      add_hash( MIDGAME_MODE, alpha, pv[level][level],
			MIDGAME_SCORE | UPPER_BOUND, remains, selectivity );
	    return alpha;
	  }
	}
      }
      else {  /* All-in-one MPC one-ply search and move ordering */
	move_count[disks_played] = 0;
	best = alpha_bound;
	empties_remaining = 60 - disks_played;
	for ( move_index = 0; move_index < MOVE_ORDER_SIZE; move_index++ ) {
	  move = sorted_move_order[disks_played][move_index];
	  if ( board[move] == EMPTY ) {
	    if ( make_move_no_hash( side_to_move, move ) != 0 ) {
	      curr_val = -static_or_terminal_evaluation( OPP( side_to_move ) );
	      unmake_move_no_hash( side_to_move, move );
	      INCREMENT_COUNTER( nodes );
	      if ( curr_val > best ) {
		best = curr_val;
		if ( best >= beta_bound ) {
		  if ( use_hash && allow_midgame_hash_update )
		    add_hash( MIDGAME_MODE, beta, pv[level][level],
			      MIDGAME_SCORE | LOWER_BOUND, remains,
			      selectivity );
		  return beta;
		}
	      }
	      evals[disks_played][move] = curr_val;
	      if ( move == hash_move )  /* Always try hash table move first */
		evals[disks_played][move] += HASH_MOVE_BONUS;
	      feas_index_list[disks_played][move_count[disks_played]] =
		move_index;
	      move_count[disks_played]++;
	    }
	    empties_remaining--;
	    if ( empties_remaining == 0 )
	      break;
	  }
	}
	if ( (best == alpha_bound) && (move_count[disks_played] > 0) ) {
	  if ( use_hash && allow_midgame_hash_update )
	    add_hash( MIDGAME_MODE, alpha, pv[level][level],
		      MIDGAME_SCORE | UPPER_BOUND, remains, selectivity );
	  return alpha;
	}
	pre_search_done = TRUE;
      }
    }
  }

  /* Full negascout search */
	
  searched = 0;
  best = -INFINITE_EVAL;
  best_move_index = -1;
  curr_alpha = alpha;

  best_list_length = 0;
  for ( i = 0; i < 4; i++ )
    best_list[i] = 0;
  if ( !pre_search_done ) {
    move_count[disks_played] = 0;
    if ( hash_hit )
      for ( i = 0; i < 4; i++ )
	if ( valid_move( entry.move[i], side_to_move ) )
	  best_list[best_list_length++] = entry.move[i];
  }

  for ( i = 0, best_list_index = 0; TRUE; i++, best_list_index++ ) {

    /* Try the hash table move(s) first if feasible */

    if ( !pre_search_done && (best_list_index < best_list_length) ) {
      move_count[disks_played]++;
      move_index = 0;
      while ( sorted_move_order[disks_played][move_index] !=
	      best_list[best_list_index] )
	move_index++;
    }
    else {  /* Otherwise use information from shallow searches */
      if ( !pre_search_done ) {
	if ( remains < DEPTH_TWO_DEPTH )
	  pre_depth = 1;
	else
	  pre_depth = 2;
	pre_best = -INFINITE_EVAL;
	empties_remaining = 60 - disks_played;
	for ( move_index = 0; move_index < MOVE_ORDER_SIZE; move_index++ ) {
	  int already_checked;

	  move = sorted_move_order[disks_played][move_index];
	  already_checked = FALSE;
	  for ( j = 0; j < best_list_length; j++ )
	    if ( move == best_list[j] )
	      already_checked = TRUE;

	  if ( board[move] == EMPTY ) {
	    if ( !already_checked &&
		 (make_move( side_to_move, move, TRUE ) != 0 ) ) {
	      curr_val = -tree_search( level + 1, level + pre_depth,
				       OPP( side_to_move ), -INFINITE_EVAL,
				       -pre_best, FALSE, FALSE, TRUE );
	      pre_best = MAX( pre_best, curr_val );
	      unmake_move( side_to_move, move );
	      evals[disks_played][move] = curr_val;
	      feas_index_list[disks_played][move_count[disks_played]] =
		move_index;
	      move_count[disks_played]++;
	    }
	    empties_remaining--;
	    if ( empties_remaining == 0 )
	      break;
	  }
	}
	pre_search_done = TRUE;
      }

      if ( i == move_count[disks_played] )  /* No moves left to try? */
	break;

      best_index = i;
      best_score =
	evals[disks_played][sorted_move_order[disks_played][feas_index_list[disks_played][i]]];
      for ( j = i + 1; j < move_count[disks_played]; j++ ) {
	int cand_move;

	cand_move =
	  sorted_move_order[disks_played][feas_index_list[disks_played][j]];
	if ( evals[disks_played][cand_move] > best_score ) {
	  best_score = 
	    evals[disks_played][cand_move];
	  best_index = j;
	}
      }

      move_index = feas_index_list[disks_played][best_index];
      feas_index_list[disks_played][best_index] =
	feas_index_list[disks_played][i];
    }

    move = sorted_move_order[disks_played][move_index];

    counter_phase = (counter_phase + 1) & 63;
    if ( counter_phase == 0 ) {
      double node_val;
      adjust_counter( &nodes );
      node_val = counter_value( &nodes );
      if ( node_val - last_panic_check >= EVENT_CHECK_INTERVAL ) {
	/* Time abort? */

	last_panic_check = node_val;
	check_panic_abort();

	/* Display available search information */

	if ( echo )
	  display_buffers();

	/* Check for events */

	handle_event( TRUE, FALSE, TRUE );

	if ( is_panic_abort() || force_return )
	  return SEARCH_ABORT;
      }
    }

    (void) make_move( side_to_move, move, TRUE );
  				
    update_pv = FALSE;

    if ( searched == 0 ) {
      best = curr_val =
	-tree_search( level + 1, max_depth, OPP( side_to_move ), -beta,
		      -curr_alpha, allow_hash, allow_mpc, TRUE );
      best_move_index = move_index;
      update_pv = TRUE;
    }
    else {
      curr_alpha = MAX( best, curr_alpha );
      curr_val =
	-tree_search( level + 1, max_depth, OPP( side_to_move ),
		      -(curr_alpha + 1), -curr_alpha, allow_hash,
		      allow_mpc, TRUE );
      if ( (curr_val > curr_alpha) && (curr_val < beta) ) {
	curr_val =
	  -tree_search( level + 1, max_depth, OPP( side_to_move ), -beta,
			INFINITE_EVAL, allow_hash, allow_mpc, TRUE );
	if ( curr_val > best ) {
	  best = curr_val;
	  best_move_index = move_index;
	  update_pv = TRUE;
	}
      }
      else if ( curr_val > best ) {
	best = curr_val;
	best_move_index = move_index;
	update_pv = TRUE;
      }
    }

    unmake_move( side_to_move, move );

    if ( is_panic_abort() || force_return )
      return SEARCH_ABORT;

    evals[disks_played][move] = curr_val;

    if ( update_pv ) {
      update_best_list( best_list, move, best_list_index, best_list_length );
      pv[level][level] = move;
      pv_depth[level] = pv_depth[level + 1];
      for ( j = level + 1; j < pv_depth[level + 1]; j++)
	pv[level][j] = pv[level + 1][j];
    }

    if ( best >= beta ) {
      advance_move( move_index );
      if ( use_hash && allow_midgame_hash_update )
	add_hash_extended( MIDGAME_MODE, best, best_list,
			   MIDGAME_SCORE | LOWER_BOUND, remains, selectivity );
      return best;
    }
    searched++;
  }

  /* Post-processing */

#if CHECK_HASH_CODES && defined( TEXT_BASED )
  if ( use_hash )
    if ( (h1 != hash1) || (h2 != hash2) )
      printf( "%s: %x%x    %s: %x%x", HASH_BEFORE, h1, h2,
	      HASH_AFTER, hash1, hash2 );
#endif
  if ( move_count[disks_played] > 0 ) {
      advance_move( best_move_index );
    if ( use_hash && allow_midgame_hash_update ) {
      if ( best > alpha )
	add_hash_extended( MIDGAME_MODE, best, best_list,
			   MIDGAME_SCORE | EXACT_VALUE, remains, selectivity );
      else
	add_hash_extended( MIDGAME_MODE, best, best_list,
			   MIDGAME_SCORE | UPPER_BOUND, remains, selectivity );
    }
    return best;
  }
  else if ( void_legal ) { /* No feasible moves */
    hash1 ^= hash_flip_color1;
    hash2 ^= hash_flip_color2;
    curr_val = -tree_search( level, max_depth, OPP( side_to_move ), -beta,
			     -alpha, allow_hash, allow_mpc, FALSE );
    hash1 ^= hash_flip_color1;
    hash2 ^= hash_flip_color2;
    return curr_val;
  }
  else {
    pv_depth[level] = level;
    return terminal_evaluation( side_to_move );
  }
}



/*
  PERTURB_SCORE
  Perturbs SCORE by PERTURBATION if it doesn't appear to be
  a midgame win.
*/

static int
perturb_score( int score, int perturbation ) {
  if ( abs( score ) < ALMOST_MIDGAME_WIN )
    return score + perturbation;
  else
    return score;
}



/*
   ROOT_TREE_SEARCH
   The recursive tree search function that is to be called only
   for the root of the search tree.
*/

int
root_tree_search( int level, int max_depth, int side_to_move, int alpha,
		  int beta, int allow_hash, int allow_mpc, int void_legal ) {
  char buffer[32];
  int i, j;
  int curr_val, best, pre_best;
  int searched;
  int move;
  int move_index, best_move_index;
  int hash_hit;
  int pre_depth;
  int update_pv;
  int remains;
  int use_hash, pre_search_done;
  int curr_alpha;
  int best_index, best_score;
  int best_list_index, best_list_length;
  int selectivity;
  int offset;
  int best_list[4];
  HashEntry entry;
#if CHECK_HASH_CODES && defined( TEXT_BASED )
  unsigned int h1, h2;
#endif

  remains = max_depth - level;

  INCREMENT_COUNTER( nodes );

  use_hash = (remains >= HASH_THRESHOLD) && USE_HASH_TABLE && allow_hash;
  if ( USE_MPC && allow_mpc )
    selectivity = 1;
  else
    selectivity = 0;
#if CHECK_HASH_CODES && defined( TEXT_BASED )
  if ( use_hash ) {
    h1 = hash1;
    h2 = hash2;
  }
#endif

  /* Hash strategy at the root: Only use hash table information for
     move ordering purposes.  This guarantees that score perturbation
     is applied for all moves. */

  hash_hit = FALSE;
  if ( use_hash && allow_midgame_hash_probe ) {
    find_hash( &entry, MIDGAME_MODE );
    if ( entry.draft != NO_HASH_MOVE )
      hash_hit = TRUE;
  }

  pre_search_done = FALSE;

  if ( !get_ponder_move() ) {
    if ( (alpha <= -MIDGAME_WIN) && (beta >= MIDGAME_WIN) )
      sprintf( buffer, "[-inf,inf]:" );
    else if ( (alpha <= -MIDGAME_WIN) && (beta < MIDGAME_WIN) )
      sprintf( buffer, "[-inf,%.1f]:", beta / 128.0 );
    else if ( (alpha > -MIDGAME_WIN) && (beta >= MIDGAME_WIN) )
      sprintf( buffer, "[%.1f,inf]:", alpha / 128.0 );
    else
      sprintf( buffer, "[%.1f,%.1f]:", alpha / 128.0, beta / 128.0 );
    clear_sweep();
    send_sweep( "%-14s ", buffer );
  }

  /* Full negascout search */
	
  searched = 0;
  best = -INFINITE_EVAL;
  best_move_index = -1,
  curr_alpha = alpha;

  best_list_length = 0;
  for ( i = 0; i < 4; i++ )
    best_list[i] = 0;
  if ( !pre_search_done ) {
    move_count[disks_played] = 0;
    if ( hash_hit )
      for ( i = 0; i < 4; i++ )
	if ( valid_move( entry.move[i], side_to_move ) )
	  best_list[best_list_length++] = entry.move[i];
  }

  for ( i = 0, best_list_index = 0; TRUE; i++, best_list_index++ ) {

    /* Try the hash table move(s) first if feasible */

    if ( !pre_search_done && (best_list_index < best_list_length) ) {
      move_count[disks_played]++;
      move_index = 0;
      while ( sorted_move_order[disks_played][move_index] !=
	      best_list[best_list_index] )
	move_index++;
    }
    else {  /* Otherwise use information from shallow searches */
      if ( !pre_search_done ) {
	if ( remains < DEPTH_TWO_DEPTH )
	  pre_depth = 1;
	else
	  pre_depth = 2;
	pre_best = -INFINITE_EVAL;
	for ( move_index = 0; move_index < MOVE_ORDER_SIZE; move_index++ ) {
	  int already_checked;

	  move = sorted_move_order[disks_played][move_index];
	  already_checked = FALSE;
	  for ( j = 0; j < best_list_length; j++ )
	    if ( move == best_list[j] )
	      already_checked = TRUE;

	  if ( !already_checked && (board[move] == EMPTY) &&
	       (make_move( side_to_move, move, TRUE ) != 0 ) ) {
	    curr_val = -tree_search( level + 1, level + pre_depth,
				     OPP( side_to_move ), -INFINITE_EVAL, -pre_best,
 				     FALSE, FALSE, TRUE );
	    pre_best = MAX( pre_best, curr_val );
	    unmake_move( side_to_move, move );
	    evals[disks_played][move] = curr_val;
	    feas_index_list[disks_played][move_count[disks_played]] =
	      move_index;
	    move_count[disks_played]++;
	  }
	}
	pre_search_done = TRUE;
      }

      if ( i == move_count[disks_played] )  /* No moves left to try? */
	break;

      best_index = i;
      best_score =
	evals[disks_played][sorted_move_order[disks_played][feas_index_list[disks_played][i]]];
      for ( j = i + 1; j < move_count[disks_played]; j++ ) {
	int cand_move;

	cand_move =
	  sorted_move_order[disks_played][feas_index_list[disks_played][j]];
	if ( evals[disks_played][cand_move] > best_score ) {
	  best_score = 
	    evals[disks_played][cand_move];
	  best_index = j;
	}
      }

      move_index = feas_index_list[disks_played][best_index];
      feas_index_list[disks_played][best_index] =
	feas_index_list[disks_played][i];
    }

    move = sorted_move_order[disks_played][move_index];

    if ( !get_ponder_move() )
      send_sweep( "%c%c", TO_SQUARE( move ) );

    (void) make_move( side_to_move, move, TRUE );

    update_pv = FALSE;
    offset = score_perturbation[move];

    if ( searched == 0 ) {
      best = curr_val =
	perturb_score( -tree_search( level + 1, max_depth, OPP( side_to_move ),
				     -(beta - offset), -(curr_alpha - offset),
				     allow_hash, allow_mpc, TRUE ),
		       offset );
      best_move_index = move_index;
      update_pv = TRUE;
      best_mid_root_move = move;
    }
    else {
      curr_alpha = MAX( best, curr_alpha );
      curr_val =
	perturb_score( -tree_search( level + 1, max_depth, OPP( side_to_move ),
				     -(curr_alpha - offset + 1),
				     -(curr_alpha - offset), allow_hash,
				     allow_mpc, TRUE ),
		       offset );
      if ( (curr_val > curr_alpha) && (curr_val < beta) ) {
	curr_val =
	  perturb_score( -tree_search( level + 1, max_depth,
				       OPP( side_to_move ),
				       -(beta - offset), INFINITE_EVAL, allow_hash,
				       allow_mpc, TRUE ),
			 offset );
	if ( curr_val > best ) {
	  best = curr_val;
	  best_move_index = move_index;
	  update_pv = TRUE;
	  if ( !is_panic_abort() && !force_return )
	    best_mid_root_move = move;
	}
      }
      else if ( curr_val > best ) {
	best = curr_val;
	best_move_index = move_index;
	update_pv = TRUE;
      }
    }

    unmake_move( side_to_move, move );

    if ( is_panic_abort() || force_return )
      return SEARCH_ABORT;

    evals[disks_played][move] = curr_val;

    if ( !get_ponder_move() ) {
      if ( update_pv ) {
	if ( curr_val <= alpha )
	  send_sweep("<%.2f", (curr_val + 1) / 128.0);
	else if ( curr_val >= beta )
	  send_sweep( ">%.2f", (curr_val - 1) / 128.0 );
	else
	  send_sweep( "=%.2f", curr_val / 128.0 );
      }
      send_sweep( " " );
      if ( update_pv && (searched > 0) && echo && (max_depth >= 10) )
	display_sweep( stdout );
    }

    if ( update_pv ) {
      update_best_list( best_list, move, best_list_index, best_list_length );
      pv[level][level] = move;
      pv_depth[level] = pv_depth[level + 1];
      for ( j = level + 1; j < pv_depth[level + 1]; j++)
	pv[level][j] = pv[level + 1][j];
    }

    if ( best >= beta ) {
      advance_move( move_index );
      if ( use_hash && allow_midgame_hash_update )
	add_hash_extended( MIDGAME_MODE, best, best_list,
			   MIDGAME_SCORE | LOWER_BOUND, remains, selectivity );
      return best;
    }

    /* For symmetry reasons, the score for any move is the score of the
       position for the initial position. */
    if ( disks_played == 0 ) {
      add_hash_extended( MIDGAME_MODE, best, best_list,
			 MIDGAME_SCORE | EXACT_VALUE, remains, selectivity );
      return best;
    }

    searched++;
  }

  /* Post-processing */

#if CHECK_HASH_CODES && defined( TEXT_BASED )
  if ( use_hash )
    if ( (h1 != hash1) || (h2 != hash2) )
      printf( "%s: %x%x    %s: %x%x", HASH_BEFORE, h1, h2,
	      HASH_AFTER, hash1, hash2 );
#endif
  if ( move_count[disks_played] > 0 ) {
      advance_move( best_move_index );
    if ( use_hash && allow_midgame_hash_update ) {
      if ( best > alpha )
	add_hash_extended( MIDGAME_MODE, best, best_list,
			   MIDGAME_SCORE | EXACT_VALUE, remains, selectivity );
      else
	add_hash_extended( MIDGAME_MODE, best, best_list,
			   MIDGAME_SCORE | UPPER_BOUND, remains, selectivity );
    }
    return best;
  }
  else if ( void_legal ) { /* No feasible moves */
    hash1 ^= hash_flip_color1;
    hash2 ^= hash_flip_color2;
    curr_val = -root_tree_search( level, max_depth, OPP( side_to_move ), -beta,
				  -alpha, allow_hash, allow_mpc, FALSE );
    hash1 ^= hash_flip_color1;
    hash2 ^= hash_flip_color2;
    return curr_val;
  }
  else {
    pv_depth[level] = level;
    return terminal_evaluation( side_to_move );
  }
}



/*
  PROTECTED_ONE_PLY_SEARCH
  Chooses the move maximizing the static evaluation function
  while avoiding all moves which allow an immediate loss
  (if that is possible).
*/

static int
protected_one_ply_search( int side_to_move ) {
  int i;
  int move;
  int depth_one_score, depth_two_score;
  int best_score_restricted, best_score_unrestricted;
  int best_move_restricted, best_move_unrestricted;

  generate_all( side_to_move );

  best_score_restricted = -INFINITE_EVAL;
  best_score_unrestricted = -INFINITE_EVAL;
  best_move_restricted = 0;
  best_move_unrestricted = 0;

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    INCREMENT_COUNTER( nodes );
    move = move_list[disks_played][i];
    (void) make_move( side_to_move,  move, TRUE );
    depth_one_score = -static_evaluation( OPP( side_to_move ) );
    depth_two_score = -tree_search( 1, 2, OPP( side_to_move ), -INFINITE_EVAL,
				    INFINITE_EVAL, FALSE, FALSE, FALSE );
    unmake_move( side_to_move, move );
    if ( depth_one_score > best_score_unrestricted ) {
      best_score_unrestricted = depth_one_score;
      best_move_unrestricted = move;
    }
    if ( (depth_two_score > -MIDGAME_WIN) &&
	 (depth_one_score > best_score_restricted) ) {
      best_score_restricted = depth_one_score;
      best_move_restricted = move;
    }
  }
  pv_depth[0] = 1;
  if ( best_score_restricted > -INFINITE_EVAL ) {  /* No immediate loss */
    pv[0][0] = best_move_restricted;
    return best_score_restricted;
  }
  else {
    pv[0][0] = best_move_unrestricted;
    return best_score_unrestricted;
  }
}


/*
   MIDDLE_GAME
   side_to_move = the side whose turn it is to move
*/

int
middle_game( int side_to_move, int max_depth,
	     int update_evals, EvaluationType *eval_info ) {
  char *eval_str;
  double node_val;
  int val, old_val;
  int adjusted_val;
  int initial_depth, depth;
  int alpha, beta;
  int enable_mpc;
  int base_stage;
  int full_length_line;
  HashEntry entry;

  last_panic_check = 0.0;
  counter_phase = 0;

  piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
  piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );
  base_stage = disc_count( BLACKSQ ) + disc_count( WHITESQ ) - 4;

  val = 0;
  old_val = -SEARCH_ABORT;

  enable_mpc = (max_depth >= MIN_MPC_DEPTH);
  initial_depth = MAX( 1, max_depth - 2 );
  initial_depth = max_depth;  /* Disable I.D. in this function */

  *eval_info = create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
				 0, 0.0, 0, FALSE );

  for ( depth = initial_depth; depth <= max_depth; depth++ ) {
#if USE_WINDOW
    {
      int center;

      if ( (base_stage + depth >= 2) &&
	   stage_reached[base_stage + depth - 2] ) {
	if ( side_to_move == BLACKSQ )
	  center = stage_score[base_stage + depth - 2];
	else
	  center = -stage_score[base_stage + depth - 2];
      }
      else
	center = 0;
      alpha = center - ALPHA_WINDOW;
      beta = center + BETA_WINDOW;
    }
#else
    alpha = -INFINITE_EVAL;
    beta = +INFINITE_EVAL;
#endif

    inherit_move_lists( disks_played + max_depth );

    /* The actual search */

    if ( depth == 1 )  /* Fix to make it harder to wipe out depth-1 Zebra */
      val = protected_one_ply_search( side_to_move );
    else if ( enable_mpc ) {
      val =  root_tree_search( 0, depth, side_to_move, alpha, beta, TRUE,
			       TRUE, TRUE );
      if ( !force_return && !is_panic_abort() &&
	   ((val <= alpha) || (val >= beta)) )
	val = root_tree_search( 0, depth, side_to_move, -INFINITE_EVAL,
				INFINITE_EVAL, TRUE, TRUE, TRUE );

    }
    else {
      val = root_tree_search( 0, depth, side_to_move, alpha, beta, TRUE,
			      FALSE, TRUE );
      if ( !is_panic_abort() && !force_return ) {
	if ( val <= alpha )
	  val = root_tree_search( 0, depth, side_to_move, -MIDGAME_WIN,
				  alpha, TRUE, FALSE, TRUE );
	else if ( val >= beta )
	  val = root_tree_search( 0, depth, side_to_move, beta,
				  MIDGAME_WIN, TRUE, FALSE, TRUE );
      }
    }

    /* Adjust scores and PV if search is aborted */

    if ( is_panic_abort() || force_return ) {
      pv[0][0] = best_mid_root_move;
      pv_depth[0] = 1;
      hash_expand_pv( side_to_move, MIDGAME_MODE, EXACT_VALUE, INFINITE_EVAL );
      if ( (base_stage + depth - 2 >= 0) &&
	   stage_reached[base_stage + depth - 2] ) {
	val = stage_score[base_stage + depth - 2];
	if ( side_to_move == WHITESQ )
	  val = -val;
      }
      else
	val = old_val;
    }

    /* Check if the search info corresponds to a variation of
       depth exactly DEPTH which would mean that the search
       gives new score information */

    full_length_line = FALSE;
    find_hash( &entry, MIDGAME_MODE );
    if ( !force_return &&
	 !is_panic_abort() &&
	 (entry.draft != NO_HASH_MOVE) &&
	 valid_move( entry.move[0], side_to_move ) &&
	 (entry.draft == depth) )
      full_length_line = TRUE;

    /* Update the stored scores */

    if ( (!stage_reached[base_stage + depth] || full_length_line) &&
	 update_evals ) {
      stage_reached[base_stage + depth] = TRUE;
      if ( side_to_move == BLACKSQ )
	stage_score[base_stage + depth] = val;
      else
	stage_score[base_stage + depth] = -val;
    }

    /* Adjust the eval for oscillations odd/even by simply averaging the
       last two stages (if they are available). */

    if ( stage_reached[base_stage + depth] &&
	 stage_reached[base_stage + depth - 1] && update_evals ) {
      if ( side_to_move == BLACKSQ )
	adjusted_val = (stage_score[base_stage + depth] +
			stage_score[base_stage + depth - 1]) / 2;
      else
	adjusted_val = -(stage_score[base_stage + depth] +
			 stage_score[base_stage + depth - 1]) / 2;
    }
    else {
      if ( depth == initial_depth )
	adjusted_val = val;
      else
	adjusted_val = (val + old_val) / 2;
    }

    /* In case the search reached the end of the game, the score
       must be converted into an endgame score */

    if ( val >= MIDGAME_WIN )
      *eval_info = create_eval_info( EXACT_EVAL, WON_POSITION,
				     (val - MIDGAME_WIN) * 128, 0.0,
				     depth, FALSE );
    else if ( val <= -MIDGAME_WIN )
      *eval_info = create_eval_info( EXACT_EVAL, LOST_POSITION,
				     (val + MIDGAME_WIN) * 128, 0.0,
				     depth, FALSE );
    else
      *eval_info = create_eval_info( MIDGAME_EVAL, UNSOLVED_POSITION,
				     adjusted_val, 0.0, depth, FALSE );

    /* Display and store search info */

    if ( depth == max_depth ) {
      clear_status();
      send_status( "--> " );
      if ( is_panic_abort() || force_return )
	send_status( "*" );
      else
	send_status( " " );
      send_status( "%2d  ", depth );

      eval_str = produce_eval_text( *eval_info, TRUE );
      send_status( "%-10s  ", eval_str );
      free( eval_str );
      node_val = counter_value( &nodes );
      send_status_nodes( node_val );
      if ( get_ponder_move() )
	send_status( "{%c%c} ", TO_SQUARE( get_ponder_move() ) );
      hash_expand_pv( side_to_move, MIDGAME_MODE, EXACT_VALUE, INFINITE_EVAL );
      send_status_pv( pv[0], max_depth );
      send_status_time( get_elapsed_time() );
      if ( get_elapsed_time() != 0.0 )
	send_status( "%6.0f %s", node_val / (get_elapsed_time() + 0.001),
		     NPS_ABBREV );
    }

    if ( is_panic_abort() || force_return )
      break;

    /* Check if search time or adjusted search time are long enough
       for the search to be discontinued */

    old_val = adjusted_val;
    if ( do_check_midgame_abort )
      if ( above_recommended() ||
	   (extended_above_recommended() &&
	    (depth >= frozen_ponder_depth)) ) {
	set_midgame_abort();
	break;
      }
  }

  root_eval = val;

  return pv[0][0];
}
