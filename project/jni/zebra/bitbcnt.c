/*
   File:          bitbcnt.c

   Modified:      November 24, 2005

   Authors:       Gunnar Andersson (gunnar@radagast.se)
		  Toshihiko Okuhara

   Contents:	  Count flips for the last move.

   This piece of software is released under the GPL.
   See the file COPYING for more information.
*/



#include <stdlib.h>	// NULL
// #include "bitboard.h"
#include "macros.h"	// REGPARM

static const char right_count[128] = {
  0, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0, 
  4, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0, 
  5, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0, 
  4, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0, 
  6, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0, 
  4, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0, 
  5, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0, 
  4, 0, 1, 0, 2, 0, 1, 0, 
  3, 0, 1, 0, 2, 0, 1, 0
};

static const char left_count[128] = {
  0, 6, 5, 5, 4, 4, 4, 4, 
  3, 3, 3, 3, 3, 3, 3, 3, 
  2, 2, 2, 2, 2, 2, 2, 2, 
  2, 2, 2, 2, 2, 2, 2, 2, 
  1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0, 
  0, 0, 0, 0, 0, 0, 0, 0
};

static const char center_count[256] = {
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 5, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  3, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  2, 5, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
  1, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
};

static int REGPARM(2)
CountFlips_bitboard_a1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_low >> (0 + 1)) & 0x7F];
  /* Down */
  flipped += right_count[(((my_bits_low & 0x01010100u) + ((my_bits_high & 0x01010101u) << 4)) * 0x01020408u) >> 25];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x08040200u) + (my_bits_high & 0x80402010u)) * 0x01010101u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_low << (7 - 7)) & 0x7F];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x10204000u) + (my_bits_high & 0x01020408u)) * 0x01010101u) >> 24];
  /* Down */
  flipped += right_count[((((my_bits_low & 0x80808000u) >> 4) + (my_bits_high & 0x80808080u)) * 0x00204081u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_a8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[my_bits_high >> (24 + 1)];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00020408u) + (my_bits_low & 0x10204080u)) * 0x01010101u) >> 25];
  /* Up */
  flipped += left_count[((((my_bits_high & 0x00010101u) << 4) + (my_bits_low & 0x01010101u)) * 0x01020408u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_high >> (31 - 7)) & 0x7F];
  /* Up */
  flipped += left_count[(((my_bits_high & 0x00808080u) + ((my_bits_low & 0x80808080) >> 4)) * 0x00204081u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00402010u) + (my_bits_low & 0x08040201u)) * 0x01010101u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_low >> (1 + 1)) & 0x3F];
  /* Down */
  flipped += right_count[(((my_bits_low & 0x02020200u) + ((my_bits_high & 0x02020202u) << 4)) * 0x00810204u) >> 25];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x10080400u) + (my_bits_high & 0x00804020u)) * 0x01010101u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_low << (7 - 6)) & 0x7E];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x08102000u) + (my_bits_high & 0x00010204u)) * 0x02020202u) >> 24];
  /* Down */
  flipped += right_count[((((my_bits_low & 0x40404000u) >> 4) + (my_bits_high & 0x40404040u)) * 0x00408102u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_a2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_low >> (8 + 1)) & 0x7F];
  /* Down */
  flipped += right_count[(((my_bits_low & 0x01010000u) + ((my_bits_high & 0x01010101u) << 4)) * 0x01020408u) >> 26];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x04020000u) + (my_bits_high & 0x40201008u)) * 0x01010101u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_low >> (15 - 7)) & 0x7F];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x20400000u) + (my_bits_high & 0x02040810u)) * 0x01010101u) >> 24];
  /* Down */
  flipped += right_count[((((my_bits_low & 0x80800000u) >> 4) + (my_bits_high & 0x80808080u)) * 0x00204081u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_a7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_high >> (16 + 1)) & 0x7F];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00000204u) + (my_bits_low & 0x08102040u)) * 0x01010101u) >> 25];
  /* Up */
  flipped += left_count[((((my_bits_high & 0x00000101u) << 4) + (my_bits_low & 0x01010101u)) * 0x02040810u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_high >> (23 - 7)) & 0x7F];
  /* Up */
  flipped += left_count[(((my_bits_high & 0x00008080u) + ((my_bits_low & 0x80808080u) >> 4)) * 0x00408102u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00004020u) + (my_bits_low & 0x10080402u)) * 0x01010101u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[my_bits_high >> (25 + 1)];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00040810u) + (my_bits_low & 0x20408000u)) * 0x01010101u) >> 26];
  /* Up */
  flipped += left_count[((((my_bits_high & 0x00020202u) << 4) + (my_bits_low & 0x02020202u)) * 0x00810204u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_high >> (30 - 7)) & 0x7E];
  /* Up */
  flipped += left_count[(((my_bits_high & 0x00404040u) + ((my_bits_low & 0x40404040u) >> 4)) * 0x00408102u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00201008u) + (my_bits_low & 0x04020100u)) * 0x02020202u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_low >> (9 + 1)) & 0x3F];
  /* Down */
  flipped += right_count[(((my_bits_low & 0x02020000u) + ((my_bits_high & 0x02020202u) << 4)) * 0x00810204u) >> 26];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x08040000u) + (my_bits_high & 0x80402010u)) * 0x01010101u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_low >> (14 - 7)) & 0x7E];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x10200000u) + (my_bits_high & 0x01020408u)) * 0x02020202u) >> 24];
  /* Down */
  flipped += right_count[((((my_bits_low & 0x40400000u) >> 4) + (my_bits_high & 0x40404040u)) * 0x00408102u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_high >> (17 + 1)) & 0x3F];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00000408u) + (my_bits_low & 0x10204080u)) * 0x01010101u) >> 26];
  /* Up */
  flipped += left_count[((((my_bits_high & 0x00000202u) << 4) + (my_bits_low & 0x02020202u)) * 0x01020408u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Left */
  flipped = left_count[(my_bits_high >> (22 - 7)) & 0x7E];
  /* Up */
  flipped += left_count[(((my_bits_high & 0x00004040u) + ((my_bits_low & 0x40404040u) >> 4)) * 0x00810204u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00002010u) + (my_bits_low & 0x08040201u)) * 0x02020202u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_low & 0x04040400u) + ((my_bits_high & 0x04040404u) << 4)) * 0x00408102u) >> 25];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x20100800u) + (my_bits_high & 0x00008040u)) * 0x01010101u) >> 27];
  /* Right */
  flipped += right_count[(my_bits_low >> (2 + 1)) & 0x1F];
  /* Left */
  flipped += (char) ((my_bits_low & 0x00000003u) == 0x00000001u);
  /* Down left */
  flipped += (char) ((my_bits_low & 0x00010200u) == 0x00010000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_high & 0x20202020u) + ((my_bits_low >> 4) & 0x02020200u)) * 0x00810204u) >> 25];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x04081000u) + (my_bits_high & 0x00000102u)) * 0x04040404u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_low << (7 - 5)) & 0x7C];
  /* Right */
  flipped += (char) ((my_bits_low & 0x000000C0u) == 0x00000080u);
  /* Down right */
  flipped += (char) ((my_bits_low & 0x00804000u) == 0x00800000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_a3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_low & 0x01000000u) + ((my_bits_high & 0x01010101u) << 4)) * 0x01020408u) >> 27];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x02000000u) + (my_bits_high & 0x20100804u)) * 0x01010101u) >> 25];
  /* Right */
  flipped += right_count[(my_bits_low >> (16 + 1)) & 0x7F];
  /* Up right */
  flipped += (char) ((my_bits_low & 0x00000204u) == 0x00000004u);
  /* Up */
  flipped += (char) ((my_bits_low & 0x00000101u) == 0x00000001u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_high & 0x80808080u) + ((my_bits_low & 0x80000000u) >> 4)) * 0x00204081u) >> 27];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x40000000u) + (my_bits_high & 0x04081020u)) * 0x01010101u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_low >> (23 - 7)) & 0x7F];
  /* Up left */
  flipped += (char) ((my_bits_low & 0x00004020u) == 0x00000020u);
  /* Up */
  flipped += (char) ((my_bits_low & 0x00008080u) == 0x00000080u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_a6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_low & 0x01010101u) + ((my_bits_high & 0x00000001u) << 4)) * 0x04081020u) >> 24];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00000002u) + (my_bits_low & 0x04081020u)) * 0x01010101u) >> 25];
  /* Right */
  flipped += right_count[(my_bits_high >> (8 + 1)) & 0x7F];
  /* Down right */
  flipped += (char) ((my_bits_high & 0x04020000u) == 0x04000000u);
  /* Down */
  flipped += (char) ((my_bits_high & 0x01010000u) == 0x01000000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_high & 0x00000080u) + ((my_bits_low & 0x80808080u) >> 4)) * 0x00810204u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00000040u) + (my_bits_low & 0x20100804u)) * 0x01010101u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_high >> (15 - 7)) & 0x7F];
  /* Down left */
  flipped += (char) ((my_bits_high & 0x20400000u) == 0x20000000u);
  /* Down */
  flipped += (char) ((my_bits_high & 0x80800000u) == 0x80000000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_low & 0x04040404u) + ((my_bits_high & 0x00040404u) << 4)) * 0x00408102u) >> 24];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00081020u) + (my_bits_low & 0x40800000u)) * 0x01010101u) >> 27];
  /* Right */
  flipped += right_count[my_bits_high >> (26 + 1)];
  /* Left */
  flipped += (char) ((my_bits_high & 0x03000000u) == 0x01000000u);
  /* Up left */
  flipped += (char) ((my_bits_high & 0x00020100u) == 0x00000100u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_high & 0x00202020u) + ((my_bits_low & 0x20202020u) >> 4)) * 0x00810204u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00100804u) + (my_bits_low & 0x02010000u)) * 0x04040404u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_high >> (29 - 7)) & 0x7C];
  /* Right */
  flipped += (char) ((my_bits_high & 0xC0000000u) == 0x80000000u);
  /* Up right */
  flipped += (char) ((my_bits_high & 0x00408000u) == 0x00008000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0x000000F7u) + (my_bits_low & 0x00000007u)) >> 0];
  /* Down left / Down right */
  flipped += center_count[((((my_bits_low & 0x01020400) << 1) + (my_bits_low & 0x40201000u) + (my_bits_high & 0x00000080u)) * 0x01010101u) >> 24];
  /* Down */
  flipped += right_count[(((my_bits_low & 0x08080800u) + ((my_bits_high & 0x08080808u) << 4)) * 0x00204081u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e1( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0x000000EFu) + (my_bits_low & 0x0000000Fu)) >> 1];
  /* Down left / Down right */
  flipped += center_count[(((((my_bits_low & 0x02040800u) + (my_bits_high & 0x00000001u)) << 1) + (my_bits_low & 0x80402000u)) * 0x01010101u) >> 25];
  /* Down */
  flipped += right_count[((((my_bits_low & 0x10101000u) >> 4) + (my_bits_high & 0x10101010u)) * 0x01020408u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_a4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[my_bits_low >> (24 + 1)];
  /* Up right */
  flipped += right_count[((my_bits_low & 0x00020408u) * 0x01010100u) >> 25];
  /* Down / Up */
  flipped += center_count[(((my_bits_low & 0x00010101u) + ((my_bits_high & 0x01010101u) << 3)) * 0x02040810u) >> 24];
  /* Down right */
  flipped += right_count[((my_bits_high & 0x10080402u) * 0x01010101u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down / Up */
  flipped = center_count[(((my_bits_high & 0x80808080u) + ((my_bits_low & 0x00808080u) >> 3)) * 0x00204081u) >> 24];
  /* Down left */
  flipped += left_count[((my_bits_high & 0x08102040u) * 0x01010101u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_low >> (31 - 7)) & 0x7F];
  /* Up left */
  flipped += left_count[((my_bits_low & 0x00402010u) * 0x01010100u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_a5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_high >> (0 + 1)) & 0x7F];
  /* Up right */
  flipped += right_count[((my_bits_low & 0x02040810u) * 0x01010101u) >> 25];
  /* Down / Up */
  flipped += center_count[(((my_bits_low & 0x01010101u) + ((my_bits_high & 0x01010100u) << 3)) * 0x02040810u) >> 25];
  /* Down right */
  flipped += right_count[((my_bits_high & 0x08040200u) * 0x00010101u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_h5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down / Up */
  flipped = center_count[(((my_bits_high & 0x80808000u) + ((my_bits_low & 0x80808080u) >> 3)) * 0x00204081u) >> 25];
  /* Up left */
  flipped += left_count[((my_bits_low & 0x40201008u) * 0x01010101u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_high << (7 - 7)) & 0x7F];
  /* Down left */
  flipped += left_count[((my_bits_high & 0x10204000u) * 0x00010101u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0xF7000000u) + (my_bits_high & 0x07000000u)) >> 24];
  /* Up right / Up left */
  flipped += center_count[(((my_bits_high & 0x00102040u) + (my_bits_low & 0x80000000u) + ((my_bits_high & 0x00040201u) << 1)) * 0x01010101u) >> 24];
  /* Up */
  flipped += left_count[((((my_bits_high & 0x00080808u) << 4) + (my_bits_low & 0x08080808u)) * 0x00204081u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e8( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0xEF000000u) + (my_bits_high & 0x0F000000u)) >> 25];
  /* Up right / Up left */
  flipped += center_count[(((my_bits_high & 0x00204080u) + (((my_bits_high & 0x00080402u) + (my_bits_low & 0x01000000u)) << 1)) * 0x01010101u) >> 25];
  /* Up */
  flipped += left_count[(((my_bits_high & 0x00101010u) + ((my_bits_low & 0x10101010u) >> 4)) * 0x01020408u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_low & 0x04040000u) + ((my_bits_high & 0x04040404u) << 4)) * 0x00408102u) >> 26];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x10080000u) + (my_bits_high & 0x00804020u)) * 0x01010101u) >> 27];
  /* Right */
  flipped += right_count[(my_bits_low >> (10 + 1)) & 0x1F];
  /* Left */
  flipped += (char) ((my_bits_low & 0x00000300u) == 0x00000100u);
  /* Down left */
  flipped += (char) ((my_bits_low & 0x01020000u) == 0x01000000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_high & 0x20202020u) + ((my_bits_low & 0x20200000u) >> 4)) * 0x00810204u) >> 26];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x08100000u) + (my_bits_high & 0x00010204u)) * 0x04040404u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_low >> (13 - 7)) & 0x7C];
  /* Right */
  flipped += (char) ((my_bits_low & 0x0000C000u) == 0x00008000u);
  /* Down right */
  flipped += (char) ((my_bits_low & 0x80400000u) == 0x80000000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_low & 0x02000000u) + ((my_bits_high & 0x02020202u) << 4)) * 0x00810204u) >> 27];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x04000000u) + (my_bits_high & 0x40201008u)) * 0x01010101u) >> 26];
  /* Right */
  flipped += right_count[(my_bits_low >> (17 + 1)) & 0x3F];
  /* Up right */
  flipped += (char) ((my_bits_low & 0x00000408u) == 0x00000008u);
  /* Up */
  flipped += (char) ((my_bits_low & 0x00000202u) == 0x00000002u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_high & 0x40404040u) + ((my_bits_low & 0x40000000u) >> 4)) * 0x00408102u) >> 27];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x20000000u) + (my_bits_high & 0x02040810u)) * 0x02020202u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_low >> (22 - 7)) & 0x7E];
  /* Up left */
  flipped += (char) ((my_bits_low & 0x00002010u) == 0x00000010u);
  /* Up */
  flipped += (char) ((my_bits_low & 0x00004040u) == 0x00000040u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_low & 0x02020202u) + ((my_bits_high & 0x00000002u) << 4)) * 0x02040810u) >> 24];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00000004u) + (my_bits_low & 0x08102040u)) * 0x01010101u) >> 26];
  /* Right */
  flipped += right_count[(my_bits_high >> (9 + 1)) & 0x3F];
  /* Down right */
  flipped += (char) ((my_bits_high & 0x08040000u) == 0x08000000u);
  /* Down */
  flipped += (char) ((my_bits_high & 0x02020000u) == 0x02000000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_high & 0x00000040u) + ((my_bits_low & 0x40404040u) >> 4)) * 0x01020408u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00000020u) + (my_bits_low & 0x10080402u)) * 0x02020202u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_high >> (14 - 7)) & 0x7E];
  /* Down left */
  flipped += (char) ((my_bits_high & 0x10200000u) == 0x10000000u);
  /* Down */
  flipped += (char) ((my_bits_high & 0x40400000u) == 0x40000000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_low & 0x04040404u) + ((my_bits_high & 0x00000404u) << 4)) * 0x00810204u) >> 24];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00000810u) + (my_bits_low & 0x20408000u)) * 0x01010101u) >> 27];
  /* Right */
  flipped += right_count[(my_bits_high >> (18 + 1)) & 0x1F];
  /* Left */
  flipped += (char) ((my_bits_high & 0x00030000u) == 0x00010000u);
  /* Up left */
  flipped += (char) ((my_bits_high & 0x00000201u) == 0x00000001u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_high & 0x00002020u) + ((my_bits_low & 0x20202020u) >> 4)) * 0x01020408u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00001008u) + (my_bits_low & 0x04020100u)) * 0x04040404u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_high >> (21 - 7)) & 0x7C];
  /* Right */
  flipped += (char) ((my_bits_high & 0x00C00000u) == 0x00800000u);
  /* Up right */
  flipped += (char) ((my_bits_high & 0x00004080u) == 0x00000080u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0x0000F700u) + (my_bits_low & 0x00000700u)) >> 8];
  /* Down left / Down right */
  flipped += center_count[(((((my_bits_low & 0x02040000u) + (my_bits_high & 0x00000001u)) << 1)
    + (my_bits_low & 0x20100000u) + (my_bits_high & 0x00008040u)) * 0x01010101u) >> 24];
  /* Down */
  flipped += right_count[(((my_bits_low & 0x08080000u) + ((my_bits_high & 0x08080808u) << 4)) * 0x00204081u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e2( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0x0000EF00u) + (my_bits_low & 0x00000F00u)) >> 9];
  /* Down left / Down right */
  flipped += center_count[(((((my_bits_low & 0x04080000u) + (my_bits_high & 0x00000102u)) << 1)
    + (my_bits_high & 0x00000080u) + (my_bits_low & 0x40200000u)) * 0x01010101u) >> 25];
  /* Down */
  flipped += right_count[((((my_bits_low & 0x10100000u) >> 4) + (my_bits_high & 0x10101010u)) * 0x01020408u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[my_bits_low >> (25 + 1)];
  /* Up right */
  flipped += right_count[((my_bits_low & 0x00040810u) * 0x01010100u) >> 26];
  /* Down / Up */
  flipped += center_count[(((my_bits_low & 0x00020202u) + ((my_bits_high & 0x02020202u) << 3)) * 0x01020408u) >> 24];
  /* Down right */
  flipped += right_count[((my_bits_high & 0x20100804u) * 0x01010101u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down / Up */
  flipped = center_count[(((my_bits_high & 0x40404040u) + ((my_bits_low & 0x00404040u) >> 3)) * 0x00408102u) >> 24];
  /* Up left */
  flipped += left_count[((my_bits_low & 0x00201008u) * 0x02020200u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_low >> (30 - 7)) & 0x7E];
  /* Down left */
  flipped += left_count[((my_bits_high & 0x04081020u) * 0x02020202u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_b5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_high >> (1 + 1)) & 0x3F];
  /* Up right */
  flipped += right_count[((my_bits_low & 0x04081020u) * 0x01010101u) >> 26];
  /* Down / Up */
  flipped += center_count[((((my_bits_high & 0x02020200u) << 3) + (my_bits_low & 0x02020202u)) * 0x01020408u) >> 25];
  /* Down right */
  flipped += right_count[((my_bits_high & 0x10080400u) * 0x00010101u) >> 26];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_g5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down / Up */
  flipped = center_count[(((my_bits_high & 0x40404000u) + ((my_bits_low & 0x40404040u) >> 3)) * 0x00408102u) >> 25];
  /* Down left */
  flipped += left_count[((my_bits_high & 0x08102000u) * 0x00020202u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_high << (7 - 6)) & 0x7F];
  /* Up left */
  flipped += left_count[((my_bits_low & 0x20100804u) * 0x02020202u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0x00F70000u) + (my_bits_high & 0x00070000u)) >> 16];
  /* Up right / Up left */
  flipped += center_count[(((my_bits_high & 0x00001020u) + (my_bits_low & 0x40800000u)
    + (((my_bits_high & 0x00000402u) + (my_bits_low & 0x01000000u)) << 1)) * 0x01010101u) >> 24];
  /* Up */
  flipped += left_count[((((my_bits_high & 0x00000808u) << 4) + (my_bits_low & 0x08080808u)) * 0x00408102u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e7( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0x00EF0000u) + (my_bits_high & 0x000F0000u)) >> 17];
  /* Up right / Up left */
  flipped += center_count[(((my_bits_high & 0x00002040u) + (my_bits_low & 0x80000000u)
    + (((my_bits_high & 0x00000804u) + (my_bits_low & 0x02010000u)) << 1)) * 0x01010101u) >> 25];
  /* Up */
  flipped += left_count[(((my_bits_high & 0x00001010u) + ((my_bits_low & 0x10101010u) >> 4)) * 0x02040810u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_low & 0x04000000u) + ((my_bits_high & 0x04040404u) << 4)) * 0x00408102u) >> 27];
  /* Down right */
  flipped += right_count[(((my_bits_low & 0x08000000u) + (my_bits_high & 0x80402010u)) * 0x01010101u) >> 27];
  /* Right */
  flipped += right_count[(my_bits_low >> (18 + 1)) & 0x1F];
  /* Up right */
  flipped += (char) ((my_bits_low & 0x00000810u) == 0x00000010u);
  /* Up */
  flipped += (char) ((my_bits_low & 0x00000404u) == 0x00000004u);
  /* Up left */
  flipped += (char) ((my_bits_low & 0x00000201u) == 0x00000001u);
  /* Left */
  flipped += (char) ((my_bits_low & 0x00030000u) == 0x00010000u);
  /* Down left */
  flipped += (~my_bits_low >> 25) & (my_bits_high >> 0) & 1;

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down */
  flipped = right_count[(((my_bits_high & 0x20202020u) + ((my_bits_low & 0x20000000u) >> 4)) * 0x00810204u) >> 27];
  /* Down left */
  flipped += left_count[(((my_bits_low & 0x10000000u) + (my_bits_high & 0x01020408u)) * 0x04040404u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_low >> (21 - 7)) & 0x7C];
  /* Up left */
  flipped += (char) ((my_bits_low & 0x00001008u) == 0x00000008u);
  /* Up */
  flipped += (char) ((my_bits_low & 0x00002020u) == 0x00000020u);
  /* Up right */
  flipped += (char) ((my_bits_low & 0x00004080u) == 0x00000080u);
  /* Right */
  flipped += (char) ((my_bits_low & 0x00C00000u) == 0x00800000u);
  /* Down right */
  flipped += (~my_bits_low >> 30) & (my_bits_high >> 7) & 1;

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_low & 0x04040404u) + ((my_bits_high & 0x00000004u) << 4)) * 0x01020408u) >> 24];
  /* Up right */
  flipped += right_count[(((my_bits_high & 0x00000008u) + (my_bits_low & 0x10204080u)) * 0x01010101u) >> 27];
  /* Right */
  flipped += right_count[(my_bits_high >> (10 + 1)) & 0x1F];
  /* Down right */
  flipped += (char) ((my_bits_high & 0x10080000u) == 0x10000000u);
  /* Down */
  flipped += (char) ((my_bits_high & 0x04040000u) == 0x04000000u);
  /* Down left */
  flipped += (char) ((my_bits_high & 0x01020000u) == 0x01000000u);
  /* Left */
  flipped += (char) ((my_bits_high & 0x00000300u) == 0x00000100u);
  /* Up left */
  flipped += (~my_bits_high >> 1) & (my_bits_low >> 24) & 1;

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Up */
  flipped = left_count[(((my_bits_high & 0x00000020u) + ((my_bits_low & 0x20202020u) >> 4)) * 0x02040810u) >> 24];
  /* Up left */
  flipped += left_count[(((my_bits_high & 0x00000010u) + (my_bits_low & 0x08040201u)) * 0x04040404u) >> 24];
  /* Left */
  flipped += left_count[(my_bits_high >> (13 - 7)) & 0x7C];
  /* Down left */
  flipped += (char) ((my_bits_high & 0x08100000u) == 0x08000000u);
  /* Down */
  flipped += (char) ((my_bits_high & 0x20200000u) == 0x20000000u);
  /* Down right */
  flipped += (char) ((my_bits_high & 0x80400000u) == 0x80000000u);
  /* Right */
  flipped += (char) ((my_bits_high & 0x0000C000u) == 0x00008000u);
  /* Up right */
  flipped += (~my_bits_high >> 6) & (my_bits_low >> 31) & 1;

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0x00F70000u) + (my_bits_low & 0x00070000u)) >> 16];
  /* Down left / Up right */
  flipped += center_count[(((my_bits_low & 0x04001020u) + ((my_bits_high & 0x00000102u) << 1)) * 0x01010102u) >> 24];
  /* Down */
  flipped += right_count[(((my_bits_low & 0x08000000u) + ((my_bits_high & 0x08080808u) << 4)) * 0x00204081u) >> 27];
  /* Up */
  flipped += (char) ((my_bits_low & 0x00000808u) == 0x00000008u);
  /* Down right / Up left */
  flipped += center_count[(((my_bits_low & 0x10000402u) + ((my_bits_high & 0x00804020u) >> 1)) * 0x02020201u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e3( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0x00EF0000u) + (my_bits_low & 0x000F0000u)) >> 17];
  /* Down left / Up right */
  flipped += center_count[(((my_bits_low & 0x08002040u) + ((my_bits_high & 0x00010204u) << 1)) * 0x01010102u) >> 25];
  /* Down */
  flipped += right_count[((((my_bits_low & 0x10000000u) >> 4) + (my_bits_high & 0x10101010u)) * 0x01020408u) >> 27];
  /* Up */
  flipped += (char) ((my_bits_low & 0x00001010u) == 0x00000010u);
  /* Down right / Up left */
  flipped += center_count[(((my_bits_low & 0x20000804u) + ((my_bits_high & 0x00008040u) >> 1)) * 0x02020201u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[my_bits_low >> (26 + 1)];
  /* Left */
  flipped += (char) ((my_bits_low & 0x03000000u) == 0x01000000u);
  /* Down left / Up right */
  flipped += center_count[(((my_bits_low & 0x00081020u) + ((my_bits_high & 0x00000102u) << 1)) * 0x02020202u) >> 24];
  /* Down / Up */
  flipped += center_count[(((my_bits_low & 0x00040404u) + ((my_bits_high & 0x04040404u) << 3)) * 0x00810204u) >> 24];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x40201008u) + ((my_bits_low & 0x00020100u) << 1)) * 0x02020202u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down left / Up right */
  flipped = center_count[(((my_bits_low & 0x00408000u) + ((my_bits_high & 0x02040810u) << 1)) * 0x01010101u) >> 26];
  /* Down / Up */
  flipped += center_count[(((my_bits_high & 0x20202020u) + ((my_bits_low & 0x00202020u) >> 3)) * 0x00810204u) >> 24];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x00008040u) + ((my_bits_low & 0x00100804u) << 1)) * 0x01010101u) >> 26];
  /* Left */
  flipped += left_count[(my_bits_low >> (29 - 7)) & 0x7C];
  /* Right */
  flipped += (char) ((my_bits_low & 0xC0000000u) == 0x80000000u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_c5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right */
  flipped = right_count[(my_bits_high >> (2 + 1)) & 0x1F];
  /* Left */
  flipped += (char) ((my_bits_high & 0x00000003u) == 0x00000001u);
  /* Down left / Up right */
  flipped += center_count[(((my_bits_low & 0x08102040u) + ((my_bits_high & 0x00010200u) << 1)) * 0x02020202u) >> 24];
  /* Down / Up */
  flipped += center_count[(((my_bits_low & 0x04040404u) + ((my_bits_high & 0x04040400u) << 3)) * 0x00810204u) >> 25];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x20100800u) + ((my_bits_low & 0x02010000u) << 1)) * 0x02020202u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_f5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Down left / Up right */
  flipped = center_count[(((my_bits_low & 0x40800000u) + ((my_bits_high & 0x04081000u) << 1)) * 0x01010101u) >> 26];
  /* Down / Up */
  flipped += center_count[(((my_bits_high & 0x20202000u) + ((my_bits_low & 0x20202020u) >> 3)) * 0x00810204u) >> 25];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x00804000u) + ((my_bits_low & 0x10080402u) << 1)) * 0x01010101u) >> 26];
  /* Left */
  flipped += left_count[(my_bits_high << (7 - 5)) & 0x7C];
  /* Right */
  flipped += (char) ((my_bits_high & 0x000000C0u) == 0x00000080u);

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0x0000F700u) + (my_bits_high & 0x00000700u)) >> 8];
  /* Down left / Up right */
  flipped += center_count[(((my_bits_high & 0x02040010u) + ((my_bits_low & 0x20408000u) >> 1)) * 0x01020202u) >> 24];
  /* Down */
  flipped += (char) ((my_bits_high & 0x08080000u) == 0x08000000u);
  /* Up */
  flipped += left_count[((((my_bits_high & 0x00000008u) << 4) + (my_bits_low & 0x08080808u)) * 0x00810204u) >> 24];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x20100004) + ((my_bits_low & 0x02010000u) << 1)) * 0x02010101u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e6( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0x0000EF00u) + (my_bits_high & 0x00000F00u)) >> 9];
  /* Down left / Up right */
  flipped += center_count[(((my_bits_high & 0x04080020u) + ((my_bits_low & 0x40800000u) >> 1)) * 0x01020202u) >> 25];
  /* Down */
  flipped += (char) ((my_bits_high & 0x10100000u) == 0x10000000u);
  /* Up */
  flipped += left_count[(((my_bits_high & 0x00000010u) + ((my_bits_low & 0x10101010u) >> 4)) * 0x04081020u) >> 24];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x40200008u) + ((my_bits_low & 0x04020100u) << 1)) * 0x02010101u) >> 25];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0xF7000000u) + (my_bits_low & 0x07000000u)) >> 24];
  /* Down left / Up right */
  flipped += center_count[((((my_bits_high & 0x00010204u) << 1) + (my_bits_low & 0x00102040u)) * 0x01010101u) >> 24];
  /* Down / Up */
  flipped += center_count[(((my_bits_low & 0x00080808u) + ((my_bits_high & 0x08080808u) << 3)) * 0x00408102u) >> 24];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x80402010) + ((my_bits_low & 0x00040201u) << 1)) * 0x01010101u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e4( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_low & 0xEF000000u) + (my_bits_low & 0x0F000000u)) >> 25];
  /* Down left / Up right */
  flipped += center_count[(((my_bits_high & 0x01020408u) + ((my_bits_low & 0x00204080u) >> 1)) * 0x01010101u) >> 24];
  /* Down / Up */
  flipped += center_count[((((my_bits_low & 0x00101010u) >> 3) + (my_bits_high & 0x10101010u)) * 0x01020408u) >> 24];
  /* Down right / Up left */
  flipped += center_count[((((my_bits_high & 0x00804020u) >> 1) + (my_bits_low & 0x00080402u)) * 0x01010101u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_d5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0x000000F7u) + (my_bits_high & 0x00000007u)) >> 0];
  /* Down left / Up right */
  flipped += center_count[((((my_bits_high & 0x01020400u) << 1) + (my_bits_low & 0x10204080u)) * 0x01010101u) >> 24];
  /* Down / Up */
  flipped += center_count[(((my_bits_low & 0x08080808u) + ((my_bits_high & 0x08080800u) << 3)) * 0x00408102u) >> 25];
  /* Down right / Up left */
  flipped += center_count[(((my_bits_high & 0x40201000u) + ((my_bits_low & 0x04020100u) << 1)) * 0x01010101u) >> 24];

  return flipped;
}

static int REGPARM(2)
CountFlips_bitboard_e5( unsigned int my_bits_high, unsigned int my_bits_low ) {
  char flipped;

  /* Right / Left */
  flipped = center_count[((my_bits_high & 0x000000EFu) + (my_bits_high & 0x0000000Fu)) >> 1];
  /* Down left / Up right */
  flipped += center_count[(((my_bits_high & 0x02040800u) + ((my_bits_low & 0x20408000u) >> 1)) * 0x01010101u) >> 24];
  /* Down / Up */
  flipped += center_count[(((my_bits_high & 0x10101000u) + ((my_bits_low & 0x10101010u) >> 3)) * 0x01020408u) >> 25];
  /* Down right / Up left */
  flipped += center_count[((((my_bits_high & 0x80402000u) >> 1) + (my_bits_low & 0x08040201u)) * 0x01010101u) >> 24];

  return flipped;
}

int (REGPARM(2) * const CountFlips_bitboard[78])( unsigned int my_bits_high, unsigned int my_bits_low ) = {
  CountFlips_bitboard_a1,
  CountFlips_bitboard_b1,
  CountFlips_bitboard_c1,
  CountFlips_bitboard_d1,
  CountFlips_bitboard_e1,
  CountFlips_bitboard_f1,
  CountFlips_bitboard_g1,
  CountFlips_bitboard_h1,
  NULL,
  NULL,
  CountFlips_bitboard_a2,
  CountFlips_bitboard_b2,
  CountFlips_bitboard_c2,
  CountFlips_bitboard_d2,
  CountFlips_bitboard_e2,
  CountFlips_bitboard_f2,
  CountFlips_bitboard_g2,
  CountFlips_bitboard_h2,
  NULL,
  NULL,
  CountFlips_bitboard_a3,
  CountFlips_bitboard_b3,
  CountFlips_bitboard_c3,
  CountFlips_bitboard_d3,
  CountFlips_bitboard_e3,
  CountFlips_bitboard_f3,
  CountFlips_bitboard_g3,
  CountFlips_bitboard_h3,
  NULL,
  NULL,
  CountFlips_bitboard_a4,
  CountFlips_bitboard_b4,
  CountFlips_bitboard_c4,
  CountFlips_bitboard_d4,
  CountFlips_bitboard_e4,
  CountFlips_bitboard_f4,
  CountFlips_bitboard_g4,
  CountFlips_bitboard_h4,
  NULL,
  NULL,
  CountFlips_bitboard_a5,
  CountFlips_bitboard_b5,
  CountFlips_bitboard_c5,
  CountFlips_bitboard_d5,
  CountFlips_bitboard_e5,
  CountFlips_bitboard_f5,
  CountFlips_bitboard_g5,
  CountFlips_bitboard_h5,
  NULL,
  NULL,
  CountFlips_bitboard_a6,
  CountFlips_bitboard_b6,
  CountFlips_bitboard_c6,
  CountFlips_bitboard_d6,
  CountFlips_bitboard_e6,
  CountFlips_bitboard_f6,
  CountFlips_bitboard_g6,
  CountFlips_bitboard_h6,
  NULL,
  NULL,
  CountFlips_bitboard_a7,
  CountFlips_bitboard_b7,
  CountFlips_bitboard_c7,
  CountFlips_bitboard_d7,
  CountFlips_bitboard_e7,
  CountFlips_bitboard_f7,
  CountFlips_bitboard_g7,
  CountFlips_bitboard_h7,
  NULL,
  NULL,
  CountFlips_bitboard_a8,
  CountFlips_bitboard_b8,
  CountFlips_bitboard_c8,
  CountFlips_bitboard_d8,
  CountFlips_bitboard_e8,
  CountFlips_bitboard_f8,
  CountFlips_bitboard_g8,
  CountFlips_bitboard_h8
};

