/*
   File:          autoplay.h

   Created:       May 21, 1998
   
   Modified:      August 1, 2002

   Author:        Gunnar Andersson (gunnar@radagast.se)

   Contents:
*/



#ifndef AUTOPLAY_H
#define AUTOPLAY_H




#ifdef __cplusplus
extern "C" {
#endif



void
handle_event( int only_passive_events,
	      int allow_delay,
	      int passive_mode );

void
toggle_event_status( int allow_event_handling );



#ifdef __cplusplus
}
#endif



#endif  /* AUTOPLAY_H */
