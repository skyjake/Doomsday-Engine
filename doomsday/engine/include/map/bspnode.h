/**
 * @file bspnode.h
 * Map BSP node. @ingroup map
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

#ifndef LIBDENG_MAP_BSPNODE
#define LIBDENG_MAP_BSPNODE

#include "resource/r_data.h"
#include "p_dmu.h"

#ifdef __cplusplus
extern "C" {
#endif

BspNode* BspNode_New(coord_t const partitionOrigin[2], coord_t const partitionDirection[2]);

/**
 * @note Does nothing about child nodes!
 */
void BspNode_Delete(BspNode* node);

BspNode* BspNode_SetChild(BspNode* node, int left, runtime_mapdata_header_t* child);

#define BspNode_SetRight(node, child) BspNode_SetChild((node), false, (child))
#define BspNode_SetLeft(node,  child) BspNode_SetChild((node), true,  (child))

BspNode* BspNode_SetChildBounds(BspNode* node, int left, AABoxd* bounds);

#define BspNode_SetRightBounds(node, bounds) BspNode_SetChildBounds((node), false, (bounds))
#define BspNode_SetLeftBounds(node,  bounds) BspNode_SetChildBounds((node), true,  (bounds))

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_BSPNODE
