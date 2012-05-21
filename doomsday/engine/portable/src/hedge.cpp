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

static int C_DECL sortWallDivNodeAsc(const void* e1, const void* e2)
{
    const coord_t f1 = *(coord_t*)e1;
    const coord_t f2 = *(coord_t*)e2;
    if(f1 > f2) return  1;
    if(f2 > f1) return -1;
    return 0;
}

static int C_DECL sortWallDivNodeDsc(const void* e1, const void* e2)
{
    const coord_t f1 = *(coord_t*)e1;
    const coord_t f2 = *(coord_t*)e2;
    if(f1 > f2) return -1;
    if(f2 > f1) return  1;
    return 0;
}

static coord_t* findWallDivNodeByZOrigin(walldiv_t* wallDivs, float height)
{
    Q_ASSERT(wallDivs);
    for(uint i = 0; i < wallDivs->num; ++i)
    {
        if(wallDivs->pos[i] == height)
            return &wallDivs->pos[i];
    }
    return NULL;
}

static void addWallDivNodesForPlaneIntercepts(HEdge* hedge, walldiv_t* wallDivs,
    SideDefSection section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    const boolean isTwoSided = (hedge->lineDef && hedge->lineDef->L_frontside && hedge->lineDef->L_backside)? true:false;
    const boolean clockwise = !doRight;
    const LineDef* line = hedge->lineDef;
    SideDef* frontSide = HEDGE_SIDEDEF(hedge);
    Sector* frontSec = frontSide->sector;

    // Check for neighborhood division?
    if(section == SS_MIDDLE && isTwoSided) return;
    if(hedge->lineDef->inFlags & LF_POLYOBJ) return;
    if(!hedge->lineDef) return;

    // Polyobj edges are never split.
    if(hedge->lineDef && (hedge->lineDef->inFlags & LF_POLYOBJ)) return;

    // Only edges at sidedef ends can/should be split.
    if(!((hedge == frontSide->hedgeLeft  && !doRight) ||
         (hedge == frontSide->hedgeRight &&  doRight)))
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
                if(!i && iter->L_frontside && iter->L_frontsector != frontSec)
                    scanSec = iter->L_frontsector;
                else if(i && iter->L_backside && iter->L_backsector != frontSec)
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
                                    wallDivs->pos[wallDivs->num++] = pln->visHeight;

                                    // Have we reached the div limit?
                                    if(wallDivs->num == RL_MAX_DIVS)
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
                                wallDivs->pos[wallDivs->num++] = z;

                                // Have we reached the div limit?
                                if(wallDivs->num == RL_MAX_DIVS)
                                    stopScan = true;
                            }
                        }
                    }
                }
            } while(!stopScan && ++i < 2);

            // Stop the scan when a single sided line is reached.
            if(!iter->L_frontside || !iter->L_backside)
                stopScan = true;
        }
    } while(!stopScan);
}

static void buildWallDiv(walldiv_t* wallDivs, HEdge* hedge,
   SideDefSection section, coord_t bottomZ, coord_t topZ, boolean doRight)
{
    wallDivs->num = 0;

    // Add the first node.
    wallDivs->pos[wallDivs->num++] = doRight? topZ : bottomZ;

    // Add nodes for intercepts.
    addWallDivNodesForPlaneIntercepts(hedge, wallDivs, section, bottomZ, topZ, doRight);

    // Add the last node.
    wallDivs->pos[wallDivs->num++] = doRight? bottomZ : topZ;

    if(!(wallDivs->num > 2)) return;
    
    // Sorting is required. This shouldn't take too long...
    // There seldom are more than two or three nodes.
    qsort(wallDivs->pos, wallDivs->num, sizeof(*wallDivs->pos),
          doRight? sortWallDivNodeDsc : sortWallDivNodeAsc);

#ifdef RANGECHECK
    for(uint i = 1; i < wallDivs->num - 1; ++i)
    {
        const coord_t pos = wallDivs->pos[i];
        if(pos > topZ || pos < bottomZ)
        {
            Con_Error("WallDiv node #%i pos (%f) <> hi (%f), low (%f), num=%i\n",
                      i, pos, topZ, bottomZ, wallDivs->num);
        }
    }
#endif
}

boolean HEdge_PrepareWallDivs(HEdge* hedge, SideDefSection section,
    Sector* frontSec, Sector* backSec,
    walldiv_t* leftWallDivs, walldiv_t* rightWallDivs, float matOffset[2])
{
    Q_ASSERT(hedge);
    SideDef* frontSide = HEDGE_SIDEDEF(hedge);
    boolean visible = false;
    coord_t low, hi;

    // Single sided?
    if(!frontSec || !backSec ||
       (hedge->twin && !HEDGE_SIDEDEF(hedge->twin))/*front side of a "window"*/)
    {
        low = frontSec->SP_floorvisheight;
        hi  = frontSec->SP_ceilvisheight;

        if(matOffset)
        {
            Surface* suf = &frontSide->SW_middlesurface;
            matOffset[0] = suf->visOffset[0] + (float)(hedge->offset);
            matOffset[1] = suf->visOffset[1];
            if(hedge->lineDef->flags & DDLF_DONTPEGBOTTOM)
            {
                matOffset[1] += -(hi - low);
            }
        }

        visible = hi > low;
    }
    else
    {
        LineDef* line = hedge->lineDef;
        Plane* ffloor = frontSec->SP_plane(PLN_FLOOR);
        Plane* fceil  = frontSec->SP_plane(PLN_CEILING);
        Plane* bfloor = backSec->SP_plane(PLN_FLOOR);
        Plane* bceil  = backSec->SP_plane(PLN_CEILING);
        Surface* suf = &frontSide->SW_surface(section);

        visible = R_FindBottomTop(hedge->lineDef, hedge->side, section,
                                  hedge->offset + suf->visOffset[VX], suf->visOffset[VY],
                                  ffloor, fceil, bfloor, bceil,
                                  (line->flags & DDLF_DONTPEGBOTTOM)? true : false,
                                  (line->flags & DDLF_DONTPEGTOP)? true : false,
                                  (frontSide->flags & SDF_MIDDLE_STRETCH)? true : false,
                                  LINE_SELFREF(line)? true : false,
                                  &low, &hi, matOffset);
    }

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

int HEdge_SetProperty(HEdge* hedge, const setargs_t* args)
{
    assert(hedge);
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
    case DMU_SIDEDEF:
        DMU_GetValue(DMT_HEDGE_SIDEDEF, &HEDGE_SIDEDEF(hedge), args, 0);
        break;
    case DMU_LINEDEF:
        DMU_GetValue(DMT_HEDGE_LINEDEF, &hedge->lineDef, args, 0);
        break;
    case DMU_FRONT_SECTOR: {
        Sector* sec = hedge->sector? hedge->sector : NULL;
        DMU_GetValue(DMT_HEDGE_SECTOR, &sec, args, 0);
        break;
      }
    case DMU_BACK_SECTOR: {
        Sector* sec = HEDGE_BACK_SECTOR(hedge);
        DMU_GetValue(DMT_HEDGE_SECTOR, &sec, args, 0);
        break;
      }
    case DMU_ANGLE:
        DMU_GetValue(DMT_HEDGE_ANGLE, &hedge->angle, args, 0);
        break;
    default:
        Con_Error("HEdge_GetProperty: No property %s.\n", DMU_Str(args->prop));
    }

    return false; // Continue iteration.
}
