#ifndef FILTER_H
#define FILTER_H

#include <feos.h>
#include "sound.h"
#include "fifo.h"

#define FILTER_REQUEST (0x13131313)

typedef struct {
	int fifoCh;
	instance_t fltr_mod;
	void (*filter)(void* buffer, AUDIO_INFO* inf, int len);
}FILTER;

#endif