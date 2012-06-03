#ifndef FIFO_H
#define FIFO_H

enum FIFO_MSG{
	FIFO_AUDIO_START = 1,
	FIFO_AUDIO_STOP,
	FIFO_AUDIO_PAUSE,
	FIFO_AUDIO_RESUME,
};

typedef struct {
	int type;			// kind of audio message
	unsigned int property;	// (tmr_value & (n_Channels << 16))
	int bufLen;		// Length of the buffer for just one channel
	void * buffer;	// pointer to sample buffer
} FIFO_AUD_MSG;

#endif