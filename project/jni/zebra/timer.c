/*
   File:         timer.c

   Created:      September 28, 1997
   
   Modified:     November 13, 2001

   Author:       Gunnar Andersson (gunnar@radagast.se)

   Contents:     The time control mechanism.
                 Note: The Cron system calls were written by Niclas Een.
*/



/* CRON_SUPPORTED should be enabled when Zebra is compiled for
   a Unix system which supports the Cron daemon. */

#ifdef __linux__
#define CRON_SUPPORTED
#endif

/* GTC_SUPPORTED should be enabled when Zebra is compiled for
   Windows 95/98/NT and the compiler supports the function
   GetTickCount() and keeps the definition in <windows.h>. */

#include "porting.h"

#define GTC_SUPPORTED



#include <math.h>
#include <stdio.h>

#ifndef _WIN32_WCE
#include <time.h>
#endif

#ifdef CRON_SUPPORTED
#include <sys/time.h>
#else
#ifdef GTC_SUPPORTED

#include <windows.h>
#endif
#endif

#include "constant.h"
#include "macros.h"
#include "timer.h"



/* By default start a new iteration if the previous took less than
   70% of the alotted time */
#define DEFAULT_SEARCH           0.7

/* Increased time allowance for time abort after successful prediction */
#define PONDER_FACTOR            1.5

/* Never use more than 1.6 times the scheduled time for the current move */
#define PANIC_FACTOR             (1.6 / DEFAULT_SEARCH)

/* Always have at least 10 seconds left on the clock */
#define SAFETY_MARGIN            10.0



/* Global variables */

double last_panic_check;
int ponder_depth[100];
int current_ponder_depth;

int frozen_ponder_depth;

/* Local variables */

double current_ponder_time, frozen_ponder_time;
static double panic_value;
static double time_per_move;
static double start_time, total_move_time;
static double ponder_time[100];
static int panic_abort;
static int do_check_abort = TRUE;

#ifdef CRON_SUPPORTED
static struct itimerval saved_real_timer;
#else
#ifdef GTC_SUPPORTED
static int init_ticks;
#else
static time_t init_time;
#endif
#endif



/*
  RESET_REAL_TIMER
*/

void
reset_real_timer( void ) {
#ifdef CRON_SUPPORTED
  getitimer( ITIMER_REAL, &saved_real_timer );
#else
#ifdef GTC_SUPPORTED
  init_ticks = GetTickCount();
#else
  time( &init_time);
#endif
#endif
}


/*
  INIT_TIMER
  Initializes the timer. This is really only needed when
  CRON_SUPPORTED is defined; in this case the cron daemon
  is used for timing.
*/

void
init_timer( void ) {
#ifdef CRON_SUPPORTED
  struct itimerval t;

  t.it_value.tv_sec  = 1000000;
  t.it_value.tv_usec = 0;
  t.it_interval.tv_sec  = 0;
  t.it_interval.tv_usec = 0;
  saved_real_timer = t;
  setitimer( ITIMER_REAL, &t, NULL );
#endif
  reset_real_timer();
}


/*
  GET_REAL_TIMER
  Returns the time passed since the last call to init_timer() or reset_timer().
*/

double
get_real_timer( void ) {
#ifdef CRON_SUPPORTED
  struct itimerval t;

  getitimer( ITIMER_REAL, &t );
  return (saved_real_timer.it_value.tv_sec  - t.it_value.tv_sec)
    + (saved_real_timer.it_value.tv_usec - t.it_value.tv_usec) / 1000000.0;
#else
#ifdef GTC_SUPPORTED
  int ticks;

  ticks = GetTickCount();

  return (ticks - init_ticks) / ((double) CLK_TCK);
#else
  time_t curr_time;

  time( &curr_time );
  return (double) (curr_time - init_time);
#endif
#endif
}


/*
  SET_DEFAULT_PANIC
  Sets the panic timeout when search immediately must stop.
*/

void
set_default_panic( void ) {
  panic_value = time_per_move * PANIC_FACTOR / total_move_time;
}


/*
  DETERMINE_MOVE_TIME
  Initializes the timing subsystem and allocates time for the current move.
*/

void
determine_move_time( double time_left, double incr, int discs ) {
  double time_available;
  int moves_left;

  frozen_ponder_time = current_ponder_time;
  frozen_ponder_depth = current_ponder_depth;
  moves_left = MAX( ((65 - discs) / 2) - 5, 2 );
  time_available = time_left + frozen_ponder_time + 
    moves_left * incr - SAFETY_MARGIN;
  if ( time_available < 1.0 )
    time_available = 1.0;
  time_per_move = (time_available / (moves_left + 1)) * DEFAULT_SEARCH;
  if ( time_per_move > time_left / 4 )
    time_per_move = time_left / 4;
  if ( time_per_move > time_left + frozen_ponder_time )
    time_per_move = time_left / 4;
  if ( time_per_move == 0 )
    time_per_move = 1;
  set_default_panic();
}


/*
  GET_ELAPSED_TIME
  Returns the time passed since START_MOVE was called.
  This is the actual time, not adjusted for pondering.
*/

INLINE double
get_elapsed_time( void ) {
  return fabs( get_real_timer() - start_time );
}


/*
  START_MOVE
*/

INLINE void
start_move( double in_total_time, double increment, int discs ) {
  /*
    This is a possible approach in time control games with increment:
      available_time = in_total_time + increment * (65 - discs) / 2.0;
    Some correction might be necessary anyway, so let's skip it for now.
  */

  /* This won't work well in games with time increment, but never mind */

  total_move_time = MAX( in_total_time - SAFETY_MARGIN, 0.1 );
  panic_abort = FALSE;
  start_time = get_real_timer();
}


/*
  SET_PANIC_THRESHOLD
  Specifies the fraction of the remaining time (VALUE must lie in [0,1])
  before the panic timeout kicks in.
*/

void
set_panic_threshold( double value ) {
  panic_value = value;
}


/*
  CHECK_PANIC_ABORT
  Checks if the alotted time has been used up and in this case
  sets the PANIC_ABORT flags.
*/

void
check_panic_abort( void ) {
  double curr_time;
  double adjusted_total_time;

  curr_time = get_elapsed_time();
  adjusted_total_time = total_move_time;

#ifndef _WIN32_WCE
  if ( do_check_abort &&
       (curr_time >= panic_value * adjusted_total_time ) )
    panic_abort = TRUE;
#endif
}


/*
  CHECK_THRESHOLD
  Checks if a certain fraction of the panic time has been used.
*/

int
check_threshold( double threshold ) {
  double curr_time;
  double adjusted_total_time;

  curr_time = get_elapsed_time();
  adjusted_total_time = total_move_time;
  return do_check_abort &&
    (curr_time >= panic_value * threshold * adjusted_total_time);
}


/*
  TOGGLE_ABORT_CHECK
  Enables/disables panic time abort checking.
*/

void
toggle_abort_check( int enable ) {
  do_check_abort = enable;
}


/*
  CLEAR_PANIC_ABORT
  Resets the panic abort flag.
*/

INLINE
void
clear_panic_abort( void ) {
  panic_abort = FALSE;
}


/*
  IS_PANIC_ABORT
  Returns the current panic status.
*/

INLINE int
is_panic_abort( void ) {
  return panic_abort;
}


/*
  CLEAR_PONDER_TIMES
  Clears the ponder times for all board positions and resets
  the time associated with the last move actually made.
*/

void
clear_ponder_times( void ) {
  int i;

  for (i = 0; i < 100; i++) {
    ponder_time[i] = 0.0;
    ponder_depth[i] = 0;
  }
  current_ponder_time = 0.0;
  current_ponder_depth = 0;
}


/*
  ADD_PONDER_TIME
  Increases the timer keeping track of the ponder time for
  a certain move.
*/

void
add_ponder_time( int move, double time ) {
  ponder_time[move] += time;
}


/*
  ADJUST_CURRENT_PONDER_TIME
  The ponder time for the move actually made in the position where
  pondering was made is stored.
*/

void
adjust_current_ponder_time( int move ) {
  current_ponder_time = ponder_time[move];
  current_ponder_depth = ponder_depth[move];
#ifdef TEXT_BASED
  printf( "Ponder time: %.1f s\n", current_ponder_time );
  printf( "Ponder depth: %d\n", current_ponder_depth );
#endif
}


/*
  ABOVE_RECOMMENDED
  EXTENDED_ABOVE_RECOMMENDED
  Checks if the time spent searching is greater than the threshold
  where no new iteration should be started.
  The extended version takes the ponder time into account.
*/

INLINE int
above_recommended( void ) {
  return (get_elapsed_time() >= time_per_move);
}

INLINE int
extended_above_recommended(void) {
  return (get_elapsed_time() + frozen_ponder_time >=
	  PONDER_FACTOR * time_per_move);
}
