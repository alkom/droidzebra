/*
   File:          hash.c

   Created:       June 29, 1997

   Modified:      November 15, 2005

   Author:        Gunnar Andersson (gunnar@radagast.se)
		  Toshihiko Okuhara

   Contents:      Routines manipulating the hash table

   This piece of software is released under the GPL.
   See the file COPYING for more information.
*/



#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "hash.h"
#include "macros.h"
#include "myrandom.h"
#include "safemem.h"
#include "search.h"



/* Substitute an old position with draft n if the new position has
   draft >= n-4 */
#define REPLACEMENT_OFFSET          4

/* An 8-bit mask to isolate the "draft" part of the hash table info. */
#define DRAFT_MASK                  255

#define KEY1_MASK                   0xFF000000u

#define SECONDARY_HASH( a )         ((a) ^ 1)



typedef struct {
  unsigned int key2;
  int eval;
  unsigned int moves;
  unsigned int key1_selectivity_flags_draft;
} CompactHashEntry;



/* Global variables */

int hash_size;
unsigned int hash1;
unsigned int hash2;
unsigned int hash_value1[3][128];
unsigned int hash_value2[3][128];
unsigned int hash_put_value1[3][128];
unsigned int hash_put_value2[3][128];
unsigned int hash_flip1[128];
unsigned int hash_flip2[128];
unsigned int hash_color1[3];
unsigned int hash_color2[3];
unsigned int hash_flip_color1;
unsigned int hash_flip_color2;
unsigned int hash_diff1[MAX_SEARCH_DEPTH];
unsigned int hash_diff2[MAX_SEARCH_DEPTH];
unsigned int hash_stored1[MAX_SEARCH_DEPTH];
unsigned int hash_stored2[MAX_SEARCH_DEPTH];



/* Local variables */

static int hash_bits;
static int hash_mask;
static int rehash_count;
static unsigned int hash_trans1 = 0;
static unsigned int hash_trans2 = 0;
static CompactHashEntry *hash_table;



/*
   INIT_HASH
   Allocate memory for the hash table.
*/

void
init_hash( int in_hash_bits ) {
  hash_bits = in_hash_bits;
  hash_size = 1 << hash_bits;
  hash_mask = hash_size - 1;
  hash_table =
    (CompactHashEntry *) safe_malloc( hash_size * sizeof( CompactHashEntry ) );
  rehash_count = 0;
}


/*
  RESIZE_HASH
  Changes the size of the hash table.
*/

void
resize_hash( int new_hash_bits ) {
  free( hash_table );
  init_hash( new_hash_bits );
  setup_hash( TRUE );
}



/*
  POPCOUNT
*/

static unsigned int
popcount( unsigned int b ) {
  unsigned int n;

  for ( n = 0; b != 0; n++, b &= (b - 1) )
    ;

  return n;
}



/*
  GET_CLOSENESS
  Returns the closeness between the 64-bit integers (a0,a1) and (b0,b1).
  A closeness of 0 means that 32 bits differ.
*/


static unsigned int
get_closeness( unsigned int a0, unsigned int a1,
	   unsigned int b0, unsigned int b1 ) {
  return abs( popcount( a0 ^ b0 ) + popcount( a1 ^ b1 ) - 32 );
}



/*
   SETUP_HASH
   Determine randomized hash masks.
*/   

void
setup_hash( int clear ) {
  int i, j;
  int pos;
  int rand_index;
  const unsigned int max_pair_closeness = 10;
  const unsigned int max_zero_closeness = 9;
  unsigned int closeness;
  unsigned int random_pair[130][2];

  if ( clear )
    for ( i = 0; i < hash_size; i++ ) {
      hash_table[i].key1_selectivity_flags_draft &= ~DRAFT_MASK;
      hash_table[i].key2 = 0;
    }

  rand_index = 0;
  while ( rand_index < 130 ) {
  TRY_AGAIN:
    random_pair[rand_index][0] = (my_random() << 3) + (my_random() >> 2);
    random_pair[rand_index][1] = (my_random() << 3) + (my_random() >> 2);

    closeness =
      get_closeness( random_pair[rand_index][0], random_pair[rand_index][1],
		     0, 0 );
    if ( closeness > max_zero_closeness )
      goto TRY_AGAIN;
    for ( i = 0; i < rand_index; i++ ) {
      closeness =
	get_closeness( random_pair[rand_index][0], random_pair[rand_index][1],
		       random_pair[i][0], random_pair[i][1] );
      if ( closeness > max_pair_closeness )
	goto TRY_AGAIN;
      closeness =
	get_closeness( random_pair[rand_index][0], random_pair[rand_index][1],
		       random_pair[i][1], random_pair[i][0] );
      if ( closeness > max_pair_closeness )
	goto TRY_AGAIN;
    }
    rand_index++;
  }

  rand_index = 0;

  for ( i = 0; i < 128; i++ ) {
    hash_value1[BLACKSQ][i] = 0;
    hash_value2[BLACKSQ][i] = 0;
    hash_value1[WHITESQ][i] = 0;
    hash_value2[WHITESQ][i] = 0;
  }
  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      pos = 10 * i + j;
      hash_value1[BLACKSQ][pos] = random_pair[rand_index][0];
      hash_value2[BLACKSQ][pos] = random_pair[rand_index][1];
      rand_index++;
      hash_value1[WHITESQ][pos] = random_pair[rand_index][0];
      hash_value2[WHITESQ][pos] = random_pair[rand_index][1];
      rand_index++;
    }
  for ( i = 0; i < 128; i++ ) {
    hash_flip1[i] = hash_value1[BLACKSQ][i] ^ hash_value1[WHITESQ][i];
    hash_flip2[i] = hash_value2[BLACKSQ][i] ^ hash_value2[WHITESQ][i];
  }

  hash_color1[BLACKSQ] = random_pair[rand_index][0];
  hash_color2[BLACKSQ] = random_pair[rand_index][1];
  rand_index++;
  hash_color1[WHITESQ] = random_pair[rand_index][0];
  hash_color2[WHITESQ] = random_pair[rand_index][1];
  rand_index++;

  hash_flip_color1 = hash_color1[BLACKSQ] ^ hash_color1[WHITESQ];
  hash_flip_color2 = hash_color2[BLACKSQ] ^ hash_color2[WHITESQ];

  for ( j = 0; j < 128; j++ ) {
    hash_put_value1[BLACKSQ][j] = hash_value1[BLACKSQ][j] ^ hash_flip_color1;
    hash_put_value2[BLACKSQ][j] = hash_value2[BLACKSQ][j] ^ hash_flip_color2;
    hash_put_value1[WHITESQ][j] = hash_value1[WHITESQ][j] ^ hash_flip_color1;
    hash_put_value2[WHITESQ][j] = hash_value2[WHITESQ][j] ^ hash_flip_color2;
  }
}


/*
  CLEAR_HASH_DRAFTS
  Resets the draft information for all entries in the hash table.
*/

void
clear_hash_drafts( void ) {
  int i;

  for ( i = 0; i < hash_size; i++ )  /* Set the draft to 0 */
    hash_table[i].key1_selectivity_flags_draft &= ~0x0FF;
}


/*
   FREE_HASH
   Remove the hash table.
*/   

void
free_hash( void ) {
  free( hash_table );
}


/*
   DETERMINE_HASH_VALUES
   Calculates the hash codes for the given board position.
*/

void
determine_hash_values( int side_to_move,
		       const int *board ) {
  int i, j;

  hash1 = 0;
  hash2 = 0;
  for ( i = 1; i <= 8; i++ )
    for ( j = 1; j <= 8; j++ ) {
      int pos = 10 * i + j;
      switch ( board[pos] ) {
      case BLACKSQ:
        hash1 ^= hash_value1[BLACKSQ][pos];
        hash2 ^= hash_value2[BLACKSQ][pos];
        break;

      case WHITESQ:
        hash1 ^= hash_value1[WHITESQ][pos];
        hash2 ^= hash_value2[WHITESQ][pos];
        break;

      default:
        break;
      }
    }
  hash1 ^= hash_color1[side_to_move];
  hash2 ^= hash_color2[side_to_move];
}


/*
   WIDE_TO_COMPACT
   Convert the easily readable representation to the more
   compact one actually stored in the hash table.
*/   

static INLINE void
wide_to_compact( const HashEntry *entry, CompactHashEntry *compact_entry ) {
  compact_entry->key2 = entry->key2;
  compact_entry->eval = entry->eval;
  compact_entry->moves = entry->move[0] + (entry->move[1] << 8) +
    (entry->move[2] << 16) + (entry->move[3] << 24);
  compact_entry->key1_selectivity_flags_draft =
    (entry->key1 & KEY1_MASK) + (entry->selectivity << 16) +
    (entry->flags << 8) + entry->draft;
}


/*
   COMPACT_TO_WIDE
   Expand the compact internal representation of entries
   in the hash table to something more usable.
*/   

static INLINE void
compact_to_wide( const CompactHashEntry *compact_entry, HashEntry *entry ) {
  entry->key2 = compact_entry->key2;
  entry->eval = compact_entry->eval;
  entry->move[0] = compact_entry->moves & 255;
  entry->move[1] = (compact_entry->moves >> 8) & 255;
  entry->move[2] = (compact_entry->moves >> 16) & 255;
  entry->move[3] = (compact_entry->moves >> 24) & 255;
  entry->key1 = compact_entry->key1_selectivity_flags_draft & KEY1_MASK;
  entry->selectivity =
    (compact_entry->key1_selectivity_flags_draft & 0x00ffffff) >> 16;
  entry->flags =
    (compact_entry->key1_selectivity_flags_draft & 0x0000ffff) >> 8;
  entry->draft =
    (compact_entry->key1_selectivity_flags_draft & 0x000000ff);
}


/*
  SET_HASH_TRANSFORMATION
  Specify the hash code transformation masks. Changing these masks
  is the poor man's way to achieve the effect of clearing the hash
  table.
*/

void
set_hash_transformation( unsigned int trans1, unsigned int trans2 ) {
  hash_trans1 = trans1;
  hash_trans2 = trans2;
}


/*
   ADD_HASH
   Add information to the hash table. Two adjacent positions are tried
   and the most shallow search is replaced.
*/

void
add_hash( int reverse_mode,
	  int score,
	  int best,
	  int flags,
	  int draft,
	  int selectivity ) {
  int old_draft;
  int change_encouragment;
  unsigned int index, index1, index2;
  unsigned int code1, code2;
  HashEntry entry;

  assert( abs( score ) != SEARCH_ABORT );

  if ( reverse_mode ) {
    code1 = hash2 ^ hash_trans2;
    code2 = hash1 ^ hash_trans1;
  }
  else {
    code1 = hash1 ^ hash_trans1;
    code2 = hash2 ^ hash_trans2;
  }

  index1 = code1 & hash_mask;
  index2 = SECONDARY_HASH( index1 );
  if ( hash_table[index1].key2 == code2 )
    index = index1;
  else {
    if ( hash_table[index2].key2 == code2 )
      index = index2;
    else {
      if ( (hash_table[index1].key1_selectivity_flags_draft & DRAFT_MASK) <=
	   (hash_table[index2].key1_selectivity_flags_draft & DRAFT_MASK) )
	index = index1;
      else
	index = index2;
    }
  }

  old_draft = hash_table[index].key1_selectivity_flags_draft & DRAFT_MASK;

  if ( flags & EXACT_VALUE )  /* Exact scores are potentially more useful */
    change_encouragment = 2;
  else
    change_encouragment = 0;
  if ( hash_table[index].key2 == code2 ) {
    if ( old_draft > draft + change_encouragment + 2 )
      return;
  }
  else if ( old_draft > draft + change_encouragment + REPLACEMENT_OFFSET )
    return;

  entry.key1 = code1;
  entry.key2 = code2;
  entry.eval = score;
  entry.move[0] = best;
  entry.move[1] = 0;
  entry.move[2] = 0;
  entry.move[3] = 0;
  entry.flags = (short) flags;
  entry.draft = (short) draft;
  entry.selectivity = selectivity;
  wide_to_compact( &entry, &hash_table[index] );
}


/*
   ADD_HASH_EXTENDED
   Add information to the hash table. Two adjacent positions are tried
   and the most shallow search is replaced.
*/

void
add_hash_extended( int reverse_mode, int score, int *best, int flags,
		   int draft, int selectivity ) {
  int i;
  int old_draft;
  int change_encouragment;
  unsigned int index, index1, index2;
  unsigned int code1, code2;
  HashEntry entry;

  if ( reverse_mode ) {
    code1 = hash2 ^ hash_trans2;
    code2 = hash1 ^ hash_trans1;
  }
  else {
    code1 = hash1 ^ hash_trans1;
    code2 = hash2 ^ hash_trans2;
  }

  index1 = code1 & hash_mask;
  index2 = SECONDARY_HASH( index1 );
  if ( hash_table[index1].key2 == code2 )
    index = index1;
  else {
    if ( hash_table[index2].key2 == code2 )
      index = index2;
    else {
      if ( (hash_table[index1].key1_selectivity_flags_draft & DRAFT_MASK) <=
	   (hash_table[index2].key1_selectivity_flags_draft & DRAFT_MASK) )
	index = index1;
      else
	index = index2;
    }
  }

  old_draft = hash_table[index].key1_selectivity_flags_draft & DRAFT_MASK;

  if ( flags & EXACT_VALUE )  /* Exact scores are potentially more useful */
    change_encouragment = 2;
  else
    change_encouragment = 0;
  if ( hash_table[index].key2 == code2 ) {
    if ( old_draft > draft + change_encouragment + 2 )
      return;
  }
  else if ( old_draft > draft + change_encouragment + REPLACEMENT_OFFSET )
    return;

  entry.key1 = code1;
  entry.key2 = code2;
  entry.eval = score;
  for ( i = 0; i < 4; i++ )
    entry.move[i] = best[i];
  entry.flags = (short) flags;
  entry.draft = (short) draft;
  entry.selectivity = selectivity;
  wide_to_compact( &entry, &hash_table[index] );
}


/*
   FIND_HASH
   Search the hash table for the current position. The two possible
   hash table positions are probed.
*/   

void REGPARM(2)
find_hash( HashEntry *entry, int reverse_mode ) {
  int index1, index2;
  unsigned int code1, code2;

  if ( reverse_mode ) {
    code1 = hash2 ^ hash_trans2;
    code2 = hash1 ^ hash_trans1;
  }
  else {
    code1 = hash1 ^ hash_trans1;
    code2 = hash2 ^ hash_trans2;
  }

  index1 = code1 & hash_mask;
  index2 = SECONDARY_HASH( index1 );
  if ( hash_table[index1].key2 == code2 ) {
    if ( ((hash_table[index1].key1_selectivity_flags_draft ^ code1) & KEY1_MASK) == 0 ) {
      compact_to_wide( &hash_table[index1], entry );
      return;
    }
  }
  else if ( (hash_table[index2].key2 == code2) &&
	    (((hash_table[index2].key1_selectivity_flags_draft ^ code1) & KEY1_MASK) == 0) ) {
    compact_to_wide( &hash_table[index2], entry );
    return;
  }

  entry->draft = NO_HASH_MOVE;
  entry->flags = UPPER_BOUND;
  entry->eval = INFINITE_EVAL;
  entry->move[0] = 44;
  entry->move[1] = 0;
  entry->move[2] = 0;
  entry->move[3] = 0;
}
