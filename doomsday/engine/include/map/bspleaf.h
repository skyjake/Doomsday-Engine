/**
 * @file bspleaf.h
 * Map BSP leaf. @ingroup map
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

#ifndef LIBDENG_MAP_BSPLEAF
#define LIBDENG_MAP_BSPLEAF

#ifndef __cplusplus
#  error "map/bspleaf.h requires C++"
#endif

#include "resource/r_data.h"
#include "render/rend_bias.h"
#include "p_mapdata.h"
#include "p_dmu.h"

/**
 * @defgroup bspLeafFlags  Bsp Leaf Flags
 * @ingroup flags
 */
///@{
#define BLF_UPDATE_FANBASE      0x1 ///< The tri-fan base requires an update.
///@}

class Sector;

typedef struct bspleaf_s {
    runtime_mapdata_header_t header;
    struct hedge_s*     hedge; /// First HEdge in this leaf.
    int                 flags; /// @ref bspLeafFlags.
    uint                index; /// Unique. Set when saving the BSP.
    int                 addSpriteCount; /// Frame number of last R_AddSprites.
    int                 validCount;
    uint                hedgeCount; /// Number of HEdge's in this leaf.
    Sector *sector;
    struct polyobj_s*   polyObj; /// First polyobj in this leaf. Can be @c NULL.
    struct hedge_s*     fanBase; /// HEdge whose vertex to use as the base for a trifan. If @c NULL then midPoint is used instead.
    struct shadowlink_s* shadows;
    AABoxd              aaBox; /// HEdge Vertex bounding box in the map coordinate space.
    coord_t             midPoint[2]; /// Center of vertices.
    coord_t             worldGridOffset[2]; /// Offset to align the top left of materials in the built geometry to the map coordinate space grid.
    struct biassurface_s** bsuf; /// [sector->planeCount] size.
    unsigned int        reverb[NUM_REVERB_DATA];
} BspLeaf;

BspLeaf* BspLeaf_New(void);

void BspLeaf_Delete(BspLeaf* bspLeaf);

biassurface_t* BspLeaf_BiasSurfaceForGeometryGroup(BspLeaf* bspLeaf, uint groupId);

/**
 * Update the BspLeaf's map space axis-aligned bounding box to encompass
 * the points defined by it's vertices.
 *
 * @param bspLeaf  BspLeaf instance.
 * @return  This BspLeaf instance, for convenience.
 */
BspLeaf* BspLeaf_UpdateAABox(BspLeaf* bspLeaf);

/**
 * Update the mid point in the map coordinate space.
 *
 * @pre Axis-aligned bounding box must have been initialized.
 *
 * @param bspLeaf  BspLeaf instance.
 * @return  This BspLeaf instance, for convenience.
 */
BspLeaf* BspLeaf_UpdateMidPoint(BspLeaf* bspLeaf);

/**
 * Update the world grid offset.
 *
 * @pre Axis-aligned bounding box must have been initialized.
 *
 * @param bspLeaf  BspLeaf instance.
 */
BspLeaf* BspLeaf_UpdateWorldGridOffset(BspLeaf* bspLeaf);

/**
 * Get a property value, selected by DMU_* name.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int BspLeaf_GetProperty(const BspLeaf* bspLeaf, setargs_t* args);

/**
 * Update a property value, selected by DMU_* name.
 *
 * @param bspLeaf  BspLeaf instance.
 * @param args  Property arguments.
 * @return  Always @c 0 (can be used as an iterator).
 */
int BspLeaf_SetProperty(BspLeaf* bspLeaf, const setargs_t* args);

#endif /// LIBDENG_MAP_BSPLEAF
