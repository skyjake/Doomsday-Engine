/**
 * @file hedge.h
 * Map Half-edge. @ingroup map
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

#ifndef LIBDENG_MAP_HEDGE
#define LIBDENG_MAP_HEDGE

#include "r_data.h"
#include "p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

struct bsphedgeinfo_s;

HEdge* HEdge_New(void);

HEdge* HEdge_NewCopy(const HEdge* other);

void HEdge_Delete(HEdge* hedge);

/**
 * Attach the specified BspHEdgeInfo to this half edge, replacing any existing
 * info already owned by this instance. Ownership of the info is relinquished
 * to this HEdge instance.
 *
 * @param hedge  HEdge instance.
 * @param info  BspHEdgeInfo to attach to this half-edge. HEdge takes ownership.
 *              It is assumed that @a info has been allocated from the Zone.
 *              If this is not the case it must be detached prior to calling
 *              HEdge_Delete()
 */
HEdge* HEdge_AttachBspBuildInfo(HEdge* hedge, struct bsphedgeinfo_s* info);

/**
 * Retrieve a pointer to the BspHEdgeInfo info associated with this instance.
 *
 * @param hedge  HEdge instance.
 * @return  BspHEdgeInfo associated with this half-edge, else @c NULL.
 */
struct bsphedgeinfo_s* HEdge_BspBuildInfo(HEdge* hedge);

/**
 * Detach any BspHEdgeInfo associated with this half-edge and relinquish
 * ownership to the caller.
 *
 * @param hedge  HEdge instance.
 * @return  BspHEdgeInfo associated with this half-edge, else @c NULL.
 */
struct bsphedgeinfo_s* HEdge_DetachBspBuildInfo(HEdge* hedge);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param hedge  HEdge instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int HEdge_GetProperty(const HEdge* hedge, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param hedge  HEdge instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int HEdge_SetProperty(HEdge* hedge, const setargs_t* args);

#ifdef __cplusplus
}
#endif

#endif /// LIBDENG_MAP_HEDGE
