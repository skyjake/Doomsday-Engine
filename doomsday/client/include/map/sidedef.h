/**
 * @file sidedef.h
 * Map SideDef. @ingroup map
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_SIDEDEF
#define LIBDENG_MAP_SIDEDEF

#ifndef __cplusplus
#  error "map/sidedef.h requires C++"
#endif

#include "MapElement"
#include "resource/r_data.h"
#include "map/p_dmu.h"
#include "map/surface.h"

// Helper macros for accessing sidedef top/middle/bottom section data elements.
#define SW_surface(n)           sections[(n)]
#define SW_surfaceflags(n)      SW_surface(n).flags
#define SW_surfaceinflags(n)    SW_surface(n).inFlags
#define SW_surfacematerial(n)   SW_surface(n).material
#define SW_surfacetangent(n)    SW_surface(n).tangent
#define SW_surfacebitangent(n)  SW_surface(n).bitangent
#define SW_surfacenormal(n)     SW_surface(n).normal
#define SW_surfaceoffset(n)     SW_surface(n).offset
#define SW_surfacevisoffset(n)  SW_surface(n).visOffset
#define SW_surfacergba(n)       SW_surface(n).rgba
#define SW_surfaceblendmode(n)  SW_surface(n).blendMode

#define SW_middlesurface        SW_surface(SS_MIDDLE)
#define SW_middleflags          SW_surfaceflags(SS_MIDDLE)
#define SW_middleinflags        SW_surfaceinflags(SS_MIDDLE)
#define SW_middlematerial       SW_surfacematerial(SS_MIDDLE)
#define SW_middletangent        SW_surfacetangent(SS_MIDDLE)
#define SW_middlebitangent      SW_surfacebitangent(SS_MIDDLE)
#define SW_middlenormal         SW_surfacenormal(SS_MIDDLE)
#define SW_middletexmove        SW_surfacetexmove(SS_MIDDLE)
#define SW_middleoffset         SW_surfaceoffset(SS_MIDDLE)
#define SW_middlevisoffset      SW_surfacevisoffset(SS_MIDDLE)
#define SW_middlergba           SW_surfacergba(SS_MIDDLE)
#define SW_middleblendmode      SW_surfaceblendmode(SS_MIDDLE)

#define SW_topsurface           SW_surface(SS_TOP)
#define SW_topflags             SW_surfaceflags(SS_TOP)
#define SW_topinflags           SW_surfaceinflags(SS_TOP)
#define SW_topmaterial          SW_surfacematerial(SS_TOP)
#define SW_toptangent           SW_surfacetangent(SS_TOP)
#define SW_topbitangent         SW_surfacebitangent(SS_TOP)
#define SW_topnormal            SW_surfacenormal(SS_TOP)
#define SW_toptexmove           SW_surfacetexmove(SS_TOP)
#define SW_topoffset            SW_surfaceoffset(SS_TOP)
#define SW_topvisoffset         SW_surfacevisoffset(SS_TOP)
#define SW_toprgba              SW_surfacergba(SS_TOP)

#define SW_bottomsurface        SW_surface(SS_BOTTOM)
#define SW_bottomflags          SW_surfaceflags(SS_BOTTOM)
#define SW_bottominflags        SW_surfaceinflags(SS_BOTTOM)
#define SW_bottommaterial       SW_surfacematerial(SS_BOTTOM)
#define SW_bottomtangent        SW_surfacetangent(SS_BOTTOM)
#define SW_bottombitangent      SW_surfacebitangent(SS_BOTTOM)
#define SW_bottomnormal         SW_surfacenormal(SS_BOTTOM)
#define SW_bottomtexmove        SW_surfacetexmove(SS_BOTTOM)
#define SW_bottomoffset         SW_surfaceoffset(SS_BOTTOM)
#define SW_bottomvisoffset      SW_surfacevisoffset(SS_BOTTOM)
#define SW_bottomrgba           SW_surfacergba(SS_BOTTOM)

#define FRONT                   0
#define BACK                    1

class LineDef;

typedef struct msidedef_s {
    // Sidedef index. Always valid after loading & pruning.
    int index;
    int refCount;
} msidedef_t;

// FakeRadio shadow data.
typedef struct shadowcorner_s {
    float           corner;
    Sector *proximity;
    float           pOffset;
    float           pHeight;
} shadowcorner_t;

typedef struct edgespan_s {
    float           length;
    float           shift;
} edgespan_t;

/**
 * @attention SideDef is in the process of being replaced by lineside_t. All
 * data/values which concern the geometry of surfaces should be relocated to
 * lineside_t. There is no need to model the side of map's line as an object
 * in Doomsday when a flag would suffice. -ds
 */
class SideDef : public de::MapElement
{
public:
    Surface             sections[3];
    LineDef*   line;
    short               flags;
    msidedef_t          buildData;
    int                 fakeRadioUpdateCount; // frame number of last update
    shadowcorner_t      topCorners[2];
    shadowcorner_t      bottomCorners[2];
    shadowcorner_t      sideCorners[2];
    edgespan_t          spans[2];      // [left, right]

public:
    SideDef();
};

/**
 * Update the SideDef's map space surface base origins according to the points
 * defined by the associated LineDef's vertices and the plane heights of the
 * Sector on this side. If no LineDef is presently associated this is a no-op.
 *
 * @param side  SideDef instance.
 */
void SideDef_UpdateBaseOrigins(SideDef* side);

/**
 * Update the SideDef's map space surface tangents according to the points
 * defined by the associated LineDef's vertices. If no LineDef is presently
 * associated this is a no-op.
 *
 * @param sideDef  SideDef instance.
 */
void SideDef_UpdateSurfaceTangents(SideDef* sideDef);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param sideDef  SideDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int SideDef_GetProperty(const SideDef* sideDef, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param sideDef  SideDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int SideDef_SetProperty(SideDef* sideDef, const setargs_t* args);

#endif /// LIBDENG_MAP_SIDEDEF
