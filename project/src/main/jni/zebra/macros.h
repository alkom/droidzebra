/*
   File:          macros.h

   Created:       May 31, 1998

   Modified:      November 14, 2005

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:      Some globally used macros.
*/



#ifndef MACROS_H
#define MACROS_H



#ifdef __cplusplus
extern "C" {
#endif



#define MAX(a,b)                (((a) > (b)) ? (a) : (b))

#define MIN(a,b)                (((a) < (b)) ? (a) : (b))

#define SQR(a)                  ((a) * (a))


/* Convert index to square, e.g. 27 -> g2 */
#define TO_SQUARE(index)        'a'+(index % 10)-1,'0'+(index / 10)


/* Define the inline directive when available */
#if defined( __GNUC__ )&& !defined( __cplusplus )
#define INLINE __inline__
#else
#define INLINE
#endif


/* Define function attributes directive when available */
#if 0 && __GNUC__ >= 3 
#define	REGPARM(num)	__attribute__((regparm(num)))
#else
#if defined (_MSC_VER) || defined(__BORLANDC__)
#define	REGPARM(num)	__fastcall
#else
#define	REGPARM(num)
#endif
#endif


#ifdef __cplusplus
}
#endif



#endif  /* MACROS_H */
