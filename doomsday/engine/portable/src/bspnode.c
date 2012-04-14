/**
 * @file bspnode.c
 * BspNode implementation. @ingroup map
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

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_misc.h"

BspNode* BspNode_New(coord_t const origin[2], coord_t const angle[2])
{
    BspNode* node = (BspNode*)Z_Malloc(sizeof *node, PU_MAP, 0);
    if(!node) Con_Error("BspNode_New: Failed on allocation of %lu bytes for new BspNode.", (unsigned long) sizeof *node);

    node->header.type = DMU_BSPNODE;
    node->partition.x = (float)origin[0];
    node->partition.y = (float)origin[1];
    node->partition.dX = (float)angle[0];
    node->partition.dY = (float)angle[1];

    node->children[RIGHT] = NULL;
    node->children[LEFT] = NULL;

    BspNode_SetRightBounds(node, NULL);
    BspNode_SetLeftBounds(node, NULL);

    return node;
}

void BspNode_Delete(BspNode* node)
{
    assert(node);
    Z_Free(node);
}

BspNode* BspNode_SetChild(BspNode* node, int left, runtime_mapdata_header_t* child)
{
    assert(node && child != (runtime_mapdata_header_t*)node);
    node->children[left? LEFT:RIGHT] = child;
    return node;
}

BspNode* BspNode_SetChildBounds(BspNode* node, int left, AABoxd* bounds)
{
    assert(node);
    if(bounds)
    {
        AABoxf* dst = &node->aaBox[left? LEFT:RIGHT];
        V2f_CopyBoxd(dst->arvec2, bounds->arvec2);
    }
    else
    {
        // Clear.
        AABoxf* dst = &node->aaBox[left? LEFT:RIGHT];
        V2f_Set(dst->min, DDMAXFLOAT, DDMAXFLOAT);
        V2f_Set(dst->max, DDMINFLOAT, DDMINFLOAT);
    }
    return node;
}
