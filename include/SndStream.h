#ifndef SND_STREAM_H
#define SND_STREAM_H

#include <feos.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Timing */
#define BUS_CLOCK   		(0x2000000)
#define TICKS_PER_SAMP(n)	((BUS_CLOCK)/(n))
#define TIMER_ENABLE		(1<<7)
#define TIMER_CASCADE   	(1<<2)

#define STREAM_BUF_SIZE   	(8192)
#define MAX_N_CHANS			(2)

/* Stream status */
enum STREAM_STATUS {
    STREAM_INITED,
    STREAM_PLAY,
    STREAM_STOP,
    STREAM_PAUSE,
    STREAM_WAIT,
};

enum AUDIO_FLAGS {
    AUDIO_INTERLEAVED = (1<<0),
    AUDIO_16BIT		  = (1<<1),	// 8BIT if not set
};

typedef struct {
	s16 * buffer;
	int bufLen;
	int bufOff;
} AUDIO_BUFFER;

typedef struct {
	unsigned int frequency;
	unsigned int channelCount;
	int flags;
} AUDIO_INFO;

typedef struct {
	int (*onOpen)(const char* , AUDIO_INFO*, void** context);
	int (*onRead)(int length, short * buf, void * context);
	void (*onClose)(void * context);
	void * context;
} AUDIO_CALLBACKS;

typedef struct {
	int state;
	unsigned int smpNc;
	AUDIO_CALLBACKS* cllbcks;
	AUDIO_INFO  inf;
} AUDIO_STREAM;

enum ERROR{
STREAM_ERR = -1,
STREAM_EOF = -2,
STREAM_UNDERRUN = -3,
STREAM_INSF = -4,
};

int initSoundStreamer(void);
void deinitSoundStreamer(void);

int createStream(AUDIO_CALLBACKS * cllbck);
int startStream(const char* , int idx);
void pauseStream(void);
void resumeStream(void);
void stopStream();
int updateStream(void);
void destroyStream(int idx);

void preFill(void);
void deFragReadbuf(unsigned char * readBuf, unsigned char ** readOff, int dataLeft);
void copySamples(short * inBuf, int samples);
int getPlayingSample(void);
int getStreamState(void);
void setStreamState(int state);
short * getoutBuf(void);

void _deInterleave(short *in, short *out, int samples);

#endif /* SND_STREAM_H */
