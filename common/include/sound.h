#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

typedef struct {
	unsigned int frequency;
	unsigned int channelCount;
	unsigned int pos;
	int flags;
} AUDIO_INFO;

/* Stream status */
enum STREAM_STATUS {
    STREAM_INITED,
    STREAM_PLAY,
    STREAM_STOP,
    STREAM_PAUSE,
    STREAM_WAIT,
};

enum {
	SOUNDBUF_0x6000000 = (2 | (( 0 )<<3)),
	SOUNDBUF_0x6020000 = (2 | (( 1 )<<3)),
};

typedef struct {
	int8_t* buffer;
	int bufLen;
	int bufOff;
} AUDIO_BUFFER;


typedef struct {
	int (*onOpen)(const char* , AUDIO_INFO*, void** context);
	int (*onRead)(AUDIO_INFO*, int length, void * buf, void * context);
	void (*onEof)(void * context);
	void (*onClose)(void * context);
	void * context;
} AUDIO_CALLBACKS;

typedef struct {
	int state;
	unsigned int smpNc;
	unsigned int lenMask;
	AUDIO_CALLBACKS* cllbcks;
	AUDIO_INFO  inf;
} AUDIO_STREAM;

enum ERROR{
STREAM_ERR = -1,
STREAM_EOF = -2,
STREAM_UNDERRUN = -3,
STREAM_INSF = -4,
};

enum AUDIO_FLAGS {
    AUDIO_INTERLEAVED = (1<<0),
    AUDIO_16BIT		  = (1<<1),	// 8BIT if not set
};


#endif /* SOUND_H */