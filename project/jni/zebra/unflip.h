/*
   File:          unflip.h

   Created:       February 26, 1999
   
   Modified:      December 25, 1999

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      Low-level macro code to flip back the discs
                  flipped by a move.
*/



#ifndef UNFLIP_H
#define UNFLIP_H



extern int *global_flip_stack[2048];
extern int **flip_stack;



#define UndoFlips_inlined( flip_count, oppcol ) {    \
  int UndoFlips__flip_count = (flip_count);          \
  int UndoFlips__oppcol = (oppcol);                  \
                                                     \
  if ( UndoFlips__flip_count & 1 ) {                 \
    UndoFlips__flip_count--;                         \
    * (*(--flip_stack)) = UndoFlips__oppcol;         \
  }                                                  \
  while ( UndoFlips__flip_count ) {                  \
    UndoFlips__flip_count -= 2;                      \
    * (*(--flip_stack)) = UndoFlips__oppcol;         \
    * (*(--flip_stack)) = UndoFlips__oppcol;         \
  }                                                  \
}

void
UndoFlips( int flip_count, int oppcol );

void
init_flip_stack( void );



#endif  /* UNFLIP_H */
