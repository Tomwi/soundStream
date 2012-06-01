#ifndef FIFO_H
#define FIFO_H

/* Message types */
enum FIFO_MESSAGE{
	FIFO_AUDIO_START = 1,
	FIFO_AUDIO_STOP,
	FIFO_AUDIO_PAUSE,
	FIFO_AUDIO_RESUME, 
};

typedef struct {
	int type;				// kind of audio message
	unsigned int property;	// (tmr_value & (n_Channels << 16))
	int bufLen;				// Length of the buffer for just one channel
	s16 * buffer;			// pointer to sample buffer
} FIFO_AUD_MSG;

#endif