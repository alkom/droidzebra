/*
   File:          bitbmob.c

   Modified:      November 18, 2005

   Authors:       Gunnar Andersson (gunnar@radagast.se)
	          Toshihiko Okuhara

   Contents:      Count feasible moves in the bitboard.

   This piece of software is released under the GPL.
   See the file COPYING for more information.
*/



#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "bitbmob.h"
#include "bitboard.h"


#ifdef USE_PENTIUM_ASM
static const unsigned long long mmx_c7e = 0x7e7e7e7e7e7e7e7eULL;
static const unsigned long long mmx_c0f = 0x0f0f0f0f0f0f0f0fULL;
static const unsigned long long mmx_c33 = 0x3333333333333333ULL;
static const unsigned long long mmx_c55 = 0x5555555555555555ULL;

static int useMMX;
#endif

void
init_mmx( void ) {
#ifdef USE_PENTIUM_ASM
  unsigned int flg1, flg2, cpuid_edx;

  useMMX = 0;
  asm volatile (
	"pushf\n\t"
	"popl	%0\n\t"
	"movl	%0, %1\n\t"
	"btc	$21, %0\n\t"	/* flip ID bit in EFLAGS */
	"pushl	%0\n\t"
	"popf\n\t"
	"pushf\n\t"
	"popl	%0"
  : "=r" (flg1), "=r" (flg2) );
  if (flg1 != flg2) {		/* CPUID supported */
    asm volatile (
	"movl	$1, %%eax\n\t"
	"cpuid"
    : "=d" (cpuid_edx) :: "%eax", "%ebx", "%ecx" );
    useMMX = ((cpuid_edx & 0x00800000u) != 0);	/* MMX bit */
  }
#endif
}


static BitBoard
generate_all_c( const BitBoard my_bits,		// mm7
	        const BitBoard opp_bits ) {	// mm6
  BitBoard moves;		// mm4
  BitBoard opp_inner_bits;	// mm5
  BitBoard flip_bits;		// mm1
  BitBoard adjacent_opp_bits;	// mm3

  opp_inner_bits.high = opp_bits.high & 0x7E7E7E7Eu;
  opp_inner_bits.low = opp_bits.low & 0x7E7E7E7Eu;

  flip_bits.high = (my_bits.high >> 1) & opp_inner_bits.high;		/* 0 m7&o6 m6&o5 .. m2&o1 0 */
  flip_bits.low = (my_bits.low >> 1) & opp_inner_bits.low;
  flip_bits.high |= (flip_bits.high >> 1) & opp_inner_bits.high;	/* 0 m7&o6 (m6&o5)|(m7&o6&o5) .. (m2&o1)|(m3&o2&o1) 0 */
  flip_bits.low |= (flip_bits.low >> 1) & opp_inner_bits.low;

  adjacent_opp_bits.high = opp_inner_bits.high & (opp_inner_bits.high >> 1);	/* 0 o7&o6 o6&o5 o5&o4 o4&o3 o3&o2 o2&o1 0 */
  adjacent_opp_bits.low = opp_inner_bits.low & (opp_inner_bits.low >> 1);
  flip_bits.high |= (flip_bits.high >> 2) & adjacent_opp_bits.high;	/* 0 m7&o6 (m6&o5)|(m7&o6&o5) ..|(m7&o6&o5&o4) ..|(m6&o5&o4&o3)|(m7&o6&o5&o4&o3) .. */
  flip_bits.low |= (flip_bits.low >> 2) & adjacent_opp_bits.low;
  flip_bits.high |= (flip_bits.high >> 2) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low >> 2) & adjacent_opp_bits.low;

  moves.high = flip_bits.high >> 1;
  moves.low = flip_bits.low >> 1;

  flip_bits.high = (my_bits.high << 1) & opp_inner_bits.high;
  flip_bits.low = (my_bits.low << 1) & opp_inner_bits.low;
  flip_bits.high |= (flip_bits.high << 1) & opp_inner_bits.high;
  flip_bits.low |= (flip_bits.low << 1) & opp_inner_bits.low;

  adjacent_opp_bits.high = opp_inner_bits.high & (opp_inner_bits.high << 1);
  adjacent_opp_bits.low = opp_inner_bits.low & (opp_inner_bits.low << 1);
  flip_bits.high |= (flip_bits.high << 2) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 2) & adjacent_opp_bits.low;
  flip_bits.high |= (flip_bits.high << 2) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 2) & adjacent_opp_bits.low;

  moves.high |= flip_bits.high << 1;
  moves.low |= flip_bits.low << 1;

  flip_bits.high = (my_bits.high >> 8) & opp_bits.high;
  flip_bits.low = ((my_bits.low >> 8) | (my_bits.high << 24)) & opp_bits.low;
  flip_bits.high |= (flip_bits.high >> 8) & opp_bits.high;
  flip_bits.low |= ((flip_bits.low >> 8) | (flip_bits.high << 24)) & opp_bits.low;

  adjacent_opp_bits.high = opp_bits.high & (opp_bits.high >> 8);
  adjacent_opp_bits.low = opp_bits.low & ((opp_bits.low >> 8) | (opp_bits.high << 24));
  flip_bits.high |= (flip_bits.high >> 16) & adjacent_opp_bits.high;
  flip_bits.low |= ((flip_bits.low >> 16) | (flip_bits.high << 16)) & adjacent_opp_bits.low;
  flip_bits.high |= (flip_bits.high >> 16) & adjacent_opp_bits.high;
  flip_bits.low |= ((flip_bits.low >> 16) | (flip_bits.high << 16)) & adjacent_opp_bits.low;

  moves.high |= flip_bits.high >> 8;
  moves.low |= (flip_bits.low >> 8) | (flip_bits.high << 24);

  flip_bits.high = ((my_bits.high << 8) | (my_bits.low >> 24)) & opp_bits.high;
  flip_bits.low = (my_bits.low << 8) & opp_bits.low;
  flip_bits.high |= ((flip_bits.high << 8) | (flip_bits.low >> 24)) & opp_bits.high;
  flip_bits.low |= (flip_bits.low << 8) & opp_bits.low;

  adjacent_opp_bits.high = opp_bits.high & ((opp_bits.high << 8) | (opp_bits.low >> 24));
  adjacent_opp_bits.low = opp_bits.low & (opp_bits.low << 8);
  flip_bits.high |= ((flip_bits.high << 16) | (flip_bits.low >> 16)) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 16) & adjacent_opp_bits.low;
  flip_bits.high |= ((flip_bits.high << 16) | (flip_bits.low >> 16)) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 16) & adjacent_opp_bits.low;

  moves.high |= (flip_bits.high << 8) | (flip_bits.low >> 24);
  moves.low |= flip_bits.low << 8;

  flip_bits.high = (my_bits.high >> 7) & opp_inner_bits.high;
  flip_bits.low = ((my_bits.low >> 7) | (my_bits.high << 25)) & opp_inner_bits.low;
  flip_bits.high |= (flip_bits.high >> 7) & opp_inner_bits.high;
  flip_bits.low |= ((flip_bits.low >> 7) | (flip_bits.high << 25)) & opp_inner_bits.low;

  adjacent_opp_bits.high = opp_inner_bits.high & (opp_inner_bits.high >> 7);
  adjacent_opp_bits.low = opp_inner_bits.low & ((opp_inner_bits.low >> 7) | (opp_inner_bits.high << 25));
  flip_bits.high |= (flip_bits.high >> 14) & adjacent_opp_bits.high;
  flip_bits.low |= ((flip_bits.low >> 14) | (flip_bits.high << 18)) & adjacent_opp_bits.low;
  flip_bits.high |= (flip_bits.high >> 14) & adjacent_opp_bits.high;
  flip_bits.low |= ((flip_bits.low >> 14) | (flip_bits.high << 18)) & adjacent_opp_bits.low;

  moves.high |= flip_bits.high >> 7;
  moves.low |= (flip_bits.low >> 7) | (flip_bits.high << 25);

  flip_bits.high = ((my_bits.high << 7) | (my_bits.low >> 25)) & opp_inner_bits.high;
  flip_bits.low = (my_bits.low << 7) & opp_inner_bits.low;
  flip_bits.high |= ((flip_bits.high << 7) | (flip_bits.low >> 25)) & opp_inner_bits.high;
  flip_bits.low |= (flip_bits.low << 7) & opp_inner_bits.low;

  adjacent_opp_bits.high = opp_inner_bits.high & ((opp_inner_bits.high << 7) | (opp_inner_bits.low >> 25));
  adjacent_opp_bits.low = opp_inner_bits.low & (opp_inner_bits.low << 7);
  flip_bits.high |= ((flip_bits.high << 14) | (flip_bits.low >> 18)) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 14) & adjacent_opp_bits.low;
  flip_bits.high |= ((flip_bits.high << 14) | (flip_bits.low >> 18)) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 14) & adjacent_opp_bits.low;

  moves.high |= (flip_bits.high << 7) | (flip_bits.low >> 25);
  moves.low |= flip_bits.low << 7;

  flip_bits.high = (my_bits.high >> 9) & opp_inner_bits.high;
  flip_bits.low = ((my_bits.low >> 9) | (my_bits.high << 23)) & opp_inner_bits.low;
  flip_bits.high |= (flip_bits.high >> 9) & opp_inner_bits.high;
  flip_bits.low |= ((flip_bits.low >> 9) | (flip_bits.high << 23)) & opp_inner_bits.low;

  adjacent_opp_bits.high = opp_inner_bits.high & (opp_inner_bits.high >> 9);
  adjacent_opp_bits.low = opp_inner_bits.low & ((opp_inner_bits.low >> 9) | (opp_inner_bits.high << 23));
  flip_bits.high |= (flip_bits.high >> 18) & adjacent_opp_bits.high;
  flip_bits.low |= ((flip_bits.low >> 18) | (flip_bits.high << 14)) & adjacent_opp_bits.low;
  flip_bits.high |= (flip_bits.high >> 18) & adjacent_opp_bits.high;
  flip_bits.low |= ((flip_bits.low >> 18) | (flip_bits.high << 14)) & adjacent_opp_bits.low;

  moves.high |= flip_bits.high >> 9;
  moves.low |= (flip_bits.low >> 9) | (flip_bits.high << 23);

  flip_bits.high = ((my_bits.high << 9) | (my_bits.low >> 23)) & opp_inner_bits.high;
  flip_bits.low = (my_bits.low << 9) & opp_inner_bits.low;
  flip_bits.high |= ((flip_bits.high << 9) | (flip_bits.low >> 23)) & opp_inner_bits.high;
  flip_bits.low |= (flip_bits.low << 9) & opp_inner_bits.low;

  adjacent_opp_bits.high = opp_inner_bits.high & ((opp_inner_bits.high << 9) | (opp_inner_bits.low >> 23));
  adjacent_opp_bits.low = opp_inner_bits.low & (opp_inner_bits.low << 9);
  flip_bits.high |= ((flip_bits.high << 18) | (flip_bits.low >> 14)) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 18) & adjacent_opp_bits.low;
  flip_bits.high |= ((flip_bits.high << 18) | (flip_bits.low >> 14)) & adjacent_opp_bits.high;
  flip_bits.low |= (flip_bits.low << 18) & adjacent_opp_bits.low;

  moves.high |= (flip_bits.high << 9) | (flip_bits.low >> 23);
  moves.low |= flip_bits.low << 9;

  moves.high &= ~(my_bits.high | opp_bits.high);
  moves.low &= ~(my_bits.low | opp_bits.low);
  return moves;
}

#ifdef USE_PENTIUM_ASM

static void pseudo_mobility_mmx(unsigned int my_high, unsigned int opp_high) {
			/* mm7: my, mm6: opp */
  asm volatile(
			/* shift=+1 */			/* shift=+8 */
	"movl	%%esi, %%eax\n\t"	"movq	%%mm7, %%mm0\n\t"
					"movq	%2, %%mm5\n\t"
	"shrl	$1, %%eax\n\t"		"psrlq	$8, %%mm0\n\t"
	"andl	$2122219134, %%edi\n\t"	"pand	%%mm6, %%mm5\n\t"
	"andl	%%edi, %%eax\n\t"	"pand	%%mm6, %%mm0\n\t"	/* 0 m7&o6 m6&o5 .. m1&o0 */
	"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
	"shrl	$1, %%eax\n\t"		"psrlq	$8, %%mm0\n\t"
	"movl	%%edi, %%ecx\n\t"	"movq	%%mm6, %%mm3\n\t"
	"andl	%%edi, %%eax\n\t"	"pand	%%mm6, %%mm0\n\t"	/* 0 0 m7&o6&o5 .. m2&o1&o0 */
	"shrl	$1, %%ecx\n\t"		"psrlq	$8, %%mm3\n\t"
	"orl	%%edx, %%eax\n\t"	"por	%%mm1, %%mm0\n\t"	/* 0 m7&o6 (m6&o5)|(m7&o6&o5) .. (m1&o0) */
	"andl	%%edi, %%ecx\n\t"	"pand	%%mm6, %%mm3\n\t"	/* 0 o7&o6 o6&o5 o5&o4 o4&o3 .. */
	"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm4\n\t"
	"shrl	$2, %%eax\n\t"		"psrlq	$16, %%mm0\n\t"
	"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"	/* 0 0 0 m7&o6&o5&o4 (m6&o5&o4&o3)|(m7&o6&o5&o4&o3) .. */
	"orl	%%eax, %%edx\n\t"	"por	%%mm0, %%mm4\n\t"
	"shrl	$2, %%eax\n\t"		"psrlq	$16, %%mm0\n\t"
	"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"	/* 0 0 0 0 0 m7&o6&..&o2 (m6&o5&..&o1)|(m7&o6&..&o1) .. */
	"orl	%%edx, %%eax\n\t"	"por	%%mm0, %%mm4\n\t"
	"shrl	$1, %%eax\n\t"		"psrlq	$8, %%mm4\n\t"		/* result of +8 */
			/* shift=-1 */			/* shift=-8 */
					"movq	%%mm7, %%mm0\n\t"
	"addl	%%esi, %%esi\n\t"	"psllq	$8, %%mm0\n\t"
	"andl	%%edi, %%esi\n\t"	"pand	%%mm6, %%mm0\n\t"
	"movl	%%esi, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
	"addl	%%esi, %%esi\n\t"	"psllq	$8, %%mm0\n\t"
	"andl	%%edi, %%esi\n\t"	"pand	%%mm6, %%mm0\n\t"
	"orl	%%edx, %%esi\n\t"	"por	%%mm1, %%mm0\n\t"
	"addl	%%ecx, %%ecx\n\t"	"psllq	$8, %%mm3\n\t"
	"movl	%%esi, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
	"shll	$2, %%esi\n\t"		"psllq	$16, %%mm0\n\t"
	"andl	%%ecx, %%esi\n\t"	"pand	%%mm3, %%mm0\n\t"
	"orl	%%esi, %%edx\n\t"	"por	%%mm0, %%mm1\n\t"
	"shll	$2, %%esi\n\t"		"psllq	$16, %%mm0\n\t"
	"andl	%%ecx, %%esi\n\t"	"pand	%%mm3, %%mm0\n\t"
	"orl	%%edx, %%esi\n\t"	"por	%%mm1, %%mm0\n\t"
	"addl	%%esi, %%esi\n\t"	"psllq	$8, %%mm0\n\t"
	"orl	%%eax, %%esi\n\t"	"por	%%mm0, %%mm4\n\t"
			/* Serialize */			/* shift=+7 */
					"movq	%%mm7, %%mm0\n\t"
	"movd	%%esi, %%mm1\n\t"
					"psrlq	$7, %%mm0\n\t"
	"psllq	$32, %%mm1\n\t"
					"pand	%%mm5, %%mm0\n\t"
	"por	%%mm1, %%mm4\n\t"
					"movq	%%mm0, %%mm1\n\t"
					"psrlq	$7, %%mm0\n\t"
					"pand	%%mm5, %%mm0\n\t"
					"movq	%%mm5, %%mm3\n\t"
					"por	%%mm1, %%mm0\n\t"
					"psrlq	$7, %%mm3\n\t"
					"movq	%%mm0, %%mm1\n\t"
					"pand	%%mm5, %%mm3\n\t"
					"psrlq	$14, %%mm0\n\t"
					"pand	%%mm3, %%mm0\n\t"
					"por	%%mm0, %%mm1\n\t"
	"movd	%%mm5, %%edi\n\t"
					"psrlq	$14, %%mm0\n\t"
	"movd	%%mm7, %%esi\n\t"
					"pand	%%mm3, %%mm0\n\t"
	"movl	%%edi, %%ecx\n\t"
					"por	%%mm1, %%mm0\n\t"
	"shrl	$1, %%ecx\n\t"
					"psrlq	$7, %%mm0\n\t"
	"andl	%%edi, %%ecx\n\t"
					"por	%%mm0, %%mm4\n\t"
			/* shift=+1 */			/* shift=-7 */
	"movl	%%esi, %%eax\n\t"	"movq	%%mm7, %%mm0\n\t"
	"shrl	$1, %%eax\n\t"		"psllq	$7, %%mm0\n\t"
	"andl	%%edi, %%eax\n\t"	"pand	%%mm5, %%mm0\n\t"
	"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
	"shrl	$1, %%eax\n\t"		"psllq	$7, %%mm0\n\t"
	"andl	%%edi, %%eax\n\t"	"pand	%%mm5, %%mm0\n\t"
	"orl	%%edx, %%eax\n\t"	"por	%%mm1, %%mm0\n\t"
					"psllq	$7, %%mm3\n\t"
	"movl	%%eax, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
	"shrl	$2, %%eax\n\t"		"psllq	$14, %%mm0\n\t"
	"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"
	"orl	%%eax, %%edx\n\t"	"por	%%mm0, %%mm1\n\t"
	"shrl	$2, %%eax\n\t"		"psllq	$14, %%mm0\n\t"
	"andl	%%ecx, %%eax\n\t"	"pand	%%mm3, %%mm0\n\t"
	"orl	%%edx, %%eax\n\t"	"por	%%mm1, %%mm0\n\t"
	"shrl	$1, %%eax\n\t"		"psllq	$7, %%mm0\n\t"
					"por	%%mm0, %%mm4\n\t"
			/* shift=-1 */			/* shift=+9 */
					"movq	%%mm7, %%mm0\n\t"
	"addl	%%esi, %%esi\n\t"	"psrlq	$9, %%mm0\n\t"
	"andl	%%edi, %%esi\n\t"	"pand	%%mm5, %%mm0\n\t"
	"movl	%%esi, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
	"addl	%%esi, %%esi\n\t"	"psrlq	$9, %%mm0\n\t"
	"andl	%%edi, %%esi\n\t"	"pand	%%mm5, %%mm0\n\t"
					"movq	%%mm5, %%mm3\n\t"
	"orl	%%edx, %%esi\n\t"	"por	%%mm1, %%mm0\n\t"
					"psrlq	$9, %%mm3\n\t"
	"movl	%%esi, %%edx\n\t"	"movq	%%mm0, %%mm1\n\t"
	"addl	%%ecx, %%ecx\n\t"	"pand	%%mm5, %%mm3\n\t"
	"shll	$2, %%esi\n\t"		"psrlq	$18, %%mm0\n\t"
	"andl	%%ecx, %%esi\n\t"	"pand	%%mm3, %%mm0\n\t"
	"orl	%%esi, %%edx\n\t"	"por	%%mm0, %%mm1\n\t"
	"shll	$2, %%esi\n\t"		"psrlq	$18, %%mm0\n\t"
	"andl	%%ecx, %%esi\n\t"	"pand	%%mm3, %%mm0\n\t"
	"orl	%%edx, %%esi\n\t"	"por	%%mm1, %%mm0\n\t"
	"addl	%%esi, %%esi\n\t"	"psrlq	$9, %%mm0\n\t"
	"orl	%%eax, %%esi\n\t"	"por	%%mm0, %%mm4\n\t"
			/* Serialize */			/* shift=-9 */
					"movq	%%mm7, %%mm0\n\t"
	"movd	%%esi, %%mm1\n\t"
					"psllq	$9, %%mm0\n\t"
	"por	%%mm1, %%mm4\n\t"
					"pand	%%mm5, %%mm0\n\t"
					"movq	%%mm0, %%mm1\n\t"
					"psllq	$9, %%mm0\n\t"
					"pand	%%mm5, %%mm0\n\t"
					"por	%%mm1, %%mm0\n\t"
					"psllq	$9, %%mm3\n\t"
					"movq	%%mm0, %%mm1\n\t"
					"psllq	$18, %%mm0\n\t"
					"pand	%%mm3, %%mm0\n\t"
					"por	%%mm0, %%mm1\n\t"
					"psllq	$18, %%mm0\n\t"
					"pand	%%mm3, %%mm0\n\t"
					"por	%%mm1, %%mm0\n\t"
					"psllq	$9, %%mm0\n\t"
					"por	%%mm0, %%mm4\n\t"
			/* mm4 is the pseudo-feasible moves at this point. */
			/* Let mm7 be the feasible moves, i.e., mm4 restricted to empty squares. */
					"por	%%mm6, %%mm7\n\t"
					"pandn	%%mm4, %%mm7"
    :: "S" (my_high), "D" (opp_high), "m" (mmx_c7e) : "eax", "edx", "ecx" );
			/* mm7: feasible moves */
}

#if 0

BitBoard
generate_all_mmx( const BitBoard my,
                  const BitBoard opp ) {
  BitBoard moves;
  asm volatile(
 #ifdef ALIGNED_LE_BITBOARD
	"movq	%1, %%mm7\n\t"
	"movq	%3, %%mm6"
 #else
	"movd	%1, %%mm7\n\t"		/* AMD 22007 pp.171 */
	"movd	%3, %%mm6\n\t"
	"punpckldq %0, %%mm7\n\t"
	"punpckldq %2, %%mm6"
 #endif
    :: "m" (my.high), "m" (my.low), "m" (opp.high), "m" (opp.low) );

  pseudo_mobility_mmx(my.high, opp.high);

  asm volatile(
 #ifdef ALIGNED_LE_BITBOARD
	"movq	%%mm7, %1\n\t"
 #else
	"movd	%%mm7, %1\n\t"
	"psrlq	$32, %%mm7\n\t"
	"movd	%%mm7, %0\n\t"
 #endif
	"emms"		/* Reset the FP/MMX unit. */
    : "=g" (moves.high), "=g" (moves.low) );
  return moves;
}


// Currently not used, but might come in handy.

int
pop_count_mmx( const BitBoard bits ) {
  unsigned int count;

  asm volatile(
	"movq	%1, %%mm0\n\t"
	"movq	%%mm0, %%mm1\n\t"
	"psrld	$1, %%mm0\n\t"
	"pand	%2, %%mm0\n\t"
	"psubd	%%mm0, %%mm1\n\t"
	"movq	%%mm1, %%mm0\n\t"
	"psrld	$2, %%mm1\n\t"
	"pand	%3, %%mm0\n\t"
	"pand	%3, %%mm1\n\t"
	"paddd	%%mm1, %%mm0\n\t"
	"movq	%%mm0, %%mm1\n\t"
	"psrld	$4, %%mm0\n\t"
	"paddd	%%mm1, %%mm0\n\t"
	"pand	%4, %%mm0\n\t"
 #ifdef SSE /* or Enhanced 3DNow (3DNOW && ATHLON) */
	"pxor	%%mm1, %%mm1\n\t"
	"psadbw	%%mm1, %%mm0\n\t"	/* AMD 22007 pp.138 */
	"movd	%%mm0, %0\n\t"
 #else
	"movq	%%mm0, %%mm1\n\t"
	"psrlq	$32, %%mm0\n\t"
	"paddd	%%mm1, %%mm0\n\t"
	"movd	%%mm0, %0\n\t"
	"imull	$16843009, %0, %0\n\t"
	"shrl	$24, %0\n\t"
 #endif
	"emms"
	: "=r" (count)
	: "m" (bits), "m" (mmx_c55), "m" (mmx_c33), "m" (mmx_c0f) );

  return count;
}

#endif
#endif // USE_PENTIUM_ASM


int
bitboard_mobility( const BitBoard my_bits,
		   const BitBoard opp_bits ) {
  BitBoard moves;
  unsigned int count;

#ifdef USE_PENTIUM_ASM
  if (useMMX) {
    asm volatile(
 #ifdef ALIGNED_LE_BITBOARD
	"movq	%1, %%mm7\n\t"
	"movq	%3, %%mm6"
 #else
	"movd	%1, %%mm7\n\t"
	"movd	%3, %%mm6\n\t"
	"punpckldq %0, %%mm7\n\t"
	"punpckldq %2, %%mm6"
 #endif
    :: "m" (my_bits.high), "m" (my_bits.low), "m" (opp_bits.high), "m" (opp_bits.low) );

    pseudo_mobility_mmx(my_bits.high, opp_bits.high);

    /* Count the moves, i.e., the number of bits set in mm7. */
    asm volatile(
	"movq	%%mm7, %%mm1\n\t"
	"psrld	$1, %%mm7\n\t"
	"pand	%1, %%mm7\n\t"
	"psubd	%%mm7, %%mm1\n\t"
	"movq	%%mm1, %%mm7\n\t"
	"psrld	$2, %%mm1\n\t"
	"pand	%2, %%mm7\n\t"
	"pand	%2, %%mm1\n\t"
	"paddd	%%mm1, %%mm7\n\t"
	"movq	%%mm7, %%mm1\n\t"
	"psrld	$4, %%mm7\n\t"
	"paddd	%%mm1, %%mm7\n\t"
	"pand	%3, %%mm7\n\t"
	"movq	%%mm7, %%mm1\n\t"
	"psrlq	$32, %%mm7\n\t"
	"paddd	%%mm1, %%mm7\n\t"
	"movd	%%mm7, %0\n\t"
	"imull	$16843009, %0, %0\n\t"	/* AMD 22007 pp.138 */
	"shrl 	$24, %0\n\t"
	"emms"		/* Reset the FP/MMX unit. */
    : "=a" (count)
    : "m" (mmx_c55), "m" (mmx_c33), "m" (mmx_c0f) );
  } else
#endif
  {
    moves = generate_all_c( my_bits, opp_bits );
    count = non_iterative_popcount( moves.high, moves.low );
  }
  return count;
}


int
weighted_mobility( const BitBoard my_bits,
		   const BitBoard opp_bits ) {
  unsigned int n1, n2;
  BitBoard moves;
#ifdef USE_PENTIUM_ASM
  static const unsigned long long mmx_c15 = 0x1555555555555515ULL;
  static const unsigned long long mmx_c01 = 0x0100000000000001ULL;

  if (useMMX) {
    asm volatile(
 #ifdef ALIGNED_LE_BITBOARD
	"movq	%1, %%mm7\n\t"
	"movq	%3, %%mm6"
 #else
	"movd	%1, %%mm7\n\t"
	"movd	%3, %%mm6\n\t"
	"punpckldq %0, %%mm7\n\t"
	"punpckldq %2, %%mm6"
 #endif
    :: "m" (my_bits.high), "m" (my_bits.low), "m" (opp_bits.high), "m" (opp_bits.low) );

    pseudo_mobility_mmx(my_bits.high, opp_bits.high);

    /* The weighted mobility is 128 * (#moves + #corner moves). */
    asm volatile(
	"movq	%%mm7, %%mm0\n\t"
	"movq	%%mm7, %%mm1\n\t"
	"psrld	$1, %%mm7\n\t"
	"pand	%1, %%mm0\n\t"		/* A8 and H8 */
	"pand	%2, %%mm7\n\t"		/* c55 exclude A1 and H1 */
	"psubd	%%mm7, %%mm1\n\t"
	"paddd	%%mm0, %%mm1\n\t"
	"movq	%%mm1, %%mm7\n\t"
	"psrld	$2, %%mm1\n\t"
	"pand	%3, %%mm7\n\t"
	"pand	%3, %%mm1\n\t"
	"paddd	%%mm1, %%mm7\n\t"
	"movq	%%mm7, %%mm1\n\t"
	"psrld	$4, %%mm7\n\t"
	"paddd	%%mm1, %%mm7\n\t"
	"pand	%4, %%mm7\n\t"
	"movq	%%mm7, %%mm1\n\t"
	"psrlq	$32, %%mm7\n\t"
	"paddd	%%mm1, %%mm7\n\t"
	"movd	%%mm7, %0\n\t"
	"emms"		/* Reset the FP/MMX unit. */
    : "=g" (n1)
    : "m" (mmx_c01), "m" (mmx_c15), "m" (mmx_c33), "m" (mmx_c0f) );
  } else
#endif
  {
    moves = generate_all_c( my_bits, opp_bits );

    n1 = moves.high - ((moves.high >> 1) & 0x15555555u) + (moves.high & 0x01000000u);	/* corner bonus for A1/H1/A8/H8 */
    n2 = moves.low - ((moves.low >> 1) & 0x55555515u) + (moves.low & 0x00000001u);
    n1 = (n1 & 0x33333333u) + ((n1 >> 2) & 0x33333333u);
    n2 = (n2 & 0x33333333u) + ((n2 >> 2) & 0x33333333u);
    n1 = (n1 + (n1 >> 4)) & 0x0F0F0F0Fu;
    n1 += (n2 + (n2 >> 4)) & 0x0F0F0F0Fu;
  }

  return ((n1 * 0x01010101u) >> 24) * 128;
}
