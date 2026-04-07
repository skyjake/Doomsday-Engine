/** @file polyobjs.h  Polyobject thinkers and management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCOMMON_PLAYSIM_POLYOBJS_H
#define LIBCOMMON_PLAYSIM_POLYOBJS_H

#include "common.h"
#ifdef __cplusplus
#  include "mapstatereader.h"
#  include "mapstatewriter.h"
#endif

typedef enum {
    PODOOR_NONE,
    PODOOR_SLIDE,
    PODOOR_SWING
} podoortype_t;

/**
 * @note Used with both @ref T_RotatePoly() and @ref T_MovePoly()
 */
typedef struct polyevent_s {
    thinker_t thinker;
    int polyobj; // tag
    int intSpeed;
    unsigned int dist;
    int fangle;
    coord_t speed[2]; // for sliding doors

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} polyevent_t;

#ifdef __cplusplus
void SV_WriteMovePoly(const polyevent_t *movepoly, MapStateWriter *msw);
int SV_ReadMovePoly(polyevent_t *movepoly, MapStateReader *msr);
#endif

typedef struct polydoor_s {
    thinker_t thinker;
    int polyobj; // tag
    int intSpeed;
    int dist;
    int totalDist;
    int direction;
    float speed[2];
    int tics;
    int waitTics;
    podoortype_t type;
    dd_bool close;

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} polydoor_t;

typedef struct polyobj_s {
// Required elements (defined by the engine's dd_share.h):
    DD_BASE_POLYOBJ_ELEMENTS()

// Game-specific data:
    void *specialData; ///< A thinker (if moving).

#ifdef __cplusplus
    void write(MapStateWriter *msw) const;
    int read(MapStateReader *msr);
#endif
} Polyobj;

enum
{
    PO_ANCHOR_DOOMEDNUM = 3000,
    PO_SPAWN_DOOMEDNUM,
    PO_SPAWNCRUSH_DOOMEDNUM
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize all polyobjects in the current map.
 */
void PO_InitForMap(void);

dd_bool PO_Busy(int tag);

dd_bool PO_FindAndCreatePolyobj(int tag, dd_bool crush, float startX, float startY);

void T_PolyDoor(void *pd);

void T_RotatePoly(void *pe);

dd_bool EV_RotatePoly(Line *line, byte *args, int direction, dd_bool override);

void T_MovePoly(void *pe);

dd_bool EV_MovePoly(Line *line, byte *args, dd_bool timesEight, dd_bool override);

dd_bool EV_OpenPolyDoor(Line *line, byte *args, podoortype_t type);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_PLAYSIM_POLYOBJS_H
