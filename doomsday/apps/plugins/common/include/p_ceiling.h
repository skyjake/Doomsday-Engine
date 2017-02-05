/** @file p_ceiling.h  Moving ceilings (lowering, crushing, raising).
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBCOMMON_THINKER_CEILING_H
#define LIBCOMMON_THINKER_CEILING_H

#include "doomsday.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

#define CEILSPEED           (1)
#define CEILWAIT            (150)

typedef enum {
    CS_DOWN,
    CS_UP
} ceilingstate_e;

typedef enum {
    CT_LOWERTOFLOOR,
    CT_RAISETOHIGHEST,
    CT_LOWERANDCRUSH,
    CT_CRUSHANDRAISE,
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    CT_CRUSHANDRAISEFAST,
#endif
#if __JDOOM__ || __JDOOM64__
    CT_SILENTCRUSHANDRAISE,
#endif
#if __JDOOM64__
    CT_CUSTOM,
#endif
#if __JHEXEN__
    CT_LOWERBYVALUE,
    CT_RAISEBYVALUE,
    CT_CRUSHRAISEANDSTAY,
    CT_MOVETOVALUEMUL8,
#endif
    NUMCEILINGTYPES
} ceilingtype_e;

typedef struct ceiling_s {
    thinker_t thinker;
    ceilingtype_e type;
    Sector* sector;
    coord_t bottomHeight;
    coord_t topHeight;
    float speed;
    dd_bool crush;
    ceilingstate_e state;
    ceilingstate_e oldState;
    int tag; // id.

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} ceiling_t;

#ifdef __cplusplus
extern "C" {
#endif

void T_MoveCeiling(void *ceilingThinkerPtr);

/**
 * Move a ceiling up/down.
 */
#if __JHEXEN__
int EV_DoCeiling(Line* line, byte* args, ceilingtype_e type);
#else
int EV_DoCeiling(Line* li, ceilingtype_e type);
#endif

/**
 * Reactivates all stopped crushers with the right tag.
 *
 * @param tag  Tag of ceilings to activate.
 *
 * @return  @c true, if a ceiling is activated.
 */
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
int P_CeilingActivate(short tag);
#endif

/**
 * Stops all active ceilings with the right tag.
 *
 * @param tag  Tag of ceilings to stop.
 *
 * @return  @c true, if a ceiling put in stasis.
 */
int P_CeilingDeactivate(short tag);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_THINKER_CEILING_H
