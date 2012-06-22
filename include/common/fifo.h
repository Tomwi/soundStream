#ifndef FIFO_H
#define FIFO_H

enum FIFO_MSG{
	FIFO_AUDIO_START = 1,
	FIFO_AUDIO_STOP,
	FIFO_AUDIO_PAUSE,
	FIFO_AUDIO_RESUME,
	FIFO_AUDIO_READTMR,
	FIFO_AUDIO_COPY,
	FIFO_AUDIO_SETFLTR,
};

typedef struct {
	int type;			// kind of audio message
	unsigned int property;	// (tmr_value & (n_Channels << 16))
	void* buffer;
	int bufLen;		// Length of the buffer for just one channel
	void* rBuf;	
	void* lBuf;
	int off;
	int len;
} FIFO_AUD_MSG;

typedef struct {
	int msgType;
	void* buffer;
	int off;
	int bufLen;
	int bytSmp;
	int nChans;
	int len;
} FIFO_FLTR_MSG;


#endif