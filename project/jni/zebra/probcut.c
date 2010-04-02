/*
   File:          probcut.c

   Created:       March 1, 1998

   Modified:      November 24, 2002
   
   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      The initialization of the Multi-ProbCut search parameters.
*/



#include "porting.h"

#include <math.h>

#include "constant.h"
#include "epcstat.h"
#include "pcstat.h"
#include "probcut.h"



/* Global variables */

int use_end_cut[61];
int end_mpc_depth[61][4];
DepthInfo mpc_cut[MAX_CUT_DEPTH + 1];



/*
   SET_PROBCUT
   Specifies that searches to depth DEPTH are to be
   estimated using searches to depth SHALLOW_DEPTH.
*/

static void
set_probcut( int depth, int shallow ) {
  int i;
  int this_try;

  this_try = mpc_cut[depth].cut_tries;
  mpc_cut[depth].cut_depth[this_try] = shallow;
  for ( i = 0; i <= 60; i++ ) {
    mpc_cut[depth].bias[this_try][i] =
      floor( 128.0 * (mid_corr[i][shallow].const_base +
		      mid_corr[i][shallow].const_slope * shallow) );
    mpc_cut[depth].window[this_try][i] =
      floor( 128.0 * (mid_corr[i][shallow].sigma_base +
		      mid_corr[i][shallow].sigma_slope * shallow) );
  }
  mpc_cut[depth].cut_tries++;
}


/*
   SET_END_PROBCUT
   Specifies that endgame searches with EMPTY empty disks
   are to be estimated using searches to depth SHALLOW_DEPTH.
*/

static void
set_end_probcut( int empty, int shallow_depth ) {
  int stage;

  stage = 60 - empty;
  if ( shallow_depth <= MAX_CORR_DEPTH )
    if ( end_stats_available[stage][shallow_depth] )
      end_mpc_depth[stage][use_end_cut[stage]++] = shallow_depth;
}


/*
   INIT_PROBCUT
   Clears the tables with MPC information and chooses some
   reasonable cut pairs.
*/

void
init_probcut( void ) {
  int i;

  for ( i = 0; i <= MAX_CUT_DEPTH; i++ )
    mpc_cut[i].cut_tries = 0;

  for ( i = 0; i <= 60; i++ )
    use_end_cut[i] = 0;

  set_probcut( 3, 1 );
  set_probcut( 4, 2 );
  set_probcut( 5, 1 );
  set_probcut( 6, 2 );
  set_probcut( 7, 3 );
  set_probcut( 8, 4 );
  set_probcut( 9, 3 );
  set_probcut( 10, 4 );
  set_probcut( 10, 6 );
  set_probcut( 11, 3 );
  set_probcut( 11, 5 );
  set_probcut( 12, 4 );
  set_probcut( 12, 6 );
  set_probcut( 13, 5 );
  set_probcut( 13, 7 );
  set_probcut( 14, 6 );
  set_probcut( 14, 8 );
  set_probcut( 15, 5 );
  set_probcut( 15, 7 );
  set_probcut( 16, 6 );
  set_probcut( 16, 8 );
  set_probcut( 17, 5 );
  set_probcut( 17, 7 );
  set_probcut( 18, 6 );
  set_probcut( 18, 8 );
  set_probcut( 20, 8 );
  set_end_probcut( 13, 1 );
  set_end_probcut( 14, 1 );
  set_end_probcut( 15, 2 );
  set_end_probcut( 16, 2 );
  set_end_probcut( 17, 2 );
  set_end_probcut( 18, 2 );
  set_end_probcut( 19, 3 );
  set_end_probcut( 20, 3 );
  set_end_probcut( 21, 4 );
  set_end_probcut( 22, 4 );
  set_end_probcut( 23, 4 );
  set_end_probcut( 24, 4 );
  set_end_probcut( 25, 4 );
  set_end_probcut( 26, 4 );
  set_end_probcut( 27, 4 );
}
