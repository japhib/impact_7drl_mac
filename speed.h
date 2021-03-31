/*
 * Licensed under BSD license.  See LICENCE.TXT  
 *
 * Produced by:	Jeff Lait
 *
 *      	7DRLl Development
 *
 * NAME:        speed.h ( Live Once Library, C++ )
 *
 * COMMENTS:
 */

#ifndef __speed__
#define __speed__

//
// We use the POWDER style speed sytem here...
//

void spd_init();
PHASE_NAMES spd_getphase();
void spd_inctime();
int spd_gettime();
void spd_settime(int time);

#endif

