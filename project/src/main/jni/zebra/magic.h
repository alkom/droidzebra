/*
   File:         magic.h

   Created:      July 5, 1999

   Modified:     December 25, 1999

   Author:       Gunnar Andersson (gunnar@radagast.se)

   Contents:     Magic numbers used to distinguish different versions
                 of the opening book and coefficient files.
*/



#ifndef MAGIC_H
#define MAGIC_H



/* Magic numbers for the file with evaluation coefficients.
   Name convention: Decimals of Pi. */

#define EVAL_MAGIC1                5358
#define EVAL_MAGIC2                9793

/* Magic numbers for the opening book file format.
   Name convention: Decimals of E. */

#define BOOK_MAGIC1                2718
#define BOOK_MAGIC2                2818



#endif  /* MAGIC_H */
