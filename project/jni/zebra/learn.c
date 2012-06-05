/*
   File:       learn.c

   Created:    November 29, 1999
   
   Modified:   April 29, 2002

   Author:     Gunnar Andersson (gunnar@radagast.se)

   Contents:   The learning module.
*/



#include "porting.h"

#include <stdio.h>
#include <string.h>
#include "constant.h"
#include "end.h"
#include "game.h"
#include "globals.h"
#include "hash.h"
#include "learn.h"
#include "moves.h"
#include "osfbook.h"
#include "patterns.h"
#include "search.h"
#include "timer.h"



#define LEARN_LOG_FILE_NAME         "learn.log"



static char database_name[256];
static int binary_database;
static int learn_depth, cutoff_empty;
static short game_move[61];



/*
   CLEAR_STORED_GAME
   Remove all stored moves.
*/

void
clear_stored_game( void ) {
  int i;

  for ( i = 0; i <= 60; i++ )
    game_move[i] = ILLEGAL;
}



/*
   STORE_MOVE
   Mark the move MOVE as being played after DISKS_PLAYED moves
   had been played.
*/

void
store_move( int disks_played, int move ) {
  game_move[disks_played] = move;
}



/*
   SET_LEARNING_PARAMETERS
   Specify the depth to which deviations are checked for and the number
   of empty squares at which the game is considered over.
*/

void
set_learning_parameters( int depth, int cutoff ) {
  learn_depth = depth;
  cutoff_empty = cutoff;
}



/*
   GAME_LEARNABLE
   Checks if the current game can be learned - i.e. if the moves of the
   game are available and the game was finished or contains enough
   moves to be learned anyway.
*/

int
game_learnable( int finished, int move_count ) {
  int i;
  int moves_available;

  moves_available = TRUE;
  for ( i = 0; (i < move_count) && (i < 60 - cutoff_empty); i++ )
    if ( game_move[i] == ILLEGAL )
      moves_available = FALSE;

  return moves_available && (finished || (move_count >= 60 - cutoff_empty));
}



/*
   INIT_LEARN
   Initialize the learning module.
*/

void
init_learn( const char *file_name, int is_binary ) {
  init_osf( FALSE );
  if ( is_binary )
    read_binary_database( file_name );
  else
    read_text_database( file_name );
  strcpy( database_name, file_name );
  binary_database = is_binary;
}



/*
   LEARN_GAME
   Play through the game and obtain an end result which assumes
   perfect endgame play from both sides. Then add the game to
   the database.
*/

void
learn_game( int game_length, int private_game, int save_database ) {
  int i;
  int dummy;
  int side_to_move;
  int full_solve, wld_solve;

  clear_panic_abort();
  toggle_abort_check( FALSE );

  full_solve = get_earliest_full_solve();
  wld_solve = get_earliest_wld_solve();

  game_init( NULL, &dummy );
  side_to_move = BLACKSQ;
  for ( i = 0; i < game_length; i++ ) {
    generate_all( side_to_move );
    if ( move_count[disks_played] == 0 ) {
      side_to_move = OPP( side_to_move );
      generate_all( side_to_move );
    }
    (void) make_move( side_to_move, game_move[i], TRUE );
    if ( side_to_move == WHITESQ )
      game_move[i] = -game_move[i];
    side_to_move = OPP( side_to_move );
  }

  set_search_depth( learn_depth );
  add_new_game( game_length, game_move, cutoff_empty, full_solve,
		wld_solve, TRUE, private_game );

  if ( save_database ) {
    if ( binary_database )
      write_binary_database( database_name );
    else
      write_text_database( database_name );
  }

  toggle_abort_check( TRUE );
}


/*
  FULL_LEARN_PUBLIC_GAME
  This all-in-one function learns a public game using the
  parameters CUTOFF and DEVIATION_DEPTH with the same
  interpretation as in a call to set_learning_parameters().
*/

void
full_learn_public_game( int length, int *moves, int cutoff,
			int deviation_depth, int exact, int wld ) {
  int i;
  int dummy;
  int side_to_move;
  FILE *stream;

  stream = fopen( LEARN_LOG_FILE_NAME, "a" );
  if ( stream != NULL ) {  /* Write the game learned to a log file. */
    for ( i = 0; i < length; i++ )
      fprintf( stream, "%c%c", TO_SQUARE( moves[i] ) );
    fputs( "\n", stream );
    fclose( stream );
  }

  clear_panic_abort();
  toggle_abort_check( FALSE );

  /* Copy the move list from the caller as it is modified below. */

  for ( i = 0; i < length; i++ )
    game_move[i] = moves[i];

  /* Determine side to move for all positions */

  game_init( NULL, &dummy );
  side_to_move = BLACKSQ;
  for ( i = 0; i < length; i++ ) {
    generate_all( side_to_move );
    if ( move_count[disks_played] == 0 ) {
      side_to_move = OPP( side_to_move );
      generate_all( side_to_move );
    }
    (void) make_move( side_to_move, game_move[i], TRUE );
    if ( side_to_move == WHITESQ )
      game_move[i] = -game_move[i];
    side_to_move = OPP( side_to_move );
  }

  /* Let the learning sub-routine in osfbook update the opening
     book and the dump it to file. */

  set_search_depth( deviation_depth );
  add_new_game( length, game_move, cutoff, exact, wld, TRUE, FALSE );
  if ( binary_database )
    write_binary_database( database_name );
  else
    write_text_database( database_name );

  toggle_abort_check( TRUE );
}
