#ifndef SND_STREAM_H
#define SND_STREAM_H

#include <feos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Timing */
#define BUS_CLOCK   		(33513982)
#define TICKS_PER_SAMP(n)	((BUS_CLOCK)/(n))
#define TIMER_ENABLE		(1<<7)
#define TIMER_CASCADE   	(1<<2)

#define STREAM_BUF_SIZE   	(8192)
#define MAX_N_CHANS			(2)

/* Message types */
#define FIFO_AUDIO_START  (1)
#define FIFO_AUDIO_STOP   (2)
#define FIFO_AUDIO_PAUSE  (3)
#define FIFO_AUDIO_RESUME (4)

/* Stream status */
enum STREAM_STATUS{
	STREAM_INITED,
	STREAM_PLAY,
	STREAM_STOP,
	STREAM_PAUSE,
	STREAM_WAIT,
};
 

typedef struct {
	int type;				// kind of audio message
	unsigned int property;	// (tmr_value & (n_Channels << 16))
	int bufLen;				// Length of the buffer for just one channel
	s16 * buffer;			// pointer to sample buffer
} FIFO_AUD_MSG;

typedef struct {
	s16 * buffer;
	int bufLen;
	int bufOff;
} AUDIO_BUFFER;

typedef struct {
	unsigned int frequency;
	unsigned int channelCount;
	unsigned int smpNc;
	int (*writeSamples)(int length, short * buf);
	int state;
} AUDIO_STREAM;

#define STREAM_ERR     (-1)
#define STREAM_EOF     (-2)

int initSoundStreamer(void);
void deinitSoundStreamer(void);

int startStream(int freq, int nChans, int (*writeCallback)(int length, short * buf));
void pauseStream(void);
void resumeStream(void);
void stopStream();
int updateStream(void);
void preFill(void);
void deFragReadbuf(unsigned char * readBuf, unsigned char ** readOff, int dataLeft);
void copySamples(short * inBuf, int deinterleave, int samples);
int getPlayingSample(void);
int getStreamState(void);
void setStreamState(int state);

void _deInterleave(short *in, short *out, int samples);

#endif /* SND_STREAM_H */
