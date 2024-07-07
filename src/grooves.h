#ifndef _GROOVES_H
#define _GROOVES_H

#include "tracks.h"
enum {
	INST_PERC,
	INST_CLAP,
	INST_CRASH,
	INST_RIDE,
	INST_HIHAT_OPEN,
	INST_HIHAT_CLOSED2,
	INST_HIHAT_CLOSED1,
	INST_SNARE2,
	INST_SNARE1,
	INST_BASS,
	INST_KICK,
	NUM_INST
};

const char *inst[] = {"perc", "clap", "crash", "ride", "OHH", "HH2", "HH1", "snare2", "snare1", "bass", "kick", "none"};

#define NUM_GROOVES 16 
#define NUM_TRACKS 8
#define NUM_TRACK_SETTINGS 2
#define TRACK_SETTING_GROOVE 0
#define TRACK_SETTING_MELODY 1


//#define WITH_STATIC 0
//#ifdef WITH_STATIC 
//#include "groove_static.h"
//#else
int tracks[NUM_TRACKS][NUM_TRACK_SETTINGS][64];
int grooves[NUM_GROOVES][NUM_INST][64];
//#endif

#endif
