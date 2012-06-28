#include "SndStream.h"
#include "fifo.h"

#define ARM7_MODULE_PATH "/data/FeOS/arm7/sndStreamStub.fx2"
#define CLAMP(n,l,u) ((n) = ((n) > (u) ? (u) : ((n)<l ? (l) : (n))))
#define BIC(a,b) ((a) &~(b))

int fifoCh;
instance_t arm7_sndModule;
FILTER* fltr;

s8 mainBuf[MAX_N_CHANS * STREAM_BUF_SIZE * 2 * 2];
void* pcmBuf;
AUDIO_BUFFER outBuf = {mainBuf, 0, 0,};
AUDIO_BUFFER workBuf = {&mainBuf[sizeof(mainBuf)/2], 0, 0,};

AUDIO_STREAM * streamLst;
AUDIO_STREAM * activeStream;
FIFO_FLTR_MSG fltrmsg;

unsigned int numStream, activeIdx, nChans, bytSmp;

hword_t  sampleCount[2];

FIFO_AUD_MSG msg;

static int readTimer()
{
	msg.type = FIFO_AUDIO_READTMR;
	fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
	while (!fifoCheckValue32(fifoCh));
	return (int)fifoGetValue32(fifoCh);
}

inline int elapsedSamples(hword_t cur, hword_t prev){
	int smpPlayed = cur-prev;
	smpPlayed += (smpPlayed < 0 ? (1<<16) : 0);
	return smpPlayed;
}

void copySamplesVRAM(void* lbuf, void* rbuf, int off, int len, bool filter){
	/* VRAM COPY */
	msg.type = FIFO_AUDIO_COPY;
	msg.rBuf = rbuf;
	msg.lBuf = lbuf;
	msg.off	 = off;
	msg.len  = len;
	msg.property = bytSmp;
	fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
	while (!fifoCheckValue32(fifoCh)) FeOS_WaitForIRQ(~0);
	fifoGetValue32(fifoCh);

	/* FILTERING */
	if(pcmBuf && fltr && filter) {
		if(fltr->fltr_mod) {
			fltrmsg.msgType 	 = FILTER_REQUEST;
			fltrmsg.off 	 	 = off;
			fltrmsg.len 		 = len;
			fifoSendDatamsg(fltr->fifoCh, sizeof(FIFO_FLTR_MSG), &fltrmsg);
		}
	}
}

void copySamples(s8* in, int samples, int req)
{
	int mask = activeStream->lenMask;
	int total = samples;

	void* lbuf = NULL;
	void* rbuf = NULL;

	while(samples > mask) {
		int toCopy = ((outBuf.bufOff + samples) > STREAM_BUF_SIZE? (STREAM_BUF_SIZE - outBuf.bufOff) : samples);
		toCopy = BIC(toCopy, mask);
		if(toCopy) {
			switch(activeStream->inf.channelCount) {
			case 2:
				if(activeStream->inf.flags & AUDIO_INTERLEAVED) {
					if(bytSmp==2)
						_deInterleave((short*)in, (short*)&outBuf.buffer[outBuf.bufOff*2], toCopy);
					else
						_8bdeInterleave(in, &outBuf.buffer[outBuf.bufOff], toCopy);
					break;
				} else {
					fastCopy(&outBuf.buffer[((STREAM_BUF_SIZE+outBuf.bufOff)*bytSmp)], &in[req*bytSmp], toCopy*bytSmp);
				}
			case 1:
				fastCopy(&outBuf.buffer[outBuf.bufOff*bytSmp], in, toCopy*bytSmp);
				break;
			}

			DC_FlushAll();
			FeOS_DrainWriteBuffer();
			if(pcmBuf) {
				lbuf = &outBuf.buffer[outBuf.bufOff*bytSmp];
				if(nChans==2)
					rbuf = &outBuf.buffer[(STREAM_BUF_SIZE+outBuf.bufOff)*bytSmp];
				copySamplesVRAM(lbuf, rbuf, outBuf.bufOff, toCopy, false);
			}
			samples -= toCopy;
			outBuf.bufOff += toCopy;
			outBuf.bufOff %= STREAM_BUF_SIZE;
			in+=toCopy*bytSmp*nChans;
		}
	}
	workBuf.bufOff = samples;
	if(workBuf.bufOff>0) {
		memmove(workBuf.buffer, &workBuf.buffer[(total-samples)*nChans*bytSmp], workBuf.bufOff*nChans*bytSmp);
	}
	/* send a filter request when all samples are copied */
	if(pcmBuf && fltr) {
		if(fltr->fltr_mod) {
			int off = outBuf.bufOff - (total - samples);
			if(off < 0)
				off+=STREAM_BUF_SIZE;
			fltrmsg.msgType 	 = FILTER_REQUEST;
			fltrmsg.off 	 	 = off;
			fltrmsg.len 		 = (total - samples);
			fifoSendDatamsg(fltr->fifoCh, sizeof(FIFO_FLTR_MSG), &fltrmsg);
		}
	}
}

void preFill(void)
{
	activeStream->smpNc = STREAM_BUF_SIZE-outBuf.bufOff;
	int ret = 0;
	while(activeStream->smpNc > 0) {
		int toDec = CLAMP(activeStream->smpNc, 0, (STREAM_BUF_SIZE-workBuf.bufOff));
		ret = activeStream->cllbcks->onRead(toDec, &workBuf.buffer[workBuf.bufOff*bytSmp*nChans], &activeStream->cllbcks->context);
		if(ret<=0) {
			break;
		}
		copySamples(workBuf.buffer, ret+workBuf.bufOff, ret);
		activeStream->smpNc -= ret;
	}
}

FEOS_EXPORT int initSoundStreamer(void)
{
	arm7_sndModule= FeOS_LoadARM7(ARM7_MODULE_PATH, &fifoCh);
	if(arm7_sndModule) {
		return 1;
	}
	return 0;
}

FEOS_EXPORT void enableFiltering(word_t bufmd, bool inBankC)
{
	if(activeStream){
		if(activeStream->state != STREAM_STOP)
			return;
	}
	if(bufmd == SOUNDBUF_0x6020000)
		pcmBuf = (void*)(0x6020000);
	else
		pcmBuf = (void*)(0x6000000);
	if(inBankC)
		vramSetBankC(bufmd);
	else
		vramSetBankD(bufmd);
}

FEOS_EXPORT void disableFiltering(void)
{
	if(activeStream){
		if(activeStream->state != STREAM_STOP)
			return;
	}
	pcmBuf = NULL;
}

FEOS_EXPORT void deinitSoundStreamer(void)
{
	if(activeStream) {
		stopStream();
	}
	FeOS_FreeARM7(arm7_sndModule, fifoCh);
	if(streamLst) {
		free(streamLst);
		streamLst = NULL;
	}
}

FEOS_EXPORT int createStream(AUDIO_CALLBACKS * cllbck)
{

	void * temp = realloc(streamLst, (numStream+1)*sizeof(AUDIO_STREAM));
	if(temp) {
		streamLst = temp;
		streamLst[numStream].state = STREAM_STOP;
		streamLst[numStream].smpNc = 0;
		streamLst[numStream].cllbcks = cllbck;
		numStream++;
		return (numStream-1);
	}

	return -1;
}

FEOS_EXPORT int startStream(const char* inf, int idx)
{
	activeIdx = idx;
	activeStream = &streamLst[activeIdx];
	if(activeStream->cllbcks->onOpen != NULL &&  activeStream->cllbcks->onRead != NULL && activeStream->cllbcks->onClose != NULL) {
		if(!activeStream->cllbcks->onOpen(inf, &activeStream->inf, &activeStream->cllbcks->context))
			return STREAM_ERR;
		bytSmp = ((activeStream->inf.flags&AUDIO_16BIT)? 2 : 1);
		nChans = activeStream->inf.channelCount;
		int frequency = activeStream->inf.frequency;
		/* Deinterleaving needs 4n samples */
		if(activeStream->inf.flags & AUDIO_INTERLEAVED && nChans == 2)
			activeStream->lenMask = 0x3;
		/* Fastcopy needs 16n bytes */
		else 
			activeStream->lenMask = ((16>>(bytSmp>>1))-1);
		if(activeStream->inf.channelCount <= MAX_N_CHANS) {
			/* Clear MAIN RAM BUFFERS
			* (optional) VRAM BUFFER is cleared by the ARM7
			*/
			memset(workBuf.buffer, 0, STREAM_BUF_SIZE*bytSmp*nChans);
			memset(outBuf.buffer, 0, STREAM_BUF_SIZE*bytSmp*nChans);
			/* Prefill won't work if msg.buffer isn't set (assuming that filtering is enabled) */
			msg.buffer 	= (pcmBuf? pcmBuf: outBuf.buffer);
			if(pcmBuf) {
				fltrmsg.buffer = pcmBuf;
				fltrmsg.bufLen = STREAM_BUF_SIZE;
				fltrmsg.nChans = nChans;
				fltrmsg.bytSmp = bytSmp;
				msg.type 	= FIFO_AUDIO_CLEAR;
				fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
				while (!fifoCheckValue32(fifoCh)) FeOS_WaitForIRQ(~0);
				fifoGetValue32(fifoCh);
			}
			workBuf.bufOff = outBuf.bufOff = 0;
			sampleCount[0] = sampleCount[1] = 0;

			preFill();

			msg.type 	= FIFO_AUDIO_START;
			msg.property 	= (frequency | (nChans<<16) | ((bytSmp<<18)));
			msg.bufLen 	= STREAM_BUF_SIZE;
			fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
			while (!fifoCheckValue32(fifoCh)) FeOS_WaitForIRQ(~0);
			if (fifoGetValue32(fifoCh)) {
				activeStream->state = STREAM_PLAY;
				return 1;
			}
		}
	}
	return 0;
}

FEOS_EXPORT void pauseStream(void)
{
	msg.type = FIFO_AUDIO_PAUSE;
	fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);
	while (!fifoCheckValue32(fifoCh)) FeOS_WaitForIRQ(~0);
	/* Update smpNc */
	sampleCount[0] = fifoGetValue32(fifoCh);
	int smpPlayed = elapsedSamples(sampleCount[0], sampleCount[1]);
	activeStream->smpNc += smpPlayed;
	sampleCount[0] = sampleCount[1] = 0;

	int toCopy = BIC((STREAM_BUF_SIZE-activeStream->smpNc), activeStream->lenMask) ;
	int offset = outBuf.bufOff - toCopy;
	if(offset < 0)
		offset += STREAM_BUF_SIZE;

	/* save unaligned samples */
	int lostSz = ((STREAM_BUF_SIZE-activeStream->smpNc)&activeStream->lenMask);
	void* temp = malloc((lostSz+workBuf.bufOff)*bytSmp*nChans);
	if(temp){
		memcpy(temp,  &outBuf.buffer[(offset+toCopy) % STREAM_BUF_SIZE], lostSz*bytSmp);
		if(nChans==2)
			memcpy(temp+lostSz*bytSmp,  &outBuf.buffer[(offset+toCopy) % STREAM_BUF_SIZE+STREAM_BUF_SIZE], lostSz*bytSmp);
		memcpy(temp+lostSz*bytSmp*nChans, workBuf.buffer, workBuf.bufOff*bytSmp*nChans);
	}

	int i;
	for(i=0; i<nChans; i++){
		int toEnd = ((offset+toCopy) > STREAM_BUF_SIZE? (STREAM_BUF_SIZE-offset) : toCopy);
		fastCopy(&workBuf.buffer[STREAM_BUF_SIZE*i*bytSmp], &outBuf.buffer[(STREAM_BUF_SIZE*i + offset)*bytSmp], toEnd*bytSmp);
		fastCopy(&workBuf.buffer[(STREAM_BUF_SIZE*i+toEnd)*bytSmp], &outBuf.buffer[STREAM_BUF_SIZE*i*bytSmp], (toCopy-toEnd)*bytSmp);
	}

	fastCopy(outBuf.buffer, workBuf.buffer, toCopy*bytSmp);
	if(nChans == 2) {
		fastCopy(&outBuf.buffer[STREAM_BUF_SIZE*bytSmp], &workBuf.buffer[toCopy*bytSmp], toCopy*bytSmp);
	}
	DC_FlushAll();
	FeOS_DrainWriteBuffer();
	if(pcmBuf)
		copySamplesVRAM(workBuf.buffer, (nChans == 2?  &workBuf.buffer[toCopy*bytSmp] : NULL), 0, toCopy, true);
	outBuf.bufOff = toCopy;
	outBuf.bufOff %= STREAM_BUF_SIZE;
	if(temp){
		memcpy(workBuf.buffer, temp, (workBuf.bufOff+lostSz)*bytSmp*nChans);
		free(temp);
	}
	activeStream->smpNc = (STREAM_BUF_SIZE - toCopy);
	activeStream->state = STREAM_PAUSE;
}

/* Resume stream, by enabling the (disabled) sound channel(s) */
FEOS_EXPORT void resumeStream(void)
{
	msg.type = FIFO_AUDIO_RESUME;
	fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);

	while (!fifoCheckValue32(fifoCh)) FeOS_WaitForIRQ(~0);
	if (fifoGetValue32(fifoCh))
		activeStream->state = STREAM_PLAY;
}

FEOS_EXPORT void stopStream(void)
{
	if(activeStream) {
		msg.type = FIFO_AUDIO_STOP;
		fifoSendDatamsg(fifoCh, sizeof(FIFO_AUD_MSG), &msg);

		activeStream->smpNc = 0;
		activeStream->state = STREAM_STOP;
		activeStream->cllbcks->onClose(activeStream->cllbcks->context);
		activeStream = NULL;
	}
}

FEOS_EXPORT int updateStream(void)
{
	sampleCount[0] = readTimer();
	int smpPlayed = elapsedSamples(sampleCount[0], sampleCount[1]); 
	activeStream->smpNc += smpPlayed;
	sampleCount[1] = sampleCount[0];

	int ret = STREAM_EOF;

	if(activeStream->smpNc>0) {
decode:
		if(activeStream->state != STREAM_WAIT) {
			int toDec = CLAMP(activeStream->smpNc, 0, (STREAM_BUF_SIZE-workBuf.bufOff));
			ret = activeStream->cllbcks->onRead(toDec, &workBuf.buffer[workBuf.bufOff*bytSmp*nChans], &activeStream->cllbcks->context);
		}
		switch(ret) {
		case STREAM_ERR:
			stopStream();
			return STREAM_ERR;
		case STREAM_EOF:
			if(activeStream->cllbcks->onEof) {
				activeStream->cllbcks->onEof(activeStream->cllbcks->context);
			} else {
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
			}
			break;
		default:
			if(ret > 0) {
				copySamples(workBuf.buffer, ret+workBuf.bufOff,  ret);
				activeStream->smpNc -= ret;
				if(activeStream->smpNc>0)
					goto decode;
			}
			return 1;
		}
	}
	return 1;
}

FEOS_EXPORT int getPlayingSample(void)
{
	sampleCount[0] = readTimer();
	int smpPlayed = elapsedSamples(sampleCount[0], sampleCount[1]); 
	activeStream->smpNc += smpPlayed;
	sampleCount[1] = sampleCount[0];
	return (((outBuf.bufOff + activeStream->smpNc) & (STREAM_BUF_SIZE-1)));
}

FEOS_EXPORT int getStreamState(void)
{
	if(activeStream)
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
		int move = numStream-idx;
		if (move)
			memmove(&streamLst[idx], &streamLst[(idx+1)], sizeof(AUDIO_STREAM)*move);
		if (numStream) {
			void * temp = realloc(streamLst, (numStream)*sizeof(AUDIO_STREAM));
			if(temp)
				streamLst = temp;
			else
				return;
		}
		if (streamLst) {
			free(streamLst);
			streamLst = NULL;
		}
		numStream = 0;
	}
}

FEOS_EXPORT AUDIO_INFO* getStreamInfo(int idx)
{
	return &streamLst[idx].inf;
}

FEOS_EXPORT bool loadFilter(FILTER* fl, char* name)
{
	fltr = fl;
	fltr->fltr_mod = FeOS_LoadARM7(name, &fltr->fifoCh);
	if(fltr->fltr_mod) {
		return true;
	}
	return false;
}

FEOS_EXPORT void unloadFilter(FILTER* fl)
{
	if(fl) {
		if(fl->fltr_mod) {
			FeOS_FreeARM7(fl->fltr_mod, fl->fifoCh);
			fl->fltr_mod = fltr = NULL;
		}
	}
}
