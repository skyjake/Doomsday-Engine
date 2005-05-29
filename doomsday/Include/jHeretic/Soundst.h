
// soundst.h

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

#define snd_SfxVolume		(Get(DD_SFX_VOLUME)/17)
#define snd_MusicVolume		(Get(DD_MUSIC_VOLUME)/17)

#include "R_local.h"

void            S_SectorSound(sector_t *sec, int id);

#endif
