/** @file p_waggle.h
 *
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef LIBHEXEN_P_WAGGLE_H
#define LIBHEXEN_P_WAGGLE_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "doomsday.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

typedef enum {
    WS_EXPAND = 1,
    WS_STABLE,
    WS_REDUCE
} wagglestate_e;

typedef struct waggle_s {
    thinker_t thinker;
    Sector *sector;
    coord_t originalHeight;
    coord_t accumulator;
    coord_t accDelta;
    coord_t targetScale;
    coord_t scale;
    coord_t scaleDelta;
    int ticker;
    wagglestate_e state;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} waggle_t;

#ifdef __cplusplus
extern "C" {
#endif

void T_FloorWaggle(waggle_t *waggle);
dd_bool EV_StartFloorWaggle(int tag, int height, int speed, int offset, int timer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_P_WAGGLE_H
