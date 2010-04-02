/*
   File:          bitbtest.c

   Modified:      November 24, 2005

   Authors:       Gunnar Andersson (gunnar@radagast.se)
	          Toshihiko Okuhara

   Contents:      Count flips and returns new_my_bits in bb_flips.

   This piece of software is released under the GPL.
   See the file COPYING for more information.
*/

#include <stdlib.h>

#include "macros.h"
#include "bitboard.h"


BitBoard bb_flips;

static const unsigned char right_contiguous[64] = {
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 5,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 4,
  0, 1, 0, 2, 0, 1, 0, 3,
  0, 1, 0, 2, 0, 1, 0, 6
};

static const unsigned char left_contiguous[64] = {
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 4, 4, 5, 6
};

static const unsigned int right_flip[7] = { 0x00000001u, 0x00000003u, 0x00000007u, 0x0000000Fu, 0x0000001Fu, 0x0000003Fu, 0x0000007Fu };
static const unsigned int left_flip[7]  = { 0x80000000u, 0xC0000000u, 0xE0000000u, 0xF0000000u, 0xF8000000u, 0xFC000000u, 0xFE000000u };
static const unsigned int lsb_mask[4]   = { 0x000000FFu, 0x0000FFFFu, 0x00FFFFFFu, 0xFFFFFFFFu };
static const unsigned int msb_mask[4]   = { 0xFF000000u, 0xFFFF0000u, 0xFFFFFF00u, 0xFFFFFFFFu };

static const unsigned char pop_count[64] = {
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,	/*  0 -- 15 */
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,	/* 16 -- 31 */
  1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,	/* 32 -- 47 */
  2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6	/* 48 -- 63 */
};

static const unsigned char c_frontier[62] = {
  0x00, 0x01, /* -0000-x- */  0, 0,
  0x10, 0x11, /* -0001-x- */  0, 0,
  0x00, 0x01, /* -0010-x- */  0, 0,
  0x20, 0x21, /* -0011-x- */  0, 0,
  0x00, 0x01, /* -0100-x- */  0, 0,
  0x10, 0x11, /* -0101-x- */  0, 0,
  0x00, 0x01, /* -0110-x- */  0, 0,
  0x40, 0x41, /* -0111-x- */  0, 0,
  0x00, 0x01, /* -1000-x- */  0, 0,
  0x10, 0x11, /* -1001-x- */  0, 0,
  0x00, 0x01, /* -1010-x- */  0, 0,
  0x20, 0x21, /* -1011-x- */  0, 0,
  0x00, 0x01, /* -1100-x- */  0, 0,
  0x10, 0x11, /* -1101-x- */  0, 0,
  0x00, 0x01, /* -1110-x- */  0, 0,
  0x80, 0x81  /* -1111-x- */
};

static const unsigned char d_frontier[60] = {
  0x00, 0x00, 0x02, 0x01, /* -000-xx- */  0, 0, 0, 0,
  0x20, 0x20, 0x22, 0x21, /* -001-xx- */  0, 0, 0, 0,
  0x00, 0x00, 0x02, 0x01, /* -010-xx- */  0, 0, 0, 0,
  0x40, 0x40, 0x42, 0x41, /* -011-xx- */  0, 0, 0, 0,
  0x00, 0x00, 0x02, 0x01, /* -100-xx- */  0, 0, 0, 0,
  0x20, 0x20, 0x22, 0x21, /* -101-xx- */  0, 0, 0, 0,
  0x00, 0x00, 0x02, 0x01, /* -110-xx- */  0, 0, 0, 0,
  0x80, 0x80, 0x82, 0x81  /* -111-xx- */
};

static const unsigned char e_frontier[56] = {
  0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x02, 0x01, /* -00-xxx- */  0, 0, 0, 0, 0, 0, 0, 0,
  0x40, 0x40, 0x40, 0x40, 0x44, 0x44, 0x42, 0x41, /* -01-xxx- */  0, 0, 0, 0, 0, 0, 0, 0,
  0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x02, 0x01, /* -10-xxx- */  0, 0, 0, 0, 0, 0, 0, 0,
  0x80, 0x80, 0x80, 0x80, 0x84, 0x84, 0x82, 0x81  /* -11-xxx- */
};

#if 0

static const unsigned char f_frontier[48] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x02, 0x01, /* -0-xxxx- */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x88, 0x88, 0x88, 0x88, 0x84, 0x84, 0x82, 0x81  /* -1-xxxx- */
};

static const unsigned char c_flip[130] = {
  0x00, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x04, 0x05, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x0C, 0x0D, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x1C, 0x1D, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x3C, 0x3D
};

static const unsigned char d_flip[132] = {
  0x00, 0x03, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x08, 0x0B, 0x0A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x18, 0x1B, 0x1A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x38, 0x3B, 0x3A, 0
};

static const unsigned char e_flip[136] = {
  0x00, 0x07, 0x06, 0, 0x04, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x10, 0x17, 0x16, 0, 0x14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x30, 0x37, 0x36, 0, 0x34, 0, 0, 0
};

static const unsigned char f_flip[140] = {
  0x00, 0x0F, 0x0E, 0, 0x0C, 0, 0, 0, 0x08, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x20, 0x2F, 0x2E, 0, 0x2C, 0, 0, 0, 0x28, 0, 0, 0
};

#else

static const unsigned char f_flip[160] = {
  0x00, 0x0F, 0x0E, 0, 0x0C, 0, 0, 0, 0x08, 0, 0, 0, 0, 0, 0, 0,
		0x00, 0x07, 0x06, 0, 0x04, 0, 0, 0,   0x00, 0x03, 0x02, 0,   0x00, 0x01, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   		                     0x04, 0x05, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,       		      0x08, 0x0B, 0x0A, 0,   0x0C, 0x0D, 0, 0,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x02, 0x01, /* -0-xxxx- */
		0x10, 0x17, 0x16, 0, 0x14, 0, 0, 0,   0x18, 0x1B, 0x1A, 0,   0x1C, 0x1D, 0, 0,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x88, 0x88, 0x88, 0x88, 0x84, 0x84, 0x82, 0x81, /* -1-xxxx- */
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0x20, 0x2F, 0x2E, 0, 0x2C, 0, 0, 0, 0x28, 0, 0, 0, 0, 0, 0, 0,
		0x30, 0x37, 0x36, 0, 0x34, 0, 0, 0,   0x38, 0x3B, 0x3A, 0,   0x3C, 0x3D, 0, 0
};

#define	e_flip(x)	f_flip[(x) + 16]
#define	d_flip(x)	f_flip[(x) + 24]
#define	c_flip(x)	f_flip[(x) + 28]
#define f_frontier(x)	f_flip[(x) + 64]

#endif

#if 0
static const BitBoard top_flip[8] = {
  {           0, 0x000000FFu },
  {           0, 0x0000FFFFu },
  {           0, 0x00FFFFFFu },
  {           0, 0xFFFFFFFFu },
  { 0x000000FFu, 0xFFFFFFFFu },
  { 0x0000FFFFu, 0xFFFFFFFFu },
  { 0x00FFFFFFu, 0xFFFFFFFFu },
  { 0xFFFFFFFFu, 0xFFFFFFFFu } };
static const BitBoard bot_flip[8] = {
  { 0xFF000000u,           0 },
  { 0xFFFF0000u,           0 },
  { 0xFFFFFF00u,           0 },
  { 0xFFFFFFFFu,           0 },
  { 0xFFFFFFFFu, 0xFF000000u },
  { 0xFFFFFFFFu, 0xFFFF0000u },
  { 0xFFFFFFFFu, 0xFFFFFF00u },
  { 0xFFFFFFFFu, 0xFFFFFFFFu } };
#endif

#if 0
static const unsigned int high_flip[56] = {
/*                      m            o           m            m m          m o           o            o m          o o */
  0x00000000u, 0xFF000000u, 0x00000000u,  0x00000000u, 0xFF000000u, 0xFE0000FFu,  0x00000000u, 0xFF000000u, 0x00000000u,
  0x00000000u, 0xFF000000u, 0x00000000u,  0x00000000u, 0xFF000000u, 0xFE0000FFu,  0x00000000u, 0xFF000000u, 0xFD00FFFFu,
  0x00000000u, 0xFF000000u, 0x00000000u,  0x00000000u, 0xFF000000u, 0xFE0000FFu,  0x00000000u, 0xFF000000u, 0x00000000u,
  0,
  0x00000000u, 0xFF000000u, 0x00000000u,  0x00000000u, 0xFF000000u, 0xFE0000FFu,  0x00000000u, 0xFF000000u, 0x00000000u,
  0x00000000u, 0xFF000000u, 0x00000000u,  0x00000000u, 0xFF000000u, 0xFE0000FFu,  0x00000000u, 0xFF000000u, 0xFD00FFFFu,
  0x00000000u, 0xFF000000u, 0x00000000u,  0x00000000u, 0xFF000000u, 0xFE0000FFu,  0x00000000u, 0xFF000000u, 0xFCFFFFFFu,
  0
};
#endif

#if 0
#define bbFlips_Right_low(pos, mask)	\
  contig = right_contiguous[(opp_bits_low >> (pos + 1)) & mask];	\
  fl = 0x7F >> (6 - contig) << (pos + 1);				\
  t = -(int)(my_bits_low & fl) >> 31;					\
  my_bits_low |= fl & t;						\
  flipped = contig & t
#else
#define bbFlips_Right_low(pos, mask)	\
  contig = right_contiguous[(opp_bits_low >> (pos + 1)) & mask];	\
  fl = right_flip[contig] << (pos + 1);					\
  t = -(int)(my_bits_low & fl) >> 31;					\
  my_bits_low |= fl & t;						\
  flipped = contig & t
#endif

#define bbFlips_Right_high(pos, mask)	\
  contig = right_contiguous[(opp_bits_high >> (pos + 1)) & mask];	\
  fl = right_flip[contig] << (pos + 1);					\
  t = -(int)(my_bits_high & fl) >> 31;					\
  my_bits_high |= fl & t;						\
  flipped = contig & t

#if 1
#define bbFlips_Left_low(pos, mask)	\
  contig = left_contiguous[(opp_bits_low >> (pos - 6)) & mask];		\
  fl = (unsigned int)((int)0x80000000 >> contig) >> (32 - pos);		\
  t = -(int)(my_bits_low & fl) >> 31;					\
  my_bits_low |= fl & t;						\
  flipped = contig & t
#else
#define bbFlips_Left_low(pos, mask)	\
  contig = left_contiguous[(opp_bits_low >> (pos - 6)) & mask];		\
  fl = left_flip[contig] >> (32 - pos);					\
  t = -(int)(my_bits_low & fl) >> 31;					\
  my_bits_low |= fl & t;						\
  flipped = contig & t
#endif

#define bbFlips_Left_high(pos, mask)	\
  contig = left_contiguous[(opp_bits_high >> (pos - 6)) & mask];	\
  fl = (unsigned int)((int)0x80000000 >> contig) >> (32 - pos);		\
  t = -(int)(my_bits_high & fl) >> 31;					\
  my_bits_high |= fl & t;						\
  flipped = contig & t


#define bbFlips_Horiz_d_low(pos)	\
  t = d_frontier[(opp_bits_low >> (pos - 2)) & 0x3B];			\
  fl = d_flip(t & (my_bits_low >> (pos - 3)));				\
  my_bits_low |= fl << (pos - 2);					\
  flipped = pop_count[fl]

#define bbFlips_Horiz_d_high(pos)	\
  t = d_frontier[(opp_bits_high >> (pos - 2)) & 0x3B];			\
  fl = d_flip(t & (my_bits_high >> (pos - 3)));				\
  my_bits_high |= fl << (pos - 2);					\
  flipped = pop_count[fl]

#define bbFlips_Horiz_e_low(pos)	\
  t = e_frontier[(opp_bits_low >> (pos - 3)) & 0x37];			\
  fl = e_flip(t & (my_bits_low >> (pos - 4)));				\
  my_bits_low |= fl << (pos - 3);					\
  flipped = pop_count[fl]

#define bbFlips_Horiz_e_high(pos)	\
  t = e_frontier[(opp_bits_high >> (pos - 3)) & 0x37];			\
  fl = e_flip(t & (my_bits_high >> (pos - 4)));				\
  my_bits_high |= fl << (pos - 3);					\
  flipped = pop_count[fl]

#define bbFlips_Horiz_c_low(pos)	\
  t = c_frontier[(opp_bits_low >> (pos - 1)) & 0x3D];			\
  fl = c_flip(t & (my_bits_low >> (pos - 2)));				\
  my_bits_low |= fl << (pos - 1);					\
  flipped = pop_count[fl]

#define bbFlips_Horiz_c_high(pos)	\
  t = c_frontier[(opp_bits_high >> (pos - 1)) & 0x3D];			\
  fl = c_flip(t & (my_bits_high >> (pos - 2)));				\
  my_bits_high |= fl << (pos - 1);					\
  flipped = pop_count[fl]

#define bbFlips_Horiz_f_low(pos)	\
  t = f_frontier((opp_bits_low >> (pos - 4)) & 0x2F);			\
  fl = f_flip[t & (my_bits_low >> (pos - 5))];				\
  my_bits_low |= fl << (pos - 4);					\
  flipped = pop_count[fl]

#define bbFlips_Horiz_f_high(pos)	\
  t = f_frontier((opp_bits_high >> (pos - 4)) & 0x2F);			\
  fl = f_flip[t & (my_bits_high >> (pos - 5))];				\
  my_bits_high |= fl << (pos - 4);					\
  flipped = pop_count[fl]


#define bbFlips_Down_1_low(pos, vec)	\
  t = opp_bits_low & (my_bits_low >> vec) & (1 << (pos + vec));		\
  my_bits_low |= t;							\
  flipped += t >> (pos + vec)

#define bbFlips_Down_1_high(pos, vec)	\
  t = opp_bits_high & (my_bits_high >> vec) & (1 << (pos + vec));	\
  my_bits_high |= t;							\
  flipped += t >> (pos + vec)

#define bbFlips_Up_1_low(pos, vec)	\
  t = opp_bits_low & (my_bits_low << vec) & (1 << (pos - vec));		\
  my_bits_low |= t;							\
  flipped += t >> (pos - vec)

#define bbFlips_Up_1_high(pos, vec)	\
  t = opp_bits_high & (my_bits_high << vec) & (1 << (pos - vec));	\
  my_bits_high |= t;							\
  flipped += t >> (pos - vec)


#if 1
#define bbFlips_Down_2_low(pos, vec, mask)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    t = opp_bits_low & (my_bits_low >> vec) & mask;			\
    my_bits_low |= t + (t >> vec);					\
    flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
  }

#define bbFlips_Down_2_high(pos, vec, mask)	\
  if (opp_bits_high & (1 << (pos + vec))) {				\
    t = opp_bits_high & (my_bits_high >> vec) & mask;			\
    my_bits_high |= t + (t >> vec);					\
    flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
  }

#define bbFlips_Up_2_low(pos, vec, mask)	\
  if (opp_bits_low & (1 << (pos - vec))) {				\
    t = opp_bits_low & (my_bits_low << vec) & mask;			\
    my_bits_low |= t + (t << vec);					\
    flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3;	\
  }

#define bbFlips_Up_2_high(pos, vec, mask)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    t = opp_bits_high & (my_bits_high << vec) & mask;			\
    my_bits_high |= t + (t << vec);					\
    flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3;	\
  }

#else
#define bbFlips_Down_2_low(pos, vec, mask)	\
  t = opp_bits_low & ((opp_bits_low | (1 << pos)) << vec) & (my_bits_low >> vec) & mask;	\
  my_bits_low |= t + (t >> vec);					\
  flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3

#define bbFlips_Down_2_high(pos, vec, mask)	\
  t = opp_bits_high & ((opp_bits_high | (1 << pos)) << vec) & (my_bits_high >> vec) & mask;	\
  my_bits_high |= t + (t >> vec);					\
  flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3

#define bbFlips_Up_2_low(pos, vec, mask)	\
  t = opp_bits_low & ((opp_bits_low | (1 << pos)) >> vec) & (my_bits_low << vec) & mask;	\
  my_bits_low |= t + (t << vec);					\
  flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3

#define bbFlips_Up_2_high(pos, vec, mask)	\
  t = opp_bits_high & ((opp_bits_high | (1 << pos)) >> vec) & (my_bits_high << vec) & mask;	\
  my_bits_high |= t + (t << vec);					\
  flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3
#endif


#define bbFlips_Down_3_3(pos, vec, maskh, maskl)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if ((~opp_bits_low & maskl) == 0) {					\
      t = (opp_bits_high >> (pos + vec * 4 - 32)) & 1;			\
      contig = 3 + t;							\
      t &= (opp_bits_high >> (pos + vec * 5 - 32));			\
      contig += t;							\
      t &= (opp_bits_high >> (pos + vec * 6 - 32));			\
      contig += t;							\
      t = lsb_mask[contig - 3] & maskh;					\
      if (my_bits_high & t) {						\
        my_bits_high |= t;						\
        my_bits_low |= maskl;						\
        flipped += contig;						\
      }									\
    } else {								\
      t = opp_bits_low & (my_bits_low >> vec) & maskl;			\
      my_bits_low |= t | (t >> vec);					\
      flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
    }									\
  }

#define bbFlips_Up_3_3(pos, vec, maskh, maskl)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    if ((~opp_bits_high & maskh) == 0) {				\
      t = (opp_bits_low >> (pos + 32 - vec * 4)) & 1;			\
      contig = 3 + t;							\
      t &= (opp_bits_low >> (pos + 32 - vec * 5));			\
      contig += t;							\
      t &= (opp_bits_low >> (pos + 32 - vec * 6));			\
      contig += t;							\
      t = msb_mask[contig - 3] & maskl;					\
      if (my_bits_low & t) {						\
        my_bits_low |= t;						\
        my_bits_high |= maskh;						\
        flipped += contig;						\
      }									\
    } else {								\
      t = opp_bits_high & (my_bits_high << vec) & maskh;		\
      my_bits_high |= t | (t << vec);					\
      flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3;	\
    }									\
  }

#if 1
#define bbFlips_Down_3_2(pos, vec, maskh, maskl)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if ((~opp_bits_low & maskl) == 0) {					\
      t = (opp_bits_high >> (pos + vec * 4 - 32)) & 1;			\
      contig = 3 + t;							\
      t &= (opp_bits_high >> (pos + vec * 5 - 32));			\
      contig += t;							\
      t = lsb_mask[contig - 3] & maskh;					\
      if (my_bits_high & t) {						\
        my_bits_high |= t;						\
        my_bits_low |= maskl;						\
        flipped += contig;						\
      }									\
    } else {								\
      t = opp_bits_low & (my_bits_low >> vec) & maskl;			\
      my_bits_low |= t | (t >> vec);					\
      flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
    }									\
  }
#else
#define bbFlips_Down_3_2(pos, vec, maskh, maskl)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if ((~opp_bits_low & maskl) == 0) {					\
      t = (opp_bits_high >> (pos + vec * 4 - 32)) & 1;			\
      contig = 3 + t;							\
      fl = (t << (pos + vec * 5 - 32)) + (1 << (pos + vec * 4 - 32));	\
      t &= (opp_bits_high >> (pos + vec * 5 - 32));			\
      contig += t;							\
      fl += (t << (pos + vec * 6 - 32));				\
      if (my_bits_high & fl) {						\
        my_bits_high |= fl;						\
        my_bits_low |= maskl;						\
        flipped += contig;						\
      }									\
    } else {								\
      t = opp_bits_low & (my_bits_low >> vec) & maskl;			\
      my_bits_low |= t | (t >> vec);					\
      flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
    }									\
  }
#endif

#define bbFlips_Up_3_2(pos, vec, maskh, maskl)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    if ((~opp_bits_high & maskh) == 0) {				\
      t = (opp_bits_low >> (pos + 32 - vec * 4)) & 1;			\
      contig = 3 + t;							\
      t &= (opp_bits_low >> (pos + 32 - vec * 5));			\
      contig += t;							\
      t = msb_mask[contig - 3] & maskl;					\
      if (my_bits_low & t) {						\
        my_bits_low |= t;						\
        my_bits_high |= maskh;						\
        flipped += contig;						\
      }									\
    } else {								\
      t = opp_bits_high & (my_bits_high << vec) & maskh;		\
      my_bits_high |= t | (t << vec);					\
      flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3;	\
    }									\
  }

#define bbFlips_Down_3_1(pos, vec, maskl)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if ((~opp_bits_low & maskl) == 0) {					\
      t = (opp_bits_high >> (pos + vec * 4 - 32)) & 1;			\
      contig = 3 + t;							\
      t = (t << (pos + vec * 5 - 32)) | (1 << (pos + vec * 4 - 32));	\
      if (my_bits_high & t) {						\
        my_bits_high |= t;						\
        my_bits_low |= maskl;						\
        flipped += contig;						\
      }									\
    } else {								\
      t = opp_bits_low & (my_bits_low >> vec) & maskl;			\
      my_bits_low |= t | (t >> vec);					\
      flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
    }									\
  }

#define bbFlips_Up_3_1(pos, vec, maskh)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    if ((~opp_bits_high & maskh) == 0) {				\
      t = (opp_bits_low >> (pos + 32 - vec * 4)) & 1;			\
      contig = 3 + t;							\
      t = (t << (pos + 32 - vec * 5)) | (1 << (pos + 32 - vec * 4));	\
      if (my_bits_low & t) {						\
        my_bits_low |= t;						\
        my_bits_high |= maskh;						\
        flipped += contig;						\
      }									\
    } else {								\
      t = opp_bits_high & (my_bits_high << vec) & maskh;		\
      my_bits_high |= t | (t << vec);					\
      flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3;	\
    }									\
  }

#define bbFlips_Down_3_0(pos, vec, maskl)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if ((~opp_bits_low & maskl) == 0) {					\
      t = (int)(my_bits_high << (31 - (pos + vec * 4 - 32))) >> 31;	\
      my_bits_low |= maskl & t;						\
      flipped += 3 & t;							\
    } else {								\
      t = opp_bits_low & (my_bits_low >> vec) & maskl;			\
      my_bits_low |= t | (t >> vec);					\
      flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
    }									\
  }

#define bbFlips_Up_3_0(pos, vec, maskh)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    if ((~opp_bits_high & maskh) == 0) {				\
      t = (int)(my_bits_low << (31 - (pos + 32 - vec * 4))) >> 31;	\
      my_bits_high |= maskh & t;					\
      flipped += 3 & t;							\
    } else {								\
      t = opp_bits_high & (my_bits_high << vec) & maskh;		\
      my_bits_high |= t | (t << vec);					\
      flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3;	\
    }									\
  }


#define bbFlips_Down_2_3(pos, vec, maskh)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if (opp_bits_low & (1 << (pos + vec * 2))) {			\
      t = (opp_bits_high >> (pos + vec * 3 - 32)) & 1;			\
      contig = 2 + t;							\
      t &= (opp_bits_high >> (pos + vec * 4 - 32));			\
      contig += t;							\
      t &= (opp_bits_high >> (pos + vec * 5 - 32));			\
      contig += t;							\
      t = lsb_mask[contig - 2] & maskh;					\
      if (my_bits_high & t) {						\
        my_bits_high |= t;						\
        my_bits_low |= (1 << (pos + vec)) | (1 << (pos + vec * 2));	\
        flipped += contig;						\
      }									\
    } else {								\
      t = (my_bits_low >> (pos + vec * 2)) & 1;				\
      my_bits_low |= t << (pos + vec);					\
      flipped += t;							\
    }									\
  }

#define bbFlips_Up_2_3(pos, vec, maskl)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    if (opp_bits_high & (1 << (pos - vec * 2))) {			\
      t = (opp_bits_low >> (pos + 32 - vec * 3)) & 1;			\
      contig = 2 + t;							\
      t &= (opp_bits_low >> (pos + 32 - vec * 4));			\
      contig += t;							\
      t &= (opp_bits_low >> (pos + 32 - vec * 5));			\
      contig += t;							\
      t = msb_mask[contig - 2] & maskl;					\
      if (my_bits_low & t) {						\
        my_bits_low |= t;						\
        my_bits_high |= (1 << (pos - vec)) | (1 << (pos - vec * 2));	\
        flipped += contig;						\
      }									\
    } else {								\
      t = (my_bits_high >> (pos - vec * 2)) & 1;			\
      my_bits_high |= t << (pos - vec);					\
      flipped += t;							\
    }									\
  }

#define bbFlips_Down_2_2(pos, vec, maskh)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if (opp_bits_low & (1 << (pos + vec * 2))) {			\
      t = (opp_bits_high >> (pos + vec * 3 - 32)) & 1;			\
      contig = 2 + t;							\
      t &= (opp_bits_high >> (pos + vec * 4 - 32));			\
      contig += t;							\
      t = lsb_mask[contig - 2] & maskh;					\
      if (my_bits_high & t) {						\
        my_bits_high |= t;						\
        my_bits_low |= (1 << (pos + vec)) | (1 << (pos + vec * 2));	\
        flipped += contig;						\
      }									\
    } else {								\
      t = (my_bits_low >> (pos + vec * 2)) & 1;				\
      my_bits_low |= t << (pos + vec);					\
      flipped += t;							\
    }									\
  }

#define bbFlips_Up_2_2(pos, vec, maskl)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    if (opp_bits_high & (1 << (pos - vec * 2))) {			\
      t = (opp_bits_low >> (pos + 32 - vec * 3)) & 1;			\
      contig = 2 + t;							\
      t &= (opp_bits_low >> (pos + 32 - vec * 4));			\
      contig += t;							\
      t = msb_mask[contig - 2] & maskl;					\
      if (my_bits_low & t) {						\
        my_bits_low |= t;						\
        my_bits_high |= (1 << (pos - vec)) | (1 << (pos - vec * 2));	\
        flipped += contig;						\
      }									\
    } else {								\
      t = (my_bits_high >> (pos - vec * 2)) & 1;			\
      my_bits_high |= t << (pos - vec);					\
      flipped += t;							\
    }									\
  }

#define bbFlips_Down_2_1(pos, vec)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    if (opp_bits_low & (1 << (pos + vec * 2))) {			\
      t = (opp_bits_high >> (pos + vec * 3 - 32)) & 1;			\
      contig = 2 + t;							\
      t = (t << (pos + vec * 4 - 32)) | (1 << (pos + vec * 3 - 32));	\
      if (my_bits_high & t) {						\
        my_bits_high |= t;						\
        my_bits_low |= (1 << (pos + vec)) | (1 << (pos + vec * 2));	\
        flipped += contig;						\
      }									\
    } else {								\
      t = (my_bits_low >> (pos + vec * 2)) & 1;				\
      my_bits_low |= t << (pos + vec);					\
      flipped += t;							\
    }									\
  }

#define bbFlips_Up_2_1(pos, vec)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    if (opp_bits_high & (1 << (pos - vec * 2))) {			\
      t = (opp_bits_low >> (pos + 32 - vec * 3)) & 1;			\
      contig = 2 + t;							\
      t = (t << (pos + 32 - vec * 4)) | (1 << (pos + 32 - vec * 3));	\
      if (my_bits_low & t) {						\
        my_bits_low |= t;						\
        my_bits_high |= (1 << (pos - vec)) | (1 << (pos - vec * 2));	\
        flipped += contig;						\
      }									\
    } else {								\
      t = (my_bits_high >> (pos - vec * 2)) & 1;			\
      my_bits_high |= t << (pos - vec);					\
      flipped += t;							\
    }									\
  }

#define bbFlips_Down_2_0(pos, vec, mask)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    t = opp_bits_low & ((my_bits_low >> vec) | (my_bits_high << (32 - vec))) & mask;	\
    my_bits_low |= t + (t >> vec);					\
    flipped += ((t >> (pos + vec)) | (t >> (pos + vec * 2 - 1))) & 3;	\
  }

#define bbFlips_Up_2_0(pos, vec, mask)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    t = opp_bits_high & ((my_bits_high << vec) | (my_bits_low >> (32 - vec))) & mask;	\
    my_bits_high |= t + (t << vec);					\
    flipped += ((t >> (pos - vec)) | (t >> (pos - vec * 2 - 1))) & 3;	\
  }


#define bbFlips_Down_1_3(pos, vec, maskh)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    t = (opp_bits_high >> (pos + vec * 2 - 32)) & 1;			\
    contig = 1 + t;							\
    t &= (opp_bits_high >> (pos + vec * 3 - 32));			\
    contig += t;							\
    t &= (opp_bits_high >> (pos + vec * 4 - 32));			\
    contig += t;							\
    t = lsb_mask[contig - 1] & maskh;					\
    if (my_bits_high & t) {						\
      my_bits_high |= t;						\
      my_bits_low |= 1 << (pos + vec);					\
      flipped += contig;						\
    }									\
  }

#define bbFlips_Up_1_3(pos, vec, maskl)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    t = (opp_bits_low >> (pos + 32 - vec * 2)) & 1;			\
    contig = 1 + t;							\
    t &= (opp_bits_low >> (pos + 32 - vec * 3));			\
    contig += t;							\
    t &= (opp_bits_low >> (pos + 32 - vec * 4));			\
    contig += t;							\
    t = msb_mask[contig - 1] & maskl;					\
    if (my_bits_low & t) {						\
      my_bits_low |= t;							\
      my_bits_high |= 1 << (pos - vec);					\
      flipped += contig;						\
    }									\
  }

#define bbFlips_Down_1_2(pos, vec, maskh)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    t = (opp_bits_high >> (pos + vec * 2 - 32)) & 1;			\
    contig = 1 + t;							\
    t &= (opp_bits_high >> (pos + vec * 3 - 32));			\
    contig += t;							\
    t = lsb_mask[contig - 1] & maskh;					\
    if (my_bits_high & t) {						\
      my_bits_high |= t;						\
      my_bits_low |= 1 << (pos + vec);					\
      flipped += contig;						\
    }									\
  }

#define bbFlips_Up_1_2(pos, vec, maskl)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    t = (opp_bits_low >> (pos + 32 - vec * 2)) & 1;			\
    contig = 1 + t;							\
    t &= (opp_bits_low >> (pos + 32 - vec * 3));			\
    contig += t;							\
    t = msb_mask[contig - 1] & maskl;					\
    if (my_bits_low & t) {						\
      my_bits_low |= t;							\
      my_bits_high |= 1 << (pos - vec);					\
      flipped += contig;						\
    }									\
  }

#define bbFlips_Down_1_1(pos, vec)	\
  if (opp_bits_low & (1 << (pos + vec))) {				\
    fl = (my_bits_high << (32 - vec)) & (1 << (pos + vec));		\
    t = opp_bits_high & (my_bits_high >> vec) & (1 << (pos + vec * 2 - 32));	\
    my_bits_low |= fl + (t << (32 - vec));				\
    my_bits_high |= t;							\
    flipped += ((fl >> (pos + vec)) | (t >> (pos + vec * 2 - 32 - 1)));	\
  }

#define bbFlips_Up_1_1(pos, vec)	\
  if (opp_bits_high & (1 << (pos - vec))) {				\
    fl = (my_bits_low >> (32 - vec)) & (1 << (pos - vec));		\
    t = opp_bits_low & (my_bits_low << vec) & (1 << (pos + 32 - vec * 2));	\
    my_bits_high |= fl + (t >> (32 - vec));				\
    my_bits_low |= t;							\
    flipped += ((fl >> (pos - vec)) | (t >> (pos + 32 - vec * 2 - 1)));	\
  }

#define bbFlips_Down_1_0(pos, vec)	\
  t = opp_bits_low & (my_bits_high << (32 - vec)) & (1 << (pos + vec));	\
  my_bits_low |= t;							\
  flipped += t >> (pos + vec)

#define bbFlips_Up_1_0(pos, vec)	\
  t = opp_bits_high & (my_bits_low >> (32 - vec)) & (1 << (pos - vec));	\
  my_bits_high |= t;							\
  flipped += t >> (pos - vec)


#if 1
#define bbFlips_Down_0_3(pos, vec, mask)	\
  if (opp_bits_high & (1 << (pos + vec - 32))) {			\
    t = (opp_bits_high >> (pos + vec * 2 - 32)) & 1;			\
    contig = 1 + t;							\
    t &= (opp_bits_high >> (pos + vec * 3 - 32));			\
    contig += t;							\
    fl = lsb_mask[contig] & mask;					\
    t = -(int)(my_bits_high & fl) >> 31;				\
    my_bits_high |= fl & t;						\
    flipped += contig & t;						\
  }
#else
#define bbFlips_Down_0_3(pos, vec, mask)	\
  if (opp_bits_high & (1 << (pos + vec - 32))) {			\
    t = opp_bits_high & (1 << (pos + vec * 2 - 32));			\
    fl = t + (1 << (pos + vec - 32));					\
    contig = 1 + (t >> (pos + vec * 2 - 32));				\
    t = opp_bits_high & (t << vec);					\
    fl += t;								\
    contig += (t >> (pos + vec * 3 - 32));				\
    t = -(int)(my_bits_high & (fl << vec)) >> 31;			\
    my_bits_high |= fl & t;						\
    flipped += contig & t;						\
  }
#endif

#define bbFlips_Up_0_3(pos, vec, mask)	\
  if (opp_bits_low & (1 << (pos + 32 - vec))) {				\
    t = (opp_bits_low >> (pos + 32 - vec * 2)) & 1;			\
    contig = 1 + t;							\
    t &= (opp_bits_low >> (pos + 32 - vec * 3));			\
    contig += t;							\
    fl = msb_mask[contig] & mask;					\
    t = -(int)(my_bits_low & fl) >> 31;					\
    my_bits_low |= fl & t;						\
    flipped += contig & t;						\
  }

#define bbFlips_Down_0_2(pos, vec, mask)	\
  t = opp_bits_high & ((opp_bits_high << vec) | (1 << (pos + vec - 32))) & (my_bits_high >> vec) & mask;	\
  my_bits_high |= t + (t >> vec);					\
  flipped += ((t >> (pos + vec - 32)) | (t >> (pos + vec * 2 - 32 - 1))) & 3

#define bbFlips_Up_0_2(pos, vec, mask)	\
  t = opp_bits_low & ((opp_bits_low >> vec) | (1 << (pos + 32 - vec))) & (my_bits_low << vec) & mask;	\
  my_bits_low |= t + (t << vec);					\
  flipped += ((t >> (pos + 32 - vec)) | (t >> (pos + 32 - vec * 2 - 1))) & 3

#define bbFlips_Down_0_1(pos, vec)	\
  t = opp_bits_high & (my_bits_high >> vec) & (1 << (pos + vec - 32));	\
  my_bits_high |= t;							\
  flipped += t >> (pos + vec - 32)

#define bbFlips_Up_0_1(pos, vec)	\
  t = opp_bits_low & (my_bits_low << vec) & (1 << (pos + 32 - vec));	\
  my_bits_low |= t;							\
  flipped += t >> (pos + 32 - vec)

////

static int REGPARM(2)
TestFlips_bitboard_a1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(0, 0x3F);
#if 1
  /* Down */
  bbFlips_Down_3_3(0, 8, 0x01010101u, 0x01010100u);
  /* Down right */
  bbFlips_Down_3_3(0, 9, 0x80402010u, 0x08040200u);
#elif 0
  /* Down */
  if (opp_bits_low & 0x00000100u) {
    contig = right_contiguous[(((opp_bits_low & 0x01010100u) + ((opp_bits_high & 0x00010101u) << 4)) * 0x01020408u) >> 25];
    fh = top_flip[contig + 1].high & 0x01010101u;
    fl = top_flip[contig + 1].low & 0x01010100u;
    t = -(int)((my_bits_low & fl) | (my_bits_high & fh)) >> 31;
    my_bits_high |= fh & t;
    my_bits_low |= fl & t;
    flipped += contig & t;
  }
  /* Down right */
  if (opp_bits_low & 0x00000200u) {
    contig = right_contiguous[(((opp_bits_low & 0x08040200u) + (opp_bits_high & 0x00402010u)) * 0x01010101u) >> 25];
    fh = top_flip[contig + 1].high & 0x80402010u;
    fl = top_flip[contig + 1].low & 0x08040200u;
    t = -(int)((my_bits_low & fl) | (my_bits_high & fh)) >> 31;
    my_bits_high |= fh & t;
    my_bits_low |= fl & t;
    flipped += contig & t;
  }
#elif 0
  /* Down */
  if (opp_bits_low & 0x00000100u) {
    contig = right_contiguous[(((opp_bits_low & 0x01010100u) + ((opp_bits_high & 0x00010101u) << 4)) * 0x01020408u) >> 25];
    if (contig >= 3) {
 #if 1
      fl = lsb_mask[contig - 3] & 0x01010101u;
 #else
      fl = 0x01010101u >> ((6 - contig) * 8);
 #endif
 #if 1
      if (my_bits_high & fl) {
        my_bits_high |= fl;
        my_bits_low |= 0x01010100u;
        flipped += contig;
      }
 #else
      t = -(int)(my_bits_high & fl) >> 31;
      my_bits_high |= fl & t;
      my_bits_low |= 0x01010100u & t;
      flipped += contig & t;
 #endif
    } else {
      fl = lsb_mask[contig + 1] & 0x01010100u;
      t = -(int)(my_bits_low & fl) >> 31;
      my_bits_low |= fl & t;
      flipped += contig & t;
    }
  }
  /* Down right */
  if (opp_bits_low & 0x00000200u) {
    contig = right_contiguous[(((opp_bits_low & 0x08040200u) + (opp_bits_high & 0x00402010u)) * 0x01010101u) >> 25];
    if (contig >= 3) {
 #if 1
      fl = lsb_mask[contig - 3] & 0x80402010u;
 #else
      fl = 0x80402010u >> ((6 - contig) * 9);
 #endif
 #if 1
      if (my_bits_high & fl) {
        my_bits_high |= fl;
        my_bits_low |= 0x08040200u;
        flipped += contig;
      }
 #else
      t = -(int)(my_bits_high & fl) >> 31;
      my_bits_high |= fl & t;
      my_bits_low |= 0x08040200u & t;
      flipped += contig & t;
 #endif
    } else {
      fl = lsb_mask[contig + 1] & 0x08040200u;
      t = -(int)(my_bits_low & fl) >> 31;
      my_bits_low |= fl & t;
      flipped += contig & t;
    }
  }
#elif 0
  /* Down */
  if (opp_bits_low & 0x00000100u) {
    if ((~opp_bits_low & 0x01010100u) == 0) {
      fl = high_flip[((((opp_bits_high & 0x00010101u) << 1) + (my_bits_high & 0x01010101u)) * 0x0103091Cu) >> 24];
      contig = (int) fl >> 24;
      t = contig >> 31;
      my_bits_high |= fl & 0x00010101u;
      my_bits_low |= 0x01010100u & t;
      flipped -= (contig - 2) & t;
    } else {
      t = opp_bits_low & (my_bits_low >> 8) & 0x00010100u;
      my_bits_low |= t | (t >> 8) ;
      flipped += ((t >> 8) + (t >> (16 - 1))) & 3;
    }
  }
  /* Down right */
  if (opp_bits_low & 0x00000200u) {
    if ((~opp_bits_low & 0x08040200u) == 0) {
      fl = high_flip[(((((opp_bits_high & 0x00402010u) << 1) + (my_bits_high & 0x80402010u)) >> 4) * 0x02030487u) >> 25];
      contig = (int) fl >> 24;
      t = contig >> 31;
      my_bits_high |= fl & 0x00402010u;
      my_bits_low |= 0x08040200u & t;
      flipped -= (contig - 2) & t;
    } else {
      t = opp_bits_low & (my_bits_low >> 9) & 0x00040200u;
      my_bits_low |= t | (t >> 9) ;
      flipped += ((t >> 9) + (t >> (18 - 1))) & 3;
    }
  }
#else
  /* Down */
  if (opp_bits_low & 0x00000100u) {
    if ((~opp_bits_low & 0x01010100u) == 0) {
      t = ((opp_bits_high | 0xFEFEFEFEu) + 0x01) & my_bits_high & 0x01010101u;
      if (t) {
        t = (t - 1) & 0x00010101u;
        my_bits_high |= t;
        my_bits_low |= 0x01010100u;
 #if 1
        flipped += 3 + (unsigned char)(t + (t >> 8) + (t >>16));
 #else
        flipped += 3 + ((t * 0x01010100u) >> 24);
 #endif
      }
    } else {
      contig = ((my_bits_low >> 16) & 1) + ((opp_bits_low >> (16 - 1)) & (my_bits_low >> (24 - 1)) & 2);
      my_bits_low |= lsb_mask[contig] & 0x01010100u;
      flipped += contig;
    }
  }
  /* Down right */
  if (opp_bits_low & 0x00000200u) {
    if ((~opp_bits_low & 0x08040200u) == 0) {
      t = ((opp_bits_high | 0x7FBFDFEFu) + 0x01) & my_bits_high & 0x80402010u;
      if (t) {
        t = (t - 1) & 0x00402010u;
        my_bits_high |= t;
        my_bits_low |= 0x08040200u;
 #if 1
        flipped += 3 + (unsigned char)((t >> 4) + (t >> 13) + (t >> 22));
 #else
        flipped += 3 + ((t * 0x00100804u) >> 24);
 #endif
      }
    } else {
      contig = ((my_bits_low >> 18) & 1) + ((opp_bits_low >> (18 - 1)) & (my_bits_low >> (27 - 1)) & 2);
      my_bits_low |= lsb_mask[contig] & 0x08040200u;
      flipped += contig;
    }
  }
#endif
  my_bits_low |= 0x00000001u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(7, 0x3F);
  /* Down left */
  bbFlips_Down_3_3(7, 7, 0x01020408u, 0x10204000u);
  /* Down */
  bbFlips_Down_3_3(7, 8, 0x80808080u, 0x80808000u);

  my_bits_low |= 0x00000080u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_a8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(24, 0x3F);
  /* Up right */
  bbFlips_Up_3_3(24, 7, 0x00020408u, 0x10204080u);
  /* Up */
  bbFlips_Up_3_3(24, 8, 0x00010101u, 0x01010101u);

  my_bits_high |= 0x01000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(31, 0x3F);
  /* Up */
  bbFlips_Up_3_3(31, 8, 0x00808080u, 0x80808080u);
  /* Up left */
  bbFlips_Up_3_3(31, 9, 0x00402010u, 0x08040201u);

  my_bits_high |= 0x80000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(1, 0x1F);
  /* Down */
  bbFlips_Down_3_3(1, 8, 0x02020202u, 0x02020200u);
  /* Down right */
  bbFlips_Down_3_2(1, 9, 0x00804020u, 0x10080400u);

  my_bits_low |= 0x00000002u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(6, 0x3E);
  /* Down left */
  bbFlips_Down_3_2(6, 7, 0x00010204u, 0x08102000u);
  /* Down */
  bbFlips_Down_3_3(6, 8, 0x40404040u, 0x40404000u);

  my_bits_low |= 0x00000040u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_a2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(8, 0x3F);
  /* Down */
  bbFlips_Down_2_3(8, 8, 0x01010101u);
  /* Down right */
  bbFlips_Down_2_3(8, 9, 0x40201008u);

  my_bits_low |= 0x00000100u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(15, 0x3F);
  /* Down left */
  bbFlips_Down_2_3(15, 7, 0x02040810u);
  /* Down */
  bbFlips_Down_2_3(15, 8, 0x80808080u);

  my_bits_low |= 0x00008000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_a7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(16, 0x3F);
  /* Up right */
  bbFlips_Up_2_3(16, 7, 0x08102040u);
  /* Up */
  bbFlips_Up_2_3(16, 8, 0x01010101u);

  my_bits_high |= 0x00010000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(23, 0x3F);
  /* Up */
  bbFlips_Up_2_3(23, 8, 0x80808080u);
  /* Up left */
  bbFlips_Up_2_3(23, 9, 0x10080402u);

  my_bits_high |= 0x00800000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(25, 0x1F);
  /* Up right */
  bbFlips_Up_3_2(25, 7, 0x00040810u, 0x20408000u);
  /* Up */
  bbFlips_Up_3_3(25, 8, 0x00020202u, 0x02020202u);

  my_bits_high |= 0x02000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(30, 0x3E);
  /* Up */
  bbFlips_Up_3_3(30, 8, 0x00404040u, 0x40404040u);
  /* Up left */
  bbFlips_Up_3_2(30, 9, 0x00201008u, 0x04020100u);

  my_bits_high |= 0x40000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(9, 0x1F);
  /* Down */
  bbFlips_Down_2_3(9, 8, 0x02020202u);
  /* Down right */
  bbFlips_Down_2_3(9, 9, 0x80402010u);

  my_bits_low |= 0x00000200u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(14, 0x3E);
  /* Down left */
  bbFlips_Down_2_3(14, 7, 0x01020408u);
  /* Down */
  bbFlips_Down_2_3(14, 8, 0x40404040u);

  my_bits_low |= 0x00004000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(17, 0x1F);
  /* Up right */
  bbFlips_Up_2_3(17, 7, 0x10204080u);
  /* Up */
  bbFlips_Up_2_3(17, 8, 0x02020202u);

  my_bits_high |= 0x00020000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(22, 0x3E);
  /* Up */
  bbFlips_Up_2_3(22, 8, 0x40404040u);
  /* Up left */
  bbFlips_Up_2_3(22, 9, 0x08040201u);

  my_bits_high |= 0x00400000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_low(2);
  /* Down left */
  bbFlips_Down_1_low(2, 7);
  /* Down */
  bbFlips_Down_3_3(2, 8, 0x04040404u, 0x04040400u);
  /* Down right */
  bbFlips_Down_3_1(2, 9, 0x20100800u);

  my_bits_low |= 0x00000004;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_low(5);
  /* Down left */
  bbFlips_Down_3_1(5, 7, 0x04081000u);
  /* Down */
  bbFlips_Down_3_3(5, 8, 0x20202020u, 0x20202000u);
  /* Down right */
  bbFlips_Down_1_low(5, 9);

  my_bits_low |= 0x00000020u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_a3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(16, 0x3F);
  /* Up right */
  bbFlips_Up_1_low(16, 7);
  /* Down */
  bbFlips_Down_1_3(16, 8, 0x01010101u);
  /* Up */
  bbFlips_Up_1_low(16, 8);
  /* Down right */
  bbFlips_Down_1_3(16, 9, 0x20100804u);

  my_bits_low |= 0x00010000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(23, 0x3F);
  /* Down left */
  bbFlips_Down_1_3(23, 7, 0x04081020u);
  /* Down */
  bbFlips_Down_1_3(23, 8, 0x80808080u);
  /* Up */
  bbFlips_Up_1_low(23, 8);
  /* Up left */
  bbFlips_Up_1_low(23, 9);

  my_bits_low |= 0x00800000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_a6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(8, 0x3F);
  /* Up right */
  bbFlips_Up_1_3(8, 7, 0x04081020u);
  /* Down */
  bbFlips_Down_1_high(8, 8);
  /* Up */
  bbFlips_Up_1_3(8, 8, 0x01010101u);
  /* Down right */
  bbFlips_Down_1_high(8, 9);

  my_bits_high |= 0x00000100u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(15, 0x3F);
  /* Down left */
  bbFlips_Down_1_high(15, 7);
  /* Down */
  bbFlips_Down_1_high(15, 8);
  /* Up */
  bbFlips_Up_1_3(15, 8, 0x80808080u);
  /* Up left */
  bbFlips_Up_1_3(15, 9, 0x20100804u);

  my_bits_high |= 0x00008000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_high(26);
  /* Up right */
  bbFlips_Up_3_1(26, 7, 0x00081020u);
  /* Up */
  bbFlips_Up_3_3(26, 8, 0x00040404u, 0x04040404u);
  /* Up left */
  bbFlips_Up_1_high(26, 9);

  my_bits_high |= 0x04000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_high(29);
  /* Up right */
  bbFlips_Up_1_high(29, 7);
  /* Up */
  bbFlips_Up_3_3(29, 8, 0x00202020u, 0x20202020u);
  /* Up left */
  bbFlips_Up_3_1(29, 9, 0x00100804u);

  my_bits_high |= 0x20000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_low(3);
  /* Down left */
  bbFlips_Down_2_low(3, 7, 0x00020400u);
  /* Down */
  bbFlips_Down_3_3(3, 8, 0x08080808u, 0x08080800u);
  /* Down right */
  bbFlips_Down_3_0(3, 9, 0x40201000u);

  my_bits_low |= 0x00000008u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e1( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_low(4);
  /* Down left */
  bbFlips_Down_3_0(4, 7, 0x02040800u);
  /* Down */
  bbFlips_Down_3_3(4, 8, 0x10101010u, 0x10101000u);
  /* Down right */
  bbFlips_Down_2_low(4, 9, 0x00402000u);

  my_bits_low |= 0x00000010u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_a4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(24, 0x3F);
  /* Up right */
  bbFlips_Up_2_low(24, 7, 0x00020400u);
  /* Down */
  bbFlips_Down_0_3(24, 8, 0x01010101u);
  /* Up */
  bbFlips_Up_2_low(24, 8, 0x00010100u);
  /* Down right */
  bbFlips_Down_0_3(24, 9, 0x10080402u);

  my_bits_low |= 0x01000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(31, 0x3F);
  /* Down left */
  bbFlips_Down_0_3(31, 7, 0x08102040u);
  /* Down */
  bbFlips_Down_0_3(31, 8, 0x80808080u);
  /* Up */
  bbFlips_Up_2_low(31, 8, 0x00808000u);
  /* Up left */
  bbFlips_Up_2_low(31, 9, 0x00402000u);

  my_bits_low |= 0x80000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_a5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(0, 0x3F);
  /* Up right */
  bbFlips_Up_0_3(0, 7, 0x02040810u);
  /* Down */
  bbFlips_Down_2_high(0, 8, 0x00010100u);
  /* Up */
  bbFlips_Up_0_3(0, 8, 0x01010101u);
  /* Down right */
  bbFlips_Down_2_high(0, 9, 0x00040200u);

  my_bits_high |= 0x00000001u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_h5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(7, 0x3F);
  /* Down left */
  bbFlips_Down_2_high(7, 7, 0x00204000u);
  /* Down */
  bbFlips_Down_2_high(7, 8, 0x00808000u);
  /* Up */
  bbFlips_Up_0_3(7, 8, 0x80808080u);
  /* Up left */
  bbFlips_Up_0_3(7, 9, 0x40201008u);

  my_bits_high |= 0x00000080u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_high(27);
  /* Up right */
  bbFlips_Up_3_0(27, 7, 0x00102040u);
  /* Up */
  bbFlips_Up_3_3(27, 8, 0x00080808u, 0x08080808u);
  /* Up left */
  bbFlips_Up_2_high(27, 9, 0x00040200u);

  my_bits_high |= 0x08000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e8( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_high(28);
  /* Up right */
  bbFlips_Up_2_high(28, 7, 0x00204000u);
  /* Up */
  bbFlips_Up_3_3(28, 8, 0x00101010u, 0x10101010u);
  /* Up left */
  bbFlips_Up_3_0(28, 9, 0x00080402u);

  my_bits_high |= 0x10000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_low(10);
  /* Down left */
  bbFlips_Down_1_low(10, 7);
  /* Down */
  bbFlips_Down_2_3(10, 8, 0x04040404u);
  /* Down right */
  bbFlips_Down_2_2(10, 9, 0x00804020u);

  my_bits_low |= 0x00000400u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_low(13);
  /* Down left */
  bbFlips_Down_2_2(13, 7, 0x00010204u);
  /* Down */
  bbFlips_Down_2_3(13, 8, 0x20202020u);
  /* Down right */
  bbFlips_Down_1_low(13, 9);

  my_bits_low |= 0x00002000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(17, 0x1F);
  /* Up right */
  bbFlips_Up_1_low(17, 7);
  /* Down */
  bbFlips_Down_1_3(17, 8, 0x02020202u);
  /* Up */
  bbFlips_Up_1_low(17, 8);
  /* Down right */
  bbFlips_Down_1_3(17, 9, 0x40201008u);

  my_bits_low |= 0x00020000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(22, 0x3E);
  /* Down left */
  bbFlips_Down_1_3(22, 7, 0x02040810u);
  /* Down */
  bbFlips_Down_1_3(22, 8, 0x40404040u);
  /* Up */
  bbFlips_Up_1_low(22, 8);
  /* Up left */
  bbFlips_Up_1_low(22, 9);

  my_bits_low |= 0x00400000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(9, 0x1F);
  /* Up right */
  bbFlips_Up_1_3(9, 7, 0x08102040u);
  /* Down */
  bbFlips_Down_1_high(9, 8);
  /* Up */
  bbFlips_Up_1_3(9, 8, 0x02020202u);
  /* Down right */
  bbFlips_Down_1_high(9, 9);

  my_bits_high |= 0x00000200u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(14, 0x3E);
  /* Down left */
  bbFlips_Down_1_high(14, 7);
  /* Down */
  bbFlips_Down_1_high(14, 8);
  /* Up */
  bbFlips_Up_1_3(14, 8, 0x40404040u);
  /* Up left */
  bbFlips_Up_1_3(14, 9, 0x10080402u);

  my_bits_high |= 0x00004000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_high(18);
  /* Up right */
  bbFlips_Up_2_2(18, 7, 0x20408000u);
  /* Up */
  bbFlips_Up_2_3(18, 8, 0x04040404u);
  /* Up left */
  bbFlips_Up_1_high(18, 9);

  my_bits_high |= 0x00040000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_high(21);
  /* Up right */
  bbFlips_Up_1_high(21, 7);
  /* Up */
  bbFlips_Up_2_3(21, 8, 0x20202020u);
  /* Up left */
  bbFlips_Up_2_2(21, 9, 0x04020100u);

  my_bits_high |= 0x00200000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_low(11);
  /* Down left */
  bbFlips_Down_2_0(11, 7, 0x02040000u);
  /* Down */
  bbFlips_Down_2_3(11, 8, 0x08080808u);
  /* Down right */
  bbFlips_Down_2_1(11, 9);

  my_bits_low |= 0x00000800u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e2( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_low(12);
  /* Down left */
  bbFlips_Down_2_1(12, 7);
  /* Down */
  bbFlips_Down_2_3(12, 8, 0x10101010u);
  /* Down right */
  bbFlips_Down_2_0(12, 9, 0x40200000u);

  my_bits_low |= 0x00001000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_low(25, 0x1F);
  /* Up right */
  bbFlips_Up_2_low(25, 7, 0x00040800u);
  /* Down */
  bbFlips_Down_0_3(25, 8, 0x02020202u);
  /* Up */
  bbFlips_Up_2_low(25, 8, 0x00020200u);
  /* Down right */
  bbFlips_Down_0_3(25, 9, 0x20100804u);

  my_bits_low |= 0x02000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_low(30, 0x3E);
  /* Down left */
  bbFlips_Down_0_3(30, 7, 0x04081020u);
  /* Down */
  bbFlips_Down_0_3(30, 8, 0x40404040u);
  /* Up */
  bbFlips_Up_2_low(30, 8, 0x00404000u);
  /* Up left */
  bbFlips_Up_2_low(30, 9, 0x00201000u);

  my_bits_low |= 0x40000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_b5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right */
  bbFlips_Right_high(1, 0x1F);
  /* Up right */
  bbFlips_Up_0_3(1, 7, 0x04081020u);
  /* Down */
  bbFlips_Down_2_high(1, 8, 0x00020200u);
  /* Up */
  bbFlips_Up_0_3(1, 8, 0x02020202u);
  /* Down right */
  bbFlips_Down_2_high(1, 9, 0x00080400u);

  my_bits_high |= 0x00000002u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_g5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Left */
  bbFlips_Left_high(6, 0x3E);
  /* Down left */
  bbFlips_Down_2_high(6, 7, 0x00102000u);
  /* Down */
  bbFlips_Down_2_high(6, 8, 0x00404000u);
  /* Up */
  bbFlips_Up_0_3(6, 8, 0x40404040u);
  /* Up left */
  bbFlips_Up_0_3(6, 9, 0x20100804u);

  my_bits_high |= 0x00000040u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_high(19);
  /* Up right */
  bbFlips_Up_2_1(19, 7);
  /* Up */
  bbFlips_Up_2_3(19, 8, 0x08080808u);
  /* Up left */
  bbFlips_Up_2_0(19, 9, 0x00000402u);

  my_bits_high |= 0x00080000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e7( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_high(20);
  /* Up right */
  bbFlips_Up_2_0(20, 7, 0x00002040u);
  /* Up */
  bbFlips_Up_2_3(20, 8, 0x10101010u);
  /* Up left */
  bbFlips_Up_2_1(20, 9);

  my_bits_high |= 0x00100000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_low(18);
  /* Down left */
  bbFlips_Down_1_0(18, 7);
  /* Up right */
  bbFlips_Up_1_low(18, 7);
  /* Down */
  bbFlips_Down_1_3(18, 8, 0x04040404u);
  /* Up */
  bbFlips_Up_1_low(18, 8);
  /* Down right */
  bbFlips_Down_1_3(18, 9, 0x80402010u);
  /* Up left */
  bbFlips_Up_1_low(18, 9);

  my_bits_low |= 0x00040000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_low(21);
  /* Down left */
  bbFlips_Down_1_3(21, 7, 0x01020408u);
  /* Up right */
  bbFlips_Up_1_low(21, 7);
  /* Down */
  bbFlips_Down_1_3(21, 8, 0x20202020u);
  /* Up */
  bbFlips_Up_1_low(21, 8);
  /* Down right */
  bbFlips_Down_1_0(21, 9);
  /* Up left */
  bbFlips_Up_1_low(21, 9);

  my_bits_low |= 0x00200000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_high(10);
  /* Down left */
  bbFlips_Down_1_high(10, 7);
  /* Up right */
  bbFlips_Up_1_3(10, 7, 0x10204080u);
  /* Down */
  bbFlips_Down_1_high(10, 8);
  /* Up */
  bbFlips_Up_1_3(10, 8, 0x04040404u);
  /* Down right */
  bbFlips_Down_1_high(10, 9);
  /* Up left */
  bbFlips_Up_1_0(10, 9);

  my_bits_high |= 0x00000400u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_high(13);
  /* Down left */
  bbFlips_Down_1_high(13, 7);
  /* Up right */
  bbFlips_Up_1_0(13, 7);
  /* Down */
  bbFlips_Down_1_high(13, 8);
  /* Up */
  bbFlips_Up_1_3(13, 8, 0x20202020u);
  /* Down right */
  bbFlips_Down_1_high(13, 9);
  /* Up left */
  bbFlips_Up_1_3(13, 9, 0x08040201u);

  my_bits_high |= 0x00002000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_low(19);
  /* Down left */
  bbFlips_Down_1_1(19, 7);
  /* Up right */
  bbFlips_Up_1_low(19, 7);
  /* Down */
  bbFlips_Down_1_3(19, 8, 0x08080808u);
  /* Up */
  bbFlips_Up_1_low(19, 8);
  /* Down right */
  bbFlips_Down_1_2(19, 9, 0x00804020u);
  /* Up left */
  bbFlips_Up_1_low(19, 9);

  my_bits_low |= 0x00080000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e3( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_low(20);
  /* Down left */
  bbFlips_Down_1_2(20, 7, 0x00010204u);
  /* Up right */
  bbFlips_Up_1_low(20, 7);
  /* Down */
  bbFlips_Down_1_3(20, 8, 0x10101010u);
  /* Up */
  bbFlips_Up_1_low(20, 8);
  /* Down right */
  bbFlips_Down_1_1(20, 9);
  /* Up left */
  bbFlips_Up_1_low(20, 9);

  my_bits_low |= 0x00100000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_low(26);
  /* Down left */
  bbFlips_Down_0_1(26, 7);
  /* Up right */
  bbFlips_Up_2_low(26, 7, 0x00081000u);
  /* Down */
  bbFlips_Down_0_3(26, 8, 0x04040404u);
  /* Up */
  bbFlips_Up_2_low(26, 8, 0x00040400u);
  /* Down right */
  bbFlips_Down_0_3(26, 9, 0x40201008u);
  /* Up left */
  bbFlips_Up_1_low(26, 9);

  my_bits_low |= 0x04000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_low(29);
  /* Down left */
  bbFlips_Down_0_3(29, 7, 0x02040810u);
  /* Up right */
  bbFlips_Up_1_low(29, 7);
  /* Down */
  bbFlips_Down_0_3(29, 8, 0x20202020u);
  /* Up */
  bbFlips_Up_2_low(29, 8, 0x00202000u);
  /* Down right */
  bbFlips_Down_0_1(29, 9);
  /* Up left */
  bbFlips_Up_2_low(29, 9, 0x00100800u);

  my_bits_low |= 0x20000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_c5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_c_high(2);
  /* Down left */
  bbFlips_Down_1_high(2, 7);
  /* Up right */
  bbFlips_Up_0_3(2, 7, 0x08102040u);
  /* Down */
  bbFlips_Down_2_high(2, 8, 0x00040400u);
  /* Up */
  bbFlips_Up_0_3(2, 8, 0x04040404u);
  /* Down right */
  bbFlips_Down_2_high(2, 9, 0x00100800u);
  /* Up left */
  bbFlips_Up_0_1(2, 9);

  my_bits_high |= 0x00000004u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_f5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_f_high(5);
  /* Down left */
  bbFlips_Down_2_high(5, 7, 0x00081000u);
  /* Up right */
  bbFlips_Up_0_1(5, 7);
  /* Down */
  bbFlips_Down_2_high(5, 8, 0x00202000u);
  /* Up */
  bbFlips_Up_0_3(5, 8, 0x20202020u);
  /* Down right */
  bbFlips_Down_1_high(5, 9);
  /* Up left */
  bbFlips_Up_0_3(5, 9, 0x10080402u);

  my_bits_high |= 0x00000020u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_high(11);
  /* Down left */
  bbFlips_Down_1_high(11, 7);
  /* Up right */
  bbFlips_Up_1_2(11, 7, 0x20408000u);
  /* Down */
  bbFlips_Down_1_high(11, 8);
  /* Up */
  bbFlips_Up_1_3(11, 8, 0x08080808u);
  /* Down right */
  bbFlips_Down_1_high(11, 9);
  /* Up left */
  bbFlips_Up_1_1(11, 9);

  my_bits_high |= 0x00000800u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e6( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_high(12);
  /* Down left */
  bbFlips_Down_1_high(12, 7);
  /* Up right */
  bbFlips_Up_1_1(12, 7);
  /* Down */
  bbFlips_Down_1_high(12, 8);
  /* Up */
  bbFlips_Up_1_3(12, 8, 0x10101010u);
  /* Down right */
  bbFlips_Down_1_high(12, 9);
  /* Up left */
  bbFlips_Up_1_2(12, 9, 0x04020100u);

  my_bits_high |= 0x00001000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_low(27);
  /* Down left */
  bbFlips_Down_0_2(27, 7, 0x00000204u);
  /* Up right */
  bbFlips_Up_2_low(27, 7, 0x00102000u);
  /* Down */
  bbFlips_Down_0_3(27, 8, 0x08080808u);
  /* Up */
  bbFlips_Up_2_low(27, 8, 0x00080800u);
  /* Down right */
  bbFlips_Down_0_3(27, 9, 0x80402010u);
  /* Up left */
  bbFlips_Up_2_low(27, 9, 0x00040200u);

  my_bits_low |= 0x08000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e4( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_low(28);
  /* Down left */
  bbFlips_Down_0_3(28, 7, 0x01020408u);
  /* Up right */
  bbFlips_Up_2_low(28, 7, 0x00204000u);
  /* Down */
  bbFlips_Down_0_3(28, 8, 0x10101010u);
  /* Up */
  bbFlips_Up_2_low(28, 8, 0x00101000u);
  /* Down right */
  bbFlips_Down_0_2(28, 9, 0x00004020u);
  /* Up left */
  bbFlips_Up_2_low(28, 9, 0x00080400u);

  my_bits_low |= 0x10000000u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_d5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_d_high(3);
  /* Down left */
  bbFlips_Down_2_high(3, 7, 0x00020400u);
  /* Up right */
  bbFlips_Up_0_3(3, 7, 0x10204080u);
  /* Down */
  bbFlips_Down_2_high(3, 8, 0x00080800u);
  /* Up */
  bbFlips_Up_0_3(3, 8, 0x08080808u);
  /* Down right */
  bbFlips_Down_2_high(3, 9, 0x00201000u);
  /* Up left */
  bbFlips_Up_0_2(3, 9, 0x04020000u);

  my_bits_high |= 0x00000008u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

static int REGPARM(2)
TestFlips_bitboard_e5( unsigned int my_bits_high, unsigned int my_bits_low, unsigned int opp_bits_high, unsigned int opp_bits_low ) {
  int flipped, contig;
  unsigned int t, fl;

  /* Right / Left */
  bbFlips_Horiz_e_high(4);
  /* Down left */
  bbFlips_Down_2_high(4, 7, 0x00040800u);
  /* Up right */
  bbFlips_Up_0_2(4, 7, 0x20400000u);
  /* Down */
  bbFlips_Down_2_high(4, 8, 0x00101000u);
  /* Up */
  bbFlips_Up_0_3(4, 8, 0x10101010u);
  /* Down right */
  bbFlips_Down_2_high(4, 9, 0x00402000u);
  /* Up left */
  bbFlips_Up_0_3(4, 9, 0x08040201u);

  my_bits_high |= 0x00000010u;
  bb_flips.high = my_bits_high;
  bb_flips.low = my_bits_low;
  return flipped;
}

int (REGPARM(2) * const TestFlips_bitboard[78])(unsigned int, unsigned int, unsigned int, unsigned int) = {
  TestFlips_bitboard_a1,
  TestFlips_bitboard_b1,
  TestFlips_bitboard_c1,
  TestFlips_bitboard_d1,
  TestFlips_bitboard_e1,
  TestFlips_bitboard_f1,
  TestFlips_bitboard_g1,
  TestFlips_bitboard_h1,
  NULL,
  NULL,
  TestFlips_bitboard_a2,
  TestFlips_bitboard_b2,
  TestFlips_bitboard_c2,
  TestFlips_bitboard_d2,
  TestFlips_bitboard_e2,
  TestFlips_bitboard_f2,
  TestFlips_bitboard_g2,
  TestFlips_bitboard_h2,
  NULL,
  NULL,
  TestFlips_bitboard_a3,
  TestFlips_bitboard_b3,
  TestFlips_bitboard_c3,
  TestFlips_bitboard_d3,
  TestFlips_bitboard_e3,
  TestFlips_bitboard_f3,
  TestFlips_bitboard_g3,
  TestFlips_bitboard_h3,
  NULL,
  NULL,
  TestFlips_bitboard_a4,
  TestFlips_bitboard_b4,
  TestFlips_bitboard_c4,
  TestFlips_bitboard_d4,
  TestFlips_bitboard_e4,
  TestFlips_bitboard_f4,
  TestFlips_bitboard_g4,
  TestFlips_bitboard_h4,
  NULL,
  NULL,
  TestFlips_bitboard_a5,
  TestFlips_bitboard_b5,
  TestFlips_bitboard_c5,
  TestFlips_bitboard_d5,
  TestFlips_bitboard_e5,
  TestFlips_bitboard_f5,
  TestFlips_bitboard_g5,
  TestFlips_bitboard_h5,
  NULL,
  NULL,
  TestFlips_bitboard_a6,
  TestFlips_bitboard_b6,
  TestFlips_bitboard_c6,
  TestFlips_bitboard_d6,
  TestFlips_bitboard_e6,
  TestFlips_bitboard_f6,
  TestFlips_bitboard_g6,
  TestFlips_bitboard_h6,
  NULL,
  NULL,
  TestFlips_bitboard_a7,
  TestFlips_bitboard_b7,
  TestFlips_bitboard_c7,
  TestFlips_bitboard_d7,
  TestFlips_bitboard_e7,
  TestFlips_bitboard_f7,
  TestFlips_bitboard_g7,
  TestFlips_bitboard_h7,
  NULL,
  NULL,
  TestFlips_bitboard_a8,
  TestFlips_bitboard_b8,
  TestFlips_bitboard_c8,
  TestFlips_bitboard_d8,
  TestFlips_bitboard_e8,
  TestFlips_bitboard_f8,
  TestFlips_bitboard_g8,
  TestFlips_bitboard_h8
};
