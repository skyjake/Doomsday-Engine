
// soundst.h

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

#define snd_MaxVolume	Get(DD_SFX_VOLUME)
#define snd_MusicVolume	Get(DD_MIDI_VOLUME)

#include "R_local.h"

void S_SectorSound(sector_t *sec, int id);

#endif

