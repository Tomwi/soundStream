#include <nds.h>
#include "fifo.h"
#include "sound.h"

FIFO_AUD_MSG  msg;
int fifoChan;
u16 tmrData;

void FifoMsgHandler(int num_bytes, void *userdata)
{
	fifoGetDatamsg(fifoChan, num_bytes, (u8*)&msg);
	int channels = (msg.property >> 16)&0x3;
	int bytSmp = (msg.property >> 18);
	int fmt = ( (bytSmp == 2)? SOUND_FORMAT_16BIT : SOUND_FORMAT_8BIT );
	
	int i;
	switch(msg.type) {
	case FIFO_AUDIO_START:
		if(channels <=2 && channels > 0) {
			int basetmr = 0x2000000 / (msg.property & 0xFFFF) &~ 1;
			for(i=0; i<channels; i++) {
				SCHANNEL_TIMER(i) = -(basetmr>>1); // Sound channels use half of the base timer
				SCHANNEL_SOURCE(i) = (u32)(msg.buffer+msg.bufLen*i*bytSmp);
				SCHANNEL_LENGTH(i) = (msg.bufLen*bytSmp)/4;	// length is counted in words, sample is halfword
				SCHANNEL_REPEAT_POINT(i) = 0;
			}

			tmrData = -basetmr;

	case FIFO_AUDIO_RESUME:

			{
				int cS = enterCriticalSection();

				TIMER_CR(0) = 0;
				TIMER_CR(1) = 0;
				TIMER_DATA(0) = tmrData;
				TIMER_DATA(1) = 0;
			
			
				if(channels == 2) {
					SCHANNEL_CR(0) = SOUND_REPEAT|fmt|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(0);
					SCHANNEL_CR(1) = SOUND_REPEAT|fmt|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(127);
				} else
					SCHANNEL_CR(0) = SOUND_REPEAT|fmt|SCHANNEL_ENABLE|SOUND_VOL(127)|SOUND_PAN(64);

				TIMER_CR(0) = TIMER_ENABLE;
				TIMER_CR(1) = TIMER_ENABLE | TIMER_CASCADE;

				leaveCriticalSection(cS);
			}

			fifoSendValue32 (fifoChan, 1);
		}
		
		break;
	case FIFO_AUDIO_STOP:
	case FIFO_AUDIO_PAUSE:
		TIMER_CR(0) = 0;
		TIMER_CR(1) = 0;
		SCHANNEL_CR(0) = 0;
		SCHANNEL_CR(1) = 0;
		break;

	case FIFO_AUDIO_READTMR:
		fifoSendValue32(fifoChan, (TIMER_DATA(1)-16)&0xFFFF);
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
