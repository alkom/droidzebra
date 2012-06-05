#ifndef MYRANDOM_H
#define MYRANDOM_H



#ifdef __cplusplus
extern "C" {
#endif



int
my_srandom( int x );

char *
my_initstate( unsigned seed, char *arg_state, int n );

char *
my_setstate( char *arg_state );

long
my_random( void );



#ifdef __cplusplus
}
#endif



#endif  // MYRANDOM_H
