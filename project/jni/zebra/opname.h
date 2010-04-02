/*
   opname.h

   Automatically created by OSF on Sun Nov 24 16:01:10 2002
*/


#ifndef OPNAME_H
#define OPNAME_H


#define OPENING_COUNT       76


typedef struct {
  const char *name;
  const char *sequence;
  int hash_val1;
  int hash_val2;
  int level;
} OpeningDescriptor;


extern OpeningDescriptor opening_list[OPENING_COUNT];


#endif  /* OPNAME_H */
