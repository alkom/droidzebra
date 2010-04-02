/*
   File:           osfbook.c

   Created:        December 31, 1997

   Modified:       December 30, 2004
   
   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       A module which implements the book algorithm which
                   simply tries to maximize the first evaluation out
                   of book by means of nega-maxing. The details get
	           a bit hairy as all transpositions are kept track of
	           using a hash table.
*/


#include "porting.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32_WCE
#include <time.h>
#endif

#include "autoplay.h"
#include "constant.h"
#include "counter.h"
#include "display.h"
#include "end.h"
#include "error.h"
#include "eval.h"
#include "game.h"
#include "getcoeff.h"
#include "hash.h"
#include "macros.h"
#include "magic.h"
#include "midgame.h"
#include "moves.h"
#include "myrandom.h"
#include "opname.h"
#include "osfbook.h"
#include "patterns.h"
#include "safemem.h"
#include "search.h"
#include "texts.h"
#include "timer.h"



#define STAGE_WINDOW              8
#define ROOT                      0
#define DEFAULT_SEARCH_DEPTH      2
#define DEFAULT_SLACK             0
#define INFINIT_BATCH_SIZE        10000000

#define DEFAULT_GAME_MODE         PRIVATE_GAME
#define DEFAULT_DRAW_MODE         OPPONENT_WINS

#define DEFAULT_FORCE_BLACK       FALSE
#define DEFAULT_FORCE_WHITE       FALSE

/* Data structure definitions */
#define NODE_TABLE_SLACK          1000
#define EMPTY_HASH_SLOT           -1
#define NOT_AVAILABLE             -1
#define MAX_HASH_FILL             0.80

/* Tree search parameters */
#define HASH_BITS                 19
#define RANDOMIZATION             0

/* The depth for reevaluation when the hash table should be cleared */
#define HASH_CLEAR_DEPTH          8

/* Get rid of some ugly warnings by disallowing usage of the
   macro version of tolower (not time-critical anyway). */
#ifdef toupper
#undef toupper
#endif



typedef struct {
  int hash_val1;
  int hash_val2;
  short black_minimax_score;
  short white_minimax_score;
  short best_alternative_move;
  short alternative_score;
  unsigned short flags;
} BookNode;


typedef struct {
  const char *out_file_name;
  double prob;
  int max_diff;
  int max_depth;
} StatisticsSpec;



/* Local variables */

static const char *correction_script_name = NULL;
static double deviation_bonus;
static int search_depth;
static int node_table_size, hash_table_size;
static int total_game_count, book_node_count;
static int evaluated_count, evaluation_stage;
static int max_eval_count;
static int max_batch_size;
static int exhausted_node_count;
static int max_slack;
static int low_deviation_threshold, high_deviation_threshold;
static int min_eval_span, max_eval_span;
static int min_negamax_span, max_negamax_span;
static int leaf_count, bad_leaf_count, really_bad_leaf_count;
static int unreachable_count;
static int candidate_count;
static int force_black, force_white;
static int used_slack[3];
static int b1_b1_map[100], g1_b1_map[100], g8_b1_map[100], b8_b1_map[100];
static int a2_b1_map[100], a7_b1_map[100], h7_b1_map[100], h2_b1_map[100];
static int exact_count[61], wld_count[61];
static int exhausted_count[61], common_count[61];
static int *symmetry_map[8], *inv_symmetry_map[8];
static int line_hash[2][8][6561];
static int *book_hash_table = NULL;
static DrawMode draw_mode = DEFAULT_DRAW_MODE;
static GameMode game_mode = DEFAULT_GAME_MODE;
static BookNode *node = NULL;
static CandidateMove candidate_list[60];



/*
   INIT_MAPS
   Initializes the 8 symmetry maps.
   Notice that the order of these MUST coincide with the returned
   orientation value from get_hash() OR YOU WILL LOSE BIG.
*/

static void
init_maps( void ) {
  int i, j, k, pos;

  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      b1_b1_map[pos] = pos;
      g1_b1_map[pos] = 10 * i + (9 - j);
      g8_b1_map[pos] = 10 * (9 - i) + (9 - j);
      b8_b1_map[pos] = 10 * (9 - i) + j;
      a2_b1_map[pos] = 10 * j + i;
      a7_b1_map[pos] = 10 * j + (9 - i);
      h7_b1_map[pos] = 10 * (9 - j) + (9 - i);
      h2_b1_map[pos] = 10 * (9 - j) + i;
    }

  symmetry_map[0] = b1_b1_map;
  inv_symmetry_map[0] = b1_b1_map;

  symmetry_map[1] = g1_b1_map;
  inv_symmetry_map[1] = g1_b1_map;

  symmetry_map[2] = g8_b1_map;
  inv_symmetry_map[2] = g8_b1_map;

  symmetry_map[3] = b8_b1_map;
  inv_symmetry_map[3] = b8_b1_map;

  symmetry_map[4] = a2_b1_map;
  inv_symmetry_map[4] = a2_b1_map;

  symmetry_map[5] = a7_b1_map;
  inv_symmetry_map[5] = h2_b1_map; 

  symmetry_map[6] = h7_b1_map;
  inv_symmetry_map[6] = h7_b1_map;

  symmetry_map[7] = h2_b1_map;
  inv_symmetry_map[7] = a7_b1_map;

  for ( i = 0; i < 8; i++ )
    symmetry_map[i][NULL_MOVE] = NULL_MOVE;

  for ( i = 0; i < 8; i++ )
    for ( j = 1; j <= 8; j++ )
      for ( k = 1; k <= 8; k++ ) {
	pos = 10 * j + k;
	if ( inv_symmetry_map[i][symmetry_map[i][pos]] != pos )
	  fatal_error( "Error in map %d: inv(map(%d))=%d\n",
		       i, pos, inv_symmetry_map[i][symmetry_map[i][pos]] );
      }
}


/*
   SELECT_HASH_SLOT
   Finds a slot in the hash table for the node INDEX
   using linear probing.
*/

static void
select_hash_slot( int index ) {
  int slot;

  slot = node[index].hash_val1 % hash_table_size;
  while ( book_hash_table[slot] != EMPTY_HASH_SLOT )
    slot = (slot + 1) % hash_table_size;
  book_hash_table[slot] = index;
}


/*
   PROBE_HASH_TABLE
   Search for a certain hash code in the hash table.
*/

static int
probe_hash_table( int val1, int val2 ) {
  int slot;

  if ( hash_table_size == 0 )
    return NOT_AVAILABLE;
  else {
    slot = val1 % hash_table_size;
    while ( (book_hash_table[slot] != EMPTY_HASH_SLOT) &&
	    (node[book_hash_table[slot]].hash_val2 != val2 ||
	     node[book_hash_table[slot]].hash_val1 != val1) )
      slot = (slot + 1) % hash_table_size;
    return slot;
  }
}


/*
   CREATE_HASH_REFERENCEE
   Takes the node list and fills the hash table with indices
   into the node list.
*/

static void
create_hash_reference( void ) {
  int i;

  for ( i = 0; i < hash_table_size; i++ )
    book_hash_table[i] = EMPTY_HASH_SLOT;      
  for ( i = 0; i < book_node_count; i++ )
    select_hash_slot( i );
}


/*
   REBUILD_HASH_TABLE
   Resize the hash table for a requested number of nodes.
*/

static void
rebuild_hash_table( int requested_items ) {
  int new_size, new_memory;

  new_size = 2 * requested_items;
  new_memory = new_size * sizeof(int);
  if ( hash_table_size == 0 )
    book_hash_table = (int *) safe_malloc( new_memory );
  else
    book_hash_table = (int *) safe_realloc(book_hash_table, new_memory);
  if ( book_hash_table == NULL )
    fatal_error( "%s %d\n", BOOK_HASH_ALLOC_ERROR, new_memory, new_size );
  hash_table_size = new_size;
  create_hash_reference();
}


/*
   PREPARE_HASH
   Compute the position hash codes.
*/   

static void
prepare_hash( void ) {
  int i, j, k;

  /* The hash keys are static, hence the same keys must be
     produced every time the program is run. */
  my_srandom( 0 );

  for ( i = 0; i < 2; i++ )
    for ( j = 0; j < 8; j++ )
      for ( k = 0; k < 6561; k++ )
	line_hash[i][j][k] = (my_random() % 2) ? my_random() :-my_random();
  hash_table_size = 0;            
}


/*
   GET_HASH
   Return the hash values for the current board position.
   The position is rotated so that the 64-bit hash value
   is minimized among all rotations. This ensures detection
   of all transpositions.
   See also init_maps().
*/

void
get_hash( int *val0, int *val1, int *orientation ) {
  int i, j;
  int min_map;
  int min_hash0, min_hash1;
  int out[8][2];

  /* Calculate the 8 different 64-bit hash values for the
     different rotations. */

  compute_line_patterns( board );

  for ( i = 0; i < 8; i++ )
    for ( j = 0; j < 2; j++ )
      out[i][j] = 0;
  for ( i = 0; i < 8; i++ ) {
    /* b1 -> b1 */      
    out[0][0] ^= line_hash[0][i][row_pattern[i]];
    out[0][1] ^= line_hash[1][i][row_pattern[i]];
    /* g1 -> b1 */
    out[1][0] ^= line_hash[0][i][flip8[row_pattern[i]]];
    out[1][1] ^= line_hash[1][i][flip8[row_pattern[i]]];
    /* g8 -> b1 */
    out[2][0] ^= line_hash[0][i][flip8[row_pattern[7 - i]]];
    out[2][1] ^= line_hash[1][i][flip8[row_pattern[7 - i]]];
    /* b8 -> b1 */
    out[3][0] ^= line_hash[0][i][row_pattern[7 - i]];
    out[3][1] ^= line_hash[1][i][row_pattern[7 - i]];
    /* a2 -> b1 */
    out[4][0] ^= line_hash[0][i][col_pattern[i]];
    out[4][1] ^= line_hash[1][i][col_pattern[i]];
    /* a7 -> b1 */
    out[5][0] ^= line_hash[0][i][flip8[col_pattern[i]]];
    out[5][1] ^= line_hash[1][i][flip8[col_pattern[i]]];
    /* h7 -> b1 */
    out[6][0] ^= line_hash[0][i][flip8[col_pattern[7 - i]]];
    out[6][1] ^= line_hash[1][i][flip8[col_pattern[7 - i]]];
    /* h2 -> b1 */
    out[7][0] ^= line_hash[0][i][col_pattern[7 - i]];
    out[7][1] ^= line_hash[1][i][col_pattern[7 - i]];
  }

  /* Find the rotation minimizing the hash index.
     If two hash indices are equal, map number is implicitly used
     as tie-breaker. */

  min_map = 0;
  min_hash0 = out[0][0];
  min_hash1 = out[0][1];
  for ( i = 1; i < 8; i++)
    if ( (out[i][0] < min_hash0) ||
	 ((out[i][0] == min_hash0) && (out[i][1] < min_hash1)) ) {
      min_map = i;
      min_hash0 = out[i][0];
      min_hash1 = out[i][1];
    }

  *val0 = abs( min_hash0 );
  *val1 = abs( min_hash1 );
  *orientation = min_map;
}


/*
   SET_ALLOCATION
   Changes the number of nodes for which memory is allocated.
*/

static void
set_allocation( int size ) {
  if ( node == NULL )
    node = (BookNode *) safe_malloc( size * sizeof( BookNode ) );
  else
    node = (BookNode *) safe_realloc( node, size * sizeof( BookNode ) );
  if ( node == NULL )
    fatal_error( "%s %d\n", BOOK_ALLOC_ERROR,
		 size * sizeof( BookNode ), size );
  node_table_size = size;
  if ( node_table_size > MAX_HASH_FILL * hash_table_size )
    rebuild_hash_table( node_table_size );
}


/*
   INCREASE_ALLOCATION
   Allocate more memory for the book tree.
*/

static void
increase_allocation( void ) {
  set_allocation( node_table_size + 50000 );
}


/*
   CREATE_BOOK_NODE
   Creates a new book node without any connections whatsoever
   to the rest of the tree.
*/   

static int
create_BookNode( int val1, int val2, unsigned short flags ) {
  int index;

  if ( book_node_count == node_table_size )
    increase_allocation();
  index = book_node_count;
  node[index].hash_val1 = val1;
  node[index].hash_val2 = val2;
  node[index].black_minimax_score = NO_SCORE;
  node[index].white_minimax_score = NO_SCORE;
  node[index].best_alternative_move = NO_MOVE;
  node[index].alternative_score = NO_SCORE;
  node[index].flags = flags;
  select_hash_slot( index );
  book_node_count++;

  return index;
}


/*
   INIT_BOOK_TREE
   Initializes the node tree by creating the root of the tree.
*/

static void
init_book_tree( void ) {
  book_node_count = 0;
  node = NULL;
}


/*
   PREPATE_TREE_TRAVERSAL
   Prepares all relevant data structures for a tree search
   or traversal.
*/

static void
prepare_tree_traversal( void ) {
  int side_to_move;

  toggle_experimental( 0 );
  game_init( NULL, &side_to_move );
  toggle_midgame_hash_usage( TRUE, TRUE );
  toggle_abort_check( FALSE );
  toggle_midgame_abort_check( FALSE );
}


/*
   CLEAR_NODE_DEPTH
   Changes the flags of a node so that the search depth
   bits are cleared.
*/

static void
clear_node_depth( int index ) {
  int depth;

  depth = node[index].flags >> DEPTH_SHIFT;
  node[index].flags ^= (depth << DEPTH_SHIFT);
}


/*
   GET_NODE_DEPTH
*/

static int
get_node_depth( int index ) {
  return node[index].flags >> DEPTH_SHIFT;
}


/*
   SET_NODE_DEPTH
   Marks a node as being searched to a certain depth.
*/

static void
set_node_depth( int index, int depth ) {
  node[index].flags |= (depth << DEPTH_SHIFT);
}


/*
   ADJUST_SCORE
   Tweak a score as to encourage early deviations.
*/

static int
adjust_score( int score, int side_to_move ) {
  int adjustment;
  int adjust_steps;

  adjust_steps = high_deviation_threshold - disks_played;
  if ( adjust_steps < 0 )
    adjustment = 0;
  else {
    if ( disks_played < low_deviation_threshold )
      adjust_steps = high_deviation_threshold - low_deviation_threshold;
    adjustment = floor( adjust_steps * deviation_bonus * 128.0 );
    if ( side_to_move == WHITESQ )
      adjustment = -adjustment;
  }

  return (score + adjustment);
}


/*
   DO_MINIMAX
   Calculates the minimax value of node INDEX.
*/   

static void
do_minimax( int index, int *black_score, int *white_score ) {
  int i;
  int child;
  int child_black_score, child_white_score;
  int side_to_move;
  int this_move, alternative_move;
  int alternative_move_found;
  int child_count;
  int best_black_child_val, best_white_child_val;
  int worst_black_child_val, worst_white_child_val;
  int slot, val1, val2, orientation;
  short best_black_score, best_white_score;

  /* If the node has been visited AND it is a midgame node, meaning
     that the minimax values are not to be tweaked, return the
     stored values. */

  if ( !(node[index].flags & NOT_TRAVERSED) ) {
    if ( !(node[index].flags & (WLD_SOLVED | FULL_SOLVED)) ) {
      *black_score = node[index].black_minimax_score;
      *white_score = node[index].white_minimax_score;
      return;
    }
  }

  /* Correct WLD solved nodes corresponding to draws to be represented
     as full solved and make sure full solved nodes are marked as
     WLD solved as well */

  if ( (node[index].flags & WLD_SOLVED) &&
       (node[index].black_minimax_score == 0) &&
       (node[index].white_minimax_score == 0) )
    node[index].flags |= FULL_SOLVED;

  if ( (node[index].flags & FULL_SOLVED) && !(node[index].flags & WLD_SOLVED) )
    node[index].flags |= WLD_SOLVED;

  /* Recursively minimax all children of the node */
  
  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  best_black_child_val = -99999;
  best_white_child_val = -99999;
  worst_black_child_val = 99999;
  worst_white_child_val = 99999;

  if ( node[index].alternative_score != NO_SCORE ) {
    best_black_score =
      adjust_score( node[index].alternative_score, side_to_move);
    best_white_score = best_black_score;
    best_black_child_val = worst_black_child_val = best_black_score;
    best_white_child_val = worst_white_child_val = best_white_score;
    alternative_move_found = FALSE;
    alternative_move = node[index].best_alternative_move;
    if ( alternative_move > 0 ) {
      get_hash( &val1, &val2, &orientation );
      alternative_move = inv_symmetry_map[orientation][alternative_move];
    }
  }
  else {
    alternative_move_found = TRUE;
    alternative_move = 0;
    if ( side_to_move == BLACKSQ ) {
      best_black_score = -INFINITE_WIN;
      best_white_score = -INFINITE_WIN;
    }
    else {
      best_black_score = +INFINITE_WIN;
      best_white_score = +INFINITE_WIN;
    }
  }

  generate_all( side_to_move );
  child_count = 0;

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
    piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT ) {
      do_minimax( child, &child_black_score, &child_white_score );

      best_black_child_val = MAX( best_black_child_val, child_black_score );
      best_white_child_val = MAX( best_white_child_val, child_white_score );
      worst_black_child_val = MIN( worst_black_child_val, child_black_score );
      worst_white_child_val = MIN( worst_white_child_val, child_white_score );

      if ( side_to_move == BLACKSQ ) {
	best_black_score = MAX( child_black_score, best_black_score );
	best_white_score = MAX( child_white_score, best_white_score );
      }
      else {
	best_black_score = MIN( child_black_score, best_black_score );
	best_white_score = MIN( child_white_score, best_white_score );
      }
      child_count++;
    }
    else if ( !alternative_move_found && (this_move == alternative_move) )
      alternative_move_found = TRUE;
    unmake_move( side_to_move, this_move );
  }
  if ( !alternative_move_found ) {
    /* The was-to-be deviation now leads to a position in the database,
       hence it can no longer be used. */
    node[index].alternative_score = NO_SCORE;
    node[index].best_alternative_move = NO_MOVE;
  }

  /* Try to infer the WLD status from the children */

  if ( !(node[index].flags & (FULL_SOLVED | WLD_SOLVED)) &&
       (child_count > 0) ) {
    if ( side_to_move == BLACKSQ ) {
      if ( (best_black_child_val >= CONFIRMED_WIN) &&
	   (best_white_child_val >= CONFIRMED_WIN) ) {  /* Black win */
	node[index].black_minimax_score = node[index].white_minimax_score =
	  MIN( best_black_child_val, best_white_child_val );
	node[index].flags |= WLD_SOLVED;
      }
      else if ( (best_black_child_val <= -CONFIRMED_WIN) &&
	       (best_white_child_val <= -CONFIRMED_WIN)) {  /* Black loss */
	node[index].black_minimax_score = node[index].white_minimax_score =
	  MAX( best_black_child_val, best_white_child_val );
	node[index].flags |= WLD_SOLVED;
      }
    }
    else {
      if ((worst_black_child_val <= -CONFIRMED_WIN) &&
	  (worst_white_child_val <= -CONFIRMED_WIN)) {  /* White win */
	node[index].black_minimax_score = node[index].white_minimax_score =
	  MAX( worst_black_child_val, worst_white_child_val );
	node[index].flags |= WLD_SOLVED;
      }
      else if ((worst_black_child_val >= CONFIRMED_WIN) &&
	       (worst_white_child_val >= CONFIRMED_WIN) ) {  /* White loss */
	node[index].black_minimax_score = node[index].white_minimax_score =
	  MIN( worst_black_child_val, worst_white_child_val );
	node[index].flags |= WLD_SOLVED;
      }
    }
  }

  /* Tweak the minimax scores for draws to give the right
     draw avoidance behavior */

  if ( node[index].flags & (FULL_SOLVED | WLD_SOLVED) ) {
    *black_score = node[index].black_minimax_score;
    *white_score = node[index].white_minimax_score;
    if ( (node[index].black_minimax_score == 0) &&
	 (node[index].white_minimax_score == 0) )

    /* Is it a position in which a draw should be avoided? */

      if ( (game_mode == PRIVATE_GAME) || !(node[index].flags & PRIVATE_NODE) )
	switch ( draw_mode ) {
	case NEUTRAL:
	  break;

	case BLACK_WINS:
	  *black_score = +UNWANTED_DRAW;
	  *white_score = +UNWANTED_DRAW;
	  break;

	case WHITE_WINS:
	  *black_score = -UNWANTED_DRAW;
	  *white_score = -UNWANTED_DRAW;
	  break;

	case OPPONENT_WINS:
	  *black_score = -UNWANTED_DRAW;
	  *white_score = +UNWANTED_DRAW;
	  break;
	}
  }
  else {
    *black_score = node[index].black_minimax_score = best_black_score;
    *white_score = node[index].white_minimax_score = best_white_score;
  }

  node[index].flags ^= NOT_TRAVERSED;
}



/*
   MINIMAX_TREE
   Calculates the minimax values of all nodes in the tree.
*/

void
minimax_tree( void ) {
  int i;
  int dummy_black_score, dummy_white_score;
  time_t start_time, stop_time;

#ifdef TEXT_BASED
  printf( "Calculating minimax value... " );
  fflush( stdout );
#endif
  prepare_tree_traversal();
  time( &start_time );

  /* Mark all nodes as not traversed */
   
  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;

  do_minimax( ROOT, &dummy_black_score, &dummy_white_score );

  time( &stop_time );
#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts("");
#endif
}


#ifdef INCLUDE_BOOKTOOL

/*
  EXPORT_POSITION
  Output the position and its value according to the database
  to file.
*/

static void
export_position( int side_to_move, int score, FILE* target_file ) {
  int i, j, pos;
  int black_mask, white_mask;
  int hi_mask, lo_mask;

  for ( i = 1; i <= 8; i++ ) {
    black_mask = 0;
    white_mask = 0;
    for ( j = 0, pos = 10 * i + 1; j < 8; j++, pos++ )
      if ( board[pos] == BLACKSQ )
	black_mask |= (1 << j);
      else if ( board[pos] == WHITESQ )
	white_mask |= (1 << j);
    hi_mask = black_mask >> 4;
    lo_mask = black_mask % 16;
    fprintf( target_file, "%c%c", hi_mask + ' ', lo_mask + ' ' );
    hi_mask = white_mask >> 4;
    lo_mask = white_mask % 16;
    fprintf( target_file, "%c%c", hi_mask + ' ', lo_mask + ' ' );
  }
  fprintf( target_file, " " );
  if ( side_to_move == BLACKSQ )
    fputc( '*', target_file );
  else
    fputc( 'O', target_file );
  fprintf( target_file, " %2d %+d\n", disks_played, score );
}



/*
   DO_RESTRICTED_MINIMAX
   Calculates the book-only minimax value of node INDEX,
   not caring about deviations from the database.
*/   

static void
do_restricted_minimax( int index, int low, int high, FILE *target_file,
		       int *minimax_values ) {
  int i;
  int child, corrected_score;
  int side_to_move;
  int this_move;
  int child_count;
  int slot, val1, val2, orientation;
  short best_score;
   
  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  /* Recursively minimax all children of the node */

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  if ( side_to_move == BLACKSQ )
    best_score = -INFINITE_WIN;
  else
    best_score = +INFINITE_WIN;

  generate_all( side_to_move );
  child_count = 0;
  for ( i = 0; i < move_count[disks_played]; i++ ) {
    piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
    piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT ) {
      do_restricted_minimax( child, low, high, target_file, minimax_values );
      corrected_score = minimax_values[child];
      if ( ((side_to_move == BLACKSQ) && (corrected_score > best_score)) ||
	   ((side_to_move == WHITESQ) && (corrected_score < best_score)) )
	best_score = corrected_score;
      child_count++;
    }
    unmake_move( side_to_move, this_move );
  }

  if ( (node[index].flags & FULL_SOLVED) ||
       ((node[index].flags & WLD_SOLVED) && child_count == 0) )
    best_score = node[index].black_minimax_score;
  else if ( child_count == 0 ) {
#ifdef TEXT_BASED
    printf( "%d disks played\n", disks_played );
    printf( "Node #%d has no children and lacks WLD status\n", index );
#endif
    exit( EXIT_FAILURE );
  }

  if ( best_score > CONFIRMED_WIN )
    best_score -= CONFIRMED_WIN;
  else if ( best_score < -CONFIRMED_WIN )
    best_score += CONFIRMED_WIN;

  minimax_values[index] = best_score;
  node[index].flags ^= NOT_TRAVERSED;

  if ( (disks_played >= low) && (disks_played <= high) )
    export_position( side_to_move, best_score, target_file );
}



/*
   RESTRICTED_MINIMAX_TREE
   Calculates the minimax values of all nodes in the tree,
   not 
*/

void
restricted_minimax_tree( int low, int high, const char *pos_file_name ) {
  FILE *pos_file;
  int i;
  int *minimax_values;
  time_t start_time, stop_time;

#ifdef TEXT_BASED
  printf( "Calculating restricted minimax value... " );
  fflush( stdout );
#endif
  prepare_tree_traversal();
  time( &start_time );

  /* Mark all nodes as not traversed */
   
  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;

  minimax_values = (int *) safe_malloc( book_node_count * sizeof( int ) );
  pos_file = fopen( pos_file_name, "a" );

  do_restricted_minimax( ROOT, low, high, pos_file, minimax_values );

  time( &stop_time );
#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif

  free( minimax_values );
  fclose( pos_file );
}


/*
   DO_MIDGAME_STATISTICS
   Recursively makes sure a subtree is evaluated to the specified depth.
*/

static void
do_midgame_statistics( int index, StatisticsSpec spec ) {
  EvaluationType dummy_info;
  int i;
  int depth;
  int child;
  int side_to_move;
  int this_move;
  int slot, val1, val2, orientation;
  int eval_list[64];
  FILE *out_file;

  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  generate_all( side_to_move );

  /* With a certain probability, search the position to a variety
     of different depths in order to determine correlations. */

  if ( ((my_random() % 1000) < 1000.0 * spec.prob) &&
       (abs( node[index].black_minimax_score ) < spec.max_diff) ) {
    display_board( stdout, board, BLACKSQ, FALSE, FALSE, FALSE );
    setup_hash( FALSE );
    determine_hash_values( side_to_move, board );
    for ( depth = 1; depth <= spec.max_depth; depth += 2 ) {
      (void) middle_game( side_to_move, depth, FALSE, &dummy_info );
      eval_list[depth] = root_eval;
#ifdef TEXT_BASED
      printf( "%2d: %-5d ", depth, eval_list[depth] );
#endif
    }
#ifdef TEXT_BASED
    puts( "" );
#endif
    setup_hash( FALSE );
    determine_hash_values( side_to_move, board );
    for ( depth = 2; depth <= spec.max_depth; depth += 2 ) {
      (void) middle_game( side_to_move, depth, FALSE, &dummy_info );
      eval_list[depth] = root_eval;
#ifdef TEXT_BASED
      printf( "%2d: %-5d ", depth, eval_list[depth] );
#endif
    }
#ifdef TEXT_BASED
    puts( "" );
#endif

    /* Store the scores if the last eval is in the range [-20,20] */

    out_file = fopen( spec.out_file_name, "a" );
    if ( (out_file != NULL) &&
	 (abs( eval_list[spec.max_depth] ) <= 20 * 128) ) {
      get_hash( &val1, &val2, &orientation );
      fprintf( out_file, "%08x%08x %2d ", val1, val2, disks_played );
      fprintf( out_file, "%2d %2d ", 1, spec.max_depth );
      for ( i = 1; i <= spec.max_depth; i++ )
	fprintf( out_file, "%5d ", eval_list[i] );
      fprintf( out_file, "\n" );
      fclose( out_file );
    }
  }

  /* Recursively search the children of the node */

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT )
      do_midgame_statistics( child, spec );
    unmake_move( side_to_move, this_move );
  }
  node[index].flags ^= NOT_TRAVERSED;
}


/*
   GENERATE_MIDGAME_STATISTICS
   Calculates the minimax values of all nodes in the tree.
*/

void
generate_midgame_statistics( int max_depth, double probability,
			     int max_diff, const char *statistics_file_name ) {
  int i;
  time_t start_time, stop_time;
  StatisticsSpec spec;

#ifdef TEXT_BASED
  puts( "Generating statistics...\n" );
#endif
  prepare_tree_traversal();
  toggle_abort_check( FALSE );
  time( &start_time );
  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;
  spec.prob = probability;
  spec.max_diff = max_diff;
  spec.max_depth = max_depth;
  spec.out_file_name = statistics_file_name;
  my_srandom( start_time );
  do_midgame_statistics( 0, spec );
  time( &stop_time );
#ifdef TEXT_BASED
  printf( "\nDone (took %d s)\n", (int) (stop_time - start_time) );
  puts( "") ;
#endif
}


/*
   ENDGAME_CORRELATION
   Compare the scores produced by shallow searches to the
   exact score in an endgame position.
*/

static void
endgame_correlation( int side_to_move, int best_score, int best_move,
		     int min_disks, int max_disks, StatisticsSpec spec ) {
  EvaluationType dummy_info;
  FILE *out_file;
  int i;
  int depth;
  int stored_side_to_move;
  int val1, val2, orientation;
  int eval_list[64];

  display_board( stdout, board, BLACKSQ, FALSE, FALSE, FALSE );
  set_hash_transformation( abs( my_random() ), abs( my_random() ) );
  determine_hash_values( side_to_move, board );
  for ( depth = 1; depth <= spec.max_depth; depth++ ) {
    (void) middle_game( side_to_move, depth, FALSE, &dummy_info );
    eval_list[depth] = root_eval;
#ifdef TEXT_BASED
    printf( "%2d: %-6.2f ", depth, eval_list[depth] / 128.0 );
#endif
  }
  out_file = fopen( spec.out_file_name, "a" );
  if ( out_file != NULL ) {
    get_hash( &val1, &val2, &orientation );
    fprintf( out_file, "%08x%08x %2d ", val1, val2, disks_played );
    fprintf( out_file, "%+3d ", best_score );
    fprintf( out_file, "%2d %2d ", 1, spec.max_depth );
    for ( i = 1; i <= spec.max_depth; i++ )
      fprintf( out_file, "%5d ", eval_list[i] );
    fprintf( out_file, "\n" );
    fclose( out_file );
  }

  if ( disks_played < max_disks ) {
    (void) make_move( side_to_move, best_move, TRUE );
    stored_side_to_move = side_to_move;
    side_to_move = OPP( side_to_move );
    generate_all( side_to_move );
    if ( move_count[disks_played] > 0 ) {
#ifdef TEXT_BASED
      printf( "\nSolving with %d empty...\n\n", 60 - disks_played );
#endif
      fill_move_alternatives( side_to_move, FULL_SOLVED );
      if ( (get_candidate_count() > 0) || (disks_played >= 40) ) {
	print_move_alternatives( side_to_move );
	set_hash_transformation( 0, 0 );
	(void) end_game( side_to_move, FALSE, TRUE, TRUE, 0, &dummy_info );
	endgame_correlation( side_to_move, root_eval, pv[0][0],
			     min_disks, max_disks, spec );
      }
    }
    unmake_move( stored_side_to_move, best_move );
  }
}


/*
   DO_ENDGAME_STATISTICS
   Recursively makes sure a subtree is evaluated to
   the specified depth.
*/

static void
do_endgame_statistics( int index, StatisticsSpec spec ) {
  EvaluationType dummy_info;
  int i;
  int child;
  int side_to_move;
  int this_move;
  int slot, val1, val2, orientation;

  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  generate_all( side_to_move );

  /* With a certain probability, search the position to a variety
     of different depths in order to determine correlations. */

  if ( (disks_played == 33) &&
       (my_random() % 1000) < 1000.0 * spec.prob) {
    setup_hash( FALSE );
    determine_hash_values( side_to_move, board );
#ifdef TEXT_BASED
    printf( "\nSolving with %d empty...\n\n", 60 - disks_played );
#endif
    fill_move_alternatives( side_to_move, FULL_SOLVED );
    if ( (get_candidate_count() > 0) || (disks_played >= 40) ) {
      print_move_alternatives( side_to_move );
      set_hash_transformation( 0, 0 );
      (void) end_game( side_to_move, FALSE, TRUE, TRUE, 0, &dummy_info );
      if ( abs( root_eval ) <= spec.max_diff )
	endgame_correlation( side_to_move, root_eval, pv[0][0],
			     disks_played, 48, spec );
    }
  }

  /* Recursively search the children of the node */

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT )
      do_endgame_statistics( child, spec );
    unmake_move( side_to_move, this_move );
  }
  node[index].flags ^= NOT_TRAVERSED;
}


/*
   GENERATE_ENDGAME_STATISTICS
   Calculates the minimax values of all nodes in the tree.
*/

void
generate_endgame_statistics( int max_depth, double probability,
			     int max_diff, const char *statistics_file_name ) {
  int i;
  time_t start_time, stop_time;
  StatisticsSpec spec;

#ifdef TEXT_BASED
  puts( "Generating endgame statistics..." );
#endif
  prepare_tree_traversal();
  toggle_abort_check( FALSE );
  time( &start_time );
  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;
  spec.prob = probability;
  spec.max_diff = max_diff;
  spec.max_depth = max_depth;
  spec.out_file_name = statistics_file_name;
  my_srandom( start_time );
  do_endgame_statistics( ROOT, spec );
  time( &stop_time );
#ifdef TEXT_BASED
  printf( "\nDone (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}



#endif



/*
  NEGA_SCOUT
  This wrapper on top of TREE_SEARCH is used by EVALUATE_NODE
  to search the possible deviations.
*/

static void
nega_scout( int depth, int allow_mpc, int side_to_move,
	    int allowed_count, int *allowed_moves,
	    int alpha, int beta, int *best_score, int *best_index ) {
  int i, j;
  int curr_alpha;
  int curr_depth;
  int low_score, high_score;
  int best_move;
  int current_score;

  reset_counter( &nodes );
  low_score = -INFINITE_EVAL;

  /* To avoid spurious hash table entries to take out the effect
     of the averaging done, the hash table drafts are changed prior
     to each node being searched. */

  clear_hash_drafts();
  determine_hash_values( side_to_move, board );

  /* First determine the best move in the current position
     and its score when searched to depth DEPTH.
     This is done using standard negascout with iterative deepening. */

  for ( curr_depth = 2 - (depth % 2); curr_depth <= depth; curr_depth += 2 ) {
    low_score = -INFINITE_EVAL;
    curr_alpha = -INFINITE_EVAL;
    for ( i = 0; i < allowed_count; i++ ) {
      (void) make_move( side_to_move, allowed_moves[i], TRUE );
      piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
      piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );
      last_panic_check = 0.0;
      if ( i == 0 ) {
	low_score = current_score =
	  -tree_search( 1, curr_depth, OPP( side_to_move ), -INFINITE_EVAL,
			+INFINITE_EVAL, TRUE, allow_mpc, TRUE );
	*best_index = i;
      }
      else {
	curr_alpha = MAX( low_score, curr_alpha );
	current_score =
	  -tree_search( 1, curr_depth, OPP( side_to_move ), -(curr_alpha + 1),
			-curr_alpha, TRUE, allow_mpc, TRUE );
	if ( current_score > curr_alpha ) {
	  current_score =
	    -tree_search( 1, curr_depth, OPP( side_to_move ), -INFINITE_EVAL,
			  INFINITE_EVAL, TRUE, allow_mpc, TRUE );
	  if ( current_score > low_score ) {
	    low_score = current_score;
	    *best_index = i;
	  }
	}
	else if ( current_score > low_score ) {
	  low_score = current_score;
	  *best_index = i;
	}
      }
      unmake_move( side_to_move, allowed_moves[i] );
    }

    /* Float the best move so far to the top of the list */

    best_move = allowed_moves[*best_index];
    for ( j = *best_index; j >= 1; j-- )
      allowed_moves[j] = allowed_moves[j - 1];
    allowed_moves[0] = best_move;
    *best_index = 0;
  }

  /* Then find the score for the best move when searched
     to depth DEPTH+1 */

  (void) make_move( side_to_move, allowed_moves[*best_index], TRUE );
  piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
  piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );
  last_panic_check = 0.0;
  high_score = -tree_search( 1, depth + 1, OPP( side_to_move ),
			     -INFINITE_EVAL, INFINITE_EVAL, TRUE, allow_mpc, TRUE );
  unmake_move( side_to_move, allowed_moves[*best_index] );

  /* To remove the oscillations between odd and even search depths
     the score for the deviation is the average between the two scores. */

  *best_score = (low_score + high_score) / 2;
}



/*
   EVALUATE_NODE
   Applies a search to a predetermined depth to find the best
   alternative move in a position.
   Note: This function assumes that generate_all() has been
         called prior to it being called.
*/

static void
evaluate_node( int index ) {
  int i;
  int side_to_move;
  int alternative_move_count;
  int this_move, best_move;
  int child;
  int allow_mpc;
  int depth;
  int best_index;
  int slot, val1, val2, orientation;
  int feasible_move[64];
  int best_score;

  /* Don't evaluate nodes that already have been searched deep enough */

  depth = get_node_depth( index );
  if ( (depth >= search_depth) &&
       (node[index].alternative_score != NO_SCORE ) )
    return;

  /* If the node has been evaluated and its score is outside the
     eval and minimax windows, bail out. */

  if ( node[index].alternative_score != NO_SCORE ) {
    if ( (abs( node[index].alternative_score ) < min_eval_span) ||
	 (abs( node[index].alternative_score ) > max_eval_span) )
      return;

    if ( (abs( node[index].black_minimax_score ) < min_negamax_span) ||
	 (abs( node[index].black_minimax_score ) > max_negamax_span) )
      return;
  }

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  remove_coeffs( disks_played - STAGE_WINDOW );

  clear_panic_abort();
  piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
  piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );

  /* Find the moves which haven't been tried from this position */

  alternative_move_count = 0;
  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table(val1, val2);
    child = book_hash_table[slot];
    if ( child == EMPTY_HASH_SLOT )
      feasible_move[alternative_move_count++] = this_move;
    unmake_move( side_to_move, this_move );
  }

  if ( alternative_move_count == 0 ) {  /* There weren't any such moves */
    exhausted_node_count++;
    node[index].best_alternative_move = POSITION_EXHAUSTED;
    node[index].alternative_score = NO_SCORE;
  }
  else {  /* Find the best of those moves */
    allow_mpc = (search_depth >= MIN_MPC_DEPTH);
    nega_scout( search_depth, allow_mpc, side_to_move, alternative_move_count,
		feasible_move, -INFINITE_EVAL, INFINITE_EVAL, &best_score, &best_index );
    best_move = feasible_move[best_index];

    evaluated_count++;
    if ( side_to_move == BLACKSQ )
      node[index].alternative_score = best_score;
    else
      node[index].alternative_score = -best_score;
    get_hash( &val1, &val2, &orientation );
    node[index].best_alternative_move = symmetry_map[orientation][best_move];
  }
  clear_node_depth( index );
  set_node_depth( index, search_depth );
}



/*
   DO_EVALUATE
   Recursively makes sure a subtree is evaluated to
   the specified depth.
*/

static void
do_evaluate( int index ) {
  int i;
  int child;
  int side_to_move;
  int this_move;
  int slot, val1, val2, orientation;

  if ( evaluated_count >= max_eval_count )
    return;

  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  generate_all( side_to_move );

  if ( !(node[index].flags & (FULL_SOLVED | WLD_SOLVED)) )
    evaluate_node( index );
  if ( evaluated_count >= (evaluation_stage + 1) * max_eval_count / 25 ) {
    evaluation_stage++;
#ifdef TEXT_BASED
    putc( '|', stdout );
    if ( evaluation_stage % 5 == 0 )
      printf( " %d%% ", 4 * evaluation_stage );
    fflush( stdout );
#endif
  }

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT )
      do_evaluate( child );
    unmake_move( side_to_move, this_move );
  }
  node[index].flags ^= NOT_TRAVERSED;
}


/*
   EVALUATE_TREE
   Finds the most promising deviations from all nodes in the tree.
*/

void
evaluate_tree( void ) {
  int i;
  int feasible_count;
  time_t start_time, stop_time;

  prepare_tree_traversal();
  exhausted_node_count = 0;
  evaluated_count = 0;
  evaluation_stage = 0;
  time( &start_time );
  feasible_count = 0;
  for ( i = 0; i < book_node_count; i++ ) {
    node[i].flags |= NOT_TRAVERSED;
    if ( ((node[i].alternative_score == NO_SCORE) ||
	  (((get_node_depth(i) < search_depth) &&
	    (abs(node[i].alternative_score) >= min_eval_span) &&
	   (abs(node[i].alternative_score) <= max_eval_span) &&
	    (abs(node[i].black_minimax_score) >= min_negamax_span) &&
	    (abs(node[i].black_minimax_score) <= max_negamax_span)))) &&
	 !(node[i].flags & (WLD_SOLVED | FULL_SOLVED)) )
      feasible_count++;
  }
  max_eval_count = MIN( feasible_count, max_batch_size );
#ifdef TEXT_BASED
  printf( "Evaluating to depth %d. ", search_depth );
  if ( (min_eval_span > 0) || (max_eval_span < INFINITE_SPREAD) )
    printf( "Eval interval is [%.2f,%.2f]. ",
	    min_eval_span / 128.0, max_eval_span / 128.0 );
  if ( (min_negamax_span > 0) || (max_negamax_span < INFINITE_SPREAD) )
    printf( "Negamax interval is [%.2f,%.2f]. ",
	    min_negamax_span / 128.0, max_negamax_span / 128.0 );
  if ( max_eval_count == feasible_count )
    printf( "\n%d relevant nodes.", feasible_count );
  else
    printf( "\nMax batch size is %d.", max_batch_size );
  puts( "" );
  printf( "Progress: " );
  fflush( stdout );
#endif
  if ( feasible_count > 0 )
    do_evaluate( ROOT );
  time( &stop_time );
#ifdef TEXT_BASED
  printf( "(took %d s)\n", (int) (stop_time - start_time) );
  printf( "%d nodes evaluated ", evaluated_count );
  printf( "(%d exhausted nodes ignored)\n", exhausted_node_count );
  puts( "" );
#endif
}



/*
   DO_VALIDATE
   Recursively makes sure a subtree doesn't contain any midgame
   node without a deviation move.
*/

static void
do_validate( int index ) {
  int i;
  int child;
  int side_to_move;
  int this_move;
  int slot, val1, val2, orientation;

  if ( evaluated_count >= max_eval_count )
    return;

  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  generate_all( side_to_move );

  if ( !(node[index].flags & (FULL_SOLVED | WLD_SOLVED)) &&
       (node[index].alternative_score == NO_SCORE) &&
       (node[index].best_alternative_move != POSITION_EXHAUSTED) )
    evaluate_node( index );

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT )
      do_validate( child );
    unmake_move( side_to_move, this_move );
  }
  node[index].flags ^= NOT_TRAVERSED;
}



/*
  VALIDATE_TREE
  Makes sure all nodes are either exhausted, solved or have a deviation.
  The number of positions evaluated is returned.
*/

int
validate_tree( void ) {
  int i;
  int feasible_count;

  prepare_tree_traversal();
  exhausted_node_count = 0;
  evaluated_count = 0;
  evaluation_stage = 0;
  feasible_count = 0;
  for ( i = 0; i < book_node_count; i++ )
    if ( !(node[i].flags & (WLD_SOLVED | FULL_SOLVED)) &&
	 (node[i].alternative_score == NO_SCORE) &&
	 (node[i].best_alternative_move != POSITION_EXHAUSTED) )
      feasible_count++;
  max_eval_count = MIN( feasible_count, max_batch_size );
  if ( feasible_count > 0 ) {
    for ( i = 0; i < book_node_count; i++ )
      node[i].flags |= NOT_TRAVERSED;
    do_validate( ROOT );
  }

  return evaluated_count;
}


#ifdef INCLUDE_BOOKTOOL


/*
   DO_CLEAR
   Clears depth and flag information for all nodes with >= LOW
   and <= HIGH discs played. FLAGS specifies what kind of information
   is to be cleared - midgame, WLD or exact.
*/

static void
do_clear( int index, int low, int high, int flags ) {
  int i;
  int child;
  int side_to_move;
  int this_move;
  int slot, val1, val2, orientation;

  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  if ( (disks_played >= low) && (disks_played <= high) ) {
    if ( flags & CLEAR_MIDGAME )
      clear_node_depth( index );
    
    if ( (node[index].flags & WLD_SOLVED) && (flags & CLEAR_WLD) )
      node[index].flags ^= WLD_SOLVED;
    
    if ( (node[index].flags & FULL_SOLVED) && (flags & CLEAR_EXACT) )
      node[index].flags ^= FULL_SOLVED;
  }

  if ( disks_played <= high ) {
    if ( node[index].flags & BLACK_TO_MOVE )
      side_to_move = BLACKSQ;
    else
      side_to_move = WHITESQ;
    
    generate_all( side_to_move );
    
    for ( i = 0; i < move_count[disks_played]; i++ ) {
      this_move = move_list[disks_played][i];
      (void) make_move( side_to_move, this_move, TRUE );
      get_hash(&val1, &val2, &orientation);
      slot = probe_hash_table(val1, val2);
      child = book_hash_table[slot];
      if ( child != EMPTY_HASH_SLOT )
	do_clear( child, low, high, flags );
      unmake_move( side_to_move, this_move );
    }
  }
  node[index].flags ^= NOT_TRAVERSED;
}


/*
   CLEAR_TREE
   Resets the labels on nodes satisfying certain conditions.
*/

void
clear_tree( int low, int high, int flags ) {
  int i;
  time_t start_time, stop_time;

  prepare_tree_traversal();

#ifdef TEXT_BASED
  printf( "Clearing from %d moves to %d modes: ", low, high );
  if ( flags & CLEAR_MIDGAME )
    printf( "midgame " );
  if ( flags & CLEAR_WLD )
    printf( "wld " );
  if ( flags & CLEAR_EXACT )
    printf( "exact " );
  puts( "" );
#endif
  time( &start_time );
  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;
  do_clear( ROOT, low, high, flags );
  time( &stop_time );
#ifdef TEXT_BASED
  printf( "(took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}



/*
   DO_CORRECT
   Performs endgame correction (WLD or full solve) of a node
   and (recursively) the subtree below it.
*/

static void
do_correct( int index, int max_empty, int full_solve,
	    const char *target_name, char *move_hist ) {
  EvaluationType dummy_info;
  int i, j, pos;
  int child;
  int side_to_move;
  int this_move;
  int outcome;
  int really_evaluate;
  int slot, val1, val2, orientation;
  int child_count;
  int child_move[64];
  int child_node[64];

  if ( evaluated_count >= max_eval_count )
    return;

  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  /* First correct the children */

  generate_all( side_to_move );
  child_count = 0;
  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT ) {
      child_move[child_count] = this_move;
      child_node[child_count] = child;
      child_count++;
    }
    unmake_move( side_to_move, this_move );
  }

  for ( i = 0; i < child_count; i++ ) {
    if ( side_to_move == BLACKSQ ) {
      if ( force_black &&
	   (node[child_node[i]].black_minimax_score !=
	    node[index].black_minimax_score) )
	continue;
    }
    else {
      if ( force_white &&
	   (node[child_node[i]].white_minimax_score !=
	    node[index].white_minimax_score) )
	continue;
    }
    this_move = child_move[i];
    sprintf( move_hist + 2 * disks_played, "%c%c", TO_SQUARE( this_move ) );
    make_move( side_to_move, this_move, TRUE );
    do_correct( child_node[i], max_empty, full_solve, target_name, move_hist );
    unmake_move( side_to_move, this_move );
    *(move_hist + 2 * disks_played) = '\0';
  }

  /* Then correct the node itself (hopefully exploiting lots
     of useful information in the hash table) */

  generate_all( side_to_move );
  determine_hash_values( side_to_move, board );

  if ( disks_played >= 60 - max_empty ) {
    really_evaluate =
      (full_solve && !(node[index].flags & FULL_SOLVED)) ||
      (!full_solve && !(node[index].flags & (WLD_SOLVED | FULL_SOLVED)));
      
    if ( (abs( node[index].alternative_score ) < min_eval_span) ||
	 (abs( node[index].alternative_score ) > max_eval_span) )
      really_evaluate = FALSE;
    
    if ( (abs( node[index].black_minimax_score) < min_negamax_span) ||
	 (abs( node[index].black_minimax_score) > max_negamax_span) )
      really_evaluate = FALSE;

    if ( really_evaluate ) {
      if ( target_name == NULL ) {  /* Solve now */
	reset_counter( &nodes );

	(void) end_game( side_to_move, !full_solve, FALSE,
			 TRUE, 0, &dummy_info );

	if ( side_to_move == BLACKSQ )
	  outcome = +root_eval;
	else
	  outcome = -root_eval;

	node[index].black_minimax_score = node[index].white_minimax_score =
	  outcome;
	if ( outcome > 0 ) {
	  node[index].black_minimax_score += CONFIRMED_WIN;
	  node[index].white_minimax_score += CONFIRMED_WIN;
	}
	if ( outcome < 0 ) {
	  node[index].black_minimax_score -= CONFIRMED_WIN;
	  node[index].white_minimax_score -= CONFIRMED_WIN;
	}
	if ( full_solve )
	  node[index].flags |= FULL_SOLVED;
	else
	  node[index].flags |= WLD_SOLVED;
      }
      else {  /* Defer solving to a standalone scripted solver */
	FILE *target_file = fopen( target_name, "a" );

	if ( target_file != NULL ) {
	  fprintf( target_file, "%% %s\n", move_hist );
	  get_hash( &val1, &val2, &orientation );
	  fprintf( target_file, "%% %d %d\n", val1, val2 );

	  for ( i = 1; i <= 8; i++ )
	    for ( j = 1; j <= 8; j++ ) {
	      pos = 10 * i + j;
	      if ( board[pos] == BLACKSQ )
		putc( 'X', target_file );
	      else if ( board[pos] == WHITESQ )
		putc( 'O', target_file );
	      else
		putc( '-', target_file );
	    }
	  if ( side_to_move == BLACKSQ )
	    fputs( " X\n", target_file );
	  else
	    fputs( " O\n", target_file );
	  fputs( "%\n", target_file );
	  fclose( target_file );
	}
      }
      evaluated_count++;
    }
  }

  if ( evaluated_count >= (evaluation_stage + 1) * max_eval_count / 25 ) {
    evaluation_stage++;
#ifdef TEXT_BASED
    putc( '|', stdout );
    if ( evaluation_stage % 5 == 0 )
      printf( " %d%% ", 4 * evaluation_stage );
    fflush( stdout );
#endif
  }

  node[index].flags ^= NOT_TRAVERSED;
}



/*
  SET_OUTPUT_SCRIPT_NAME
  Makes SCRIPT_NAME the target for the positions generated by
  do_correct() (instead of the positions being solved, the normal
  mode of operation).
*/

void
set_output_script_name( const char *script_name ) {
  correction_script_name = script_name;
}



/*
   CORRECT_TREE
   Endgame-correct the lowest levels of the tree.
*/

void
correct_tree( int max_empty, int full_solve ) {
  char move_buffer[150];
  int i;
  int feasible_count;
  time_t start_time, stop_time;

  prepare_tree_traversal();
  exhausted_node_count = 0;
  evaluated_count = 0;
  evaluation_stage = 0;
  time( &start_time );
  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;
  feasible_count = 0;
  for ( i = 0; i < book_node_count; i++ ) {
    node[i].flags |= NOT_TRAVERSED;
    if ( (get_node_depth( i ) < max_empty) &&
	 (abs( node[i].alternative_score ) >= min_eval_span) &&
	 (abs( node[i].alternative_score ) <= max_eval_span) &&
	 (abs( node[i].black_minimax_score ) >= min_negamax_span) &&
	 (abs( node[i].black_minimax_score ) <= max_negamax_span) )
      feasible_count++;
  }
  max_eval_count = MIN( feasible_count, max_batch_size );
#ifdef TEXT_BASED
  printf( "Correcting <= %d empty ", max_empty );
  if ( full_solve )
    printf( "(full solve). " );
  else
    printf( "(WLD solve). " );
  if ( (min_eval_span > 0) || (max_eval_span < INFINITE_SPREAD) )
    printf( "Eval interval is [%.2f,%.2f]. ",
	    min_eval_span / 128.0, max_eval_span / 128.0 );
  if ( (min_negamax_span > 0) || (max_negamax_span < INFINITE_SPREAD) )
    printf( "Negamax interval is [%.2f,%.2f]. ",
	    min_negamax_span / 128.0, max_negamax_span / 128.0 );
  if ( max_eval_count == feasible_count )
    printf( "\n%d relevant nodes.", feasible_count );
  else
    printf( "\nMax batch size is %d.", max_batch_size );
  puts( "" );
  printf( "Progress: " );
  fflush( stdout );
#endif

  move_buffer[0] = '\0';
  do_correct( ROOT, max_empty, full_solve,
	      correction_script_name, move_buffer );

  time( &stop_time );
#ifdef TEXT_BASED
  printf( "(took %d s)\n", (int) (stop_time - start_time) );
  if ( correction_script_name == NULL )  /* Positions solved */
    printf( "%d nodes solved\n", evaluated_count );
  else
    printf( "%d nodes exported to %s\n", evaluated_count,
	    correction_script_name );
  puts( "" );
#endif
}


/*
   DO_EXPORT
   Recursively exports all variations rooted at book position # INDEX.
*/

static void
do_export( int index, FILE *stream, int *move_vec ) {
  int i;
  int child_count;
  int allow_branch;
  int side_to_move;

  allow_branch = node[index].flags & NOT_TRAVERSED;

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  generate_all( side_to_move );

  child_count = 0;

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    int child;
    int slot, val1, val2, orientation;
    int this_move = move_list[disks_played][i];

    move_vec[disks_played] = this_move;
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT ) {
      do_export( child, stream, move_vec );
      child_count++;
    }
    unmake_move( side_to_move, this_move );

    if ( (child_count == 1) && !allow_branch )
      break;
  }

  if ( child_count == 0 ) {  /* We've reached a leaf in the opening tree. */
    for ( i = 0; i < disks_played; i++ )
      fprintf( stream, "%c%c", TO_SQUARE( move_vec[i] ) );
    fprintf( stream, "\n" );
  }

  node[index].flags &= ~NOT_TRAVERSED;
}



/*
  EXPORT_TREE
  Exports a set of lines that cover the tree.
*/

void
export_tree( const char *file_name ) {
  int i;
  int move_vec[60];
  FILE *stream;

  stream = fopen( file_name, "w" );
  if ( stream == NULL ) {
#ifdef TEXT_BASED
    fprintf( stderr, "Cannot open %s for writing.\n", file_name );
#endif
    return;
  }

  prepare_tree_traversal();

  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;
  do_export( ROOT, stream, move_vec );

  fclose( stream );
}


#endif



#define  MAX_LEVEL      5

/*
  FILL_ENDGAME_HASH
  Recursively transfer information from all solved nodes in the
  book hash table to the game hash table.
*/

void
fill_endgame_hash( int cutoff, int level ) {
  int i;
  int this_index, child_index;
  int matching_move;
  int signed_score;
  int bound;
  int side_to_move, this_move;
  int is_full, is_wld;
  int slot, val1, val2, orientation;

  if ( level >= MAX_LEVEL )
    return;

  get_hash( &val1, &val2, &orientation );
  slot = probe_hash_table( val1, val2 );
  this_index = book_hash_table[slot];

  /* If the position wasn't found in the hash table, return. */
  
  if ( (slot == NOT_AVAILABLE) || (book_hash_table[slot] == EMPTY_HASH_SLOT) )
    return;

  /* Check the status of the node */

  is_full = node[this_index].flags & FULL_SOLVED;
  is_wld = node[this_index].flags & WLD_SOLVED;

  /* Match the status of the node with those of the children and
     recursively treat the entire subtree of the node */

  if ( node[book_hash_table[slot]].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  matching_move = NO_MOVE;

  generate_all( side_to_move );
  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child_index = book_hash_table[slot];
    if ( child_index != EMPTY_HASH_SLOT ) {
      if ( disks_played < 60 - cutoff )
	fill_endgame_hash( cutoff, level + 1 );
      if ( is_full ) {  /* Any child with matching exact score? */
	if ( (node[child_index].flags & FULL_SOLVED) &&
	     (node[child_index].black_minimax_score ==
	      node[this_index].black_minimax_score) )
	  matching_move = this_move;
      }
      else if ( is_wld ) {  /* Any child with matching WLD results? */
	if ( node[child_index].flags & (FULL_SOLVED | WLD_SOLVED) ) {
	  if ( side_to_move == BLACKSQ ) {
	    if ( node[child_index].black_minimax_score >=
		 node[this_index].black_minimax_score )
	      matching_move = this_move;
	  }
	  else {
	    if ( node[child_index].black_minimax_score <=
		 node[this_index].black_minimax_score )
	      matching_move = this_move;
	  }
	}
      }
    }
    unmake_move( side_to_move, this_move );
  }

  if ( matching_move != NO_MOVE ) {  /* Store the information */
    signed_score = node[this_index].black_minimax_score;
    if ( side_to_move == WHITESQ )
      signed_score = -signed_score;
    if ( signed_score > CONFIRMED_WIN )
      signed_score -= CONFIRMED_WIN;
    else if ( signed_score < -CONFIRMED_WIN )
      signed_score += CONFIRMED_WIN;
    else if ( abs( signed_score ) == UNWANTED_DRAW )
      signed_score = 0;
    if ( is_full )
      bound = EXACT_VALUE;
    else {
      if ( signed_score >= 0 )
	bound = LOWER_BOUND;
      else
	bound = UPPER_BOUND;
    }
    add_hash( ENDGAME_MODE, signed_score, matching_move,
	      ENDGAME_SCORE | bound, 60 - disks_played, 0 );
  }
}


/*
   DO_EXAMINE
   Add the properties of node INDEX to the statistics being gathered
   and recursively traverse the subtree of the node, doing the same
   thing in all nodes.
*/   

static void
do_examine( int index ) {
  int i;
  int child;
  int side_to_move;
  int this_move;
  int slot, val1, val2, orientation;
  int child_count;
  int child_move[64], child_node[64];
   
  if (!(node[index].flags & NOT_TRAVERSED))
    return;

  if (node[index].flags & FULL_SOLVED)
    exact_count[disks_played]++;
  else if (node[index].flags & WLD_SOLVED)
    wld_count[disks_played]++;
  else if (node[index].best_alternative_move == POSITION_EXHAUSTED)
    exhausted_count[disks_played]++;
  else
    common_count[disks_played]++;

  /* Examine all the children of the node */

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  generate_all( side_to_move );
  child_count = 0;
  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    child = book_hash_table[slot];
    if ( child != EMPTY_HASH_SLOT ) {
      child_move[child_count] = this_move;
      child_node[child_count] = child;
      child_count++;
    }
    unmake_move( side_to_move, this_move );
  }

  if ( child_count == 0 ) {
    leaf_count++;
    if ( !(node[index].flags & FULL_SOLVED) )
      bad_leaf_count++;
    if ( !(node[index].flags & WLD_SOLVED) )
      really_bad_leaf_count++;
  }
  else
    for ( i = 0; i < child_count; i++ ) {
      if ( side_to_move == BLACKSQ ) {
	if ( force_black &&
	     (node[child_node[i]].black_minimax_score !=
	      node[index].black_minimax_score) )
	  continue;
      }
      else {
	if ( force_white &&
	   (node[child_node[i]].white_minimax_score !=
	    node[index].white_minimax_score) )
	  continue;
      }
      this_move = child_move[i];
      make_move( side_to_move, this_move, TRUE );
      do_examine( child_node[i] );
      unmake_move( side_to_move, this_move );
    }

  node[index].flags ^= NOT_TRAVERSED;
}


/*
   EXAMINE_TREE
   Generates some statistics about the book tree.
*/

void
examine_tree( void ) {
  int i;
  time_t start_time, stop_time;

#ifdef TEXT_BASED
  printf( "Examining tree... " );
  fflush( stdout );
#endif
  prepare_tree_traversal();
  time( &start_time );

  for ( i = 0; i <= 60; i++ ) {
    exact_count[i] = 0;
    wld_count[i] = 0;
    exhausted_count[i] = 0;
    common_count[i] = 0;
  }
  unreachable_count = 0;
  leaf_count = 0;
  bad_leaf_count = 0;

  /* Mark all nodes as not traversed and examine the tree */
   
  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;
  do_examine( ROOT );

  /* Any nodes not reached by the walkthrough? */

  for ( i = 0; i < book_node_count; i++ )
    if ( node[i].flags & NOT_TRAVERSED ) {
      unreachable_count++;
      node[i].flags ^= NOT_TRAVERSED;
    }

  time( &stop_time );
#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}


#ifdef _WIN32_WCE
static int CE_CDECL
#else
static int
#endif
int_compare( const void *i1, const void *i2 ) {
  return (*((int *) i1)) - (*((int *) i2));
}


/*
   BOOK_STATISTICS
   Describe the status of the nodes in the tree.
*/

void
book_statistics( int full_statistics ) {
  double strata[11] = { 0.01, 0.02, 0.03, 0.05, 0.10,
			0.30, 0.50, 0.70, 0.90, 0.99, 1.01 };
  double eval_strata[10];
  double negamax_strata[10];
  int i;
  int full_solved, wld_solved, unevaluated;
  int eval_count, negamax_count;
  int private_count;
  int this_strata, strata_shift;
  int first, last;
  int *evals, *negamax;
  int depth[60];
  int total_count[61];

  evals = (int *) safe_malloc( book_node_count * sizeof( int ) );
  negamax = (int *) safe_malloc( book_node_count * sizeof( int ) );
  full_solved = wld_solved = 0;
  eval_count = 0;
  negamax_count = 0;
  private_count = 0;
  unevaluated = 0;
  for ( i = 0; i < 60; i++ )
    depth[i] = 0;
  for ( i = 0; i < book_node_count; i++ ) {
    if ( node[i].flags & FULL_SOLVED )
      full_solved++;
    else if ( node[i].flags & WLD_SOLVED )
      wld_solved++;
    else {
      depth[get_node_depth( i )]++;
      if ( (node[i].alternative_score == NO_SCORE) &&
	  (node[i].best_alternative_move == NO_MOVE) )
	unevaluated++;
      else {
	if ( node[i].alternative_score != NO_SCORE )
	  evals[eval_count++] = abs( node[i].alternative_score );
	negamax[negamax_count++] = abs( node[i].black_minimax_score );
      }
    }
    if ( node[i].flags & PRIVATE_NODE )
      private_count++;
  }
  qsort( evals, eval_count, sizeof(int), int_compare );
  qsort( negamax, negamax_count, sizeof(int), int_compare );
#ifdef TEXT_BASED
  puts( "" );
  printf( "#nodes:       %d", book_node_count );
  if ( private_count > 0 )
    printf("  (%d private)", private_count );
  puts( "" );
  printf( "#full solved: %d\n", full_solved );
  printf( "#WLD solved:  %d\n", wld_solved );
  printf( "#unevaluated: %d\n\n", unevaluated );
  for ( i = 0; i <= 59; i++ )
    if ( depth[i] > 0 )
      printf( "#nodes with %2d-ply deviations: %d\n", i, depth[i] );
  puts( "" );
#endif
  this_strata = 0;
  strata_shift = floor( strata[this_strata] * eval_count );
  for ( i = 0; i < eval_count; i++ )
    if ( i == strata_shift ) {
      eval_strata[this_strata] = evals[i] / 128.0;
      strata_shift = floor( strata[++this_strata] * eval_count );
    }
  this_strata = 0;
  strata_shift = floor( strata[this_strata] * negamax_count );
  for ( i = 0; i < negamax_count; i++ )
    if ( i == strata_shift) {
      negamax_strata[this_strata] = evals[i] / 128.0;
      strata_shift = floor( strata[++this_strata] * negamax_count );
    }
#ifdef TEXT_BASED
  for ( i = 0; i < 10; i++ ) {
    printf( "%2.0f%%:  ", 100 * strata[i] );
    printf( "%5.2f   ", eval_strata[i] );
    printf( "%5.2f   ", negamax_strata[i] );
    puts( "" );
  }
  puts( "" );
#endif
  free( negamax );
  free( evals );

  if ( full_statistics ) {
    examine_tree();
    first = 61;
    last = -1;
    for ( i = 0; i <= 60; i++ ) {
      total_count[i] = exact_count[i] + wld_count[i] +
	exhausted_count[i] + common_count[i];
      if ( total_count[i] > 0 ) {
	first = MIN( first, i );
	last = MAX( last, i );
      }
    }
#ifdef TEXT_BASED
    printf( "%d unreachable nodes\n\n", unreachable_count );
    printf( "%d leaf nodes; %d lack exact score and %d lack WLD status\n",
	   leaf_count, bad_leaf_count, really_bad_leaf_count );
    for ( i = first; i <= last; i++ ) {
      printf( "%2d moves", i );
      printf( "   " );
      printf( "%5d node", total_count[i] );
      if ( total_count[i] == 1 )
	printf( " :  " );
      else
	printf( "s:  " );

      if ( common_count[i] > 0 )
	printf( "%5d midgame", common_count[i] );
      else
	printf( "             " );
      printf( "  " );
      if ( wld_count[i] > 0 )
	printf( "%5d WLD", wld_count[i] );
      else
	printf( "         " );
      printf( "  " );
      if ( exact_count[i] > 0 )
	printf( "%5d exact", exact_count[i] );
      else
	printf( "           " );
      printf( "  " );
      if ( exhausted_count[i] > 0 )
	printf( "%2d exhausted", exhausted_count[i] );
      puts( "" );
    }
    puts( "" );
#endif
  }
}


/*
   DISPLAY_OPTIMAL_LINE
   Outputs the sequence of moves which is optimal according
   to both players.
*/

void
display_doubly_optimal_line( int original_side_to_move ) {
  int i;
  int done, show_move;
  int line;
  int root_score, child_score;
  int slot, val1, val2;
  int base_orientation, child_orientation;
  int side_to_move;
  int this_move;
  int current, child, next;

  prepare_tree_traversal();
#ifdef TEXT_BASED
  printf( "Root evaluation with Zebra playing " );
#endif
  if ( original_side_to_move == BLACKSQ ) {
    root_score = node[ROOT].black_minimax_score;
#ifdef TEXT_BASED
    printf( "black" );
#endif
  }
  else {
    root_score = node[ROOT].white_minimax_score;
#ifdef TEXT_BASED
    printf( "white" );
#endif
  }
#ifdef TEXT_BASED
  printf( ": %+.2f\n", root_score / 128.0 );
#endif
  current = ROOT;
#ifdef TEXT_BASED
  puts( "Preferred line: " );
#endif
  line = 0;
  done = FALSE;
  show_move = TRUE;
  while ( !(node[current].flags & (FULL_SOLVED | WLD_SOLVED)) && !done ) {
    if ( node[current].flags & BLACK_TO_MOVE )
      side_to_move = BLACKSQ;
    else
      side_to_move = WHITESQ;
    generate_all( side_to_move );
    next = -1;
    this_move = NO_MOVE;
    for ( i = 0; i < move_count[disks_played]; i++ ) {
      get_hash( &val1, &val2, &base_orientation );
      this_move = move_list[disks_played][i];
      (void) make_move( side_to_move, this_move, TRUE );
      get_hash( &val1, &val2, &child_orientation );
      slot = probe_hash_table( val1, val2 );
      child = book_hash_table[slot];
      if ( child != EMPTY_HASH_SLOT ) {
	if ( original_side_to_move == BLACKSQ )
	  child_score = node[child].black_minimax_score;
	else
	  child_score = node[child].white_minimax_score;
	if ( child_score == root_score )
	  next = child;
      }
      if ( (child != EMPTY_HASH_SLOT) && (next == child) )
	break;
      else
	unmake_move( side_to_move, this_move );
    }
    if ( next == -1 ) {
      done = TRUE;
      if ( adjust_score(node[current].alternative_score, side_to_move) !=
	  root_score ) {
#ifdef TEXT_BASED
	puts( "(failed to find continuation)" );
#endif
	show_move = FALSE;
      }
      else {
	this_move = node[current].best_alternative_move;
	this_move = inv_symmetry_map[base_orientation][this_move];
      }
    }
#ifdef TEXT_BASED
    if ( show_move ) {
      if ( side_to_move == BLACKSQ )
	printf( "%2d. ", ++line );
      printf( "%c%c  ", TO_SQUARE( this_move ) );
      if ( side_to_move == WHITESQ )
	puts( "" );
      if ( done )
	puts( "(deviation)" );
    }
#endif
    current = next;
  }
#ifdef TEXT_BASED
  puts( "" );
#endif
}


/*
  ADD_NEW_GAME
  Adds a new game to the game tree.
*/

void
add_new_game( int move_count, short *game_move_list,
	      int min_empties, int max_full_solve, int max_wld_solve,
	      int update_path, int private_game ) {
  EvaluationType dummy_info;
  int i, j;
  int stored_echo;
  int dummy_black_score, dummy_white_score;
  int force_eval, midgame_eval_done;
  int side_to_move, this_move;
  int slot, this_node;
  int last_move_number;
  int first_new_node;
  int val1, val2, orientation;
  int outcome;
  int visited_node[61];
  unsigned short flags[61];

  stored_echo = echo;
  echo = FALSE;
  toggle_event_status( FALSE );

  /* First create new nodes for new positions */

  prepare_tree_traversal();
  for ( i = 0; i < move_count; i++ )
    if ( game_move_list[i] > 0 )
      flags[i] = BLACK_TO_MOVE;
    else
      flags[i] = WHITE_TO_MOVE;
  flags[move_count] = 0;
  first_new_node = 61;

  this_node = ROOT;
  side_to_move = BLACKSQ;

  last_move_number = MIN( move_count, 60 - min_empties );

  for ( i = 0; i <= last_move_number; i++ ) {
    /* Look for the position in the hash table */

    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    if ( (slot == NOT_AVAILABLE) ||
	 (book_hash_table[slot] == EMPTY_HASH_SLOT) ) {
      this_node = create_BookNode( val1, val2, flags[i] );
      if ( private_game )
	node[this_node].flags |= PRIVATE_NODE;
      if ( i < first_new_node )
	first_new_node = i;
    }
    else
      this_node = book_hash_table[slot];
    visited_node[i] = this_node;

    /* Make the moves of the game until the cutoff point */

    if ( i < last_move_number ) {
      this_move = abs( game_move_list[i] );
      if ( game_move_list[i] > 0 )
	side_to_move = BLACKSQ;
      else
	side_to_move = WHITESQ;
      if ( !generate_specific( this_move, side_to_move ) ) {
#ifdef TEXT_BASED
	puts( "" );
	printf( "i=%d, side_to_move=%d, this_move=%d\n",
		i, side_to_move, this_move );
	printf( "last_move_number=%d, move_count=%d\n",
		last_move_number, move_count );
	for ( j = 0; j < move_count; j++ )
	  printf( "%3d ", game_move_list[j] );
#endif
	fatal_error( "%s: %d\n", BOOK_INVALID_MOVE, this_move );
      }
      (void) make_move( side_to_move, this_move, TRUE );
    }
    else  /* No more move to make, only update the player to move */
      side_to_move = OPP( side_to_move );
  }

  if ( last_move_number == move_count ) {  /* No cutoff applies */
    int black_count, white_count;

    black_count = disc_count( BLACKSQ );
    white_count = disc_count( WHITESQ );
    if ( black_count > white_count )
      outcome = 64 - 2 * white_count;
    else if ( white_count > black_count )
      outcome = 2 * black_count - 64;
    else
      outcome = 0;
  }
  else {
    generate_all( side_to_move );
    determine_hash_values( side_to_move, board );
#ifdef TEXT_BASED
    if ( echo ) {
      puts( "" );
      if ( side_to_move == BLACKSQ )
	printf( "Full solving with %d empty (black)\n", 60 - disks_played );
      else
	printf( "Full solving with %d empty (white)\n", 60 - disks_played );
    }
#endif
    (void) end_game( side_to_move, FALSE, FALSE, TRUE, 0, &dummy_info );
    outcome = root_eval;
    if ( side_to_move == WHITESQ )
      outcome = -outcome;
  }
  node[this_node].black_minimax_score = outcome;
  node[this_node].white_minimax_score = outcome;
  if ( outcome > 0 ) {
    node[this_node].black_minimax_score += CONFIRMED_WIN;
    node[this_node].white_minimax_score += CONFIRMED_WIN;
  }
  if ( outcome < 0 ) {
    node[this_node].black_minimax_score -= CONFIRMED_WIN;
    node[this_node].white_minimax_score -= CONFIRMED_WIN;
  }
  node[this_node].flags |= FULL_SOLVED;

  /* Take another pass through the midgame to update move
     alternatives and minimax information if requested. */

  if ( update_path ) {
    prepare_tree_traversal();
    for ( i = 0; i < last_move_number; i++ ) {
      this_move = abs( game_move_list[i] );
      if ( game_move_list[i] > 0 )
	side_to_move = BLACKSQ;
      else
	side_to_move = WHITESQ;
      if ( !generate_specific( this_move, side_to_move ) )
	fatal_error( "%s: %d\n", BOOK_INVALID_MOVE, this_move );
      (void) make_move( side_to_move, this_move, TRUE );
    }
#ifdef TEXT_BASED
    if ( echo )
      fflush( stdout );
#endif
    midgame_eval_done = FALSE;
    for ( i = last_move_number - 1; i >= 0; i-- ) {
      this_move = abs( game_move_list[i] );
      if ( game_move_list[i] > 0 )
	side_to_move = BLACKSQ;
      else
	side_to_move = WHITESQ;
      unmake_move( side_to_move, this_move );

      /* If the game was public, make sure that all nodes that
	 previously marked as private nodes are marked as public. */

      this_node = visited_node[i];
      if ( !private_game && (node[this_node].flags & PRIVATE_NODE) )
	node[this_node].flags ^= PRIVATE_NODE;

      if ( node[this_node].flags & BLACK_TO_MOVE )
	side_to_move = BLACKSQ;
      else
	side_to_move = WHITESQ;
      generate_all( side_to_move );
      determine_hash_values( side_to_move, board );
      if ( disks_played >= 60 - max_full_solve ) {
	/* Only solve the position if it hasn't been solved already */
	if ( !(node[this_node].flags & FULL_SOLVED) ) {
	  (void) end_game( side_to_move, FALSE, FALSE, TRUE, 0, &dummy_info );
	  if ( side_to_move == BLACKSQ )
	    outcome = +root_eval;
	  else
	    outcome = -root_eval;
	  node[this_node].black_minimax_score = outcome;
	  node[this_node].white_minimax_score = outcome;
	  if ( outcome > 0 ) {
	    node[this_node].black_minimax_score += CONFIRMED_WIN;
	    node[this_node].white_minimax_score += CONFIRMED_WIN;
	  }
	  if (outcome < 0) {
	    node[this_node].black_minimax_score -= CONFIRMED_WIN;
	    node[this_node].white_minimax_score -= CONFIRMED_WIN;
	  }
	  node[this_node].flags |= FULL_SOLVED;
	}
      }
      else if ( disks_played >= 60 - max_wld_solve ) {
	/* Only solve the position if its WLD status is unknown */
	if ( !(node[this_node].flags & WLD_SOLVED) ) {
	  (void) end_game( side_to_move, TRUE, FALSE, TRUE, 0, &dummy_info );
	  if ( side_to_move == BLACKSQ )
	    outcome = +root_eval;
	  else
	    outcome = -root_eval;
	  node[this_node].black_minimax_score = outcome;
	  node[this_node].white_minimax_score = outcome;
	  if ( outcome > 0 ) {
	    node[this_node].black_minimax_score += CONFIRMED_WIN;
	    node[this_node].white_minimax_score += CONFIRMED_WIN;
	  }
	  if ( outcome < 0 ) {
	    node[this_node].black_minimax_score -= CONFIRMED_WIN;
	    node[this_node].white_minimax_score -= CONFIRMED_WIN;
	  }
	  node[this_node].flags |= WLD_SOLVED;
	}
      }
      else {
	force_eval = (i >= first_new_node - 1) ||
	  (node[this_node].best_alternative_move == abs(game_move_list[i]));
#ifdef TEXT_BASED
	if ( !midgame_eval_done ) {
	  printf( "Evaluating: " );
	  fflush( stdout );
	}
#endif
	midgame_eval_done = TRUE;
	if ( force_eval )
	  clear_node_depth( this_node );
	evaluate_node( this_node );
#ifdef TEXT_BASED
	printf( "|" );
	fflush( stdout );
#endif
      }
      node[this_node].flags |= NOT_TRAVERSED;
      do_minimax( this_node, &dummy_black_score, &dummy_white_score );
      if ( !(node[this_node].flags & WLD_SOLVED) &&
	   (node[this_node].best_alternative_move == NO_MOVE) &&
	   (node[this_node].alternative_score == NO_SCORE) ) {
	/* Minimax discovered that the node hasn't got a deviation any
	   longer because that move has been played. */
	evaluate_node( this_node );
#ifdef TEXT_BASED
	printf( "-|-" );
#endif
	do_minimax( this_node, &dummy_black_score, &dummy_white_score );
      }
    }
#ifdef TEXT_BASED
    puts( "" );
#endif
  }
  toggle_event_status( TRUE );

  echo = stored_echo;
  total_game_count++;
}


/*
   BUILD_TREE
   Reads games from the file pointed to by FILE_NAME and
   incorporates them into the game tree.
*/

void
build_tree( const char *file_name, int max_game_count,
	    int max_diff, int min_empties ) {
  char move_string[200];
  char line_buffer[1000];
  char sign, column, row;
  int i;
  int games_parsed, games_imported;
  int move_count;
  int diff;
  short game_move_list[60];
  time_t start_time, stop_time;
  FILE *stream;

#ifdef TEXT_BASED
  puts( "Importing game list..." );
  fflush( stdout );
#endif

  stream = fopen( file_name, "r" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", NO_GAME_FILE_ERROR, file_name );

  time( &start_time );

  games_parsed = 0;
  games_imported = 0;
  do {
    fgets( line_buffer, 998, stream );
    sscanf( line_buffer, "%s %d", move_string, &diff );
    move_count = (strlen( move_string ) - 1) / 3;
    games_parsed++;
    for ( i = 0; i < move_count; i++ ) {
      sscanf( move_string + 3 * i, "%c%c%c", &sign, &column, &row );
      game_move_list[i] = 10 * (row - '0') + (column - 'a' + 1);
      if ( sign == '-' )
	game_move_list[i] = -game_move_list[i];
    }
    if ( abs( diff ) <= max_diff ) {
      add_new_game( move_count, game_move_list, min_empties, 0, 0,
		    FALSE, FALSE );
#ifdef TEXT_BASED
      printf( "|" );
      if ( games_imported % 100 == 0 )
        printf( " --- %d games --- ", games_imported );
      fflush( stdout );
#endif
      games_imported++;
    }
  } while ( games_parsed < max_game_count );

  time( &stop_time );

  fclose( stream );

#ifdef TEXT_BASED
  printf( "\ndone (took %d s)\n", (int) (stop_time - start_time) );
  printf( "%d games read; %d games imported\n", games_parsed, games_imported );
  printf( "Games with final difference <= %d were read until %d empties.\n",
	  max_diff, min_empties );
  puts( "" );
#endif
}


/*
   READ_TEXT_DATABASE
   Reads an existing ASCII database file.
*/

void
read_text_database( const char *file_name ) {
  int i;
  int magic1, magic2;
  int new_book_node_count;
  time_t start_time, stop_time;
  FILE *stream;

  time( &start_time );

#ifdef TEXT_BASED
  printf( "Reading text opening database... " );
  fflush( stdout );
#endif

  stream = fopen( file_name, "r" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", NO_DB_FILE_ERROR, file_name );

  fscanf( stream, "%d", &magic1 );
  fscanf( stream, "%d", &magic2 );
  if ( (magic1 != BOOK_MAGIC1) || (magic2 != BOOK_MAGIC2) )
    fatal_error( "%s: %s", BOOK_CHECKSUM_ERROR, file_name );

  fscanf( stream, "%d", &new_book_node_count );
  set_allocation( new_book_node_count + NODE_TABLE_SLACK );
  for ( i = 0; i < new_book_node_count; i++ )
    fscanf( stream, "%d %d %hd %hd %hd %hd %hd\n",
	    &node[i].hash_val1, &node[i].hash_val2,
	    &node[i].black_minimax_score,
	    &node[i].white_minimax_score,
	    &node[i].best_alternative_move,
	    &node[i].alternative_score, &node[i].flags );
  book_node_count = new_book_node_count;
             
  create_hash_reference();

  fclose( stream );

  time( &stop_time );

#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}


/*
   READ_BINARY_DATABASE
   Reads a binary database file.
*/

void
read_binary_database( const char *file_name ) {
  int i;
  int new_book_node_count;
  short magic1, magic2;
  time_t start_time, stop_time;
  FILE *stream;

  time( &start_time );

#ifdef TEXT_BASED
  printf( "Reading binary opening database... " );
  fflush( stdout );
#endif

  stream = fopen( file_name, "rb" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", NO_DB_FILE_ERROR, file_name );

  fread( &magic1, sizeof( short ), 1, stream );
  fread( &magic2, sizeof( short ), 1, stream );
  if ( (magic1 != BOOK_MAGIC1) || (magic2 != BOOK_MAGIC2) )
    fatal_error( "%s: %s", BOOK_CHECKSUM_ERROR, file_name ); 

  fread( &new_book_node_count, sizeof( int ), 1, stream );
  set_allocation( new_book_node_count + NODE_TABLE_SLACK );

  for ( i = 0; i < new_book_node_count; i++ ) {
    fread( &node[i].hash_val1, sizeof( int ), 1, stream );
    fread( &node[i].hash_val2, sizeof( int ), 1, stream );

    fread( &node[i].black_minimax_score, sizeof( short ), 1, stream );
    fread( &node[i].white_minimax_score, sizeof( short ), 1, stream );

    fread( &node[i].best_alternative_move, sizeof( short ), 1, stream );
    fread( &node[i].alternative_score, sizeof( short ), 1, stream );

    fread( &node[i].flags, sizeof( unsigned short ), 1, stream );
  }

  fclose( stream );

  book_node_count = new_book_node_count;
  create_hash_reference();

  time( &stop_time );

#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
#endif
}



/*
   MERGE_BINARY_DATABASE
   Merges a binary database file with the current book.
*/

void
merge_binary_database( const char *file_name ) {
  time_t start_time;
  time( &start_time );

#ifdef TEXT_BASED
  printf( "Importing binary opening database... " );
  fflush( stdout );
#endif

  FILE *stream = fopen( file_name, "rb" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", NO_DB_FILE_ERROR, file_name );

  short magic1, magic2;
  fread( &magic1, sizeof( short ), 1, stream );
  fread( &magic2, sizeof( short ), 1, stream );
  if ( (magic1 != BOOK_MAGIC1) || (magic2 != BOOK_MAGIC2) )
    fatal_error( "%s: %s", BOOK_CHECKSUM_ERROR, file_name ); 

  int merge_book_node_count;
  fread( &merge_book_node_count, sizeof( int ), 1, stream );

  int merge_use_count = 0;

  int i;
  for ( i = 0; i < merge_book_node_count; i++ ) {
    BookNode merge_node;

    /* Read node. */

    fread( &merge_node.hash_val1, sizeof( int ), 1, stream );
    fread( &merge_node.hash_val2, sizeof( int ), 1, stream );

    fread( &merge_node.black_minimax_score, sizeof( short ), 1, stream );
    fread( &merge_node.white_minimax_score, sizeof( short ), 1, stream );

    fread( &merge_node.best_alternative_move, sizeof( short ), 1, stream );
    fread( &merge_node.alternative_score, sizeof( short ), 1, stream );

    fread( &merge_node.flags, sizeof( unsigned short ), 1, stream );

    /* Look up node in existing database. */

    int slot = probe_hash_table( merge_node.hash_val1, merge_node.hash_val2 );
    if ( (slot == NOT_AVAILABLE) ||
	 (book_hash_table[slot] == EMPTY_HASH_SLOT) ) {
      /* New position, add it without modifications. */
      int this_node = create_BookNode( merge_node.hash_val1,
				       merge_node.hash_val2,
				       merge_node.flags );
      node[this_node] = merge_node;
      merge_use_count++;
    }
    else {
      /* Existing position, use the book from the merge file if it contains
	 better endgame information. */

      int index = book_hash_table[slot];
      if ( ((merge_node.flags & FULL_SOLVED) &&
	    !(node[index].flags & FULL_SOLVED)) ||
	   ((merge_node.flags & WLD_SOLVED) &&
	    !(node[index].flags & WLD_SOLVED)) ) {
	node[index] = merge_node;
	merge_use_count++;
      }
    }
  }
  fclose( stream );

  /* Make sure the tree is in reasonably good shape after the merge. */

  minimax_tree();

#ifdef TEXT_BASED
  time_t stop_time;
  time( &stop_time );
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  printf( "Used %d out of %d nodes from the merge file.",
	  merge_use_count, merge_book_node_count );
#endif
}



/*
   WRITE_TEXT_DATABASE
   Writes the database to an ASCII file.
*/

void
write_text_database( const char *file_name ) {
  int i;
  time_t start_time, stop_time;
  FILE *stream;

  time(&start_time);

#ifdef TEXT_BASED
  printf( "Writing text database... " );
  fflush( stdout );
#endif

  stream = fopen( file_name, "w" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", DB_WRITE_ERROR, file_name );

  fprintf( stream, "%d\n%d\n", BOOK_MAGIC1, BOOK_MAGIC2 );

  fprintf( stream, "%d\n", book_node_count );

  for ( i = 0; i < book_node_count; i++ )
    fprintf( stream, "%d %d %d %d %d %d %d\n",
	     node[i].hash_val1, node[i].hash_val2,
	     node[i].black_minimax_score, node[i].white_minimax_score,
	     node[i].best_alternative_move,
	     node[i].alternative_score, node[i].flags );
  fclose( stream );

  time( &stop_time );

#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}


/*
   WRITE_BINARY_DATABASE
   Writes the database to a binary file.
*/

void
write_binary_database( const char *file_name ) {
  int i;
  short magic;
  time_t start_time, stop_time;
  FILE *stream;

  time( &start_time );

#ifdef TEXT_BASED
  printf( "Writing binary database... " );
  fflush( stdout );
#endif

  stream = fopen( file_name, "wb" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", DB_WRITE_ERROR, file_name );

  magic = BOOK_MAGIC1;
  fwrite( &magic, sizeof( short ), 1, stream );
  magic = BOOK_MAGIC2;
  fwrite( &magic, sizeof( short ), 1, stream );

  fwrite( &book_node_count, sizeof( int ), 1, stream );

  for ( i = 0; i < book_node_count; i++ ) {
    fwrite( &node[i].hash_val1, sizeof( int ), 1, stream );
    fwrite( &node[i].hash_val2, sizeof( int ), 1, stream );

    fwrite( &node[i].black_minimax_score, sizeof( short ), 1, stream );
    fwrite( &node[i].white_minimax_score, sizeof( short ), 1, stream );

    fwrite( &node[i].best_alternative_move, sizeof( short ), 1, stream );
    fwrite( &node[i].alternative_score, sizeof( short ), 1, stream );

    fwrite( &node[i].flags, sizeof( unsigned short ), 1, stream );
  }

  fclose( stream );

  time( &stop_time );

#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}



/*
   DO_COMPRESS
   Compresses the subtree below the current node.
*/

static void
do_compress( int index, int *node_order, short *child_count,
	     int *node_index, short *child_list, int *child_index ) {
  int i, j;
  int child;
  int valid_child_count;
  int side_to_move;
  int slot, val1, val2, orientation;
  int found;
  int local_child_list[64];
  short this_move;
  short local_child_move[64];

  if ( !(node[index].flags & NOT_TRAVERSED) )
    return;

  node_order[*node_index] = index;

  if ( node[index].flags & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;
  valid_child_count = 0;

  generate_all( side_to_move );

  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2);
    child = book_hash_table[slot];
    if ( (child != EMPTY_HASH_SLOT) && (node[child].flags & NOT_TRAVERSED) ) {
      for ( j = 0, found = FALSE; j < valid_child_count; j++ )
	if ( child == local_child_list[j] )
	  found = TRUE;
      if ( !found ) {
	local_child_list[valid_child_count] = child;
	local_child_move[valid_child_count] = this_move;
	valid_child_count++;
	child_list[*child_index] = this_move;
	(*child_index)++;
      }
    }
    unmake_move( side_to_move, this_move );
  }

  child_count[*(node_index)] = valid_child_count;
  (*node_index)++;

  for ( i = 0; i < valid_child_count; i++ ) {
    this_move = local_child_move[i];
    (void) make_move( side_to_move, this_move, TRUE );
    do_compress( local_child_list[i], node_order, child_count, node_index,
		 child_list, child_index );
    unmake_move( side_to_move, this_move );
  }

  node[index].flags ^= NOT_TRAVERSED;
}



/*
   WRITE_COMPRESSED_DATABASE
   Creates and saves a compressed database file.
*/

void
write_compressed_database( const char *file_name ) {
  int i;
  int node_index, child_index;
  int *node_order;
  short *child_count;
  short *child;
  time_t start_time, stop_time;
  FILE *stream;

  time( &start_time );

#ifdef TEXT_BASED
  printf( "Writing compressed database... " );
  fflush( stdout );
#endif

  stream = fopen( file_name, "wb" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", DB_WRITE_ERROR, file_name );


  prepare_tree_traversal();

  node_order = (int *) safe_malloc( book_node_count * sizeof( int ) );
  child_count = (short *) safe_malloc( book_node_count * sizeof( short ) );
  child = (short *) malloc( book_node_count * sizeof( short ) );

  for ( i = 0; i < book_node_count; i++ )
    node[i].flags |= NOT_TRAVERSED;
  node_index = 0;
  child_index = 0;

  do_compress( ROOT, node_order, child_count, &node_index,
	       child, &child_index );

  fwrite( &book_node_count, sizeof( int ), 1, stream );

  fwrite( &child_index, sizeof( int ), 1, stream );

  fwrite( child_count, sizeof( short ), book_node_count, stream );

  fwrite( child, sizeof( short ), child_index, stream );

  for ( i = 0; i < book_node_count; i++ ) {
    fwrite( &node[node_order[i]].black_minimax_score,
	    sizeof( short ), 1, stream );
    fwrite( &node[node_order[i]].white_minimax_score,
	    sizeof( short ), 1, stream );
  }

  for ( i = 0; i < book_node_count; i++ )
    fwrite( &node[node_order[i]].best_alternative_move,
	    sizeof( short ), 1, stream );

  for ( i = 0; i < book_node_count; i++ )
    fwrite( &node[node_order[i]].alternative_score,
	    sizeof( short ), 1, stream );

  for ( i = 0; i < book_node_count; i++ )
    fwrite( &node[node_order[i]].flags,
	    sizeof( unsigned short ), 1, stream );

  fclose( stream );

  free( node_order );
  free( child_count );
  free( child );

  time( &stop_time );

#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}



/*
  DO_UNCOMPRESS
  Uncompress the subtree below the current node. This is done
  in preorder.
*/

static void
do_uncompress( int depth, FILE *stream, int *node_index, int *child_index,
	       short *child_count, short *child,
	       short *black_score, short *white_score,
	       short *alt_move, short *alt_score, unsigned short *flags ) {
  int i;
  int side_to_move;
  int saved_child_index;
  int saved_child_count;
  int val1, val2, orientation;
  int this_move;

  if ( flags[*node_index] & BLACK_TO_MOVE )
    side_to_move = BLACKSQ;
  else
    side_to_move = WHITESQ;

  saved_child_count = child_count[*node_index];
  saved_child_index = *child_index;
  (*child_index) += saved_child_count;

  /* Write the data for the current node */

  get_hash( &val1, &val2, &orientation );

  fwrite( &val1, sizeof( int ), 1, stream );
  fwrite( &val2, sizeof( int ), 1, stream );

  fwrite( &black_score[*node_index], sizeof( short ), 1, stream );
  fwrite( &white_score[*node_index], sizeof( short ), 1, stream );

  fwrite( &alt_move[*node_index], sizeof( short ), 1, stream );
  fwrite( &alt_score[*node_index], sizeof( short ), 1, stream );

  fwrite( &flags[*node_index], sizeof( unsigned short ), 1, stream );

  (*node_index)++;

  /* Recursively traverse the children */

  for ( i = 0; i < saved_child_count; i++ ) {
    int flipped;
    this_move = child[saved_child_index + i];
    flipped = make_move_no_hash( side_to_move, this_move );
    if ( flipped == 0 )
      printf( "%c%c flips %d discs for %d\n",
	      TO_SQUARE( this_move ), flipped, side_to_move );
    do_uncompress( depth + 1,stream, node_index, child_index,
		   child_count, child, black_score, white_score,
		   alt_move, alt_score, flags );
    unmake_move_no_hash( side_to_move, this_move );

  }
}



/*
  UNPACK_COMPRESSED_DATABASE
  Reads a database compressed with WRITE_COMPRESSED_DATABASE
  and unpacks it into an ordinary .bin file.
*/

void
unpack_compressed_database( const char *in_name, const char *out_name ) {
  int i;
  int dummy;
  int node_count, child_list_size;
  int node_index, child_index;
  short magic;
  short *child_count, *child;
  short *black_score, *white_score;
  short *alt_move, *alt_score;
  time_t start_time, stop_time;
  unsigned short *flags;
  FILE *stream;

#ifdef TEXT_BASED
  printf( "Uncompressing compressed database... " );
  fflush( stdout );
#endif

  time( &start_time );

  /* Read the compressed database */

  stream = fopen( in_name, "rb" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", NO_DB_FILE_ERROR, in_name );

  fread( &node_count, sizeof( int ), 1, stream );

  fread( &child_list_size, sizeof( int ), 1, stream );

  child_count = (short *) safe_malloc( node_count * sizeof( short ) );
  child = (short *) safe_malloc( child_list_size * sizeof( short ) );

  fread( child_count, sizeof( short ), node_count, stream );

  fread( child, sizeof( short ), child_list_size, stream );

  black_score = (short *) safe_malloc( node_count * sizeof( short ) );
  white_score = (short *) safe_malloc( node_count * sizeof( short ) );
  alt_move = (short *) safe_malloc( node_count * sizeof( short ) );
  alt_score = (short *) safe_malloc( node_count * sizeof( short ) );
  flags =
    (unsigned short *) safe_malloc( node_count * sizeof( unsigned short ) );

  for ( i = 0; i < node_count; i++ ) {
    fread( &black_score[i], sizeof( short ), 1, stream );
    fread( &white_score[i], sizeof( short ), 1, stream );
  }

  fread( alt_move, sizeof( short ), node_count, stream );

  fread( alt_score, sizeof( short ), node_count, stream );

  fread( flags, sizeof( unsigned short ), node_count, stream );

  fclose( stream );

  /* Traverse the tree described by the database and create the .bin file */

  stream = fopen( out_name, "wb" );
  if ( stream == NULL )
    fatal_error( "%s '%s'\n", DB_WRITE_ERROR, out_name );

  toggle_experimental( 0 );
  game_init( NULL, &dummy );
  toggle_midgame_hash_usage( TRUE, TRUE );
  toggle_abort_check( FALSE );
  toggle_midgame_abort_check( FALSE );

  magic = BOOK_MAGIC1;
  fwrite( &magic, sizeof( short ), 1, stream );
  magic = BOOK_MAGIC2;
  fwrite( &magic, sizeof( short ), 1, stream );

  fwrite( &node_count, sizeof( int ), 1, stream );

  node_index = 0;
  child_index = 0;
  do_uncompress( 0, stream, &node_index, &child_index, child_count, child,
		 black_score, white_score, alt_move, alt_score, flags );

  fclose( stream );

  /* Free tables */

  free( child_count );
  free( child );

  free( black_score );
  free( white_score );
  free( alt_move );
  free( alt_score );
  free( flags );

  time( &stop_time );

#ifdef TEXT_BASED
  printf( "done (took %d s)\n", (int) (stop_time - start_time) );
  puts( "" );
#endif
}



/*
   SET_SEARCH_DEPTH
   When finding move alternatives, searches to depth DEPTH
   will be performed.
*/

void
set_search_depth( int depth ) {
  search_depth = depth;
}


/*
  SET_EVAL_SPAN
  Specify the evaluation value interval where nodes are re-evaluated.
*/

void set_eval_span( double min_span, double max_span ) {
  min_eval_span = ceil(min_span * 128.0);
  max_eval_span = ceil(max_span * 128.0);
}


/*
  SET_NEGAMAX_SPAN
  Specify the negamax value interval where nodes are re-evaluated.
*/

void
set_negamax_span( double min_span, double max_span ) {
  min_negamax_span = ceil( min_span * 128.0 );
  max_negamax_span = ceil( max_span * 128.0 );
}


/*
  SET_MAX_BATCH_SIZE
  Specify the maximum number of nodes to evaluate.
*/

void
set_max_batch_size( int size ) {
  max_batch_size = size;
}


/*
   SET_DEVIATION_VALUE
   Sets the number of disks where a penalty is incurred if
   the deviation from the book line comes later than that
   stage; also set the punishment per move after the threshold.
*/

void
set_deviation_value( int low_threshold, int high_threshold, double bonus ) {
  low_deviation_threshold = low_threshold;
  high_deviation_threshold = high_threshold;
  deviation_bonus = bonus;
}


/*
   RESET_BOOK_SEARCH
   Sets the used slack count to zero.
*/   

void
reset_book_search( void ) {
  used_slack[BLACKSQ] = 0.0;
  used_slack[WHITESQ] = 0.0;
}


/*
   SET_SLACK
   Sets the total amount of negamaxed evaluation that
   the program is willing to trade for randomness.
*/

void
set_slack( int slack ) {
  max_slack = slack;
}


/*
  SET_DRAW_MODE
  Specifies how book draws should be treated.
*/

void
set_draw_mode(DrawMode mode) {
  draw_mode = mode;
}


/*
  SET_GAME_MODE
  Specifies if the book is in private or public mode.
*/

void
set_game_mode(GameMode mode) {
  game_mode = mode;
}



/*
  SET_BLACK_FORCE
  SET_WHITE_FORCE
  Specifies if the moves for either of the players are to
  be forced when recursing the tree.
*/

void
set_black_force( int force ) {
  force_black = force;
}

void
set_white_force( int force ) {
  force_white = force;
}



/*
  MERGE_POSITION_LIST
  Adds the scores from the positions defined in SCRIPT_FILE and solved
  in OUTPUT_FILE to the book.  The two files are checked for sanity -
  if they don't describe the same set of positions, something has gone awry.
*/

void
merge_position_list( const char *script_file, const char *output_file ) {
  char script_buffer[1024];
  char result_buffer[1024];
  char move_buffer[1024];
  int i, j, pos;
  int side_to_move;
  int col;
  int line;
  int score;
  int move;
  int wld_only;
  int val1, val2, orientation;
  int slot, index;
  int position_count, already_wld_count, already_exact_count;
  int tokens_read, moves_read;
  int new_nodes_created;
  int probable_error;
  FILE *script_stream;
  FILE *result_stream;

  script_stream = fopen( script_file, "r" );
  if ( script_stream == NULL ) {
    fprintf( stderr, "Can't open %s\n", script_file );
    exit( EXIT_FAILURE );
  }

  result_stream = fopen( output_file, "r" );
  if ( result_stream == NULL ) {
    fprintf( stderr, "Can't open %s\n", output_file );
    exit( EXIT_FAILURE );
  }

  prepare_tree_traversal();

  line = 1;
  position_count = 0;
  already_wld_count = 0;
  already_exact_count = 0;
  new_nodes_created = 0;

  fgets( script_buffer, 1024, script_stream );
  fgets( result_buffer, 1024, result_stream );
  while ( !feof( script_stream ) && !feof( result_stream ) ) {
    char *ch;

    ch = script_buffer + strlen( script_buffer ) - 1;
    while ( (ch >= script_buffer) && !isgraph( *ch ) ) {
      *ch = 0;
      ch--;
    }
    ch = result_buffer + strlen( result_buffer ) - 1;
    while ( (ch >= result_buffer) && !isgraph( *ch ) ) {
      *ch = 0;
      ch--;
    }

    if ( line % 4 == 3 ) {  /* The position/result lines */
      position_count++;

      /* Parse the board */
      disks_played = 0;
      col = 0;
      for ( i = 1; i <= 8; i++ )
	for ( j = 1; j <= 8; j++ ) {
	  pos = 10 * i + j;
	  switch ( script_buffer[col] ) {
	  case '*':
	  case 'X':
	  case 'x':
	    board[pos] = BLACKSQ;
	    disks_played++;
	    break;
	  case 'O':
	  case '0':
	  case 'o':
	    board[pos] = WHITESQ;
	    disks_played++;
	    break;
	  case '-':
	  case '.':
	    board[pos] = EMPTY;
	    break;
	  default:
	    fprintf( stderr, "\nBad character '%c' in board on line %d\n\n",
		    script_buffer[col], line );
	    exit( EXIT_FAILURE );
	    break;
	  }
	  col++;
	}
      switch ( script_buffer[65] ) {
      case '*':
      case 'X':
      case 'x':
	side_to_move = BLACKSQ;
	break;
      case 'O':
      case '0':
      case 'o':
	side_to_move = WHITESQ;
	break;
      default:
	fprintf( stderr, "\nBad side to move '%c' in board on line %d\n\n",
		 script_buffer[65], line );
	exit( EXIT_FAILURE );
	break;
      }
      disks_played -= 4;  /* The initial board contains 4 discs */

      /* Parse the result */

      wld_only = TRUE;
      if ( strstr( result_buffer, "Black win" ) == result_buffer ) {
	score = CONFIRMED_WIN + 2;
	tokens_read = sscanf( result_buffer, "%*s %*s %s", move_buffer );
	moves_read = tokens_read;
      }
      else if ( strstr( result_buffer, "White win" ) == result_buffer ) {
	score = -(CONFIRMED_WIN + 2);
	tokens_read = sscanf( result_buffer, "%*s %*s %s", move_buffer );
	moves_read = tokens_read;
      }
      else if ( strstr( result_buffer, "Draw" ) == result_buffer ) {
	score = 0;
	tokens_read = sscanf( result_buffer, "%*s %s", move_buffer );
	moves_read = tokens_read;
      }
      else {  /* Exact score */
	int black_discs, white_discs;

	wld_only = FALSE;
	tokens_read = sscanf( result_buffer, "%d %*s %d %s", &black_discs,
			      &white_discs, move_buffer );
	moves_read = tokens_read - 2;
	score = black_discs - white_discs;
	if ( score > 0 )
	  score += CONFIRMED_WIN;
	else if ( score < 0 )
	  score -= CONFIRMED_WIN;
      }

      /* Set the score for the node corresponding to the position */

      get_hash( &val1, &val2, &orientation );
      slot = probe_hash_table( val1, val2 );
      index = book_hash_table[slot];
      if ( index == EMPTY_HASH_SLOT ) {
	fprintf( stderr, "Position on line %d not found in book\n", line );
	exit( EXIT_SUCCESS );
      }

      probable_error = FALSE;
      if ( node[index].flags & WLD_SOLVED ) {
	already_wld_count++;
	if ( ((score > 0) && (node[index].black_minimax_score <= 0)) ||
	     ((score == 0) && (node[index].black_minimax_score != 0)) ||
	     ((score < 0) && (node[index].black_minimax_score > 0)) ) {
	  probable_error = TRUE;
	  fprintf( stderr, "Line %d: New WLD score %d conflicts with "
		    "old score %d\n", line, 
		   score, node[index].black_minimax_score );
	}
      }

      if ( node[index].flags & FULL_SOLVED ) {
	already_exact_count++;
	if ( !wld_only && (score != node[index].black_minimax_score) ) {
	  probable_error = TRUE;
	  fprintf( stderr, "Line %d: New exact score %d conflicts with "
		    "old score %d\n", line, 
		   score, node[index].black_minimax_score );
	}
      }

      if ( probable_error || !wld_only || !(node[index].flags & FULL_SOLVED) )
	node[index].black_minimax_score = node[index].white_minimax_score =
	  score;

      if ( probable_error )  /* Clear the old flags if score was wrong */
	node[index].flags &= ~(WLD_SOLVED | FULL_SOLVED);
      if ( wld_only )
	node[index].flags |= WLD_SOLVED;
      else
	node[index].flags |= (WLD_SOLVED | FULL_SOLVED);

      /* Examine the position arising from the PV move; if it exists it
	 need only be checked for sanity, otherwise a new node is
	 created. */

      if ( moves_read > 0 ) {
	/* Make sure the optimal move leads to a position in the hash table */
	int row, col;

	row = move_buffer[1] - '0';
	col = tolower( move_buffer[0] )- 'a' + 1;
	move = 10 * row + col;
	if ( (row >= 1) && (row <= 8) && (col >= 1) && (col <= 8) &&
	     make_move_no_hash( side_to_move, move ) ) {
	  int new_side_to_move = OPP( side_to_move );

	  generate_all( new_side_to_move );
	  if ( move_count[disks_played] == 0 )
	    new_side_to_move = side_to_move;

	  get_hash( &val1, &val2, &orientation );
	  slot = probe_hash_table( val1, val2 );
	  index = book_hash_table[slot];
	  if ( index == EMPTY_HASH_SLOT ) {
	    index = create_BookNode( val1, val2, PRIVATE_NODE );
	    node[index].black_minimax_score =
	      node[index].white_minimax_score = score;
	    if ( new_side_to_move == BLACKSQ )
	      node[index].flags |= BLACK_TO_MOVE;
	    else
	      node[index].flags |= WHITE_TO_MOVE;
	    if ( wld_only )
	      node[index].flags |= WLD_SOLVED;
	    else
	      node[index].flags |= (WLD_SOLVED | FULL_SOLVED);

	    new_nodes_created++;
	  }
	  else {  /* Position already exists, sanity-check it */
	    probable_error = FALSE;
	    if ( node[index].flags & WLD_SOLVED ) {
	      if ( ((score > 0) && (node[index].black_minimax_score <= 0)) ||
		   ((score == 0) && (node[index].black_minimax_score != 0)) ||
		   ((score < 0) && (node[index].black_minimax_score > 0)) ) {
		probable_error = TRUE;
		fprintf( stderr, "Line %d: New child WLD score %d "
			 "conflicts with old score %d\n", line,
			 score, node[index].black_minimax_score );
	      }
	    }
	    if ( node[index].flags & FULL_SOLVED ) {
	      if ( !wld_only && (score != node[index].black_minimax_score) ) {
		probable_error = TRUE;
		fprintf( stderr, "Line %d: New child exact score %d "
			 "conflicts with old score %d\n", line, 
			 score, node[index].black_minimax_score );
	      }
	    }

	    if ( probable_error ) {  /* Correct errors encountered */
	      node[index].black_minimax_score =
		node[index].white_minimax_score = score;
	      node[index].flags &= ~(WLD_SOLVED | FULL_SOLVED);
	      if ( wld_only )
		node[index].flags |= WLD_SOLVED;
	      else
		node[index].flags |= (WLD_SOLVED | FULL_SOLVED);
	    }
	  }

	  unmake_move_no_hash( side_to_move, move );
	}
	else {
	  fprintf( stderr, "Line %d: The PV move '%s' is invalid\n",
		   line, move_buffer );
	  exit( EXIT_FAILURE );
	}
      }
    }
    else if ( strcmp( script_buffer, result_buffer ) ) {
      fprintf( stderr,
	       "Script and result files differ unexpectedly on line %d\n",
	       line );
      exit( EXIT_FAILURE );
    }

    fgets( script_buffer, 1024, script_stream );
    fgets( result_buffer, 1024, result_stream );
    line++;
  }
  line--;

  printf( "%d lines read from the script and result files\n", line );
  if ( !feof( script_stream ) || !feof( result_stream ) )
    puts( "Warning: The two files don't have the same number of lines." );

  printf( "%d positions merged with the book\n", position_count );
  printf( "%d positions were already solved for exact score\n",
	  already_exact_count );
  printf( "%d positions were already solved WLD\n", already_wld_count );
  printf( "%d positions had optimal moves leading to new positions\n",
	  new_nodes_created );
  puts( "" );

  fclose( script_stream );
  fclose( result_stream );
}



/*
  CHECK_FORCED_OPENING
  Checks if the board position fits the provided forced opening line OPENING
  in any rotation; if this is the case, the next move is returned,
  otherwise PASS is returned.
*/

int
check_forced_opening( int side_to_move, const char *opening ) {
  int i, j;
  int pos;
  int count;
  int move_count;
  int local_side_to_move;
  int same_position;
  int symm_index, symmetry;
  int move[60];
  int local_board[100];
  int move_offset[8] = { 1, -1, 9, -9, 10, -10, 11, -11 };

  move_count = strlen( opening ) / 2;
  if ( move_count <= disks_played )
    return PASS;

  for ( i = 0; i < move_count; i++ )
    move[i] = 10 * (opening[2 * i + 1] - '0') +
      tolower( opening[2 * i] ) - 'a' + 1;

  /* Play through the given opening line until the number of discs
     matches that on the actual board. */

  for ( pos = 11; pos <= 88; pos++ )
    local_board[pos] = EMPTY;
  local_board[45] = local_board[54] = BLACKSQ;
  local_board[44] = local_board[55] = WHITESQ;

  local_side_to_move = BLACKSQ;
  for ( i = 0; i < disks_played; i++ ) {
    for ( j = 0; j < 8; j++ ) {
      for ( pos = move[i] + move_offset[j], count = 0;
	    local_board[pos] == OPP( local_side_to_move );
	    pos += move_offset[j], count++ )
	;
      if ( local_board[pos] == local_side_to_move ) {
	pos -= move_offset[j];
	while ( pos != move[i] ) {
	  local_board[pos] = local_side_to_move;
	  pos -= move_offset[j];
	}
      }
    }
    local_board[move[i]] = local_side_to_move;

    local_side_to_move = OPP( local_side_to_move );
  }

  if ( local_side_to_move != side_to_move )
    return PASS;

  /* Check if any of the 8 symmetries make the board after the opening
     line match the current board. The initial symmetry is chosen
     randomly to avoid the same symmetry being chosen all the time.
     This is not a perfect scheme but good enough. */

  symmetry = abs( my_random() ) % 8;
  for ( symm_index = 0; symm_index < 8;
	symm_index++, symmetry = (symmetry + 1) % 8 ) {
    same_position = TRUE;
    for ( i = 1; (i <= 8) && same_position; i++ )
      for ( j = 1; j <= 8; j++ ) {
	pos = 10 * i + j;
	if ( board[pos] != local_board[symmetry_map[symmetry][pos]] )
	  same_position = FALSE;
      }
    if ( same_position )
      return inv_symmetry_map[symmetry][move[disks_played]];
  }

  return PASS;
}



/*
  FILL_MOVE_ALTERNATIVES
  Fills the data structure CANDIDATE_LIST with information
  about the book moves available in the current position.
  FLAGS specifies a subset of the flag bits which have to be set
  for a position to be considered. Notice that FLAGS=0 accepts
  any flag combination.
*/

void
fill_move_alternatives( int side_to_move,
			int flags ) {
  CandidateMove temp;
  int sign;
  int i;
  int slot;
  int changed;
  int index;
  int val1, val2, orientation;
  int this_move, alternative_move;
  int score, alternative_score;
  int child_feasible, deviation;
  int root_flags;
   
  get_hash( &val1, &val2, &orientation );
  slot = probe_hash_table( val1, val2 );

  /* If the position wasn't found in the hash table, return. */
  
  if ( (slot == NOT_AVAILABLE) ||
       (book_hash_table[slot] == EMPTY_HASH_SLOT) ) {
    candidate_count = 0;
    return;
  }
  else
    index = book_hash_table[slot];

  /* If the position hasn't got the right flag bits set, return. */

  root_flags = node[index].flags;

  if ( (flags != 0) && !(root_flags & flags) ) {
    candidate_count = 0;
    return;
  }

  if ( side_to_move == BLACKSQ )
    sign = +1;
  else
    sign = -1;

  alternative_move = node[index].best_alternative_move;
  if ( alternative_move > 0 ) {
    alternative_move = inv_symmetry_map[orientation][alternative_move];
    alternative_score =
      adjust_score( node[index].alternative_score, side_to_move );
  }
  else
    alternative_score = -INFINITE_EVAL;
  generate_all( side_to_move );
  candidate_count = 0;
  for ( i = 0; i < move_count[disks_played]; i++ ) {
    this_move = move_list[disks_played][i];
    (void) make_move( side_to_move, this_move, TRUE );
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    unmake_move( side_to_move, this_move );

    /* Check if the move leads to a book position and, if it does,
       whether it has the solve status (WLD or FULL) specified by FLAGS. */
 
    deviation = FALSE;
    if ( (slot == NOT_AVAILABLE) ||
	 (book_hash_table[slot] == EMPTY_HASH_SLOT) ) {
      if ( (this_move == alternative_move) && !flags ) {
	score = alternative_score;
	child_feasible = TRUE;
	deviation = TRUE;
      }
      else {
	child_feasible = FALSE;
	score = 0;
      }
    }
    else if ( (node[book_hash_table[slot]].flags & flags) || !flags ) {
      if ( side_to_move == BLACKSQ )
	score = node[book_hash_table[slot]].black_minimax_score;
      else
	score = node[book_hash_table[slot]].white_minimax_score;
      child_feasible = TRUE;
    }
    else {
      child_feasible = FALSE;
      score = 0;
    }

    if ( child_feasible && (score == 0) &&
	 !(node[index].flags & WLD_SOLVED) &&
	 (node[book_hash_table[slot]].flags & WLD_SOLVED) ) {
      /* Check if this is a book draw that should be avoided, i.e., one
         where the current position is not solved but the child position
         is solved for a draw, and the draw mode dictates this draw to
         be a bad one. */
      if ( (game_mode == PRIVATE_GAME) ||
	   !(node[book_hash_table[slot]].flags & PRIVATE_NODE) ) {
	if ( side_to_move == BLACKSQ ) {
	  if ( (draw_mode == WHITE_WINS) || (draw_mode == OPPONENT_WINS) ) {
#ifdef TEXT_BASED
	    printf( "%c%c leads to an unwanted book draw\n",
		    TO_SQUARE( this_move ) );
#endif
	    child_feasible = FALSE;
	  }
	}
	else {
	  if ( (draw_mode == BLACK_WINS) || (draw_mode == OPPONENT_WINS) ) {
#ifdef TEXT_BASED
	    printf( "%c%c leads to an unwanted book draw\n",
		    TO_SQUARE( this_move ) );
#endif
	    child_feasible = FALSE;
	  }
	}
      }
    }

    if ( child_feasible ) {
      candidate_list[candidate_count].move = move_list[disks_played][i];
      candidate_list[candidate_count].score = sign * score;
      if ( deviation )
	candidate_list[candidate_count].flags = DEVIATION;
      else
	candidate_list[candidate_count].flags =
	  node[book_hash_table[slot]].flags;
      candidate_list[candidate_count].parent_flags = root_flags;
      candidate_count++;
    }
  }

  if ( candidate_count > 0 ) {
    /* Sort the book moves using bubble sort */
    do {
      changed = FALSE;
      for ( i = 0; i < candidate_count - 1; i++ )
	if ( candidate_list[i].score < candidate_list[i + 1].score ) {
	  changed = TRUE;
	  temp = candidate_list[i];
	  candidate_list[i] = candidate_list[i + 1];
	  candidate_list[i + 1] = temp;
	}
    } while ( changed );
  }
}


/*
  GET_CANDIDATE_COUNT
  GET_CANDIDATE
  Accessor functions for the data structure created by
  FILL_MOVE_ALTERNATIVES.
*/

int
get_candidate_count( void ) {
  return candidate_count;
}

CandidateMove
get_candidate( int index ) {
  return candidate_list[index];
}


/*
   PRINT_MOVE_ALTERNATIVES
   Displays all available book moves from a position.
   FLAGS specifies a subset of the flag bits which have to be set
   for a position to be considered. Notice that FLAGS=0 accepts
   any flag combination.
*/

void
print_move_alternatives( int side_to_move ) {
  int i;
  int sign;
  int slot;
  int val1, val2, orientation;
  int score, output_score;
  
  if ( candidate_count > 0 ) {
    if ( side_to_move == BLACKSQ )
      sign = +1;
    else
      sign = -1;
    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );

    /* Check that the position is in the opening book after all */

    if (slot == NOT_AVAILABLE || book_hash_table[slot] == EMPTY_HASH_SLOT)
      return;

    /* Pick the book score corresponding to the player to move and
       remove draw avoidance and the special scores for nodes WLD. */

    if ( side_to_move == BLACKSQ )
      score = node[book_hash_table[slot]].black_minimax_score; 
    else
      score = node[book_hash_table[slot]].white_minimax_score; 
    if ( (score == +UNWANTED_DRAW) || (score == -UNWANTED_DRAW) )
      score = 0;
    if ( score > +CONFIRMED_WIN )
      score -= CONFIRMED_WIN;
    if ( score < -CONFIRMED_WIN )
      score += CONFIRMED_WIN;

#ifdef TEXT_BASED
    printf( "Book score is " );
    if ( node[book_hash_table[slot]].flags & FULL_SOLVED )
      printf( "%+d (exact score).", sign * score );
    else if ( node[book_hash_table[slot]].flags & WLD_SOLVED )
      printf( "%+d (W/L/D solved).", sign * score );
    else
      printf( "%+.2f.", (sign * score) / 128.0 );
    if ( node[book_hash_table[slot]].flags & PRIVATE_NODE )
      printf( " Private node." );
    puts( "" );
#endif

    for ( i = 0; i < candidate_count; i++ ) {
#ifdef TEXT_BASED
      printf( "   %c%c   ", TO_SQUARE( candidate_list[i].move ) );
#endif
      output_score = candidate_list[i].score;
      if ( output_score >= CONFIRMED_WIN )
	output_score -= CONFIRMED_WIN;
      else if ( output_score <= -CONFIRMED_WIN )
	output_score += CONFIRMED_WIN;

#ifdef TEXT_BASED
      if ( candidate_list[i].flags & FULL_SOLVED )
	printf( "%+-6d  (exact score)", output_score );
      else if ( candidate_list[i].flags & WLD_SOLVED )
	printf( "%+-6d  (W/L/D solved)", output_score );
      else {
	printf( "%+-6.2f", output_score / 128.0 );
	if ( candidate_list[i].flags & DEVIATION )
	  printf( "  (deviation)" );
      }
      puts( "" );
#endif
    }
  }
}


/*
   GET_BOOK_MOVE
   Chooses a book move from the list of candidates
   which don't worsen the negamaxed out-of-book
   evaluation by too much.
*/

int
get_book_move( int side_to_move,
	       int update_slack,
	       EvaluationType *eval_info ) {
  int i;
  int original_side_to_move;
  int remaining_slack;
  int score, chosen_score, best_score, alternative_score;
  int feasible_count;
  int index;
  int chosen_index;
  int flags, base_flags;
  int random_point;
  int level;
  int continuation, is_feasible;
  int acc_weight, total_weight;
  int best_move, this_move, alternative_move;
  int sign;
  int val1, val2, orientation, slot;
  int weight[60];
  int temp_move[60], temp_stm[60];

  /* Disable opening book randomness unless the move is going to
     be played on the board by Zebra */

  if ( update_slack )
    remaining_slack = MAX( max_slack - used_slack[side_to_move], 0 );
  else
    remaining_slack = 0;

  if ( echo && (candidate_count > 0) && !get_ponder_move() ) {
#ifdef TEXT_BASED
    printf( "Slack left is %.2f. ", remaining_slack / 128.0 );
#endif
    print_move_alternatives( side_to_move );
  }

  /* No book move found? */

  if ( candidate_count == 0 )
    return PASS;

  /* Find the book flags of the original position. */

  get_hash( &val1, &val2, &orientation );
  slot = probe_hash_table( val1, val2 );

  if ( (slot == NOT_AVAILABLE) ||
       (book_hash_table[slot] == EMPTY_HASH_SLOT) )
    fatal_error( "Internal error in book code." );
  base_flags = node[book_hash_table[slot]].flags;

  /* If we have an endgame score for the position, we only want to
     consult the book if there is at least one move realizing that score. */

  index = book_hash_table[slot];
  if ( node[index].flags & FULL_SOLVED ) {
    if ( candidate_list[0].score < node[index].black_minimax_score )
      return PASS;
  }
  else if ( node[index].flags & WLD_SOLVED ) {
    if ( (node[index].black_minimax_score > 0) &&
	 (candidate_list[0].score <= 0) )
      return PASS;
  }

  /* Don't randomize among solved moves */

  score = candidate_list[0].score;

  if ( score >= CONFIRMED_WIN )
    remaining_slack = 0;

  feasible_count = 0;
  total_weight = 0;
  while ( (feasible_count < candidate_count) &&
	  (candidate_list[feasible_count].score >= score - remaining_slack) ) {
    weight[feasible_count] = 2 * remaining_slack + 1 -
      (score - candidate_list[feasible_count].score);
    total_weight += weight[feasible_count];
    feasible_count++;
  }

  /* Chose a move at random from the moves which don't worsen
     the position by more than the allowed slack (and, optionally,
     update it). A simple weighting scheme makes the moves with
     scores close to the best move most likely to be chosen. */

  if ( feasible_count == 1 )
    chosen_index = 0;
  else {
    random_point = (my_random() >> 10) % total_weight;
    chosen_index = 0;
    acc_weight = weight[chosen_index];
    while ( random_point > acc_weight ) {
      chosen_index++;
      acc_weight += weight[chosen_index];
    }
  }

  chosen_score = candidate_list[chosen_index].score;
  if ( update_slack )
    used_slack[side_to_move] += (score - chosen_score);

  /* Convert the book score to the normal form.
     Note that this should work also for old-style book values. */

  if ( chosen_score >= +CONFIRMED_WIN ) {
    chosen_score -= CONFIRMED_WIN;
    if ( chosen_score <= 64 )
      chosen_score *= 128;
  }
  if ( chosen_score <= -CONFIRMED_WIN ) {
    chosen_score += CONFIRMED_WIN;
    if ( chosen_score >= -64 )
      chosen_score *= 128;
  }

  /* Return the score via the EvaluationType structure */

  flags = candidate_list[chosen_index].flags;
  *eval_info = create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
				 chosen_score, 0.0, 0, TRUE );
  if ( (base_flags & (FULL_SOLVED | WLD_SOLVED)) &&
       (flags & (FULL_SOLVED | WLD_SOLVED)) ) {
    /* Both the base position and the position after the book move
       are solved. */
    if ( (base_flags & FULL_SOLVED) && (flags & FULL_SOLVED) )
      eval_info->type = EXACT_EVAL;
    else
      eval_info->type = WLD_EVAL;
    if ( chosen_score > 0 )
      eval_info->res = WON_POSITION;
    else if ( chosen_score == 0 )
      eval_info->res = DRAWN_POSITION;
    else
      eval_info->res = LOST_POSITION;
  }
  else if ( (flags & WLD_SOLVED) && (chosen_score > 0) ) {
    /* The base position is unknown but the move played leads
       to a won position. */
    eval_info->type = WLD_EVAL;
    eval_info->res = WON_POSITION;
  }
  else {
    /* No endgame information available. */
    eval_info->type = MIDGAME_EVAL;
  }

  if ( echo ) {
    send_status( "-->   Book     " );
    if ( flags & FULL_SOLVED )
      send_status( "%+3d (exact)   ", chosen_score / 128 );
    else if ( flags & WLD_SOLVED )
      send_status( "%+3d (WLD)     ", chosen_score / 128 );
    else
      send_status( "%+6.2f        ", chosen_score / 128.0 );
    if ( get_ponder_move() )
      send_status( "{%c%c} ", TO_SQUARE( get_ponder_move() ) );
    send_status( "%c%c", TO_SQUARE( candidate_list[chosen_index].move ) );
  }

  /* Fill the PV structure with the optimal book line */

  original_side_to_move = side_to_move;

  level = 0;
  temp_move[0] = candidate_list[chosen_index].move;
  do {
    temp_stm[level] = side_to_move;
    (void) make_move( side_to_move, temp_move[level], TRUE );
    level++;

    get_hash( &val1, &val2, &orientation );
    slot = probe_hash_table( val1, val2 );
    continuation = TRUE;
    if ( (slot == NOT_AVAILABLE) ||
	 (book_hash_table[slot] == EMPTY_HASH_SLOT) )
      continuation = FALSE;
    else {
      alternative_move = node[book_hash_table[slot]].best_alternative_move;
      if ( alternative_move > 0 ) {
	alternative_move = inv_symmetry_map[orientation][alternative_move];
	alternative_score =
	  adjust_score( node[book_hash_table[slot]].alternative_score,
			side_to_move );
      }
      else
	alternative_score = -INFINITE_EVAL;

      if ( node[book_hash_table[slot]].flags & BLACK_TO_MOVE ) {
	side_to_move = BLACKSQ;
	sign = 1;
      }
      else {
	side_to_move = WHITESQ;
	sign = -1;
      }
      generate_all( side_to_move );
      best_score = -INFINITE_EVAL;
      best_move = NO_MOVE;
      for ( i = 0; i < move_count[disks_played]; i++ ) {
	this_move = move_list[disks_played][i];
	(void) make_move( side_to_move, this_move, TRUE );
	get_hash( &val1, &val2, &orientation );
	slot = probe_hash_table( val1, val2 );
	unmake_move( side_to_move, this_move );

	if ( (slot == NOT_AVAILABLE) ||
	     (book_hash_table[slot] == EMPTY_HASH_SLOT) ) {
	  if ( this_move == alternative_move ) {
	    score = alternative_score;
	    is_feasible = TRUE;
	  }
	  else
	    is_feasible = FALSE;
	}
	else {
	  if ( original_side_to_move == BLACKSQ )
	    score = node[book_hash_table[slot]].black_minimax_score;
	  else
	    score = node[book_hash_table[slot]].white_minimax_score;
	  is_feasible = TRUE;
	}
	if ( is_feasible ) {
	  score *= sign;
	  if ( score > best_score ) {
	    best_score = score;
	    best_move = this_move;
	  }
	}
      }
      if ( best_move == NO_MOVE )
	continuation = FALSE;
      else
	temp_move[level] = best_move;
    }
  } while ( continuation );
  pv_depth[0] = level;
  for ( i = 0; i < level; i++ )
    pv[0][i] = temp_move[i];
  do {
    level--;
    unmake_move( temp_stm[level], temp_move[level] );
  } while ( level > 0 );

  return candidate_list[chosen_index].move;
}



/*
  DUPSTR
  A strdup clone.
*/

static char *
dupstr( const char *str ) {
  char *new_str = malloc( strlen( str ) + 1);
  strcpy( new_str, str );

  return new_str;
}



/*
  CONVERT_OPENING_LIST
  Convert a list of openings on Robert Gatliff's format
  to a hash table representation containing the same information.
*/

void
convert_opening_list( const char *base_file ) {
  FILE *in_stream, *out_stream;
  char *name_start, *scan_ptr, *move_ptr;
  const char *source_file_name;
  const char *header_file_name;
  char *parent[1000];  /* Max number of opening names occurring */
  char buffer[1024];
  char move_seq[256];
  int i, j;
  int row, col;
  int opening_count;
  int op_move_count;
  int level;
  int hash_val1, hash_val2, orientation;
  int op_move[60], side_to_move[60];
  time_t timer;

  in_stream = fopen( base_file, "r" );
  if ( in_stream == NULL ) {
#ifdef TEXT_BASED
    printf( "Cannot open opening file '%s'\n", base_file );
#endif
    exit( EXIT_FAILURE );
  }

  /* Get the number of openings */

  fgets( buffer, 1023, in_stream );
  sscanf( buffer, "%d", &opening_count );

  /* Prepare the header file */

  header_file_name = "opname.h";

  out_stream = fopen( header_file_name, "w" );
  if ( out_stream == NULL ) {
#ifdef TEXT_BASED
    printf( "Cannot create header file '%s'\n", header_file_name );
#endif
    exit( EXIT_FAILURE );
  }

  time( &timer );
  fprintf( out_stream, "/*\n" );
  fprintf( out_stream, "   %s\n\n", header_file_name );
  fprintf( out_stream, "   Automatically created by BOOKTOOL on %s",
	   ctime( &timer ) );
  fprintf( out_stream, "*/" );
  fprintf( out_stream, "\n\n\n" );

  fputs( "#ifndef OPNAME_H\n", out_stream );
  fputs( "#define OPNAME_H\n\n\n", out_stream );
  fprintf( out_stream, "#define OPENING_COUNT       %d\n\n\n", opening_count );
  fputs( "typedef struct {\n", out_stream );
  fputs( "  const char *name;\n", out_stream );
  fputs( "  const char *sequence;\n", out_stream );
  fputs( "  int hash_val1;\n", out_stream );
  fputs( "  int hash_val2;\n", out_stream );
  fputs( "  int level;\n", out_stream );
  fputs( "} OpeningDescriptor;\n\n\n", out_stream );
  fputs( "extern OpeningDescriptor opening_list[OPENING_COUNT];\n",
	 out_stream);
  fputs( "\n\n#endif  /* OPNAME_H */\n", out_stream );

  fclose( out_stream );

  /* Prepare the source file */

  source_file_name = "opname.c";

  out_stream = fopen( source_file_name, "w" );
  if ( out_stream == NULL ) {
    printf( "Cannot create source file '%s'\n", source_file_name );
    exit( EXIT_FAILURE );
  }

  time( &timer );
  fprintf( out_stream, "/*\n" );
  fprintf( out_stream, "   %s\n\n", source_file_name );
  fprintf( out_stream, "   Automatically created by BOOKTOOL on %s",
	   ctime( &timer ) );
  fprintf( out_stream, "*/" );
  fprintf( out_stream, "\n\n\n" );

  fprintf( out_stream, "#include \"%s\"\n\n\n", header_file_name );

  fputs( "OpeningDescriptor opening_list[OPENING_COUNT] = {\n", out_stream );

  /* Read the list of openings */ 

  prepare_tree_traversal();

  level = 0;

  for ( i = 0; i < opening_count; i++ ) {
    fgets( buffer, 1023, in_stream );

    /* Each line in the input file corresponds to one opening.
       First separate the line into opening moves and name. */

    sscanf( buffer, "%s", move_seq );
    name_start = buffer + strlen( move_seq );
    while ( isspace( (int) (*name_start) ) )
      name_start++;
    scan_ptr = name_start;
    while ( isprint( (int) (*scan_ptr) ) )
      scan_ptr++;
    *scan_ptr = 0;
    op_move_count = strlen( move_seq ) / 2;
    for ( j = 0, move_ptr = buffer; j < op_move_count; j++ ) {
      if ( isupper( (int) (*move_ptr) ) )
	side_to_move[j] = BLACKSQ;
      else
	side_to_move[j] = WHITESQ;
      col = toupper( *move_ptr ) - 'A' + 1;
      move_ptr++;
      row = ( *move_ptr ) - '0';
      move_ptr++;
      op_move[j] = 10 * row + col;
    }

    /* Check out how the relation between this openings and the ones
       in the hierachy created to far */

    while ( (level > 0) &&
	    (strstr(move_seq, parent[level - 1]) != move_seq) ) {
      level--;
      free( parent[level] );
    }
    parent[level] = dupstr( move_seq );
    level++;

    /* Create the board position characteristic for the opening. */

    for ( j = 0; j < op_move_count; j++ ) {
      if ( !generate_specific( op_move[j], side_to_move[j] ) ) {
#ifdef TEXT_BASED
	printf( "Move %d in opening #%d is illegal\n", j + 1, i );
#endif
	exit( EXIT_FAILURE );
      }
      (void) make_move( side_to_move[j], op_move[j], TRUE );
    }

    /* Write the code fragment  */

    get_hash( &hash_val1, &hash_val2, &orientation );
    fprintf( out_stream, "   { \"%s\",\n     \"%s\",\n     %d, %d, %d }",
	     name_start, move_seq, hash_val1, hash_val2, level - 1 );
    if ( i != opening_count - 1 )
      fputs(" ,\n", out_stream);

    /* Undo the moves */

    for ( j = op_move_count - 1; j >= 0; j-- )
      unmake_move( side_to_move[j], op_move[j] );
  }
  fputs( "\n};\n", out_stream );

  /* Remove the hierarchy data */

  while ( level > 0 ) {
    level--;
    free( parent[level] );
  }

  fclose( out_stream );
  fclose( in_stream );
}


/*
  FIND_OPENING_NAME
  Searches the opening name database read by READ_OPENING_LIST
  and returns a pointer to the name if the position was found,
  NULL otherwise.
*/

const char *
find_opening_name( void ) {
  int i;
  int val1, val2, orientation;

  get_hash( &val1, &val2, &orientation );
  for ( i = 0; i < OPENING_COUNT; i++ )
    if ( (val1 == opening_list[i].hash_val1) &&
	 (val2 == opening_list[i].hash_val2) )
      return opening_list[i].name;

  return NULL;
}


/*
   INIT_OSF
   Makes sure all data structures are initialized.
*/

void
init_osf( int do_global_setup ) {
  init_maps();
  prepare_hash();
  setup_hash( TRUE );
  init_book_tree();
  reset_book_search();
  search_depth = DEFAULT_SEARCH_DEPTH;
  max_slack = DEFAULT_SLACK;
  low_deviation_threshold = 60;
  high_deviation_threshold = 60;
  deviation_bonus = 0.0;
  min_eval_span = 0;
  max_eval_span = INFINITE_SPREAD;
  min_negamax_span = 0;
  max_negamax_span = INFINITE_SPREAD;
  max_batch_size = INFINIT_BATCH_SIZE;
  force_black = FALSE;
  force_white = FALSE;

  if ( do_global_setup )
    global_setup( RANDOMIZATION, HASH_BITS );
}



/*
  CLEAR_OSF
  Free all dynamically allocated memory.
*/

void
clear_osf( void ) {
  free( book_hash_table );
  book_hash_table = NULL;

  free( node );
  node = NULL;
}
