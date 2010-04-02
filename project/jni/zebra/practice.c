/*
   File:         practice.c

   Created:      January 29, 1998
   
   Modified:     July 12, 1999

   Author:       Gunnar Andersson (gunnar@radagast.se)

   Contents:     A small utility which enables the user to browse
                 an opening book file.
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constant.h"
#include "display.h"
#include "game.h"
#include "globals.h"
#include "macros.h"
#include "moves.h"
#include "osfbook.h"
#include "patterns.h"
#include "search.h"


#define RANDOM         1
#define HASHBITS       5


int
main( int argc, char *argv[] ) {
  char *book_name;
  char *buffer;
  const char *opening_name;
  char move_string[10];
  int i;
  int side_to_move;
  int quit, repeat;
  int command, move;
  int old_stm[61];
  int move_list[61];
  int row[61];

  if ( argc == 2 )
    book_name = argv[1];
  else if ( argc == 1 )
    book_name = strdup( "book.bin" );
  else {
    puts( "Usage:\n  [practice <book file>]" );
    puts( "\nDefault book file is book.bin\n" );
    puts( "Commands: When prompted for a move, a legal move may" );
    puts( "          a number of moves to take back must be entered." );
    puts( "To exit the program, type 'quit'." );
    puts( "" );
    printf( "Gunnar Andersson, %s\n", __DATE__ );
    exit( EXIT_FAILURE );
  }
   
  init_osf( TRUE );
  read_binary_database( book_name );

  game_init( NULL, &side_to_move );
  toggle_human_openings( FALSE );

  set_names( "", "" );

  quit = FALSE;
  while ( !quit ) {
    int val0, val1, orientation;

    set_move_list( black_moves, white_moves, score_sheet_row );
    opening_name = find_opening_name();
    if ( opening_name != NULL )
      printf( "\nOpening: %s\n", opening_name );
    get_hash( &val0, &val1, &orientation );
    display_board( stdout, board, side_to_move, TRUE, FALSE, FALSE );
    printf( "Book hash: %d %d (%d)\n\n", val0, val1, orientation );
    extended_compute_move( side_to_move, FALSE, TRUE, 6, 16, 18 );
    printf( "Scores for the %d moves:\n", get_evaluated_count() );
    for ( i = 0; i < get_evaluated_count(); i++ ) {
      buffer = produce_eval_text( get_evaluated(i).eval, FALSE );
      printf( "   %c%c   %s\n", TO_SQUARE( get_evaluated(i).move ), buffer );
      free( buffer );
    }
    puts( "" );
    do {
      repeat = FALSE;
      if ( side_to_move == BLACKSQ )
	printf( "Black move: " );
      else
	printf( "White move: " );
      fflush( stdout );
      scanf( "%s", move_string );
      if ( !strcmp( move_string, "quit" ) )
	quit = TRUE;
      else {
	command = atoi( move_string );
	if ( (command >= 1) && (command <= disks_played) ) {
	  for ( i = 1; i <= command; i++ )
	    unmake_move( old_stm[disks_played - 1],
			 move_list[disks_played - 1] );
	  side_to_move = old_stm[disks_played];
	  score_sheet_row = row[disks_played];
	}
	else if ( command != 0 ) {
	  printf( "Can't back up %d moves\n\n", command );
	  repeat = TRUE;
	}
	else {
	  generate_all( side_to_move );
	  move = (move_string[0] - 'a' + 1) + 10 * (move_string[1] - '0');
	  if ( (move_string[0] >= 'a') && (move_string[0] <= 'h') &&
	       (move_string[1] >= '1') && (move_string[1] <= '8') &&
	       valid_move( move, side_to_move ) ) {
	    old_stm[disks_played] = side_to_move;
	    row[disks_played] = score_sheet_row;
	    move_list[disks_played] = move;
	    (void) make_move( side_to_move, move, TRUE );
	    if ( side_to_move == BLACKSQ )
	      black_moves[++score_sheet_row] = move;
	    else
	      white_moves[score_sheet_row] = move;
	    side_to_move = OPP( side_to_move );
	  }
	  else {
	    puts( "Move infeasible\n" );
	    repeat = TRUE;
	  }
	}
      }
    } while ( repeat );
  }

  return EXIT_SUCCESS;
}
