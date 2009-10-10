/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * p_polyob.h: Polygon Objects
 */

#ifndef __DOOMSDAY_POLYOB_H__
#define __DOOMSDAY_POLYOB_H__

#ifdef __cplusplus

#include "dd_share.h"
#include <de/Object>

// We'll use the base polyobj template directly as our mobj.
typedef struct polyobj_s : public de::Object {
    float           pos[3];
    struct subsector_s* subsector; /* subsector in which this resides */ \
    unsigned int    idx; /* Idx of polyobject. */ \
    int             tag; /* Reference tag. */ \
    int             validCount; \
    float           box[2][2]; \
    float           dest[2]; /* Destination XY. */ \
    angle_t         angle; \
    angle_t         destAngle; /* Destination angle. */ \
    angle_t         angleSpeed; /* Rotation speed. */ \
    unsigned int    numSegs; \
    struct seg_s**  segs; \
    struct fvertex_s* originalPts; /* Used as the base for the rotations. */ \
    struct fvertex_s* prevPts; /* Use to restore the old point values. */ \
    float           speed; /* Movement speed. */ \
    boolean         crush; /* Should the polyobj attempt to crush mobjs? */ \
    int             seqType; \
    struct { \
        int         index; \
        unsigned int lineCount; \
        struct linedef_s** lineDefs; \
    } buildData;
} polyobj_t;

#define POLYOBJ_SIZE        gx.polyobjSize

extern polyobj_t** polyObjs; // List of all poly-objects on the map.
extern uint numPolyObjs;

// Polyobj system.
void            P_MapInitPolyobjs(void);

boolean         P_IsPolyobjOrigin(void* ddMobjBase);

// Polyobject interface.
void            P_PolyobjUpdateBBox(polyobj_t* po);

void            P_PolyobjLinkToRing(polyobj_t* po, linkpolyobj_t** link);
void            P_PolyobjUnlinkFromRing(polyobj_t* po, linkpolyobj_t** link);
boolean         P_PolyobjLinesIterator(polyobj_t* po, boolean (*func) (struct linedef_s*, void*),
                                       void* data);

#include "doomsday.h"

#endif

#endif
