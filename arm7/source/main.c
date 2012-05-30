#include <nds.h>

/* Message types */
#define FIFO_AUDIO_START	1
#define FIFO_AUDIO_STOP		2
#define FIFO_AUDIO_PAUSE	3
#define FIFO_AUDIO_RESUME	4

typedef struct {
	int type;			// kind of audio message
	unsigned int property;	// (tmr_value & (n_Channels << 16))
	int bufLen;		// Length of the buffer for just one channel
	void * buffer;	// pointer to sample buffer
} FIFO_AUD_MSG;

FIFO_AUD_MSG  msg;
int fifoChan;

int chanReg[2];

void FifoMsgHandler(int num_bytes, void *userdata)
{
	fifoGetDatamsg(fifoChan, num_bytes, (u8*)&msg);
	int channels = msg.property >> 16;
	int i;
	switch(msg.type) {
	case FIFO_AUDIO_START:
		if(channels <=2 && channels > 0) {
			for(i=0; i<channels; i++) {
				SCHANNEL_TIMER(i) = SOUND_FREQ((u16)(msg.property));
				SCHANNEL_SOURCE(i) = (u32)(msg.buffer+msg.bufLen*i*2);
				SCHANNEL_LENGTH(i) = msg.bufLen/2;	// length is counted in words, sample is halfword
				SCHANNEL_REPEAT_POINT(i) = 0;
			}


			if(channels == 2) {
				SCHANNEL_CR(0) = SOUND_REPEAT|SOUND_FORMAT_16BIT|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(0);
				SCHANNEL_CR(1) = SOUND_REPEAT|SOUND_FORMAT_16BIT|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(127);
			} else
				SCHANNEL_CR(0) = SOUND_REPEAT|SOUND_FORMAT_16BIT|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(64);
		fifoSendValue32 (fifoChan, FIFO_AUDIO_START);
		}
		
		break;
	case FIFO_AUDIO_STOP:
		SCHANNEL_CR(0) = 0;
		SCHANNEL_CR(1) = 0;
		break;
	case FIFO_AUDIO_PAUSE:
		SCHANNEL_CR(0) = 0;
		SCHANNEL_CR(1) = 0;
		break;

	case FIFO_AUDIO_RESUME:
		if(channels == 2) {
			SCHANNEL_CR(0) = SOUND_REPEAT|SOUND_FORMAT_16BIT|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(0);
			SCHANNEL_CR(1) = SOUND_REPEAT|SOUND_FORMAT_16BIT|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(127);
		} else
			SCHANNEL_CR(0) = SOUND_REPEAT|SOUND_FORMAT_16BIT|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(64);
		break;
	default:
		break;
	}
}

int arm7_main(int fifoCh)
{
	fifoChan = fifoCh;
	powerOn(POWER_SOUND);
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	REG_SOUNDCNT = SOUND_ENABLE;
	REG_MASTER_VOLUME = 127;
	fifoSetDatamsgHandler(fifoCh,  FifoMsgHandler, 0);
	return 0;
}

void arm7_fini()
{
	SCHANNEL_CR(0) = 0;
	REG_SOUNDCNT &= ~SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_AMP ) | PM_SOUND_MUTE );
	powerOff(POWER_SOUND);
	fifoSetDatamsgHandler(fifoChan,  0, 0);
}
