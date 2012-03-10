/**
 * @file linedef.h
 * Map LineDef. @ingroup map
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_LINEDEF
#define LIBDENG_MAP_LINEDEF

#include "r_data.h"
#include "p_dmu.h"

/**
 * Update the LineDef's slopetype and map space angle delta according to
 * the points defined by it's vertices.
 *
 * @param lineDef  LineDef instance.
 */
void LineDef_UpdateSlope(LineDef* lineDef);

/**
 * Update the LineDef's map space axis-aligned bounding box to encompass
 * the points defined by it's vertices.
 *
 * @param lineDef  LineDef instance.
 */
void LineDef_UpdateAABox(LineDef* lineDef);

/**
 * @param lineDef  LineDef instance.
 * @return  Minimal floor Plane which interfaces with this LineDef's face.
 *     If both front and back floor Plane interfaces exist and are equal,
 *     the front Plane is returned. May return @c NULL if no interfaces.
 */
Plane* LineDef_FloorMin(const LineDef* lineDef);

/**
 * @param lineDef  LineDef instance.
 * @return  Maximal floor Plane which interfaces with this LineDef's face.
 *     If both front and back floor Plane interfaces exist and are equal,
 *     the front Plane is returned. May return @c NULL if no interfaces.
 */
Plane* LineDef_FloorMax(const LineDef* lineDef);

/**
 * @param lineDef  LineDef instance.
 * @return  Minimal ceiling Plane which interfaces with this LineDef's face.
 *     If both front and back ceiling Plane interfaces exist and are equal,
 *     the front Plane is returned. May return @c NULL if no interfaces.
 */
Plane* LineDef_CeilingMin(const LineDef* lineDef);

/**
 * @param lineDef  LineDef instance.
 * @return  Maximal ceiling Plane which interfaces with this LineDef's face.
 *     If both front and back ceiling Plane interfaces exist and are equal,
 *     the front Plane is returned. May return @c NULL if no interfaces.
 */
Plane* LineDef_CeilingMax(const LineDef* lineDef);

/**
 * @param lineDef  LineDef instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 * @return  @c true if this LineDef's side is considered "closed" (i.e.,
 *     there is no opening through which the back Sector can be seen).
 *     Tests consider all Planes which interface with this and the "middle"
 *     Material used on the relative front side (if any).
 */
boolean LineDef_BackClosed(const LineDef* lineDef, int side, boolean ignoreOpacity);

/**
 * The DOOM lighting model applies a sector light level delta when drawing
 * line segments based on their 2D world angle.
 *
 * @param lineDef  LineDef instance.
 * @param side  Side of the LineDef we are interested in.
 * @param deltaL  Light delta for the left edge written here.
 * @param deltaR  Light delta for the right edge written here.
 */
void LineDef_LightLevelDelta(const LineDef* lineDef, int side, float* deltaL, float* deltaR);

/**
 * @param lineDef  LineDef instance.
 * @param side  Side of LineDef to test. Non-zero value signifies the BACK side.
  *
 * @return  @c true iff the SideDef on the referenced @a lineDef @a side has
 *     a "middle" Material which completely covers any opening (gap between
 *     floor and ceiling planes) on that side of the line.
 */
boolean LineDef_MiddleMaterialCoversOpening(const LineDef* lineDef, int side, boolean ignoreAlpha);

/**
 * Calculate coordinates for a "middle" Material if present.
 *
 * @param lineDef  LineDef instance.
 * @param side  Side of the LineDef we are interested in.
 * @param bottomLeft  Z map space coordinate of the bottom left of the Material written here.
 * @param bottomRight  Z map space coordinate of the bottom right of the Material written here.
 * @param topLeft  Z map space coordinate of the top left of the Material written here.
 * @param topRight Z map space coordinate of the top right of the Material written here.
 * @param texOffY  Offset to the top of the Material written here.
 *
 * @return  @c true iff the middle Material is visible (in the opening).
 */
int LineDef_MiddleMaterialCoords(const LineDef* lineDef, int side,
    float* bottomLeft, float* bottomRight, float* topLeft, float* topRight, float* texOffY,
    boolean lowerUnpeg, boolean clipTop, boolean clipBottom);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param lineDef  LineDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int LineDef_GetProperty(const LineDef* lineDef, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param lineDef  LineDef instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int LineDef_SetProperty(LineDef* lineDef, const setargs_t* args);

#endif /// LIBDENG_MAP_LINEDEF
