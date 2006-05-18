/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

#ifndef __S_SOUND__
#define __S_SOUND__

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "r_defs.h"

#include "SndIdx.h"                // Sfx and music indices.

enum {
    SORG_CENTER,
    SORG_FLOOR,
    SORG_CEILING
};

void            S_LevelMusic(void);
void            S_SectorSound(sector_t *sector, int origin, int sound_id);

#endif
