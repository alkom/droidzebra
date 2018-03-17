/*
   File:           display.c

   Created:        July 10, 1997

   Modified:       November 23, 2001

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       Some I/O routines.
*/



#include "porting.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constant.h"
#include "display.h"
#include "eval.h"
#include "globals.h"
#include "macros.h"
#include "safemem.h"
#include "search.h"
#include "texts.h"
#include "timer.h"



/* Global variables */

int echo;
int display_pv;



/* Local variables */

static char *black_player = NULL;
static char *white_player = NULL;
static char status_buffer[256], sweep_buffer[256];
static char stored_status_buffer[256];
static double black_eval = 0.0;
static double white_eval = 0.0;
static double last_output = 0.0;
static double interval1, interval2;
static int black_time, white_time;
static int current_row;
static int status_modified = FALSE;
static int sweep_modified = FALSE;
static int timed_buffer_management = TRUE;
static int status_pos, sweep_pos;
static int *black_list, *white_list;



/*
   DUMPCH
   Reads a character off standard input and terminates the program
   if the character typed is ' '.
*/

void
dumpch( void ) {
  char ch;

  ch = getc( stdin );
  if ( ch == ' ' )
    exit( EXIT_FAILURE );
}


/*
  SET_NAMES
  SET_TIMES
  SET_EVALS
  SET_MOVE_LIST
  Specify some information to be output along with the
  board by DISPLAY_BOARD.
*/

void
set_names( const char *black_name, const char *white_name ) {
  if ( black_player != NULL )
    free( black_player );
  if ( white_player != NULL )
    free( white_player );
  black_player = strdup( black_name );
  white_player = strdup( white_name );
}

void
set_times( int black, int white ) {
  black_time = black;
  white_time = white;
}

void
set_evals( double black, double white ) {
  black_eval = black;
  white_eval = white;
}

void
set_move_list( int *black, int *white, int row ) {
  black_list = black;
  white_list = white;
  current_row = row;
}


/*
   DISPLAY_BOARD
   side_to_move = the player whose turn it is
   black_moves = a list of black moves so far
   white_moves = a list of white moves so far
   current_row = the row of the score sheet

   The board is displayed using '*' for black and 'O' for white.
*/

void
display_board( FILE *stream, int *board, int side_to_move,
	       int give_game_score, int give_time, int give_evals ) {
  char buffer[16];
  int i, j;
  int written;
  int first_row, row;
#define SPACING              22
#define MARGIN               "      "

#ifndef TEXT_BASED
  if ( stream == stdout )
    return;
#endif

  if ( side_to_move == BLACKSQ )
    first_row = MAX( 0, current_row - 8 );
  else
    first_row = MAX( 0, current_row - 7 );
   
  buffer[15] = 0;
  fputs( "\n", stream );
  fprintf( stream, "%s   a b c d e f g h\n", MARGIN );
  fputs( "\n", stream );
  for ( i = 1; i <= 8; i++ ) {
    for ( j = 0; j < 15; j++ )
      buffer[j] = ' ';
    for ( j = 1; j <= 8; j++ ) {
      switch ( board[10 * i + j] ) {
      case BLACKSQ:
	buffer[2 * (j - 1)] = '*';
	break;
      case WHITESQ:
	buffer[2 * (j - 1)] = 'O';
	break;
      default:
	buffer[2 * (j - 1)] = ' ';
	break;
      }
    }
    fprintf( stream, "%s%d  %s      ", MARGIN, i, buffer );
    written = 0;
    if ( i == 1 ) {
      written += fprintf( stream, "%-9s", BLACK_TEXT );
      if ( black_player != NULL )
	written += fprintf( stream, "%s", black_player );
    }
    if ( (i == 2) && give_time )
      written += fprintf( stream, "         %02d:%02d",
			  black_time / 60, black_time % 60 );
    if ( i == 3 ) {
      if ( side_to_move == BLACKSQ )
	written += fprintf( stream, " (*)  " );
      else if ( give_evals && (black_eval != 0.0) ) {
	if ( (black_eval >= 0.0) && (black_eval <= 1.0) )
	  written += fprintf( stream, "%-6.2f", black_eval );
        else
	  written += fprintf( stream, "%+-6.2f", black_eval );
      }
      else
	written += fprintf( stream, "      " );
      written += fprintf( stream, "   %d %s", disc_count( BLACKSQ ),
			  DISCS_TEXT);
    }
    if ( i == 5 ) {
      written += fprintf( stream, "%-9s", WHITE_TEXT );
      if ( white_player != NULL )
	written += fprintf( stream, "%s", white_player );
    }
    if ( (i == 6) && give_time )
      written += fprintf( stream, "         %02d:%02d",
			  white_time / 60, white_time % 60 );
    if ( i == 7 ) {
      if ( side_to_move == WHITESQ )
	written += fprintf( stream, " (O)  " );
      else if ( give_evals && (white_eval != 0.0) ) {
	if ( (white_eval >= 0.0) && (white_eval <= 1.0) )
	  written += fprintf( stream, "%-6.2f", white_eval );
        else
	  written += fprintf( stream, "%+-6.2f", white_eval );
      }
      else
	written += fprintf( stream, "      " );
      written += fprintf( stream, "   %d %s", disc_count( WHITESQ ),
			  DISCS_TEXT);
    }
    if ( give_game_score ) {
      fprintf( stream, "%*s", SPACING - written, "" );
      row = first_row + (i - 1);
      if ( (row < current_row) ||
	   ((row == current_row) && (side_to_move == WHITESQ)) ) {
	fprintf( stream, "%2d. ", row + 1 );
	if ( black_moves[row] == PASS )
	  fprintf( stream, "- " );
	else
	  fprintf( stream, "%c%c", TO_SQUARE( black_moves[row] ) );
	fprintf( stream, "  " );
	if ( (row < current_row) ||
	     ((row == current_row) && (side_to_move == BLACKSQ)) ) {
	  if ( white_moves[row] == PASS )
	    fprintf( stream, "- " );
	  else
	    fprintf( stream, "%c%c", TO_SQUARE( white_moves[row] ) );
	}
      }
    }
    fputs( "\n", stream );
  }
  fputs( "\n", stream );
}


/*
  DISPLAY_MOVE
  Outputs a move or a pass to STREAM.
*/

void
display_move( FILE *stream, int move ) {
  if ( move == PASS )
    fprintf( stream, "--" );
  else
    fprintf( stream, "%c%c", TO_SQUARE( move ) );
}


/*
   DISPLAY_OPTIMAL_LINE
   Displays the principal variation found during the tree search.
*/

void
display_optimal_line( FILE *stream ) {
  int i;

  if ( full_pv_depth == 0 )
    return;

#ifndef TEXT_BASED
  if ( stream == stdout )
    return;
#endif
  fprintf( stream, "%s: ", PV_ABBREV );
  for ( i = 0; i < full_pv_depth; i++ ) {
    if ( i % 25 != 0 )
      fputc( ' ', stream );
    else
      if ( i > 0 )
	fprintf( stream, "\n    " );
    display_move( stream, full_pv[i] );
  }
  fputs( "\n", stream );
}


/*
  SEND_STATUS
  Store information about the last completed search.
*/

void
send_status( const char *format, ... ) {
  int written;
  va_list arg_ptr;

  va_start( arg_ptr, format );
  written = vsprintf( status_buffer + status_pos, format, arg_ptr );
  status_pos += written;
  status_modified = TRUE;
  va_end( arg_ptr );
}


/*
  SEND_STATUS_TIME
  Sends the amount of time elapsed to SEND_STATUS.
  The purpose of this function is to unify the format for
  the time string.
*/

void
send_status_time( double elapsed_time ) {
  if ( elapsed_time < 10000.0 )
    send_status( "%6.1f %c", elapsed_time, SECOND_ABBREV );
  else
    send_status( "%6d %c", (int) ceil( elapsed_time ), SECOND_ABBREV );
  send_status( "  " );
}


/*
  SEND_STATUS_NODES
  Pipes the number of nodes searched to SEND_STATUS.
  The purpose of this function is to unify the format for
  the number of nodes.
*/

void
send_status_nodes( double node_count ) {
  if ( node_count < 1.0e8 )
    send_status( "%8.0f  ", node_count );
  else {
    if ( node_count < 1.0e10 )
      send_status( "%7.0f%c  ", node_count / 1000.0, KILO_ABBREV );
    else {
      if ( node_count < 1.0e13 )
	send_status( "%7.0f%c  ", node_count / 1000000.0, MEGA_ABBREV );
      else
	send_status( "%7.0f%c  ", node_count / 1000000000.0, GIGA_ABBREV );
    }
  }
}


/*
  SEND_STATUS_PV
  Pipes the principal variation to SEND_STATUS.
*/

void
send_status_pv( int *pv, int max_depth ) {
  int i;

  for ( i = 0; i < MIN( max_depth, 5 ); i++ )
    if ( i < pv_depth[0] )
      send_status( "%c%c ", TO_SQUARE( pv[i] ) );
    else
      send_status( "   " );
  send_status( " " );
}


/*
  CLEAR_STATUS
  Clear the current status information.
*/

void
clear_status( void ) {
  status_pos = 0;
  status_buffer[0] = 0;
  status_modified = TRUE;
}


/*
  DISPLAY_STATUS
  Output and clear the stored status information.
*/

void
display_status( FILE *stream, int allow_repeat ) {
  if ( ((status_pos != 0) || allow_repeat ) &&
       (strlen( status_buffer ) > 0) ) {
#ifndef TEXT_BASED
    if ( stream != stdout )
#endif
    fprintf( stream, "%s\n", status_buffer );
    strcpy( stored_status_buffer, status_buffer );
  }
  status_pos = 0;
}


const char *
get_last_status( void ) {
  return stored_status_buffer;
}



/*
  SEND_SWEEP
  Store information about the current search.
*/

void
send_sweep( const char *format, ... ) {
  int written;
  va_list arg_ptr;

  va_start( arg_ptr, format );
  written = vsprintf( sweep_buffer + sweep_pos, format, arg_ptr );
  sweep_pos += written;
  sweep_modified = TRUE;
  va_end( arg_ptr );
}


/*
  CLEAR_SWEEP
  Clear the search information.
*/

void
clear_sweep( void ) {
  sweep_pos = 0;
  sweep_buffer[0] = 0;
  sweep_modified = TRUE;
}


/*
  DISPLAY_SWEEP
  Display and clear the current search information.
*/

void
display_sweep( FILE *stream ) {
#ifndef TEXT_BASED
  if ( stream != stdout )
#endif
  if ( sweep_pos != 0 )
    fprintf( stream, "%s\n", sweep_buffer );
  sweep_modified = FALSE;
}


/*
  RESET_BUFFER_DISPLAY
  Clear all buffers and initialize time variables.
*/

void
reset_buffer_display( void ) {
  /* The first two Fibonacci numbers */
  clear_status();
  clear_sweep();
  interval1 = 0.0;
  interval2 = 1.0;
  last_output = get_real_timer();
}


/*
  DISPLAY_BUFFERS
  If an update has happened and the last display was long enough ago,
  output relevant buffers.
*/

void
display_buffers( void ) {
  double timer;
  double new_interval;

  timer = get_real_timer();
  if ( (timer - last_output >= interval2) || !timed_buffer_management ) {
    display_status( stdout, FALSE );
    status_modified = FALSE;
    if ( timer - last_output >= interval2 ) {
      if ( sweep_modified )
	display_sweep( stdout );
      last_output = timer;
      /* Display the sweep at Fibonacci-spaced times */
      new_interval = interval1 + interval2;
      interval1 = interval2;
      interval2 = new_interval;
    }
  }
}


/*
  TOGGLE_SMART_BUFFER_MANAGEMENT
  Allow the user between timed, "smart", buffer management
  and the simple "you asked for it, you got it"-approach which
  displays everything that is fed to the buffer.
*/

void 
toggle_smart_buffer_management( int use_smart ) {
  timed_buffer_management = use_smart;
}


#define MAX_STRING_LEN           32


/*
  PRODUCE_EVAL_TEXT
  Convert a result descriptor into a string intended for output.
*/

char *
produce_eval_text( EvaluationType eval_info, int short_output ) {
  char *buffer;
  double disk_diff;
  int len;
  int int_confidence;

  buffer = (char *) safe_malloc( MAX_STRING_LEN );
  len = 0;
  switch ( eval_info.type ) {

  case MIDGAME_EVAL:
    if ( eval_info.score >= MIDGAME_WIN )
      len = sprintf( buffer, WIN_TEXT );
    else if ( eval_info.score <= -MIDGAME_WIN )
      len = sprintf( buffer, LOSS_TEXT );
    else {
      disk_diff = eval_info.score / 128.0;
      if ( short_output )
	len = sprintf( buffer, "%+.2f", disk_diff );
      else
	len = sprintf( buffer, "%+.2f %s", disk_diff, DISCS_TEXT );
    }
    break;

  case EXACT_EVAL:
    if ( short_output )
      len = sprintf( buffer, "%+d", eval_info.score >> 7 );
    else
      if ( eval_info.score > 0 )
	len = sprintf( buffer, "%s %d-%d", WIN_BY_TEXT,
		       32 + (eval_info.score >> 8),
		       32 - (eval_info.score >> 8) );
      else if ( eval_info.score < 0 )
        len = sprintf( buffer, "%s %d-%d", LOSS_BY_TEXT,
		       32 - (abs( eval_info.score ) >> 8),
		       32 + (abs( eval_info.score ) >> 8) );
      else
        len = sprintf( buffer, DRAW_TEXT );
    break;

  case WLD_EVAL:
    if ( short_output )
      switch ( eval_info.res ) {

      case WON_POSITION:
	len = sprintf( buffer, WIN_TEXT );
	break;

      case DRAWN_POSITION:
	len = sprintf( buffer, DRAW_TEXT );
	break;

      case LOST_POSITION:
	len = sprintf( buffer, LOSS_TEXT );
	break;

      case UNSOLVED_POSITION:
	len = sprintf( buffer, "???" );
	break;
      }
    else
      switch ( eval_info.res ) {

      case WON_POSITION:
	if ( eval_info.score != +1 * 128 )  /* Lower bound on win */
	  len = sprintf( buffer, "%s %d-%d", WIN_BY_BOUND_TEXT,
			 32 + (eval_info.score >> 8),
			 32 - (eval_info.score >> 8) );
	else
	  len = sprintf( buffer, WIN_TEXT );
	break;

      case DRAWN_POSITION:
	len = sprintf( buffer, DRAW_TEXT );
	break;

      case LOST_POSITION:
	if ( eval_info.score != -1 * 128 )  /* Upper bound on win */
	  len = sprintf( buffer, "%s %d-%d", LOSS_BY_BOUND_TEXT,
			 32 - (abs( eval_info.score ) >> 8),
			 32 + (abs( eval_info.score ) >> 8) );
	else
	  len = sprintf( buffer, LOSS_TEXT );
	break;

      case UNSOLVED_POSITION:
	len = sprintf( buffer, "???" );
	break;
      }
    break;

  case SELECTIVE_EVAL:
    int_confidence = (int)  floor( eval_info.confidence * 100.0 );
    switch ( eval_info.res ) {
    case WON_POSITION:
      if ( eval_info.score != +1 * 128 )
	len = sprintf( buffer, "%+d @ %d%%", eval_info.score / 128,
		       int_confidence );
      else
	len = sprintf( buffer, "%s @ %d%%", WIN_TEXT, int_confidence );
      break;
    case DRAWN_POSITION:
      len = sprintf( buffer, "%s @ %d%%", DRAW_TEXT, int_confidence );
      break;
    case LOST_POSITION:
      if ( eval_info.score != -1 * 128 )
	len = sprintf( buffer, "%+d @ %d%%", eval_info.score >> 7,
		       int_confidence );
      else
	len = sprintf( buffer, "%s @ %d%%", LOSS_TEXT, int_confidence );
      break;
    case UNSOLVED_POSITION:
      if ( eval_info.score == 0 )
	len = sprintf( buffer, "Draw @ %d%%", int_confidence );
      else
	len = sprintf( buffer, "%+d @ %d%%", eval_info.score / 128,
		       int_confidence );
      break;
    }
    break;

  case FORCED_EVAL:
    if ( short_output )
      len = sprintf( buffer, "-" );
    else
      len = sprintf( buffer, FORCED_TEXT );
    break;

  case PASS_EVAL:
    if ( short_output )
      len = sprintf( buffer, "-" );
    else
      len = sprintf( buffer, PASS_TEXT );
    break;

  case INTERRUPTED_EVAL:
    len = sprintf( buffer, INCOMPLETE_TEXT );
    break;

  case UNDEFINED_EVAL:
    /* We really want to perform len = sprintf( buffer, "" ); */
    buffer[0] = 0;
    len = 0;
    break;

  case UNINITIALIZED_EVAL:
    len = sprintf( buffer, "--" );
    break;
  }
  if ( eval_info.is_book )
    len += sprintf( buffer + len, " (%s)", BOOK_TEXT );

  return buffer;
}
