/** @file bspnode.c Map BspNode.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"

BspNode *BspNode_New(coord_t const partitionOrigin[], coord_t const partitionDirection[])
{
    BspNode* node = (BspNode *)Z_Malloc(sizeof *node, PU_MAP, 0);
    if(!node) Con_Error("BspNode_New: Failed on allocation of %lu bytes for new BspNode.", (unsigned long) sizeof *node);

    node->header.type = DMU_BSPNODE;
    V2d_Copy(node->partition.origin, partitionOrigin);
    V2d_Copy(node->partition.direction, partitionDirection);

    node->children[RIGHT] = NULL;
    node->children[LEFT] = NULL;

    BspNode_SetRightBounds(node, NULL);
    BspNode_SetLeftBounds(node, NULL);

    return node;
}

void BspNode_Delete(BspNode *node)
{
    DENG_ASSERT(node);
    Z_Free(node);
}

BspNode *BspNode_SetChild(BspNode *node, int left, runtime_mapdata_header_t *child)
{
    DENG_ASSERT(node && child != (runtime_mapdata_header_t *)node);
    node->children[left? LEFT:RIGHT] = child;
    return node;
}

BspNode *BspNode_SetChildBounds(BspNode *node, int left, AABoxd *bounds)
{
    DENG_ASSERT(node);
    if(bounds)
    {
        AABoxd *dst = &node->aaBox[left? LEFT:RIGHT];
        V2d_CopyBox(dst->arvec2, bounds->arvec2);
    }
    else
    {
        // Clear.
        AABoxd *dst = &node->aaBox[left? LEFT:RIGHT];
        V2d_Set(dst->min, DDMAXFLOAT, DDMAXFLOAT);
        V2d_Set(dst->max, DDMINFLOAT, DDMINFLOAT);
    }
    return node;
}
