#include "SndStream.h"
#include "fifo.h"

#define ARM7_MODULE_PATH "/data/FeOS/arm7/sndStreamStub.fx2"

int fifoCh;
instance_t arm7_sndModule;

/* sample is 2 bytes, work and outBuf */
s8 mainBuf[MAX_N_CHANS * STREAM_BUF_SIZE * 2 * 2];

AUDIO_BUFFER outBuf = {mainBuf, 0, 0,};
AUDIO_BUFFER workBuf = {&mainBuf[sizeof(mainBuf)/2], 0, 0,};

AUDIO_STREAM * streamLst;
AUDIO_STREAM * activeStream;

int numStream, activeIdx;
unsigned int shifter, bytSmp;

hword_t  sampleCount[2];

FIFO_AUD_MSG msg;

void fifoValHandler(u32 value32, void *userdata)
{
	switch(value32) {
	case FIFO_AUDIO_START:
		FeOS_TimerWrite(0, ((1<<16)-TICKS_PER_SAMP(activeStream->inf.frequency))|((TIMER_ENABLE)<<16));
		FeOS_TimerWrite(1, ((TIMER_CASCADE|TIMER_ENABLE)<<16));
		activeStream->state = STREAM_PLAY;
		break;
	default:
		break;
	}
}

void copySamples(s8* inBuf, int samples)
{
	int mask = 3;
	samples &= (~mask); // bic
	int toCopy = ((outBuf.bufOff + samples) > STREAM_BUF_SIZE? (STREAM_BUF_SIZE - outBuf.bufOff) : samples);
	toCopy  	&= (~mask);

copy:

	if(toCopy) {
		switch(activeStream->inf.channelCount) {
		case 2:
			if(!(activeStream->inf.flags & AUDIO_INTERLEAVED))
				memcpy(&outBuf.buffer[((STREAM_BUF_SIZE+outBuf.bufOff)*bytSmp)], &inBuf[toCopy*bytSmp], toCopy*bytSmp);
			else {
				if(bytSmp==2)
					_deInterleave((short*)inBuf, (short*)&outBuf.buffer[outBuf.bufOff*2], toCopy);
				else
					_8bdeInterleave(inBuf, &outBuf.buffer[outBuf.bufOff], toCopy);
				break;
			}
			//Left channel
		case 1:
			memcpy(&outBuf.buffer[outBuf.bufOff*bytSmp], inBuf, toCopy*bytSmp);
			break;
		}
	}

	samples -= toCopy;
	
	if(samples) {
		outBuf.bufOff = 0;
		inBuf += (toCopy*activeStream->inf.channelCount)*bytSmp;
		toCopy = samples;
		goto copy;
	}
	outBuf.bufOff += toCopy;
	
	DC_FlushAll();
	FeOS_DrainWriteBuffer();
}

void preFill(void)
{
	activeStream->smpNc = STREAM_BUF_SIZE-outBuf.bufOff;
	int ret = 0;
	while(activeStream->smpNc > 0) {
		ret = activeStream->cllbcks->onRead(activeStream->smpNc, workBuf.buffer, &activeStream->cllbcks->context);
		if(ret<=0) {
			break;
		}
		copySamples(workBuf.buffer, ret);
		activeStream->smpNc -=ret;
	}
}

FEOS_EXPORT int initSoundStreamer(void)
{
	arm7_sndModule= FeOS_LoadARM7(ARM7_MODULE_PATH, &fifoCh);

	if(arm7_sndModule) {
		fifoSetValue32Handler(fifoCh, fifoValHandler, NULL);
		return 1;
	}
	return 0;
}

FEOS_EXPORT void deinitSoundStreamer(void)
{
	if(activeStream) {
		if(activeStream->state == STREAM_PLAY) {
			msg.type = FIFO_AUDIO_STOP;
			fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
			FeOS_TimerWrite(0, 0);
			FeOS_TimerWrite(1, 0);
		}
	}
	fifoSetValue32Handler(fifoCh, NULL, NULL);
	FeOS_FreeARM7(arm7_sndModule, fifoCh);
	if(streamLst)
		free(streamLst);
}

FEOS_EXPORT int createStream(AUDIO_CALLBACKS * cllbck)
{
	if(cllbck->onOpen != NULL &&  cllbck->onRead != NULL) {
		void * temp = realloc(streamLst, (numStream+1)*sizeof(AUDIO_STREAM));
		if(temp) {
			streamLst = temp;
			streamLst[numStream].state = STREAM_STOP;
			streamLst[numStream].smpNc = 0;
			streamLst[numStream].cllbcks = cllbck;
			numStream++;
			return (numStream-1);
		}
	}
	return -1;
}

FEOS_EXPORT int startStream(const char* inf, int idx)
{
	activeIdx = idx;
	activeStream = &streamLst[activeIdx];
	if(!activeStream->cllbcks->onOpen(inf, &activeStream->inf, &activeStream->cllbcks->context))
		return STREAM_ERR;
	bytSmp = ((activeStream->inf.flags&AUDIO_16BIT)? 2 : 1);
	shifter = 1>>bytSmp;
		
	int nChans = activeStream->inf.channelCount;
	int frequency = activeStream->inf.frequency;
	if(activeStream->inf.channelCount <= MAX_N_CHANS) {
		memset(workBuf.buffer, 0, STREAM_BUF_SIZE*bytSmp*nChans);
		memset(outBuf.buffer, 0, STREAM_BUF_SIZE*bytSmp*nChans);
		workBuf.bufOff = outBuf.bufOff = 0;
		sampleCount[0] = sampleCount[1] = 0;
		preFill();
		
		msg.type = FIFO_AUDIO_START;
		msg.property = (frequency | (nChans<<16) | ((bytSmp<<18)));
		msg.buffer = outBuf.buffer;
		msg.bufLen = STREAM_BUF_SIZE;
		fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
		
		return 1;
	}
	return 0;
}

FEOS_EXPORT void pauseStream(void)
{
	msg.type = FIFO_AUDIO_PAUSE;
	fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);

	sampleCount[0] = sampleCount[1] = 0;
	FeOS_TimerWrite(0, 0);
	FeOS_TimerWrite(1, 0);

	int i,j;
	int samples = STREAM_BUF_SIZE-activeStream->smpNc;
	int start = (outBuf.bufOff - samples);
	if(start < 0)
		start+=STREAM_BUF_SIZE;

	for(j=0; j<activeStream->inf.channelCount; j++) {
		for(i = 0; i<samples; i++) {
			workBuf.buffer[(STREAM_BUF_SIZE*j + i)*bytSmp] = outBuf.buffer[(STREAM_BUF_SIZE*j + ((start + i)%8192))*bytSmp];
		}
	}
	memcpy(outBuf.buffer, workBuf.buffer, samples*bytSmp);
	if(activeStream->inf.channelCount == 2)
		memcpy((outBuf.buffer+(STREAM_BUF_SIZE*bytSmp)), workBuf.buffer+STREAM_BUF_SIZE*bytSmp, samples*bytSmp);

	outBuf.bufOff = samples;
	activeStream->state = STREAM_PAUSE;
	DC_FlushAll();
	FeOS_DrainWriteBuffer();
}

FEOS_EXPORT void resumeStream(void)
{
	msg.type = FIFO_AUDIO_RESUME;
	fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);

	FeOS_TimerWrite(0, ((1<<16)-TICKS_PER_SAMP(activeStream->inf.frequency))|((TIMER_ENABLE)<<16));
	FeOS_TimerWrite(1, ((TIMER_CASCADE|TIMER_ENABLE)<<16));
	activeStream->state = STREAM_PLAY;
}

FEOS_EXPORT void stopStream(void)
{
	msg.type = FIFO_AUDIO_STOP;
	fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
	FeOS_TimerWrite(0, 0);
	FeOS_TimerWrite(1, 0);
	activeStream->state = STREAM_STOP;
	activeStream->cllbcks->onClose(activeStream->cllbcks->context);
}

FEOS_EXPORT int updateStream(void)
{
	sampleCount[0] = FeOS_TimerTick(1);
	int smpPlayed = sampleCount[0]-sampleCount[1];
	smpPlayed += (smpPlayed < 0 ? (1<<16) : 0);

	sampleCount[1] = sampleCount[0];
	activeStream->smpNc += smpPlayed;

	int ret = STREAM_EOF;

	if(activeStream->smpNc>0) {
decode:

		if(activeStream->state != STREAM_WAIT)
			ret = activeStream->cllbcks->onRead((activeStream->smpNc&(~3)), workBuf.buffer, activeStream->cllbcks->context);
		switch(ret) {
		case STREAM_ERR:
			stopStream();
			return STREAM_ERR;
		case STREAM_EOF:
			activeStream->state = STREAM_WAIT;
			if(activeStream->smpNc >= STREAM_BUF_SIZE) {
				stopStream();
				return STREAM_EOF;
			}
			int i,j;
			/* No more samples to decode, but still playing, fill zeroes */
			for(j=0; j<activeStream->inf.channelCount; j++) {
				for(i=outBuf.bufOff; i<(outBuf.bufOff+smpPlayed); i++) {
					/* STREAM_BUF_SIZE is a power of 2 */
					outBuf.buffer[(i%STREAM_BUF_SIZE)+STREAM_BUF_SIZE*j] = 0;
				}
			}
			outBuf.bufOff = (outBuf.bufOff + smpPlayed)%STREAM_BUF_SIZE;
			break;
		default:
			if(ret > 0) {
				copySamples(workBuf.buffer, ret);
				activeStream->smpNc -= ret;
				if(activeStream->smpNc>0)
					goto decode;
			}
		}
	}
	return 1;
}

FEOS_EXPORT int getPlayingSample(void)
{
	return (((outBuf.bufOff + activeStream->smpNc) & (STREAM_BUF_SIZE-1)));
}

FEOS_EXPORT int getStreamState(void)
{
	if(numStream)
		return activeStream->state;
	return STREAM_STOP;
}

FEOS_EXPORT void setStreamState(int state)
{
	if(numStream)
		activeStream->state = state;
}

FEOS_EXPORT void * getoutBuf(void)
{
	return outBuf.buffer;
}

FEOS_EXPORT void destroyStream(int idx)
{
	if(idx >= 0 && idx < numStream) {
		if(idx == activeIdx) {
			if(streamLst[idx].state != STREAM_STOP )
				stopStream();
		}
		numStream--;
		if(idx < (numStream))
			memmove(&streamLst[idx], &streamLst[(idx+1)], sizeof(AUDIO_STREAM)*(numStream-idx));
		void * temp = realloc(streamLst, (numStream)*sizeof(AUDIO_STREAM));
		if(temp)
			streamLst = temp;
		else {
			free(streamLst);
			numStream = 0;
		}
	}

}

FEOS_EXPORT AUDIO_INFO* getStreamInfo(int idx)
{
	return &streamLst[idx].inf;
}
