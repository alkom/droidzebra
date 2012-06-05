/*
   File:          search.c

   Created:       July 1, 1997

   Modified:      January 2, 2003

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      Common search routines and variables.
*/



#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "constant.h"
#include "counter.h"
#include "error.h"
#include "hash.h"
#include "globals.h"
#include "macros.h"
#include "moves.h"
#include "search.h"
#include "texts.h"



/* Global variables */

double total_time;
int root_eval;
int force_return;
int full_pv_depth;
int full_pv[120];
int list_inherited[61];
int sorted_move_order[64][64];  /* 61*60 used */
Board evals[61];
CounterType nodes, total_nodes;
CounterType evaluations, total_evaluations;

/* When no other information is available, JCW's endgame
   priority order is used also in the midgame. */
int position_list[100] = {
  /*A1*/        11 , 18 , 81 , 88 , 
  /*C1*/        13 , 16 , 31 , 38 , 61 , 68 , 83 , 86 ,
  /*C3*/        33 , 36 , 63 , 66 ,
  /*D1*/        14 , 15 , 41 , 48 , 51 , 58 , 84 , 85 ,
  /*D3*/        34 , 35 , 43 , 46 , 53 , 56 , 64 , 65 ,
  /*D2*/        24 , 25 , 42 , 47 , 52 , 57 , 74 , 75 ,
  /*C2*/        23 , 26 , 32 , 37 , 62 , 67 , 73 , 76 ,
  /*B1*/        12 , 17 , 21 , 28 , 71 , 78 , 82 , 87 ,
  /*B2*/        22 , 27 , 72 , 77 ,
  /*D4*/        44 , 45 , 54 , 45 ,
  /*North*/      0 ,  1 ,  2 ,  3 ,  4 ,  5 ,  6 ,  7 , 8,
  /*East*/       9 , 19 , 29 , 39 , 49 , 59 , 69 , 79 , 89,
  /*West*/      10 , 20 , 30 , 40 , 50 , 60 , 70 , 80 , 90,
  /*South*/     91 , 92 , 93 , 94 , 95 , 96 , 97 , 98 , 99 };



/* Local variables */

static int pondered_move = 0;
static int negate_eval;
static EvaluationType last_eval;



/*
  INIT_MOVE_LISTS
  Initalize the self-organizing move lists.
*/

static void
init_move_lists( void ) {
  int i, j;

  for ( i = 0; i <= 60; i++ ) {
    for ( j = 0; j < MOVE_ORDER_SIZE; j++ )
      sorted_move_order[i][j] = position_list[j];
  }
  for ( i = 0; i <= 60; i++ )
    list_inherited[i] = FALSE;
}



/*
  INHERIT_MOVE_LISTS
  If possible, initialize the move list corresponding to STAGE
  moves being played with an earlier move list from a stage
  corresponding to the same parity (i.e., in practice side to move).
*/

void
inherit_move_lists( int stage ) {
  int i;
  int last;

  if ( list_inherited[stage] )
    return;
  list_inherited[stage] = TRUE;
  if ( stage == 0 )
    return;
  last = stage - 2;
  while ( (last >= 0) && (!list_inherited[last]) )
    last -= 2;
  if ( last < 0 )
    return;
  for ( i = 0; i < MOVE_ORDER_SIZE; i++ )
    sorted_move_order[stage][i] = sorted_move_order[last][i];
}



/*
  REORDER_MOVE_LIST
  Move the empty squares to the front of the move list.  Empty squares
  high up in the ranking are kept in place as they probably are empty
  in many variations in the tree.
*/

void
reorder_move_list( int stage ) {
  const int dont_touch = 24;
  int i;
  int move;
  int empty_pos;
  int nonempty_pos;
  int empty_buffer[MOVE_ORDER_SIZE];
  int nonempty_buffer[MOVE_ORDER_SIZE];

  empty_pos = 0;
  for ( i = 0; i < MOVE_ORDER_SIZE; i++ ) {
    move = sorted_move_order[stage][i];
    if ( (board[move] == EMPTY) || (i < dont_touch) ) {
      empty_buffer[empty_pos] = move;
      empty_pos++;
    }
  }
  nonempty_pos = MOVE_ORDER_SIZE - 1;
  for ( i = MOVE_ORDER_SIZE - 1; i >= 0; i-- ) {
    move = sorted_move_order[stage][i];
    if ( (board[move] != EMPTY) && (i >= dont_touch) ) {
      nonempty_buffer[nonempty_pos] = move;
      nonempty_pos--;
    }
  }
  for ( i = 0; i < empty_pos; i++ )
    sorted_move_order[stage][i] = empty_buffer[i];
  for ( i = empty_pos; i < MOVE_ORDER_SIZE; i++ )
    sorted_move_order[stage][i] = nonempty_buffer[i];
}



/*
   SETUP_SEARCH
   Initialize the history of the game in the search driver.
*/   

void
setup_search( void ) {
  init_move_lists();
  create_eval_info( UNINITIALIZED_EVAL, UNSOLVED_POSITION, 0, 0.0, 0, FALSE );
  negate_eval = FALSE;
}



/*
   DISC_COUNT
   side_to_move = the player whose disks are to be counted
   Returns the number of disks of a specified color.
*/

INLINE int
disc_count( int side_to_move ) {
  int i, j, sum;

  sum = 0;
  for ( i = 1; i <= 8; i++ )
    for ( j = 10 * i + 1; j <= 10 * i + 8; j++ )
      if ( board[j] == side_to_move )
	sum++;

  return sum;
}



/*
   SORT_MOVES
   Sort the available in decreasing order based on the results
   from a shallow search.
*/   

INLINE void
sort_moves( int list_size ) {
  int i;
  int modified;
  int temp_move;

  do {
    modified = FALSE;
    for ( i = 0; i < list_size - 1; i++ )
      if ( evals[disks_played][move_list[disks_played][i]] <
	   evals[disks_played][move_list[disks_played][i + 1]] ) {
	modified = TRUE;
	temp_move = move_list[disks_played][i];
	move_list[disks_played][i] = move_list[disks_played][i + 1];
	move_list[disks_played][i + 1] = temp_move;
      }
  } while ( modified );
}



/*
  SELECT_MOVE
  Finds the best move in the move list neglecting the first FIRST moves.
  Moves this move to the front of the sub-list.
*/

INLINE int
select_move( int first, int list_size ) {
  int i;
  int temp_move;
  int best, best_eval;

  best = first;
  best_eval = evals[disks_played][move_list[disks_played][first]];
  for ( i = first + 1; i < list_size; i++ )
    if ( evals[disks_played][move_list[disks_played][i]] > best_eval ) {
      best = i;
      best_eval = evals[disks_played][move_list[disks_played][i]];
    }
  if ( best != first ) {
    temp_move = move_list[disks_played][first];
    move_list[disks_played][first] = move_list[disks_played][best];
    move_list[disks_played][best] = temp_move;
  }

  return move_list[disks_played][first];
}



/*
  FLOAT_MOVE
  "Float" a move which is believed to be good to the top
  of the list of available moves.
  Return 1 if the move was found, 0 otherwise.
*/    

INLINE int
float_move( int move, int list_size ) {
  int i, j;

  for ( i = 0; i < list_size; i++ )
    if ( move_list[disks_played][i] == move ) {
      for ( j = i; j >= 1; j-- )
	move_list[disks_played][j] = move_list[disks_played][j - 1];
      move_list[disks_played][0] = move;
      return TRUE;
    }
  return FALSE;
}



/*
   STORE_PV
   Saves the principal variation (the first row of the PV matrix).
*/   

void
store_pv( int *pv_buffer, int *depth_buffer ) {
  int i;

  for ( i = 0; i < pv_depth[0]; i++ )
    pv_buffer[i] = pv[0][i];
  *depth_buffer = pv_depth[0];
}



/*
   RESTORE_PV
   Put the stored principal variation back into the PV matrix.
*/   

void
restore_pv( int *pv_buffer, int depth_buffer ) {
  int i;

  for ( i = 0; i < depth_buffer; i++ )
    pv[0][i] = pv_buffer[i];
  pv_depth[0] = depth_buffer;
}



/*
  CLEAR_PV
  Clears the principal variation.
*/

void
clear_pv( void ) {
  pv_depth[0] = 0;
}



/*
  COMPLETE_PV
  Complete the principal variation with passes (if any there are any).
*/

void
complete_pv( int side_to_move ) {
  int i;
  int actual_side_to_move[60];

  full_pv_depth = 0;
  for ( i = 0; i < pv_depth[0]; i++ ) {
    if ( make_move( side_to_move, pv[0][i], TRUE ) ) {
      actual_side_to_move[i] = side_to_move;
      full_pv[full_pv_depth] = pv[0][i];
      full_pv_depth++;
    }
    else {
      full_pv[full_pv_depth] = PASS;
      full_pv_depth++;
      side_to_move = OPP( side_to_move );
      if ( make_move( side_to_move, pv[0][i], TRUE ) ) {
	actual_side_to_move[i] = side_to_move;
	full_pv[full_pv_depth] = pv[0][i];
	full_pv_depth++;
      }
      else {
#ifdef TEXT_BASED
	int j;

	printf( "pv_depth[0] = %d\n", pv_depth[0] );
	for ( j = 0; j < pv_depth[0]; j++ )
	  printf( "%c%c ", TO_SQUARE( pv[0][j] ) );
	puts( "" );
	printf( "i=%d\n", i );
#endif
	fatal_error( PV_ERROR );
      }
    }
    side_to_move = OPP( side_to_move );
  }
  for ( i = pv_depth[0] - 1; i >= 0; i-- )
    unmake_move( actual_side_to_move[i], pv[0][i] );
}



/*
  HASH_EXPAND_PV
  Pad the existing PV with the move sequence suggested by the hash table.
*/

void
hash_expand_pv( int side_to_move,
		int mode,
		int flags,
		int max_selectivity ) {
  int i;
  int pass_count;
  int new_pv_depth;
  int new_pv[61];
  int new_side_to_move[61];
  HashEntry entry;

  determine_hash_values( side_to_move, board );
  new_pv_depth = 0;
  pass_count = 0;

  while ( pass_count < 2 ) {
    new_side_to_move[new_pv_depth] = side_to_move;
    if ( (new_pv_depth < pv_depth[0]) && (new_pv_depth == 0) ) {
      if ( (board[pv[0][new_pv_depth]] == EMPTY) &&
	   make_move( side_to_move, pv[0][new_pv_depth], TRUE ) ) {
	new_pv[new_pv_depth] = pv[0][new_pv_depth];
	new_pv_depth++;
	pass_count = 0;
      }
      else {
	hash1 ^= hash_flip_color1;
	hash2 ^= hash_flip_color2;
	pass_count++;
      }
    }
    else {
      find_hash( &entry, mode );
      if ( (entry.draft != NO_HASH_MOVE) &&
	   (entry.flags & flags) &&
	   (entry.selectivity <= max_selectivity) &&
	   (board[entry.move[0]] == EMPTY) &&
	   make_move( side_to_move, entry.move[0], TRUE ) ) {
	new_pv[new_pv_depth] = entry.move[0];
	new_pv_depth++;
	pass_count = 0;
      }
      else {
	hash1 ^= hash_flip_color1;
	hash2 ^= hash_flip_color2;
	pass_count++;
      }
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
  SET_PONDER_MOVE
  CLEAR_PONDER_MOVE
  GET_PONDER_MOVE
  A value of 0 denotes a normal search while anything else means
  that the search is performed given that the move indicated has
  been made.
*/

void
set_ponder_move( int move ) {
  pondered_move = move;
}

void
clear_ponder_move( void ) {
  pondered_move = 0;
}

int
get_ponder_move( void ) {
  return pondered_move;
}



/*
  CREATE_EVAL_INFO
  Creates a result descriptor given all the information available
  about the last search.
*/

EvaluationType
create_eval_info( EvalType in_type, EvalResult in_res,
		  int in_score, double in_conf,
		  int in_depth, int in_book ) {
  EvaluationType out;

  out.type = in_type;
  out.res = in_res;
  out.score = in_score;
  out.confidence = in_conf;
  out.search_depth = in_depth;
  out.is_book = in_book;

  return out;
}


/*
  PRODUCE_COMPACT_EVAL
  Converts a result descriptor into a number between -99.99 and 99.99 a la GGS.
*/

double
produce_compact_eval( EvaluationType eval_info ) {
  double eval;

  switch ( eval_info.type ) {

  case MIDGAME_EVAL:
    /*
    eval = eval_info.search_depth + logistic_map( eval_info.score );
    if ( eval_info.is_book )
      eval = -eval;
      */
    eval = eval_info.score / 128.0;
    return eval;

  case EXACT_EVAL:
    return eval_info.score / 128.0;

  case WLD_EVAL:
    switch ( eval_info.res ) {
    case WON_POSITION:
      if ( eval_info.score > 2 * 128 )  /* Win by more than 2 */
	return (eval_info.score / 128.0) - 0.01;
      else
	return 1.99;
    case DRAWN_POSITION:
      return 0.0;
    case LOST_POSITION:
      if ( eval_info.score < -2 * 128 )  /* Loss by more than 2 */
	return (eval_info.score / 128.0) + 0.01;
      else
	return -1.99;
    case UNSOLVED_POSITION:
      return 0.0;
    }

  case SELECTIVE_EVAL:
    switch ( eval_info.res ) {
    case WON_POSITION:
      return 1.0 + eval_info.confidence;
    case DRAWN_POSITION:
      return -1.0 + eval_info.confidence;
    case LOST_POSITION:
      return -1.0 - eval_info.confidence;
    case UNSOLVED_POSITION:
      return eval_info.score / 128.0;
    }

  case FORCED_EVAL:
  case PASS_EVAL:
  case INTERRUPTED_EVAL:
  case UNDEFINED_EVAL:
  case UNINITIALIZED_EVAL:
    return 0.0;

  }

  return 0.0;  /* This statement shouldn't be reached */
}



/*
  SET_CURRENT_EVAL
  GET_CURRENT_EVAL
  NEGATE_CURRENT_EVAL
  Mutator and accessor functions for the global variable
  holding the last available position evaluation.
*/

void
set_current_eval( EvaluationType eval ) {
  last_eval = eval;
  if ( negate_eval ) {
    last_eval.score = -last_eval.score;
    if ( last_eval.res == WON_POSITION )
      last_eval.res = LOST_POSITION;
    else if ( last_eval.res == LOST_POSITION )
      last_eval.res = WON_POSITION;
  }
}

EvaluationType
get_current_eval( void ) {
  return last_eval;
}

void
negate_current_eval( int negate ) {
  negate_eval = negate;
}
