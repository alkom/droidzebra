/*
   File:           game.c

   Created:        September 20, 1997

   Modified:       December 31, 2002

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       All the routines needed to play a game.
*/



#include "porting.h"



#if !defined( _WIN32_WCE ) && !defined( __linux__ ) && !defined( __CYGWIN__ )
#include "dir.h"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32_WCE
#include <assert.h>
#include <time.h>
#endif

#include "bitboard.h"
#include "constant.h"
#include "display.h"
#include "end.h"
#include "error.h"
#include "eval.h"
#include "game.h"
#include "getcoeff.h"
#include "globals.h"
#include "hash.h"
#include "macros.h"
#include "midgame.h"
#include "moves.h"
#include "myrandom.h"
#include "osfbook.h"
#include "patterns.h"
#include "probcut.h"
#include "search.h"
#include "stable.h"
#include "texts.h"
#include "thordb.h"
#include "timer.h"
#include "unflip.h"



/* The pre-ordering depth used when guessing the opponent's move */
#define PONDER_DEPTH             8

/* The depth to abandon the book move and try to solve the position
   in time control games. */
#define FORCE_BOOK_SOLVE         30

/* The somewhat arbitrary point where endgame typically commences
   when Zebra is running on today's hardware */
#define TYPICAL_SOLVE            27

/* The recommended extra depth gained by switching to the endgame searcher;
   i.e., use the endgame solver with n+ENDGAME_OFFSET empty instead of the
   midgame module with depth n. */
#define ENDGAME_OFFSET           7                 /* WAS: 4 */

/* Same as ENDGAME_OFFSET but forces a solve to begin */
#define ENDGAME_FORCE_OFFSET     3

/* Solve earlier for lopsided positions? */
#define ADAPTIVE_SOLVE_DEPTH     FALSE

/* The depth where no time is to be gained by using the fact that
   there is only one move available. */
#ifdef _WIN32_WCE
#define DISABLE_FORCED_MOVES     70
#else
#define DISABLE_FORCED_MOVES     60
#endif

/* The file to which compute_move() writes its status reports */
#define LOG_FILE_NAME            "zebra.log"

/* The maximum length of any system path. */
#define MAX_PATH_LENGTH          2048



static const char *forced_opening = NULL;
static char log_file_path[MAX_PATH_LENGTH];
static double last_time_used;
static int max_depth_reached;
#ifdef _WIN32_WCE
static int use_log_file = FALSE;
#else
static int use_log_file = TRUE;
#endif
static int play_human_openings = TRUE;
static int play_thor_match_openings = TRUE;
static int game_evaluated_count;
static int komi = 0;
static int prefix_move = 0;
static int endgame_performed[3];
static EvaluatedMove evaluated_list[60];



/*
  TOGGLE_STATUS_LOG
  Enable/disable the use of logging all the output that the
  text version of Zebra would output to the screen.
*/

void
toggle_status_log( int write_log ) {
  use_log_file = write_log;
}



/*
   GLOBAL_SETUP
   Initialize the different sub-systems.
*/
   
void
global_setup( int use_random, int hash_bits ) {
  FILE *log_file;
  time_t timer;

  /* Clear the log file. No error handling done. */

#if defined(ANDROID)
  sprintf(log_file_path, "%s/%s", android_files_dir, LOG_FILE_NAME);
#elif defined(__linux__)
  strcpy( log_file_path, LOG_FILE_NAME );
#else
  char directory_buffer[MAX_PATH_LENGTH];
  getcwd( directory_buffer, MAX_PATH_LENGTH );
  if ( directory_buffer[strlen( directory_buffer ) - 1] == '\\' )
    sprintf( log_file_path, "%s%s", directory_buffer, LOG_FILE_NAME );
  else
    sprintf( log_file_path, "%s\\%s", directory_buffer, LOG_FILE_NAME );
#endif

  if ( use_log_file ) {
    log_file = fopen( log_file_path, "w" );
    if ( log_file != NULL ) {
      time( &timer );
      fprintf( log_file, "%s %s\n", LOG_TEXT, ctime( &timer ) );
      fprintf( log_file, "%s %s %s\n", ENGINE_TEXT, __DATE__, __TIME__ );
      fclose( log_file );
    }
  }

  if ( use_random ) {
    time( &timer );
    my_srandom( timer );
  }
  else
    my_srandom( 1 );

  init_hash( hash_bits );
  init_bitboard();
  init_moves();
  init_patterns();
  init_coeffs();
  init_timer();
  init_probcut();
  init_stable();
  setup_search();
}


/*
   GLOBAL_TERMINATE
   Free all dynamically allocated memory.
*/   

void
global_terminate( void ) {
  free_hash();
  clear_coeffs();
  clear_osf();
}



/*
   SETUP_GAME
   Prepares the board.
*/

static void
setup_game( const char *file_name, int *side_to_move ) {
  char buffer[65];
  int i, j;
  int pos, token;
  FILE *stream;
   
  for ( i = 0; i < 10; i++ )
    for ( j = 0; j < 10; j++ ) {
      pos = 10 * i + j;
      if ( (i == 0) || (i == 9) || (j == 0) || (j == 9) )
	board[pos] = OUTSIDE;
      else
	board[pos] = EMPTY;
    }

  if ( file_name == NULL ) {
    board[45] = board[54] = BLACKSQ;
    board[44] = board[55] = WHITESQ;
    *side_to_move = BLACKSQ;
  }
  else {
    stream = fopen( file_name, "r" );
    if ( stream == NULL )
      fatal_error( "%s '%s'\n", GAME_LOAD_ERROR, file_name );
    fgets( buffer, 70, stream );
    token = 0;
    for ( i = 1; i <= 8; i++ )
      for ( j = 1; j <= 8; j++ ) {
	pos = 10 * i + j;
	switch ( buffer[token] ) {
	case '*':
	case 'X':
	  board[pos] = BLACKSQ;
	  break;
	case 'O':
	case '0':
	  board[pos] = WHITESQ;
	  break;
	case '-':
	case '.':
	  break;
	default:
#if TEXT_BASED
	  printf( "%s '%c' %s\n", BAD_CHARACTER_ERROR, buffer[pos],
		  GAME_FILE_TEXT);
#endif
	  break;
	}
	token++;
      }

    fgets( buffer, 10, stream );
    if ( buffer[0] == 'B' )
      *side_to_move = BLACKSQ;
    else if ( buffer[0] == 'W' )
      *side_to_move = WHITESQ;
    else
      fatal_error( "%s '%c' %s\n", BAD_CHARACTER_ERROR, buffer[0],
		   GAME_FILE_TEXT);
  }
  disks_played = disc_count( BLACKSQ ) + disc_count( WHITESQ ) - 4;

  determine_hash_values( *side_to_move, board );

  /* Make the game score look right */

  if ( *side_to_move == BLACKSQ )
    score_sheet_row = -1;
  else {
    black_moves[0] = PASS;
    score_sheet_row = 0;
  }
}



/*
   GAME_INIT
   Prepare the relevant data structures so that a game
   can be played. The position is read from the file
   specified by FILE_NAME.
*/

void
game_init( const char *file_name, int *side_to_move ) {
  setup_game( file_name, side_to_move );
  setup_search();
  setup_midgame();
  setup_end();
  init_eval();

  clear_ponder_times();

  reset_counter( &total_nodes );

  reset_counter( &total_evaluations );

  init_flip_stack();

  total_time = 0.0;
  max_depth_reached = 0;
  last_time_used = 0.0;
  endgame_performed[BLACKSQ] = endgame_performed[WHITESQ] = FALSE;
}


void
clear_endgame_performed()
{
  endgame_performed[BLACKSQ] = endgame_performed[WHITESQ] = FALSE;
}


/*
  SET_KOMI
  Set the endgame komi value.
*/

void
set_komi( int in_komi ) {
  komi = in_komi;
}



/*
  TOGGLE_HUMAN_OPENINGS
  Specifies whether the Thor statistics should be queried for
  openings moves before resorting to the usual opening book.
*/

void
toggle_human_openings( int toggle ) {
  play_human_openings = toggle;
}



/*
  TOGGLE_THOR_MATCH_OPENINGS
  Specifies whether matching Thor games are used as opening book
  before resorting to the usual opening book.
*/

void
toggle_thor_match_openings( int toggle ) {
  play_thor_match_openings = toggle;
}



/*
  SET_FORCED_OPENING
  Specifies an opening line that Zebra is forced to follow when playing.
*/

void
set_forced_opening( const char *opening_str ) {
  forced_opening = opening_str;
}



/*
  PONDER_MOVE
  Perform searches in response to the opponent's next move.
  The results are not returned, but the hash table is filled
  with useful scores and moves.
*/

void
ponder_move( int side_to_move, int book, int mid, int exact, int wld ) {
  EvaluationType eval_info;
  HashEntry entry;
  double move_start_time, move_stop_time;
  int i, j;
  int this_move, hash_move;
  int expect_count;
  int stored_echo;
  int best_pv_depth;
  int expect_list[64];
  int best_pv[61];

  /* Disable all time control mechanisms as it's the opponent's
     time we're using */

  toggle_abort_check( FALSE );
  toggle_midgame_abort_check( FALSE );
  start_move( 0, 0, disc_count( BLACKSQ ) + disc_count( WHITESQ ) );
  clear_ponder_times();
  determine_hash_values( side_to_move, board );

  reset_counter( &nodes );

  /* Find the scores for the moves available to the opponent. */

  hash_move = 0;
  find_hash( &entry, ENDGAME_MODE );
  if ( entry.draft != NO_HASH_MOVE )
    hash_move = entry.move[0];
  else {
    find_hash( &entry, MIDGAME_MODE );
    if ( entry.draft != NO_HASH_MOVE )
      hash_move = entry.move[0];
  }

  stored_echo = echo;
  echo = FALSE;
  (void) compute_move( side_to_move, FALSE, 0, 0, FALSE, FALSE,
		       MIN( PONDER_DEPTH, mid ), 0, 0, FALSE, &eval_info );
  echo = stored_echo;

  /* Sort the opponents on the score and push the table move (if any)
     to the front of the list */

  if ( force_return )
    expect_count = 0;
  else {
    sort_moves( move_count[disks_played] );
    (void) float_move( hash_move, move_count[disks_played] );

    expect_count = move_count[disks_played];
    for ( i = 0; i < expect_count; i++ )
      expect_list[i] = move_list[disks_played][i];

#if TEXT_BASED
    printf( "%s=%d\n", HASH_MOVE_TEXT, hash_move );
    for ( i = 0; i < expect_count; i++ ) {
      printf( "%c%c %-6.2f  ", TO_SQUARE( move_list[disks_played][i] ),
	      evals[disks_played][move_list[disks_played][i]] / 128.0 );
      if ( (i % 7 == 6) || (i == expect_count - 1) )
	puts( "" );
    }
#endif
  }

  /* Go through the expected moves in order and prepare responses. */

  best_pv_depth = 0;
  for ( i = 0; !force_return && (i < expect_count); i++ ) {
    move_start_time = get_real_timer();
    set_ponder_move( expect_list[i] );
    this_move = expect_list[i];
    prefix_move = this_move;
    (void) make_move( side_to_move, this_move, TRUE );
    (void) compute_move( OPP( side_to_move ), FALSE, 0, 0, TRUE, FALSE,
			 mid, exact, wld, FALSE, &eval_info );
    unmake_move( side_to_move, this_move );
    clear_ponder_move();
    move_stop_time =  get_real_timer();
    add_ponder_time( expect_list[i], move_stop_time - move_start_time );
    ponder_depth[expect_list[i]] =
      MAX( ponder_depth[expect_list[i]], max_depth_reached - 1 );
    if ( (i == 0) && !force_return ) {  /* Store the PV for the first move */
      best_pv_depth = pv_depth[0];
      for ( j = 0; j < pv_depth[0]; j++ )
	best_pv[j] = pv[0][j];
    }
  }

  /* Make sure the PV looks reasonable when leaving - either by
     clearing it altogether or, preferrably, using the stored PV for
     the first move if it is available. */

  max_depth_reached++;
  prefix_move = 0;
  if ( best_pv_depth == 0 )
    pv_depth[0] = 0;
  else {
    pv_depth[0] = best_pv_depth + 1;
    pv[0][0] = expect_list[0];
    for ( i = 0; i < best_pv_depth; i++ )
      pv[0][i + 1] = best_pv[i];
  }

  /* Don't forget to enable the time control mechanisms when leaving */

  toggle_abort_check( TRUE );
  toggle_midgame_abort_check( TRUE );
}



/*
  COMPARE_EVAL
  Comparison function for two evals.  Same return value conventions
  as QuickSort.
*/

static int
compare_eval( EvaluationType e1, EvaluationType e2 ) {
  if ( (e1.type == WLD_EVAL) || (e1.type == EXACT_EVAL) )
    if ( e1.score > 0 )
      e1.score += 100000;
  if ( (e2.type == WLD_EVAL) || (e2.type == EXACT_EVAL) )
    if ( e2.score > 0 )
      e2.score += 100000;

  return e1.score - e2.score;
}



/*
  EXTENDED_COMPUTE_MOVE
  This wrapper on top of compute_move() calculates the evaluation
  of all moves available as opposed to upper bounds for all moves
  except for the best.
*/

int
extended_compute_move( int side_to_move, int book_only,
		       int book, int mid, int exact, int wld ) {
  int i, j;
  int index;
  int changed;
  int this_move;
  int disc_diff, corrected_diff;
  int best_move, temp_move;
  int best_score;
  int best_pv_depth;
  int stored_echo;
  int shallow_eval;
  int empties;
  int current_mid, current_exact, current_wld;
  int first_iteration;
  int unsearched;
  int unsearched_count;
  int unsearched_move[61];
  int best_pv[60];
  unsigned int transform1[60], transform2[60];
  CandidateMove book_move;
  EvaluatedMove temp;
  EvaluationType book_eval_info;
  EvalResult res;

  /* Disable all time control mechanisms and randomization */

  toggle_abort_check( FALSE );
  toggle_midgame_abort_check( FALSE );
  toggle_perturbation_usage( FALSE );
  start_move( 0, 0, disc_count( BLACKSQ ) + disc_count( WHITESQ ) );
  clear_ponder_times();
  determine_hash_values( side_to_move, board );

  empties = 60 - disks_played;

  best_move = 0;
  game_evaluated_count = 0;

  reset_counter( &nodes );

  generate_all( side_to_move );

  if ( book_only || book ) {  /* Evaluations for database moves */
    int flags = 0;

    if ( empties <= exact )
      flags = FULL_SOLVED;
    else if ( empties <= wld )
      flags = WLD_SOLVED;

    fill_move_alternatives( side_to_move, flags );

    game_evaluated_count = get_candidate_count();
    for ( i = 0; i < game_evaluated_count; i++ ) {
      int child_flags;

      book_move = get_candidate( i );
      evaluated_list[i].side_to_move = side_to_move;
      evaluated_list[i].move = book_move.move;
      evaluated_list[i].pv_depth = 1;
      evaluated_list[i].pv[0] = book_move.move;
      evaluated_list[i].eval =
	create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
			  book_move.score, 0.0, 0, TRUE );
      child_flags = book_move.flags & book_move.parent_flags;
      if ( child_flags & (FULL_SOLVED | WLD_SOLVED) ) {
	if ( child_flags & FULL_SOLVED )
	  evaluated_list[i].eval.type = EXACT_EVAL;
	else
	  evaluated_list[i].eval.type = WLD_EVAL;
	if ( book_move.score > 0 ) {
	  evaluated_list[i].eval.res = WON_POSITION;
	  /* Normalize the scores so that e.g. 33-31 becomes +256 */
	  evaluated_list[i].eval.score -= CONFIRMED_WIN;
	  evaluated_list[i].eval.score *= 128;
	}
	else if ( book_move.score == 0 )
	  evaluated_list[i].eval.res = DRAWN_POSITION;
	else {  /* score < 0 */
	  evaluated_list[i].eval.res = LOST_POSITION;
	  /* Normalize the scores so that e.g. 30-34 becomes -512 */
	  evaluated_list[i].eval.score += CONFIRMED_WIN;
	  evaluated_list[i].eval.score *= 128;
	}
      }
      else
	  evaluated_list[i].eval.type = MIDGAME_EVAL;
    }
  }
  if ( book_only ) {  /* Only book moves are to be considered */
    if ( game_evaluated_count > 0 ) {
      best_move = get_book_move( side_to_move, FALSE, &book_eval_info );
      set_current_eval( book_eval_info );
    }
    else {
      pv_depth[0] = 0;
      best_move = PASS;
      book_eval_info = create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
					 0, 0.0, 0, FALSE );
      set_current_eval( book_eval_info );
    }
  }
  else {  /* Make searches for moves not in the database */
    int shallow_depth;
    int empties = 60 - disks_played;

    book = FALSE;

    best_score = -INFINITE_EVAL;
    if ( game_evaluated_count > 0 ) {  /* Book PV available */
      best_score = evaluated_list[0].eval.score;
      best_move = evaluated_list[0].move;
    }

    negate_current_eval( TRUE );

    /* Store the available moves, clear their evaluations and sort
       them on shallow evaluation. */

    if ( empties < 12 )
      shallow_depth = 1;
    else {
      int max_depth = MAX( mid, MAX( exact, wld ) );
      if ( max_depth >= 16 )
	shallow_depth = 6;
      else
      shallow_depth = 4;
    }

    unsearched_count = 0;
    for ( i = 0; i < move_count[disks_played]; i++ ) {
      this_move = move_list[disks_played][i];
      unsearched = TRUE;
      for ( j = 0; j < game_evaluated_count; j++ )
	if ( evaluated_list[j].move == this_move )
	  unsearched = FALSE;
      if ( !unsearched )
	continue;
      unsearched_move[unsearched_count] = this_move;
      unsearched_count++;
      (void) make_move( side_to_move, this_move, TRUE );
      if ( shallow_depth == 1 )  /* Compute move doesn't allow depth 0 */
	shallow_eval = -static_evaluation( OPP( side_to_move ) );
      else {
	EvaluationType shallow_info;

	(void) compute_move( OPP( side_to_move ), FALSE, 0, 0, FALSE, book,
			     shallow_depth - 1, 0, 0, TRUE, &shallow_info );

	if ( shallow_info.type == PASS_EVAL ) {
	  /* Don't allow pass */
	  (void) compute_move( side_to_move, FALSE, 0, 0, FALSE, book,
			       shallow_depth - 1, 0, 0, TRUE, &shallow_info );
	  if ( shallow_info.type == PASS_EVAL ) {  /* Game over */
	    disc_diff =
	      disc_count( side_to_move ) - disc_count( OPP( side_to_move ) );
	    if ( disc_diff > 0 )
	      corrected_diff = 64 - 2 * disc_count( OPP( side_to_move) );
	    else if ( disc_diff == 0 )
	      corrected_diff = 0;
	    else
	      corrected_diff = 2 * disc_count( side_to_move ) - 64;
	    shallow_eval = 128 * corrected_diff;
	  }
	  else
	    shallow_eval = shallow_info.score;
	}
	else  /* Sign-correct the score produced */
	  shallow_eval = -shallow_info.score;
      }

      unmake_move( side_to_move, this_move );
      evals[disks_played][this_move] = shallow_eval;
    }

    do {
      changed = FALSE;
      for ( i = 0; i < unsearched_count - 1; i++ )
	if ( evals[disks_played][unsearched_move[i]] <
	     evals[disks_played][unsearched_move[i + 1]] ) {
	  temp_move = unsearched_move[i];
	  unsearched_move[i] = unsearched_move[i + 1];
	  unsearched_move[i + 1] = temp_move;
	  changed = TRUE;
	}
    } while ( changed );

    /* Initialize the entire list as being empty */

    for ( i = 0, index = game_evaluated_count; i < unsearched_count;
	  i++, index++ ) {
      evaluated_list[index].side_to_move = side_to_move;
      evaluated_list[index].move = unsearched_move[i];
      evaluated_list[index].eval =
	create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
			  0, 0.0, 0, FALSE );
      evaluated_list[index].pv_depth = 1;
      evaluated_list[index].pv[0] = unsearched_move[i];

      if ( empties > MAX( wld, exact ) ) {
	transform1[i] = abs( my_random() );
	transform2[i] = abs( my_random() );
      }
      else {
	transform1[i] = 0;
	transform2[i] = 0;
      }
    }

    stored_echo = echo;
    echo = FALSE;
    best_pv_depth = 0;
    if ( mid == 1 ) {  /* compute_move won't be called */
      pv_depth[0] = 0;
      piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
      piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );
    }

    /* Perform iterative deepening if the search depth is large enough */

#define ID_STEP 2

    if ( exact > empties )
      exact = empties;
    if ( (exact < 12) || (empties > exact) )
      current_exact = exact;
    else
      current_exact = (8 + (exact % 2)) - ID_STEP;

    if ( wld > empties )
      wld = empties;
    if ( (wld < 14) || (empties > wld) )
      current_wld = wld;
    else
      current_wld = (10 + (wld % 2)) - ID_STEP;

    if ( ((empties == exact) || (empties == wld)) &&
	 (empties > 16) && (mid < empties - 12) )
      mid = empties - 12;
    if ( mid < 10 )
      current_mid = mid;
    else
      current_mid = (6 + (mid % 2)) - ID_STEP;

    first_iteration = TRUE;

    do {
      if ( current_mid < mid ) {
	current_mid += ID_STEP;

	/* Avoid performing deep midgame searches if the endgame
	   is reached anyway. */

	if ( (empties <= wld) && (current_mid + 7 >= empties) ) {
	  current_wld = wld;
	  current_mid = mid;
	}
	if ( (empties <= exact) && (current_mid + 7 >= empties) ) {
	  current_exact = exact;
	  current_mid = mid;
	}
      }
      else if ( current_wld < wld )
	current_wld = wld;
      else
	current_exact = exact;

      for ( i = 0; (i < unsearched_count) && !force_return; i++ ) {
	EvaluationType this_eval;

	this_move = unsearched_move[i];

	/* Locate the current move in the list.  This has to be done
	   because the moves might have been reordered during the
	   iterative deepening. */

	index = 0;
	while ( evaluated_list[index].move != this_move )
	  index++;

	/* To avoid strange effects when browsing back and forth through
	   a game during the midgame, rehash the hash transformation masks
	   for each move unless the endgame is reached */

	set_hash_transformation( transform1[i], transform2[i] );

	/* Determine the score for the ith move */

	prefix_move = this_move;
	(void) make_move( side_to_move, this_move, TRUE );
	if ( current_mid == 1 ) {
	  /* compute_move doesn't like 0-ply searches */
	  shallow_eval = static_evaluation( OPP( side_to_move ) );
	  this_eval =
	    create_eval_info( MIDGAME_EVAL, UNSOLVED_POSITION,
			      shallow_eval, 0.0, 0, FALSE );
	}
	else
	  (void) compute_move( OPP( side_to_move ), FALSE, 0, 0, FALSE, book,
			       current_mid - 1, current_exact - 1,
			       current_wld - 1, TRUE,
			       &this_eval );
	if ( force_return ) {  /* Clear eval and exit search immediately */
	  this_eval =
	    create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
			      0, 0.0, 0, FALSE );
	  unmake_move( side_to_move, this_move );
	  break;
	}

	if ( this_eval.type == PASS_EVAL ) {
	  /* Don't allow pass */
	  if ( current_mid == 1 ) {
	    /* compute_move doesn't like 0-ply searches */
	    shallow_eval = static_evaluation( side_to_move );
	    this_eval = 
	      create_eval_info( MIDGAME_EVAL, UNSOLVED_POSITION,
				shallow_eval, 0.0, 0, FALSE );
	  }
	  else
	    (void) compute_move( side_to_move, FALSE, 0, 0, FALSE, book,
				 current_mid - 1, current_exact - 1,
				 current_wld - 1, TRUE,
				 &this_eval );
	  if ( this_eval.type == PASS_EVAL ) {  /* Game over */
	    disc_diff =
	      disc_count( side_to_move ) - disc_count( OPP( side_to_move ) );
	    if ( disc_diff > 0 ) {
	      corrected_diff = 64 - 2 * disc_count( OPP( side_to_move) );
	      res = WON_POSITION;
	    }
	    else if ( disc_diff == 0 ) {
	      corrected_diff = 0;
	      res = DRAWN_POSITION;
	    }
	    else {
	      corrected_diff = 2 * disc_count( side_to_move ) - 64;
	      res = LOST_POSITION;
	    }
	    this_eval =
	      create_eval_info( EXACT_EVAL, res, 128 * corrected_diff,
				0.0, 60 - disks_played, FALSE );
	  }
	}
	else {  /* Sign-correct the score produced */
	  this_eval.score =
	    -this_eval.score;
	  if ( this_eval.res == WON_POSITION )
	    this_eval.res = LOST_POSITION;
	  else if ( this_eval.res == LOST_POSITION )
	    this_eval.res = WON_POSITION;
	}

	if ( force_return )
	  break;
	else
	  evaluated_list[index].eval = this_eval;

	/* Store the PV corresponding to the move */

	evaluated_list[index].pv_depth = pv_depth[0] + 1;
	evaluated_list[index].pv[0] = this_move;
	for ( j = 0; j < pv_depth[0]; j++ )
	  evaluated_list[index].pv[j + 1] = pv[0][j];

	/* Store the PV corresponding to the best move */

	if ( evaluated_list[index].eval.score > best_score ) {
	  best_score = evaluated_list[index].eval.score;
	  best_move = this_move;
	  best_pv_depth = pv_depth[0];
	  for ( j = 0; j < best_pv_depth; j++ )
	    best_pv[j] = pv[0][j];
	}
	unmake_move( side_to_move, this_move );

	/* Sort the moves evaluated */

	if ( first_iteration )
	  game_evaluated_count++;
	if ( !force_return )
	  do {
	    changed = FALSE;
	    for ( j = 0; j < game_evaluated_count - 1; j++ )
	      if ( compare_eval( evaluated_list[j].eval,
				 evaluated_list[j + 1].eval ) < 0 ) {
		changed = TRUE;
		temp = evaluated_list[j];
		evaluated_list[j] = evaluated_list[j + 1];
		evaluated_list[j + 1] = temp;
	      }
	  } while ( changed );
      }

      first_iteration = FALSE;

      /* Reorder the moves after each iteration.  Each move is moved to
	 the front of the list, starting with the bad moves and ending
	 with the best move.  This ensures that unsearched_move will be
	 sorted w.r.t. the order in evaluated_list. */

      for ( i = game_evaluated_count - 1; i >= 0; i-- ) {
	int this_move = evaluated_list[i].move;

	j = 0;
	while ( (j != unsearched_count) && (unsearched_move[j] != this_move) )
	  j++;

	if ( j == unsearched_count )  /* Must be book move, skip */
	  continue;

	/* Move the move to the front of the list. */

	while ( j >= 1 ) {
	  unsearched_move[j] = unsearched_move[j - 1];
	  j--;
	}
	unsearched_move[0] = this_move;
      }

    } while ( !force_return &&
	      ((current_mid != mid) ||
	       (current_exact != exact) || (current_wld != wld)) );

    echo = stored_echo;

    game_evaluated_count = move_count[disks_played];
    
    /* Make sure that the PV and the score correspond to the best move */
    
    pv_depth[0] = best_pv_depth + 1;
    pv[0][0] = best_move;
    for ( i = 0; i < best_pv_depth; i++ )
      pv[0][i + 1] = best_pv[i];

    negate_current_eval( FALSE );
    if ( move_count[disks_played] > 0 )
      set_current_eval( evaluated_list[0].eval );
  }

  /* Reset the hash transformation masks prior to leaving */

  set_hash_transformation( 0, 0 );

  /* Don't forget to enable the time control mechanisms when leaving */

  toggle_abort_check( TRUE );
  toggle_midgame_abort_check( TRUE );
  toggle_perturbation_usage( TRUE );

  max_depth_reached++;
  prefix_move = 0;

  return best_move;
}



/*
  PERFORM_EXTENDED_SOLVE
  Calculates exact score or WLD status for the move ACTUAL_MOVE as
  well as for the best move in the position (if it is any other move).
*/

void
perform_extended_solve( int side_to_move, int actual_move,
			int book, int exact_solve ) {
  int i;
  int mid, wld, exact;
  int best_move;
  int disc_diff, corrected_diff;
  EvaluatedMove temp;
  EvalResult res;

  /* Disable all time control mechanisms */

  toggle_abort_check( FALSE );
  toggle_midgame_abort_check( FALSE );
  toggle_perturbation_usage( FALSE );

  start_move( 0, 0, disc_count( BLACKSQ ) + disc_count( WHITESQ ) );
  clear_ponder_times();
  determine_hash_values( side_to_move, board );
  reset_counter( &nodes );

  /* Set search depths that result in Zebra solving after a brief
     midgame analysis */

  mid = 60;
  wld = 60;
  if ( exact_solve )
    exact = 60;
  else
    exact = 0;

  game_evaluated_count = 1;

  /* Calculate the score for the preferred move */

  evaluated_list[0].side_to_move = side_to_move;
  evaluated_list[0].move = actual_move;
  evaluated_list[0].eval =
    create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
		      0, 0.0, 0, FALSE );
  evaluated_list[0].pv_depth = 1;
  evaluated_list[0].pv[0] = actual_move;

  prefix_move = actual_move;
  negate_current_eval( TRUE );

  (void) make_move( side_to_move, actual_move, TRUE );
  (void) compute_move( OPP( side_to_move ), FALSE, 0, 0, FALSE, book,
			     mid - 1, exact - 1, wld - 1, TRUE,
			     &evaluated_list[0].eval );

  if ( evaluated_list[0].eval.type == PASS_EVAL ) {  /* Don't allow pass */
    (void) compute_move( side_to_move, FALSE, 0, 0, FALSE, book,
			 mid - 1, exact - 1, wld - 1, TRUE,
			 &evaluated_list[0].eval );
    if ( evaluated_list[0].eval.type == PASS_EVAL ) {  /* Game has ended */
      disc_diff =
	disc_count( side_to_move ) - disc_count( OPP( side_to_move ) );
      if ( disc_diff > 0 ) {
	corrected_diff = 64 - 2 * disc_count( OPP( side_to_move) );
	res = WON_POSITION;
      }
      else if ( disc_diff == 0 ) {
	corrected_diff = 0;
	res = DRAWN_POSITION;
      }
      else {
	corrected_diff = 2 * disc_count( side_to_move ) - 64;
	res = LOST_POSITION;
      }
      evaluated_list[0].eval =
	create_eval_info( EXACT_EVAL, res, 128 * corrected_diff,
			  0.0, 60 - disks_played, FALSE );
    }
  }
  else {  /* Sign-correct the score produced */
    evaluated_list[0].eval.score = -evaluated_list[0].eval.score;
    if ( evaluated_list[0].eval.res == WON_POSITION )
      evaluated_list[0].eval.res = LOST_POSITION;
    else if ( evaluated_list[0].eval.res == LOST_POSITION )
      evaluated_list[0].eval.res = WON_POSITION;
  }

  if ( force_return )
    evaluated_list[0].eval =
      create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION, 0, 0.0, 0, FALSE );
  else {
    evaluated_list[0].pv_depth = pv_depth[0] + 1;
    evaluated_list[0].pv[0] = actual_move;
    for ( i = 0; i < pv_depth[0]; i++ )
      evaluated_list[0].pv[i + 1] = pv[0][i];
  }

  unmake_move( side_to_move, actual_move );

  prefix_move = 0;
  negate_current_eval( FALSE );
  max_depth_reached++;

  /* Compute the score for the best move and store it in the move list
     if it isn't ACTUAL_MOVE */

  best_move = compute_move( side_to_move, FALSE, 0, 0, FALSE, book, mid,
			    exact, wld, TRUE, &evaluated_list[1].eval );

  if ( !force_return && (best_move != actual_move) ) {
    /* Move list will contain best move first and then the actual move */
    game_evaluated_count = 2;
    evaluated_list[1].side_to_move = side_to_move;
    evaluated_list[1].move = best_move;
    evaluated_list[1].pv_depth = pv_depth[0];
    for ( i = 0; i < pv_depth[0]; i++ )
      evaluated_list[1].pv[i] = pv[0][i];
    temp = evaluated_list[0];
    evaluated_list[0] = evaluated_list[1];
    evaluated_list[1] = temp;
  }

  /* The PV and current eval should when correspond to the best move
     when leaving */

  pv_depth[0] = evaluated_list[0].pv_depth;
  for ( i = 0; i < pv_depth[0]; i++ )
    pv[0][i] = evaluated_list[0].pv[i];

  set_current_eval( evaluated_list[0].eval );

  /* Don't forget to enable the time control mechanisms when leaving */

  toggle_abort_check( TRUE );
  toggle_midgame_abort_check( TRUE );
  toggle_perturbation_usage( FALSE );
}



/*
  GET_EVALUATED_COUNT
  GET_EVALUATED
  Accessor functions for the data structure filled by extended_compute_move().
*/

int
get_evaluated_count( void ) {
  return game_evaluated_count;
}

EvaluatedMove
get_evaluated( int index ) {
  return evaluated_list[index];
}



/*
   COMPUTE_MOVE
   Returns the best move in a position given search parameters.
*/

int
compute_move( int side_to_move,
	      int update_all,
	      int my_time,
	      int my_incr,
	      int timed_depth,
	      int book,
	      int mid,
	      int exact,
	      int wld,
	      int search_forced,
	      EvaluationType *eval_info ) {
  FILE *log_file;
  EvaluationType book_eval_info, mid_eval_info, end_eval_info;
  char *eval_str;
  double midgame_diff;
  enum { INTERRUPTED_MOVE, BOOK_MOVE, MIDGAME_MOVE, ENDGAME_MOVE } move_type;
  int i;
  int curr_move, midgame_move;
  int empties;
  int midgame_depth, interrupted_depth, max_depth;
  int book_move_found;
  int endgame_reached;
  int offset;

  log_file = NULL;
  if ( use_log_file )
    log_file = fopen( log_file_path, "a" );

  if ( log_file )
    display_board( log_file, board, side_to_move, FALSE, FALSE, FALSE );

  /* Initialize various components of the move system */

  piece_count[BLACKSQ][disks_played] = disc_count( BLACKSQ );
  piece_count[WHITESQ][disks_played] = disc_count( WHITESQ );
  init_moves();
  generate_all( side_to_move );
  determine_hash_values( side_to_move, board );

  calculate_perturbation();

  if ( log_file ) {
    fprintf( log_file, "%d %s: ", move_count[disks_played], MOVE_GEN_TEXT );
    for ( i = 0; i < move_count[disks_played]; i++ )
      fprintf( log_file, "%c%c ", TO_SQUARE( move_list[disks_played][i] ) );
    fputs( "\n", log_file );
  }

  if ( update_all ) {
    reset_counter( &evaluations );
    reset_counter( &nodes );
  }

  for ( i = 0; i < 100; i++ )
    evals[disks_played][i] = 0;
  max_depth_reached = 1;
  empties = 60 - disks_played;
  reset_buffer_display();

  determine_move_time( my_time, my_incr, disks_played + 4 );
  if ( !get_ponder_move() )
    clear_ponder_times();

  remove_coeffs( disks_played );

  /* No feasible moves? */

  if ( move_count[disks_played] == 0 ) {
    *eval_info = create_eval_info( PASS_EVAL, UNSOLVED_POSITION,
				   0.0, 0.0, 0, FALSE );
    set_current_eval( *eval_info );
    if ( echo ) {
      eval_str = produce_eval_text( *eval_info, FALSE );
      send_status( "-->         " );
      send_status( "%-8s  ", eval_str );
      display_status( stdout, FALSE );
      free( eval_str );
    }
    if ( log_file ) {
      fprintf( log_file, "%s: %s\n", BEST_MOVE_TEXT, PASS_TEXT );
      fclose( log_file );
    }
    last_time_used = 0.0;
    clear_pv();
    return PASS;
  }

  /* If there is only one move available:
     Don't waste any time, unless told so or very close to the end,
     searching the position. */
   	
  if ( (empties > DISABLE_FORCED_MOVES) &&
       (move_count[disks_played] == 1) &&
       !search_forced ) {  /* Forced move */
    *eval_info = create_eval_info( FORCED_EVAL, UNSOLVED_POSITION,
				   0.0, 0.0, 0, FALSE );
    set_current_eval( *eval_info );
    if ( echo ) {
      eval_str = produce_eval_text( *eval_info, FALSE );
      send_status( "-->         " );
      send_status( "%-8s  ", eval_str );
      free( eval_str );
      send_status( "%c%c ", TO_SQUARE( move_list[disks_played][0] ) );
      display_status( stdout, FALSE );
    }
    if ( log_file ) {
      fprintf( log_file, "%s: %c%c  (%s)\n", BEST_MOVE_TEXT,
	       TO_SQUARE(move_list[disks_played][0]), FORCED_TEXT );
      fclose( log_file );
    }
    last_time_used = 0.0;
    return move_list[disks_played][0];
  }

  /* Mark the search as interrupted until a successful search
     has been performed. */

  move_type = INTERRUPTED_MOVE;
  interrupted_depth = 0;
  curr_move = move_list[disks_played][0];

  /* Check the opening book for midgame moves */

  book_move_found = FALSE;
  midgame_move = PASS;

  if ( forced_opening != NULL ) {
    /* Check if the position fits the currently forced opening */
    curr_move = check_forced_opening( side_to_move, forced_opening );
    if ( curr_move != PASS ) {
      book_eval_info = create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
					 0, 0.0, 0, TRUE );
      midgame_move = curr_move;
      book_move_found = TRUE;
      move_type = BOOK_MOVE;
      if ( echo ) {
	send_status( "-->   Forced opening move        " );
	if ( get_ponder_move() )
	  send_status( "{%c%c} ", TO_SQUARE( get_ponder_move() ) );
	send_status( "%c%c", TO_SQUARE( curr_move ) );
	display_status( stdout, FALSE );
      }
      clear_pv();
      pv_depth[0] = 1;
      pv[0][0] = curr_move;
    }
  }

  if ( !book_move_found && play_thor_match_openings ) {

    /* Optionally use the Thor database as opening book. */

    int threshold = 2;
    database_search( board, side_to_move );
    if ( get_match_count() >= threshold ) {
      int game_index = (my_random() >> 8) % get_match_count();
      curr_move = get_thor_game_move( game_index, disks_played );

      if ( valid_move( curr_move, side_to_move ) ) {
	book_eval_info = create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
					   0, 0.0, 0, TRUE );
	midgame_move = curr_move;
	book_move_found = TRUE;
	move_type = BOOK_MOVE;
	if ( echo ) {
	  send_status( "-->   %s        ", THOR_TEXT );
	  if ( get_ponder_move() )
	    send_status( "{%c%c} ", TO_SQUARE( get_ponder_move() ) );
	  send_status( "%c%c", TO_SQUARE( curr_move ) );
	  display_status( stdout, FALSE );
	}
	clear_pv();
	pv_depth[0] = 1;
	pv[0][0] = curr_move;
      }
      else
	fatal_error( "Thor book move %d is invalid!", curr_move );
    }
  }

  if ( !book_move_found && play_human_openings && book ) {
    /* Check Thor statistics for a move */
    curr_move = choose_thor_opening_move( board, side_to_move, FALSE );
    if ( curr_move != PASS ) {
      book_eval_info = create_eval_info( UNDEFINED_EVAL, UNSOLVED_POSITION,
					 0, 0.0, 0, TRUE );
      midgame_move = curr_move;
      book_move_found = TRUE;
      move_type = BOOK_MOVE;
      if ( echo ) {
	send_status( "-->   %s        ", THOR_TEXT );
	if ( get_ponder_move() )
	  send_status( "{%c%c} ", TO_SQUARE( get_ponder_move() ) );
	send_status( "%c%c", TO_SQUARE( curr_move ) );
	display_status( stdout, FALSE );
      }
      clear_pv();
      pv_depth[0] = 1;
      pv[0][0] = curr_move;
    }
  }

  if ( !book_move_found && book ) {  /* Check ordinary opening book */
    int flags = 0;

    if ( empties <= FORCE_BOOK_SOLVE ) {
      if ( empties <= wld )
	flags = WLD_SOLVED;
      if ( empties <= exact )
	flags = FULL_SOLVED;
    }
    fill_move_alternatives( side_to_move, flags );
    curr_move = get_book_move( side_to_move, update_all, &book_eval_info );
    if ( curr_move != PASS ) {
      set_current_eval( book_eval_info );
      midgame_move = curr_move;
      book_move_found = TRUE;
      move_type = BOOK_MOVE;
      display_status( stdout, FALSE );
    }
  }

  /* Use iterative deepening in the midgame searches until the endgame
     is reached. If an endgame search already has been performed,
     make a much more shallow midgame search. Also perform much more
     shallow searches when there is no time limit and hence no danger
     starting to solve only to get interrupted. */

  if ( !timed_depth && (empties <= MAX( exact, wld ) ) )
    mid = MAX( MIN( MIN( mid, empties - 7 ), 28 ), 2 );

  endgame_reached = !timed_depth && endgame_performed[side_to_move];

  if ( !book_move_found && !endgame_reached ) {
    clear_panic_abort();
    clear_midgame_abort();
    toggle_midgame_abort_check( update_all );
    toggle_midgame_hash_usage( TRUE, TRUE );

    if ( timed_depth )
      max_depth = 64;
    else if ( empties <= MAX( exact, wld ) )
      max_depth = MAX( MIN( MIN( mid, empties - 12 ), 18 ), 2 );
    else
      max_depth = mid;
    midgame_depth = MIN( 2, max_depth );
    do {
      max_depth_reached = midgame_depth;
      midgame_move = middle_game( side_to_move, midgame_depth,
				  update_all, &mid_eval_info );
      set_current_eval( mid_eval_info );
      midgame_diff = 1.3 * mid_eval_info.score / 128.0;
      if ( side_to_move == BLACKSQ )
	midgame_diff -= komi;
      else
	midgame_diff += komi;
      if ( timed_depth ) {  /* Check if the endgame zone has been reached */
	offset = ENDGAME_OFFSET;

	/* These constants were chosen rather arbitrarily but intend
	   to make Zebra solve earlier if the position is lopsided. */

	if ( is_panic_abort() )
	  offset--;

#if ADAPTIVE_SOLVE_DEPTH
	if ( fabs( midgame_diff ) > 4.0 )
	  offset++;
	if ( fabs( midgame_diff ) > 8.0 )
	  offset++;
	if ( fabs( midgame_diff ) > 16.0 )
	  offset++;
	if ( fabs( midgame_diff ) > 32.0 )
	  offset++;
	if ( fabs( midgame_diff ) > 48.0 )
	  offset++;
	if ( fabs( midgame_diff ) > 56.0 )
	  offset++;
#endif

	if ( endgame_performed[side_to_move] )
	  offset += 2;
	if ( ((midgame_depth + offset + TYPICAL_SOLVE) >= 2 * empties) ||
	     (midgame_depth + ENDGAME_OFFSET >= empties) )
	  endgame_reached = TRUE;
      }
      midgame_depth++;
    } while ( !is_panic_abort() &&
	      !is_midgame_abort() &&
	      !force_return &&
	      (midgame_depth <= max_depth) &&
	      (midgame_depth + disks_played <= 61) &&
	      !endgame_reached );

    if ( echo )
      display_status( stdout, FALSE );
    if ( abs( mid_eval_info.score ) == abs( SEARCH_ABORT ) ) {
      move_type = INTERRUPTED_MOVE;
      interrupted_depth = midgame_depth - 1;  /* compensate for increment */
    }
    else
      move_type = MIDGAME_MOVE;
  }

  curr_move = midgame_move;

  /* If the endgame has been reached, solve the position */

  if ( !force_return )
    if ( (timed_depth && endgame_reached)  ||
	 (timed_depth && book_move_found &&
	  (disks_played >= 60 - FORCE_BOOK_SOLVE)) ||
	 (!timed_depth && (empties <= MAX( exact, wld ))) ) {
      max_depth_reached = empties;
      clear_panic_abort();
      if ( timed_depth )
	curr_move = end_game( side_to_move, (disks_played < 60 - exact),
			      FALSE, book, komi, &end_eval_info );
      else
	if ( empties <= exact )
	  curr_move = end_game( side_to_move, FALSE, FALSE,
				book, komi, &end_eval_info );
	else
	  curr_move = end_game( side_to_move, TRUE, FALSE,
				book, komi, &end_eval_info);
      set_current_eval( end_eval_info );
      if ( abs( root_eval ) == abs( SEARCH_ABORT ) )
	move_type = INTERRUPTED_MOVE;
      else
	move_type = ENDGAME_MOVE;
      if ( update_all )
	endgame_performed[side_to_move] = TRUE;
    }

  switch ( move_type ) {
  case INTERRUPTED_MOVE:
    *eval_info = create_eval_info( INTERRUPTED_EVAL, UNSOLVED_POSITION,
				   0.0, 0.0, 0, FALSE );
    clear_status();
    send_status( "--> *%2d", interrupted_depth );
    eval_str = produce_eval_text( *eval_info, TRUE );
    send_status( "%10s  ", eval_str );
    free( eval_str );
    send_status_nodes( counter_value( &nodes ) );
    send_status_pv( pv[0], interrupted_depth );
    send_status_time( get_elapsed_time() );
    if ( get_elapsed_time() != 0.0 )
      send_status( "%6.0f %s",
		   counter_value( &nodes ) / (get_elapsed_time() + 0.001),
		   NPS_ABBREV );
    break;
  case BOOK_MOVE:
    *eval_info = book_eval_info;
    break;
  case MIDGAME_MOVE:
    *eval_info = mid_eval_info;
    break;
  case ENDGAME_MOVE:
    *eval_info = end_eval_info;
    break;
  }

  set_current_eval( *eval_info );

  last_time_used = get_elapsed_time();
  if ( update_all ) {
    total_time += last_time_used;
    add_counter( &total_evaluations, &evaluations );
    add_counter( &total_nodes, &nodes );
  }

  clear_panic_abort();

  /* Write the contents of the status buffer to the log file. */

  if ( move_type == BOOK_MOVE ) {
    eval_str = produce_eval_text( *eval_info, FALSE );
    if ( log_file )
      fprintf( log_file, "%s: %c%c  %s\n", MOVE_CHOICE_TEXT,
	       TO_SQUARE( curr_move ), eval_str );
    free( eval_str );
  }
  else if ( log_file )
    display_status( log_file, TRUE );

  /* Write the principal variation, if available, to the log file
     and, optionally, to screen. */

  if ( !get_ponder_move() ) {
    complete_pv( side_to_move );
    if ( display_pv && echo )
      display_optimal_line( stdout ); 
    if ( log_file )
      display_optimal_line( log_file );
  }

  if ( log_file )
    fclose( log_file );

  return curr_move;
}


/*
  GET_SEARCH_STATISTICS
  Returns some statistics about the last search made.
*/

void
get_search_statistics( int *max_depth, double *node_count ) {
  *max_depth = max_depth_reached;
  if ( prefix_move != 0 )
    (*max_depth)++;
  adjust_counter( &nodes );
  *node_count = counter_value( &nodes );
}


/*
  GET_PV
  Returns the principal variation.
*/

int
get_pv( int *destin ) {
  int i;

  if ( prefix_move == 0 ) {
    for ( i = 0; i < pv_depth[0]; i++ )
      destin[i] = pv[0][i];
    return pv_depth[0];
  }
  else {
    destin[0] = prefix_move;
    for ( i = 0; i < pv_depth[0]; i++ )
      destin[i + 1] = pv[0][i];
    return pv_depth[0] + 1;
  }
}
