
// soundst.h

#ifndef __SOUNDSTH__
#define __SOUNDSTH__

#ifndef __JHERETIC__
#  error "Using jHeretic headers without __JHERETIC__"
#endif

#include "jHeretic/r_defs.h"

#include "jHeretic/Sounds.h"                // Sfx and music indices.

enum {
    SORG_CENTER,
    SORG_FLOOR,
    SORG_CEILING
};

void            S_LevelMusic(void);
void            S_SectorSound(sector_t *sector, int origin, int sound_id);

#endif
