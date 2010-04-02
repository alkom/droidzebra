/*
   File:          timer.h

   Created:       September 28, 1997
   
   Modified:      August 1, 2002

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The time control mechanism.
*/



#ifndef TIMER_H
#define TIMER_H



#ifdef __cplusplus
extern "C" {
#endif



extern int current_ponder_depth;
extern int ponder_depth[100];

extern double frozen_ponder_time;

extern int frozen_ponder_depth;

/* Holds the value of the variable NODES the last time the
   timer module was called to check if a panic abort occured. */
extern double last_panic_check;



void
determine_move_time( double time_left, double incr, int discs );

void
start_move( double in_total_time, double increment, int discs );

void
set_panic_threshold( double value );

void
check_panic_abort( void );

int
check_threshold( double threshold );

void
clear_panic_abort( void );

int
is_panic_abort( void );

void
toggle_abort_check( int enable );

void
init_timer( void );

double
get_real_timer( void );

void
reset_real_timer( void );

double
get_elapsed_time( void );

void
clear_ponder_times( void );

void
add_ponder_time( int move, double time );

void
adjust_current_ponder_time( int move );

int
above_recommended( void );

int
extended_above_recommended( void );



#ifdef __cplusplus
}
#endif



#endif  /* TIMER_H */
