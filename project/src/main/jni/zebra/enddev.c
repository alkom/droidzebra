/*
   File:         enddev.c

   Created:      September 1, 2002
   
   Modified:

   Author:       Gunnar Andersson (gunnar@radagast.se)

   Contents:
*/



#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "constant.h"
#include "display.h"
#include "game.h"
#include "globals.h"
#include "hash.h"
#include "learn.h"
#include "macros.h"
#include "moves.h"
#include "myrandom.h"
#include "osfbook.h"
#include "patterns.h"
#include "search.h"
#include "timer.h"



static const int VERBOSE = 1;



static void
read_game( FILE *stream,
	   int *game_moves,
	   int *game_length ) {
  char buffer[1000];

  if ( fgets( buffer, sizeof buffer, stream ) == NULL )
    return;
  else {
    int i;
    char *ch = buffer;

    while ( isalnum( *ch ) )
      ch++;
    *ch = 0;

    *game_length = strlen( buffer ) / 2;
    if ( (*game_length > 60) || (strlen( buffer ) % 2 == 1) ) {
      fprintf( stderr, "Bad move string %s.\n", buffer );
      exit( EXIT_FAILURE );
    }
    for ( i = 0; i < *game_length; i++ ) {
      int col = tolower( buffer[2 * i] ) - 'a' + 1;
      int row = buffer[2 * i + 1] - '0';
      if ( (col < 1) || (col > 8) || (row < 1) || (row > 8) )
	fprintf( stderr, "Unexpected character in move string" );
      game_moves[i] = 10 * row + col;
    }
  }
}



int
main( int argc, char *argv[] ) {
  double rand_prob;
  const int hash_bits = 20;
  const int earliest_dev = 38;
  const int latest_dev = 52;
  int first_allowed_dev;
  int side_to_move;
  int last_was_pass;
  int restart, in_branch;
  int games_read;
  int game_length;
  int game_moves[60];
  FILE *stream;

  if ( (argc != 3) ||
       (sscanf( argv[2], "%lf", &rand_prob ) != 1) ) {
    fputs( "Usage:\n  enddev <game file> <randomization prob.>\n", stderr );
    exit( EXIT_FAILURE );
  }

  stream = fopen( argv[1], "r" );
  if ( stream == NULL ) {
    fprintf( stderr, "Cannot open %s for reading.\n", argv[1] );
    exit( EXIT_FAILURE );
  }

  init_learn( "book.bin", TRUE );
  global_setup( 0, hash_bits );
  games_read = 0;

  first_allowed_dev = earliest_dev;
  last_was_pass = FALSE;
  restart = TRUE;
  in_branch = FALSE;

  while ( 1 ) {
    if ( restart ) {
      if ( in_branch )
	game_length = disks_played;
      else {
	read_game( stream, game_moves, &game_length );
	if ( games_read % 1000 == 0 )
	  fprintf( stderr, "%d games processed\n", games_read );
	if ( feof( stream ) )
	  break;
	games_read++;

	first_allowed_dev = earliest_dev;
      }

      game_init( NULL, &side_to_move );
      setup_hash( TRUE );
      last_was_pass = FALSE;
      restart = FALSE;
      in_branch = FALSE;
    }

    assert( disc_count( BLACKSQ ) + disc_count( WHITESQ ) ==
	    disks_played + 4 );

    determine_hash_values( side_to_move, board );

    generate_all( side_to_move );
    if ( move_count[disks_played] == 0 ) {
      if ( last_was_pass ) {  /* Game over? */
	restart = TRUE;
	if ( in_branch ) {
	  int i;
	  for ( i = 0; i < disks_played; i++ )
	    printf( "%c%c", TO_SQUARE( game_moves[i] ) );
	  puts( "" );
	}
      }
      else {
	side_to_move = OPP( side_to_move );  /* Must pass. */
	last_was_pass = TRUE;
      }
    }
    else {
      int move;

      start_move( 100000, 0, disks_played + 4 );

      if ( in_branch ) {
	EvaluationType ev_info;
	move = compute_move( side_to_move, FALSE, 100000, 0, FALSE,
			     FALSE, 8, 60, 60, TRUE, &ev_info );
      }
      else {
	move = game_moves[disks_played];

	if ( (disks_played >= first_allowed_dev) &&
	     (disks_played <= latest_dev) &&
	     (0.0001 * ((my_random() >> 9) % 10000) < rand_prob) ) {
	  int i;
	  int best_score;
	  int accum_prob, total_prob;
	  int rand_val;
	  struct {
	    int move;
	    int score;
	    int prob;
	  } choices[60];

	  if ( VERBOSE )
	    fprintf( stderr, "Evaluating moves in game %d after %d moves:\n",
		     games_read, disks_played );

	  extended_compute_move( side_to_move, FALSE, FALSE, 8, 60, 60 );

	  assert( get_evaluated_count() == move_count[disks_played] );

	  best_score = -INFINITE_EVAL;
	  for ( i = 0; i < get_evaluated_count(); i++ ) {
	    EvaluatedMove ev_info = get_evaluated( i );
	    choices[i].move = ev_info.move;
	    choices[i].score = ev_info.eval.score / 128;

	    best_score = MAX( choices[i].score, best_score );
	  }

	  total_prob = 0;
	  for ( i = 0; i < get_evaluated_count(); i++ ) {
	    choices[i].prob =
	      100000 * exp( (choices[i].score - best_score) * 0.2 ) + 1;
	    if ( choices[i].move == move )  /* Encourage deviations. */
	      choices[i].prob = choices[i].prob / 2;
	    total_prob += choices[i].prob;
	  }

	  if ( VERBOSE )
	    for ( i = 0; i < get_evaluated_count(); i++ )
	      fprintf( stderr, "  %c%c  %+3d    p=%.03f\n",
		       TO_SQUARE( choices[i].move ), choices[i].score,
		       choices[i].prob / (double) total_prob );

	  rand_val = (my_random() >> 4) % (total_prob + 1);
	  accum_prob = 0;
	  i = 0;
	  while ( (accum_prob += choices[i].prob) < rand_val )
	    i++;

	  assert( i < move_count[disks_played] );

	  move = choices[i].move;
	  if ( VERBOSE )
	    fprintf( stderr, "  %c%c chosen, %c%c in game\n",
		     TO_SQUARE( move ),
		     TO_SQUARE( game_moves[disks_played] ) );

	  if ( move != game_moves[disks_played] ) {
	    in_branch = TRUE;
	    first_allowed_dev = disks_played + 1;
	    if ( VERBOSE )
	      fputs( "  branching\n", stderr );
	  }
	}
      }

      if ( !valid_move( move, side_to_move ) ) {
	fprintf( stderr, "Game #%d contains illegal move %d @ #%d.\n",
		 games_read, move, disks_played );
	display_board( stderr, board, side_to_move, FALSE, FALSE, FALSE );
	exit( EXIT_FAILURE );
      }

      game_moves[disks_played] = move;

      if ( make_move( side_to_move, move, TRUE ) == 0 ) {
	fprintf( stderr, "Internal error: 'Legal' move flips no discs.\n" );
	exit( EXIT_FAILURE );
      }

      side_to_move = OPP( side_to_move );
      last_was_pass = FALSE;
    }
  }

  fprintf( stderr, "%d games processed\n", games_read );

  return EXIT_SUCCESS;
}
