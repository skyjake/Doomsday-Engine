/** @file p_pillar.h
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

#ifndef LIBHEXEN_P_PILLAR_H
#define LIBHEXEN_P_PILLAR_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "doomsday.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

typedef struct pillar_s {
    thinker_t thinker;
    Sector *sector;
    float ceilingSpeed;
    float floorSpeed;
    coord_t floorDest;
    coord_t ceilingDest;
    int direction;
    int crush;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} pillar_t;

#ifdef __cplusplus
extern "C" {
#endif

void T_BuildPillar(pillar_t *pillar);

int EV_BuildPillar(Line *line, byte *args, dd_bool crush);

int EV_OpenPillar(Line *line, byte *args);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_P_PILLAR_H
