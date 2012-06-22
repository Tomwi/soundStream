#include <nds.h>
#include <feos.h>
#include "filter.h"

FIFO_FLTR_MSG msg;
int fifoChan;

/*
 * Example of how you should NOT filter stuff
 * this pretty much b0rks your samplesXD (After
 * all this is the b0rklter)
 */
void filter()
{
	int i;
	s16* out = msg.buffer;
	if(msg.off >= (msg.bufLen/4)*3) {
		for(i=msg.off; i<msg.off+msg.len; i++) {
			int off = i&(msg.bufLen - 1);
			out[off] ^= out[13];
		}
	}

	else {
		for(i=msg.off; i<msg.off+msg.len; i++) {
			if(i%128==0) {
				int off = i&(msg.bufLen - 1);
				out[off] = out[off]>>4;
			}
		}
	}

}

void FifoMsgHandler(int num_bytes, void *userdata)
{
	fifoGetDatamsg(fifoChan, num_bytes, (u8*)&msg);
	filter();
}

int arm7_main(int fifoCh)
{
	fifoChan = fifoCh;
	coopFifoSetDatamsgHandler(fifoCh,  FifoMsgHandler, 0);
	return 0;
}

void arm7_fini()
{
	fifoSetDatamsgHandler(fifoChan,  0, 0);
}
