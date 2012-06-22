#ifndef SND_STREAM_H
#define SND_STREAM_H

#include <feos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/sound.h"
#include "common/filter.h"

/* Timing */
#define BUS_CLOCK   		(0x2000000)
#define TICKS_PER_SAMP(n)	((BUS_CLOCK)/(n))
#define TIMER_ENABLE		(1<<7)
#define TIMER_CASCADE   	(1<<2)

#define STREAM_BUF_SIZE   	(8192)
#define MAX_N_CHANS			(2)

int initSoundStreamer(void);
void deinitSoundStreamer(void);

void enableFiltering(word_t bufmd, bool inBankC);
bool loadFilter(FILTER* fltr, char* name);
void unloadFilter(FILTER* fl);

int createStream(AUDIO_CALLBACKS * cllbck);
int startStream(const char* , int idx);
void pauseStream(void);
void resumeStream(void);
void stopStream();
int updateStream(void);
void destroyStream(int idx);

/* WARNING: use only for b0rking */
AUDIO_INFO * getStreamInfo(int idx);
int getPlayingSample(void);
int getStreamState(void);
void setStreamState(int state);
void* getoutBuf(void);

/* WARNING: use only with samples = 4n */
void _deInterleave(short *in, short *out, int samples);
void _8bdeInterleave(s8 *in, s8 *out, int samples);

#endif /* SND_STREAM_H */
