/*
   File:           unflip.c

   Created:        February 26, 1999
   
   Modified:       July 12, 1999

   Author:         Gunnar Andersson (gunnar@radagast.se)

   Contents:       Low-level code to flip back the discs flipped by a move.
*/



#include "macros.h"
#include "unflip.h"



/* Global variables */

int *global_flip_stack[2048];
int **flip_stack = &(global_flip_stack[0]);



/*
  UNDOFLIPS
  Flip back the FLIP_COUNT topmost squares on GLOBALFLIPSTACK.
  This seems to be the fastest way to do it and doesn't matter
  much anyway as this function consumes a negligible percentage
  of the time.
*/

INLINE void
UndoFlips( int flip_count, int oppcol ) {
  UndoFlips_inlined( flip_count, oppcol );
}


/*
  INIT_FLIP_STACK
  Reset the flip stack.
*/

void
init_flip_stack( void ) {
  flip_stack = &global_flip_stack[0];
}
