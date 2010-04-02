/*
   File:          hash.h

   Created:       June 29, 1997

   Modified:      October 25, 2005

   Author:        Gunnar Andersson (gunnar@radagast.se)
		  Toshihiko Okuhara

   Contents:      Routines manipulating the hash table
*/



#ifndef HASH_H
#define HASH_H



#include "constant.h"
#include "macros.h"



#ifdef __cplusplus
extern "C" {
#endif



#define LOWER_BOUND               1
#define UPPER_BOUND               2
#define EXACT_VALUE               4
#define MIDGAME_SCORE             8
#define ENDGAME_SCORE             16
#define SELECTIVE                 32

#define ENDGAME_MODE              TRUE
#define MIDGAME_MODE              FALSE

#define NO_HASH_MOVE              0



/* The structure returned when a hash probe resulted in a hit.
   DRAFT is the depth of the subtree beneath the node and FLAGS
   contains a flag mask (see the flag bits above). */
typedef struct {
   unsigned int key1, key2;
   int eval;
   int move[4];
   short draft;
   short selectivity;
   short flags;
} HashEntry;


/* The number of entries in the hash table. Always a power of 2. */
extern int hash_size;

/* The 64-bit hash key. */
extern unsigned int hash1;
extern unsigned int hash2;

/* The 64-bit hash masks for a piece of a certain color in a
   certain position. */
extern unsigned int hash_value1[3][128];
extern unsigned int hash_value2[3][128];

/* 64-bit hash masks used when a disc is played on the board;
   the relation
     hash_put_value?[][] == hash_value?[][] ^ hash_flip_color?
   is guaranteed to hold. */

extern unsigned int hash_put_value1[3][128];
extern unsigned int hash_put_value2[3][128];

/* XORs of hash_value* - used for disk flipping. */
extern unsigned int hash_flip1[128];
extern unsigned int hash_flip2[128];

/* 64-bit hash mask for the two different sides to move. */
extern unsigned int hash_color1[3];
extern unsigned int hash_color2[3];

/* The XOR of the hash_color*, used for disk flipping. */
extern unsigned int hash_flip_color1;
extern unsigned int hash_flip_color2;

/* Stored 64-bit hash mask which hold the hash codes at different nodes
   in the search tree. */
extern unsigned int hash_stored1[MAX_SEARCH_DEPTH];
extern unsigned int hash_stored2[MAX_SEARCH_DEPTH];



void
init_hash( int in_hash_bits );

void
resize_hash( int new_hash_bits );

void
setup_hash( int clear );

void
clear_hash_drafts( void );

void
free_hash( void );

void
determine_hash_values( int side_to_move,
		       const int *board );

void
set_hash_transformation( unsigned int trans1,
			 unsigned int trans2 );

void
add_hash( int reverse_mode,
	  int score,
	  int best,
	  int flags,
	  int draft,
	  int selectivity );

void
add_hash_extended( int reverse_mode,
		   int score,
		   int *best,
		   int flags,
		   int draft,
		   int selectivity );

void REGPARM(2)
find_hash( HashEntry *entry, int reverse_mode );



#ifdef __cplusplus
}
#endif



#endif  /* HASH_H */
