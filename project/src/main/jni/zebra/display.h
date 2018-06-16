/*
   File:         display.h

   Created:      July 10, 1997

   Modified:     November 17, 2002

   Author:       Gunnar Andersson (gunnar@radagast.se)

   Contents:     Declarations of the screen output functions.
*/



#ifndef DISPLAY_H
#define DISPLAY_H



#include "search.h"



#ifdef __cplusplus
extern "C" {
#endif



/* Flag variable, non-zero if output should be written to stdout. */
extern int echo;

/* Flag variable, non-zero if the principal variation is to be
   displayed. */
extern int display_pv;



void
dumpch( void );

void
set_names( const char *black_name, const char *white_name );

void
set_times( int black, int white );

void
set_evals( double black, double white );

void
set_move_list( int *black, int *white, int row );

void
display_board( FILE *stream, int *board, int side_to_move,
	       int give_game_score, int give_time, int give_evals );

void
display_move( FILE *stream, int move );

void
display_optimal_line( FILE *stream );

void
send_status( const char *format, ... );

void
send_status_time( double elapsed_time );

void
send_status_pv( int *pv, int max_depth );

void
send_status_nodes( double node_count );

const char *
get_last_status( void );

void
clear_status( void );

void
display_status( FILE *stream, int allow_repeat );

void
send_sweep( const char *format, ... );

void
clear_sweep( void );

void
display_sweep( FILE *stream );

void
reset_buffer_display( void );

void
toggle_smart_buffer_management( int use_smart );

void
display_buffers( void );

char *
produce_eval_text( EvaluationType eval_info, int short_output );



#ifdef __cplusplus
}
#endif



#endif  /* DISPLAY_H */
