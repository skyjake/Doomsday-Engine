/**
 * @file hedge.cpp
 * Map half-edge implementation. @ingroup map
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
#include "de_refresh.h"

#include <de/Log>

coord_t WallDivNode_Height(walldivnode_t* node)
{
    Q_ASSERT(node);
    return node->height;
}

walldivnode_t* WallDivNode_Next(walldivnode_t* node)
{
    Q_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx+1 >= node->divs->num) return 0;
    return &node->divs->nodes[idx+1];
}

walldivnode_t* WallDivNode_Prev(walldivnode_t* node)
{
    Q_ASSERT(node);
    uint idx = node - node->divs->nodes;
    if(idx == 0) return 0;
    return &node->divs->nodes[idx-1];
}

uint WallDivs_Size(const walldivs_t* wd)
{
    Q_ASSERT(wd);
    return wd->num;
}

walldivnode_t* WallDivs_First(walldivs_t* wd)
{
    Q_ASSERT(wd);
    return &wd->nodes[0];
}

walldivnode_t* WallDivs_Last(walldivs_t* wd)
{
    Q_ASSERT(wd);
    return &wd->nodes[wd->num-1];
}

walldivs_t* WallDivs_Append(walldivs_t* wd, coord_t height)
{
    Q_ASSERT(wd);
    struct walldivnode_s* node = &wd->nodes[wd->num++];
    node->divs = wd;
    node->height = height;
    return wd;
}

/**
 * Ensure the divisions are sorted (in ascending Z order).
 */
void WallDivs_AssertSorted(walldivs_t* wd)
{
#if _DEBUG
    walldivnode_t* node = WallDivs_First(wd);
    coord_t highest = WallDivNode_Height(node);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        Q_ASSERT(node->height >= highest);
        highest = node->height;
    }
#else
    DENG_UNUSED(wd);
#endif
}

/**
 * Ensure the divisions do not exceed the specified range.
 */
void WallDivs_AssertInRange(walldivs_t* wd, coord_t low, coord_t hi)
{
#if _DEBUG
    Q_ASSERT(wd);
    walldivnode_t* node = WallDivs_First(wd);
    for(uint i = 0; i < wd->num; ++i, node = WallDivNode_Next(node))
    {
        Q_ASSERT(node->height >= low && node->height <= hi);
    }
#else
    DENG_UNUSED(wd);
    DENG_UNUSED(low);
    DENG_UNUSED(hi);
#endif
}

#if _DEBUG
void WallDivs_DebugPrint(walldivs_t* wd)
{
    Q_ASSERT(wd);
    LOG_DEBUG("WallDivs [%p]:") << wd;
    for(uint i = 0; i < wd->num; ++i)
    {
        walldivnode_t* node = &wd->nodes[i];
        LOG_DEBUG("  %i: %f") << i << node->height;
    }
}
#endif

static walldivnode_t* findWallDivNodeByZOrigin(walldivs_t* wallDivs, coord_t height)
{
    Q_ASSERT(wallDivs);
    for(uint i = 0; i < wallDivs->num; ++i)
    {
        walldivnode_t* node = &wallDivs->nodes[i];
        if(node->height == height)
            return node;
    }
    return NULL;
}

static void addWallDivNodesForPlaneIntercepts(HEdge* hedge, walldivs_t* wallDivs,
    SideDefSection section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    const boolean isTwoSided = (hedge->lineDef && hedge->lineDef->L_frontsidedef && hedge->lineDef->L_backsidedef)? true:false;
    const boolean clockwise = !doRight;
    const LineDef* line = hedge->lineDef;
    Sector* frontSec = line->L_sector(hedge->side);

    // Check for neighborhood division?
    if(section == SS_MIDDLE && isTwoSided) return;
    if(!hedge->lineDef) return;
    if(hedge->lineDef->inFlags & LF_POLYOBJ) return;

    // Polyobj edges are never split.
    if(hedge->lineDef && (hedge->lineDef->inFlags & LF_POLYOBJ)) return;

    // Only edges at sidedef ends can/should be split.
    if(!((hedge == HEDGE_SIDE(hedge)->hedgeLeft  && !doRight) ||
         (hedge == HEDGE_SIDE(hedge)->hedgeRight &&  doRight)))
        return;

    if(bottomZ >= topZ) return; // Obviously no division.

    // Retrieve the start owner node.
    lineowner_t* base = R_GetVtxLineOwner(line->L_v(hedge->side^doRight), line);
    lineowner_t* own = base;
    boolean stopScan = false;
    do
    {
        own = own->link[clockwise];

        if(own == base)
        {
            stopScan = true;
        }
        else
        {
            LineDef* iter = own->lineDef;

            if(LINE_SELFREF(iter))
                continue;

            uint i = 0;
            do
            {   // First front, then back.
                Sector* scanSec = NULL;
                if(!i && iter->L_frontsidedef && iter->L_frontsector != frontSec)
                    scanSec = iter->L_frontsector;
                else if(i && iter->L_backsidedef && iter->L_backsector != frontSec)
                    scanSec = iter->L_backsector;

                if(scanSec)
                {
                    if(scanSec->SP_ceilvisheight - scanSec->SP_floorvisheight > 0)
                    {
                        for(uint j = 0; j < scanSec->planeCount && !stopScan; ++j)
                        {
                            Plane* pln = scanSec->SP_plane(j);

                            if(pln->visHeight > bottomZ && pln->visHeight < topZ)
                            {
                                if(!findWallDivNodeByZOrigin(wallDivs, pln->visHeight))
                                {
                                    WallDivs_Append(wallDivs, pln->visHeight);

                                    // Have we reached the div limit?
                                    if(wallDivs->num == WALLDIVS_MAX_NODES)
                                        stopScan = true;
                                }
                            }

                            if(!stopScan)
                            {
                                // Clip a range bound to this height?
                                if(pln->type == PLN_FLOOR && pln->visHeight > bottomZ)
                                    bottomZ = pln->visHeight;
                                else if(pln->type == PLN_CEILING && pln->visHeight < topZ)
                                    topZ = pln->visHeight;

                                // All clipped away?
                                if(bottomZ >= topZ)
                                    stopScan = true;
                            }
                        }
                    }
                    else
                    {
                        /**
                         * A zero height sector is a special case. In this
                         * instance, the potential division is at the height
                         * of the back ceiling. This is because elsewhere
                         * we automatically fix the case of a floor above a
                         * ceiling by lowering the floor.
                         */
                        coord_t z = scanSec->SP_ceilvisheight;

                        if(z > bottomZ && z < topZ)
                        {
                            if(!findWallDivNodeByZOrigin(wallDivs, z))
                            {
                                WallDivs_Append(wallDivs, z);

                                // All clipped away.
                                stopScan = true;
                            }
                        }
                    }
                }
            } while(!stopScan && ++i < 2);

            // Stop the scan when a single sided line is reached.
            if(!iter->L_frontsidedef || !iter->L_backsidedef)
                stopScan = true;
        }
    } while(!stopScan);
}

static int C_DECL sortWallDivNode(const void* e1, const void* e2)
{
    const coord_t h1 = ((walldivnode_t*)e1)->height;
    const coord_t h2 = ((walldivnode_t*)e2)->height;
    if(h1 > h2) return  1;
    if(h2 > h1) return -1;
    return 0;
}

static void buildWallDiv(walldivs_t* wallDivs, HEdge* hedge,
   SideDefSection section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    wallDivs->num = 0;

    // Nodes are arranged according to their Z axis height in ascending order.
    // The first node is the bottom.
    WallDivs_Append(wallDivs, bottomZ);

    // Add nodes for intercepts.
    addWallDivNodesForPlaneIntercepts(hedge, wallDivs, section, bottomZ, topZ, doRight);

    // The last node is the top.
    WallDivs_Append(wallDivs, topZ);

    if(!(wallDivs->num > 2)) return;
    
    // Sorting is required. This shouldn't take too long...
    // There seldom are more than two or three nodes.
    qsort(wallDivs->nodes, wallDivs->num, sizeof(*wallDivs->nodes), sortWallDivNode);

    WallDivs_AssertSorted(wallDivs);
    WallDivs_AssertInRange(wallDivs, bottomZ, topZ);
}

boolean HEdge_PrepareWallDivs(HEdge* hedge, SideDefSection section,
    Sector* frontSec, Sector* backSec,
    walldivs_t* leftWallDivs, walldivs_t* rightWallDivs, float matOffset[2])
{
    DENG_ASSERT(hedge);

    int lineFlags = hedge->lineDef? hedge->lineDef->flags : 0;
    SideDef* frontDef = HEDGE_SIDEDEF(hedge);
    SideDef* backDef  = hedge->twin? HEDGE_SIDEDEF(hedge->twin) : 0;
    coord_t low, hi;
    boolean visible = R_FindBottomTop2(section, lineFlags, frontSec, backSec, frontDef, backDef,
                                       &low, &hi, matOffset);
    matOffset[0] += float(hedge->offset);
    if(!visible) return false;

    buildWallDiv(leftWallDivs,  hedge, section, low, hi, false/*is-left-edge*/);
    buildWallDiv(rightWallDivs, hedge, section, low, hi, true/*is-right-edge*/);

    return true;
}

HEdge* HEdge_New(void)
{
    HEdge* hedge = static_cast<HEdge*>(Z_Calloc(sizeof *hedge, PU_MAPSTATIC, 0));
    hedge->header.type = DMU_HEDGE;
    return hedge;
}

HEdge* HEdge_NewCopy(const HEdge* other)
{
    assert(other);
    HEdge* hedge = static_cast<HEdge*>(Z_Malloc(sizeof *hedge, PU_MAPSTATIC, 0));
    memcpy(hedge, other, sizeof *hedge);
    return hedge;
}

void HEdge_Delete(HEdge* hedge)
{
    assert(hedge);
    for(uint i = 0; i < 3; ++i)
    {
        if(hedge->bsuf[i])
        {
            SB_DestroySurface(hedge->bsuf[i]);
        }
    }
    Z_Free(hedge);
}

coord_t HEdge_PointDistance(HEdge* hedge, coord_t const point[2], coord_t* offset)
{
    coord_t direction[2];
    DENG_ASSERT(hedge);
    V2d_Subtract(direction, hedge->HE_v2origin, hedge->HE_v1origin);
    return V2d_PointLineDistance(point, hedge->HE_v1origin, direction, offset);
}

coord_t HEdge_PointXYDistance(HEdge* hedge, coord_t x, coord_t y, coord_t* offset)
{
    coord_t point[2] = { x, y };
    return HEdge_PointDistance(hedge, point, offset);
}

coord_t HEdge_PointOnSide(const HEdge* hedge, coord_t const point[2])
{
    coord_t direction[2];
    DENG_ASSERT(hedge);
    if(!point)
    {
        DEBUG_Message(("HEdge_PointOnSide: Invalid arguments, returning >0.\n"));
        return 1;
    }
    V2d_Subtract(direction, hedge->HE_v2origin, hedge->HE_v1origin);
    return V2d_PointOnLineSide(point, hedge->HE_v1origin, direction);
}

coord_t HEdge_PointXYOnSide(const HEdge* hedge, coord_t x, coord_t y)
{
    coord_t point[2] = { x, y };
    return HEdge_PointOnSide(hedge, point);
}

int HEdge_SetProperty(HEdge* hedge, const setargs_t* args)
{
    DENG_UNUSED(hedge);
    Con_Error("HEdge_SetProperty: Property %s is not writable.\n", DMU_Str(args->prop));
    return false; // Continue iteration.
}

int HEdge_GetProperty(const HEdge* hedge, setargs_t* args)
{
    switch(args->prop)
    {
    case DMU_VERTEX0:
        DMU_GetValue(DMT_HEDGE_V, &hedge->HE_v1, args, 0);
        break;
    case DMU_VERTEX1:
        DMU_GetValue(DMT_HEDGE_V, &hedge->HE_v2, args, 0);
        break;
    case DMU_LENGTH:
        DMU_GetValue(DMT_HEDGE_LENGTH, &hedge->length, args, 0);
        break;
    case DMU_OFFSET:
        DMU_GetValue(DMT_HEDGE_OFFSET, &hedge->offset, args, 0);
        break;
    case DMU_SIDEDEF: {
        SideDef* side = HEDGE_SIDEDEF(hedge);
        DMU_GetValue(DMT_HEDGE_SIDEDEF, &side, args, 0);
        break; }
    case DMU_LINEDEF:
        DMU_GetValue(DMT_HEDGE_LINEDEF, &hedge->lineDef, args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector* sec = hedge->sector? hedge->sector : NULL;
        DMU_GetValue(DMT_HEDGE_SECTOR, &sec, args, 0);
        break; }
    case DMU_BACK_SECTOR: {
        Sector* sec = HEDGE_BACK_SECTOR(hedge);
        DMU_GetValue(DMT_HEDGE_SECTOR, &sec, args, 0);
        break; }
    case DMU_ANGLE:
        DMU_GetValue(DMT_HEDGE_ANGLE, &hedge->angle, args, 0);
        break;
    default:
        Con_Error("HEdge_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
