/** @file r_world.cpp World Setup/Refresh.
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

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_play.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

#include "map/plane.h"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "MaterialVariantSpec"
#endif

#include <de/Observers>

// $smoothmatoffset: Maximum speed for a smoothed material offset.
#define MAX_SMOOTH_MATERIAL_MOVE (8)

using namespace de;

float rendLightWallAngle      = 1.2f; // Intensity of angle-based wall lighting.
byte rendLightWallAngleSmooth = true;

float rendSkyLight            = .2f; // Intensity factor.
byte rendSkyLightAuto         = true;

boolean firstFrameAfterLoad;
boolean ddMapSetup;

/// Notified when the current map changes.
MapChangeAudience audienceForMapChange;

/**
 * $smoothmatoffset: Roll the surface material offset tracker buffers.
 */
void R_UpdateSurfaceScroll()
{
    if(!theMap) return;

    foreach(Surface *surface, theMap->scrollingSurfaces())
    {
        // X Offset
        surface->oldOffset[0][0] = surface->oldOffset[0][1];
        surface->oldOffset[0][1] = surface->offset[0];
        if(surface->oldOffset[0][0] != surface->oldOffset[0][1])
        if(de::abs(surface->oldOffset[0][0] - surface->oldOffset[0][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            surface->oldOffset[0][0] = surface->oldOffset[0][1];
        }

        // Y Offset
        surface->oldOffset[1][0] = surface->oldOffset[1][1];
        surface->oldOffset[1][1] = surface->offset[1];
        if(surface->oldOffset[1][0] != surface->oldOffset[1][1])
        if(de::abs(surface->oldOffset[1][0] - surface->oldOffset[1][1]) >=
           MAX_SMOOTH_MATERIAL_MOVE)
        {
            // Too fast: make an instantaneous jump.
            surface->oldOffset[1][0] = surface->oldOffset[1][1];
        }
    }
}

/**
 * $smoothmatoffset: interpolate the visual offset.
 */
void R_InterpolateSurfaceScroll(boolean resetNextViewer)
{
    if(!theMap) return;
    SurfaceSet &surfaces = theMap->scrollingSurfaces();
    SurfaceSet::iterator it = surfaces.begin();
    while(it != surfaces.end())
    {
        Surface &suf = **it;

        if(resetNextViewer)
        {
            // Reset the material offset trackers.
            // X Offset.
            suf.visOffsetDelta[0] = 0;
            suf.oldOffset[0][0] = suf.oldOffset[0][1] = suf.offset[0];

            // Y Offset.
            suf.visOffsetDelta[1] = 0;
            suf.oldOffset[1][0] = suf.oldOffset[1][1] = suf.offset[1];

            suf.update();
            it++;
        }
        // While the game is paused there is no need to calculate any
        // visual material offsets.
        else //if(!clientPaused)
        {
            // Set the visible material offsets.
            // X Offset.
            suf.visOffsetDelta[0] =
                suf.oldOffset[0][0] * (1 - frameTimePos) +
                        suf.offset[0] * frameTimePos - suf.offset[0];

            // Y Offset.
            suf.visOffsetDelta[1] =
                suf.oldOffset[1][0] * (1 - frameTimePos) +
                        suf.offset[1] * frameTimePos - suf.offset[1];

            // Visible material offset.
            suf.visOffset[0] = suf.offset[0] + suf.visOffsetDelta[0];
            suf.visOffset[1] = suf.offset[1] + suf.visOffsetDelta[1];

            suf.update();

            // Has this material reached its destination?
            if(suf.visOffset[0] == suf.offset[0] && suf.visOffset[1] == suf.offset[1])
            {
                it = surfaces.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}

void R_AddTrackedPlane(PlaneSet *plist, Plane *pln)
{
    if(!plist || !pln) return;

    plist->insert(pln);
}

boolean R_RemoveTrackedPlane(PlaneSet *plist, Plane *pln)
{
    if(!plist || !pln) return false;

    return plist->remove(pln);
}

/**
 * $smoothplane: Roll the height tracker buffers.
 */
void R_UpdateTrackedPlanes()
{
    if(!theMap) return;

    PlaneSet* plist = GameMap_TrackedPlanes(theMap);
    if(!plist) return;

    foreach(Plane *plane, *plist)
    {
        plane->updateHeightTracking();
    }
}

/**
 * $smoothplane: interpolate the visual offset.
 */
void R_InterpolateTrackedPlanes(boolean resetNextViewer)
{
    if(!theMap) return;

    PlaneSet* plist = GameMap_TrackedPlanes(theMap);
    if(!plist) return;

    if(resetNextViewer)
    {
        // Reset the plane height trackers.
        foreach(Plane *plane, *plist)
        {
            plane->resetVisHeight();
        }

        // Tracked movement is now all done.
        plist->clear();
    }
    // While the game is paused there is no need to calculate any
    // visual plane offsets $smoothplane.
    else //if(!clientPaused)
    {
        // Set the visible offsets.
        QMutableSetIterator<Plane *> iter(*plist);
        while(iter.hasNext())
        {
            Plane *plane = iter.next();

            plane->lerpVisHeight();

            // Has this plane reached its destination?
            if(plane->visHeight() == plane->height()) /// @todo  Can this fail? (float equality)
            {
                iter.remove();
            }
        }
    }
}

void R_UpdateMapSurfacesOnMaterialChange(Material *material)
{
    if(!material || ddMapSetup) return;
    if(!theMap) return;

#ifdef __CLIENT__
    foreach(Surface *surface, theMap->decoratedSurfaces())
    {
        if(material == surface->material)
        {
            surface->update();
        }
    }
#endif
}

/**
 * Create a new plane for the given sector. The plane will be initialized
 * with default values.
 *
 * Post: The sector's plane list will be replaced, the new plane will be
 *       linked to the end of the list.
 *
 * @param sec  Sector for which a new plane will be created.
 *
 * @return  Ptr to the newly created plane.
 */
Plane *R_NewPlaneForSector(Sector *sec)
{
    if(!sec) return NULL; // Do wha?

    // Allocate the new plane.
    Plane *plane = new Plane(*sec, de::Vector3f(0, 0, 1));

    // Resize this sector's plane list.
    sec->_planes.append(plane);

    plane->_type = Plane::Middle;
    plane->_inSectorIndex = sec->planeCount() - 1;

    // Initialize the surface.
    /// @todo The initial material should be the "unknown" material.
    plane->surface().updateBaseOrigin();

#ifdef __CLIENT__
    /**
     * Resize the biassurface lists for the BSP leaf planes.
     * If we are in map setup mode, don't create the biassurfaces now,
     * as planes are created before the bias system is available.
     */
    foreach(BspLeaf *bspLeaf, sec->bspLeafs())
    {
        uint n = 0;

        biassurface_t **newList = (biassurface_t **) Z_Calloc(sec->planeCount() * sizeof(biassurface_t *), PU_MAP, 0);
        // Copy the existing list?
        if(bspLeaf->_bsuf)
        {
            for(; n < sec->planeCount() - 1 /* exclude newly added */; ++n)
            {
                newList[n] = bspLeaf->_bsuf[n];
            }
            Z_Free(bspLeaf->_bsuf);
            bspLeaf->_bsuf = 0;
        }

        if(!ddMapSetup)
        {
            biassurface_t *bsuf = SB_CreateSurface();

            bsuf->size = Rend_NumFanVerticesForBspLeaf(bspLeaf);
            bsuf->illum = (vertexillum_t *) Z_Calloc(sizeof(vertexillum_t) * bsuf->size, PU_MAP, 0);

            for(uint i = 0; i < bsuf->size; ++i)
            {
                SB_InitVertexIllum(&bsuf->illum[i]);
            }

            newList[n] = bsuf;
        }

        bspLeaf->_bsuf = newList;
    }
#endif

    return plane;
}

/**
 * Permanently destroys the specified plane of the given sector.
 * The sector's plane list is updated accordingly.
 *
 * @param planeIndex    The sector, plane index to be destroyed.
 * @param sec           Ptr to sector for which a plane will be destroyed.
 */
void R_DestroyPlaneOfSector(uint planeIndex, Sector *sec)
{
    if(!sec) return; // Do wha?

    if(planeIndex > sec->planeCount())
        Con_Error("P_DestroyPlaneOfSector: Plane id #%i is not valid for "
                  "sector #%u", planeIndex, (uint) GET_SECTOR_IDX(sec));

    Plane &plane = sec->plane(planeIndex);

    // If this plane is currently being watched, remove it.
    R_RemoveTrackedPlane(GameMap_TrackedPlanes(theMap), &plane);

    // If this plane's surface is in the moving list, remove it.
    theMap->scrollingSurfaces().remove(&plane.surface());

#ifdef __CLIENT__

    // If this plane's surface is in the glowing list, remove it.
    theMap->glowingSurfaces().remove(&plane.surface());

    // If this plane's surface is in the decorated list, remove it.
    theMap->decoratedSurfaces().remove(&plane.surface());

    // Destroy the biassurfaces for this plane.
    foreach(BspLeaf *bspLeaf, sec->bspLeafs())
    {
        DENG2_ASSERT(bspLeaf->_bsuf != 0);
        SB_DestroySurface(bspLeaf->_bsuf[planeIndex]);
        if(planeIndex < sec->planeCount())
        {
            std::memmove(bspLeaf->_bsuf + planeIndex, bspLeaf->_bsuf + planeIndex + 1, sizeof(biassurface_t *));
        }
    }

#endif // __CLIENT__

    // Destroy the specified plane.
    sec->_planes.removeOne(&plane);
    delete &plane;
}

void GameMap_UpdateSkyFixForSector(GameMap *map, Sector *sec)
{
    DENG_ASSERT(map);

    if(!sec || !sec->lineCount()) return;

    bool skyFloor = sec->floorSurface().isSkyMasked();
    bool skyCeil  = sec->ceilingSurface().isSkyMasked();

    if(!skyFloor && !skyCeil) return;

    if(skyCeil)
    {
        // Adjust for the plane height.
        if(sec->ceiling().visHeight() > map->skyFix[Plane::Ceiling].height)
        {
            // Must raise the skyfix ceiling.
            map->skyFix[Plane::Ceiling].height = sec->ceiling().visHeight();
        }

        // Check that all the mobjs in the sector fit in.
        for(mobj_t *mo = sec->mobjList; mo; mo = mo->sNext)
        {
            float extent = mo->origin[VZ] + mo->height;

            if(extent > map->skyFix[Plane::Ceiling].height)
            {
                // Must raise the skyfix ceiling.
                map->skyFix[Plane::Ceiling].height = extent;
            }
        }
    }

    if(skyFloor)
    {
        // Adjust for the plane height.
        if(sec->floor().visHeight() < map->skyFix[Plane::Floor].height)
        {
            // Must lower the skyfix floor.
            map->skyFix[Plane::Floor].height = sec->floor().visHeight();
        }
    }

    // Update for middle textures on two sided lines which intersect the
    // floor and/or ceiling of their front and/or back sectors.
    foreach(LineDef *line, sec->lines())
    {
        // Must be twosided.
        if(!line->hasFrontSideDef() || !line->hasBackSideDef())
            continue;

        int side = line->frontSectorPtr() == sec? FRONT : BACK;
        SideDef const &sideDef = line->sideDef(side);

        if(!sideDef.SW_middlematerial)
            continue;

        if(skyCeil)
        {
            float const top = sec->ceiling().visHeight() + sideDef.SW_middlevisoffset[VY];

            if(top > map->skyFix[Plane::Ceiling].height)
            {
                // Must raise the skyfix ceiling.
                map->skyFix[Plane::Ceiling].height = top;
            }
        }

        if(skyFloor)
        {
            float const bottom = sec->floor().visHeight() +
                sideDef.SW_middlevisoffset[VY] - sideDef.SW_middlematerial->height();

            if(bottom < map->skyFix[Plane::Floor].height)
            {
                // Must lower the skyfix floor.
                map->skyFix[Plane::Floor].height = bottom;
            }
        }
    }
}

void GameMap_InitSkyFix(GameMap *map)
{
    DENG_ASSERT(map);

    map->skyFix[Plane::Floor].height = DDMAXFLOAT;
    map->skyFix[Plane::Ceiling].height = DDMINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    for(uint i = 0; i < map->sectorCount(); ++i)
    {
        GameMap_UpdateSkyFixForSector(map, &map->sectors[i]);
    }
}

/**
 * @return  Lineowner for this line for this vertex; otherwise @c 0.
 */
LineOwner *R_GetVtxLineOwner(Vertex const *v, LineDef const *line)
{
    if(v == &line->v1())
        return line->v1Owner();

    if(v == &line->v2())
        return line->v2Owner();

    return 0;
}

#undef R_SetupFog
DENG_EXTERN_C void R_SetupFog(float start, float end, float density, float *rgb)
{
    Con_Execute(CMDS_DDAY, "fog on", true, false);
    Con_Executef(CMDS_DDAY, true, "fog start %f", start);
    Con_Executef(CMDS_DDAY, true, "fog end %f", end);
    Con_Executef(CMDS_DDAY, true, "fog density %f", density);
    Con_Executef(CMDS_DDAY, true, "fog color %.0f %.0f %.0f",
                 rgb[0] * 255, rgb[1] * 255, rgb[2] * 255);
}

#undef R_SetupFogDefaults
DENG_EXTERN_C void R_SetupFogDefaults()
{
    // Go with the defaults.
    Con_Execute(CMDS_DDAY,"fog off", true, false);
}

void R_OrderVertices(LineDef *line, Sector const *sector, Vertex *verts[2])
{
    byte edge = (sector == line->frontSectorPtr()? 0:1);
    verts[0] = &line->vertex(edge);
    verts[1] = &line->vertex(edge^1);
}

boolean R_FindBottomTop2(SideDefSection section, int lineFlags,
    const Sector *frontSec, const Sector *backSec, const SideDef *frontDef, const SideDef *backDef,
    coord_t *low, coord_t *hi, float matOffset[2])
{
    bool const unpegBottom = !!(lineFlags & DDLF_DONTPEGBOTTOM);
    bool const unpegTop    = !!(lineFlags & DDLF_DONTPEGTOP);

    // Single sided?
    if(!frontSec || !backSec || !backDef/*front side of a "window"*/)
    {
        *low = frontSec->floor().visHeight();
        *hi  = frontSec->ceiling().visHeight();

        if(matOffset)
        {
            Surface const &suf = frontDef->SW_middlesurface;
            matOffset[0] = suf.visOffset[0];
            matOffset[1] = suf.visOffset[1];
            if(unpegBottom)
            {
                matOffset[1] -= *hi - *low;
            }
        }
    }
    else
    {
        boolean const stretchMiddle = !!(frontDef->flags & SDF_MIDDLE_STRETCH);
        Plane const *ffloor = &frontSec->floor();
        Plane const *fceil  = &frontSec->ceiling();
        Plane const *bfloor = &backSec->floor();
        Plane const *bceil  = &backSec->ceiling();
        Surface const *suf = &frontDef->SW_surface(section);

        switch(section)
        {
        case SS_TOP:
            // Can't go over front ceiling (would induce geometry flaws).
            if(bceil->visHeight() < ffloor->visHeight())
                *low = ffloor->visHeight();
            else
                *low = bceil->visHeight();
            *hi = fceil->visHeight();

            if(matOffset)
            {
                matOffset[0] = suf->visOffset[0];
                matOffset[1] = suf->visOffset[1];
                if(!unpegTop)
                {
                    // Align with normal middle texture.
                    matOffset[1] -= fceil->visHeight() - bceil->visHeight();
                }
            }
            break;

        case SS_BOTTOM: {
            bool const raiseToBackFloor = (fceil->surface().isSkyMasked() && bceil->surface().isSkyMasked() &&
                                           fceil->visHeight() < bceil->visHeight() &&
                                           bfloor->visHeight() > fceil->visHeight());
            coord_t t = bfloor->visHeight();

            *low = ffloor->visHeight();
            // Can't go over the back ceiling, would induce polygon flaws.
            if(bfloor->visHeight() > bceil->visHeight())
                t = bceil->visHeight();

            // Can't go over front ceiling, would induce polygon flaws.
            // In the special case of a sky masked upper we must extend the bottom
            // section up to the height of the back floor.
            if(t > fceil->visHeight() && !raiseToBackFloor)
                t = fceil->visHeight();
            *hi = t;

            if(matOffset)
            {
                matOffset[0] = suf->visOffset[0];
                matOffset[1] = suf->visOffset[1];
                if(bfloor->visHeight() > fceil->visHeight())
                {
                    matOffset[1] -= (raiseToBackFloor? t : fceil->visHeight()) - bfloor->visHeight();
                }

                if(unpegBottom)
                {
                    // Align with normal middle texture.
                    matOffset[1] += (raiseToBackFloor? t : fceil->visHeight()) - bfloor->visHeight();
                }
            }
            break; }

        case SS_MIDDLE:
            *low = MAX_OF(bfloor->visHeight(), ffloor->visHeight());
            *hi  = MIN_OF(bceil->visHeight(),  fceil->visHeight());

            if(matOffset)
            {
                matOffset[0] = suf->visOffset[0];
                matOffset[1] = 0;
            }

            if(suf->material && !stretchMiddle)
            {
                bool const clipBottom = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && ffloor->surface().isSkyMasked() && bfloor->surface().isSkyMasked());
                bool const clipTop    = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && fceil->surface().isSkyMasked()  && bceil->surface().isSkyMasked());

                coord_t const openBottom = *low;
                coord_t const openTop    = *hi;
                int const matHeight      = suf->material->height();
                coord_t const matYOffset = suf->visOffset[VY];

                if(openTop > openBottom)
                {
                    if(unpegBottom)
                    {
                        *low += matYOffset;
                        *hi   = *low + matHeight;
                    }
                    else
                    {
                        *hi += matYOffset;
                        *low = *hi - matHeight;
                    }

                    if(matOffset && *hi > openTop)
                    {
                        matOffset[1] = *hi - openTop;
                    }

                    // Clip it?
                    if(clipTop || clipBottom)
                    {
                        if(clipBottom && *low < openBottom)
                            *low = openBottom;

                        if(clipTop && *hi > openTop)
                            *hi = openTop;
                    }

                    if(matOffset && !clipTop)
                    {
                        matOffset[1] = 0;
                    }
                }
            }
            break;
        }
    }

    return /*is_visible=*/ *hi > *low;
}

boolean R_FindBottomTop(SideDefSection section, int lineFlags,
    const Sector *frontSec, const Sector *backSec, const SideDef *frontDef, const SideDef *backDef,
    coord_t *low, coord_t *hi)
{
    return R_FindBottomTop2(section, lineFlags, frontSec, backSec, frontDef, backDef,
                            low, hi, 0/*offset not needed*/);
}

coord_t R_OpenRange(Sector const *frontSec, Sector const *backSec, coord_t *retBottom, coord_t *retTop)
{
    DENG_ASSERT(frontSec);

    coord_t top;
    if(backSec && backSec->ceiling().height() < frontSec->ceiling().height())
    {
        top = backSec->ceiling().height();
    }
    else
    {
        top = frontSec->ceiling().height();
    }

    coord_t bottom;
    if(backSec && backSec->floor().height() > frontSec->floor().height())
    {
        bottom = backSec->floor().height();
    }
    else
    {
        bottom = frontSec->floor().height();
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

coord_t R_VisOpenRange(Sector const *frontSec, Sector const *backSec, coord_t *retBottom, coord_t *retTop)
{
    DENG_ASSERT(frontSec);

    coord_t top;
    if(backSec && backSec->ceiling().visHeight() < frontSec->ceiling().visHeight())
    {
        top = backSec->ceiling().visHeight();
    }
    else
    {
        top = frontSec->ceiling().visHeight();
    }

    coord_t bottom;
    if(backSec && backSec->floor().visHeight() > frontSec->floor().visHeight())
    {
        bottom = backSec->floor().visHeight();
    }
    else
    {
        bottom = frontSec->floor().visHeight();
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;

    return top - bottom;
}

#ifdef __CLIENT__
boolean R_MiddleMaterialCoversOpening(int lineFlags, Sector const *frontSec,
    Sector const *backSec, SideDef const *frontDef, SideDef const *backDef,
    boolean ignoreOpacity)
{
    if(!frontSec || !frontDef) return false; // Never.

    Material *material = frontDef->SW_middlematerial;
    if(!material) return false;

    // Ensure we have up to date info about the material.
    MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());

    if(ignoreOpacity || (ms.isOpaque() && !frontDef->SW_middleblendmode && frontDef->SW_middlergba[3] >= 1))
    {
        coord_t openRange, openBottom, openTop;

        // Stretched middles always cover the opening.
        if(frontDef->flags & SDF_MIDDLE_STRETCH) return true;

        // Might the material cover the opening?
        openRange = R_VisOpenRange(frontSec, backSec, &openBottom, &openTop);
        if(ms.height() >= openRange)
        {
            // Possibly; check the placement.
            coord_t bottom, top;
            if(R_FindBottomTop(SS_MIDDLE, lineFlags, frontSec, backSec, frontDef, backDef,
                               &bottom, &top))
            {
                return (top >= openTop && bottom <= openBottom);
            }
        }
    }

    return false;
}

boolean R_MiddleMaterialCoversLineOpening(LineDef const *line, int side, boolean ignoreOpacity)
{
    DENG_ASSERT(line);
    Sector const *frontSec  = line->sectorPtr(side);
    Sector const *backSec   = line->sectorPtr(side ^ 1);
    SideDef const *frontDef = line->sideDefPtr(side);
    SideDef const *backDef  = line->sideDefPtr(side ^ 1);
    return R_MiddleMaterialCoversOpening(line->flags(), frontSec, backSec, frontDef, backDef, ignoreOpacity);
}

LineDef *R_FindLineNeighbor(Sector const *sector, LineDef const *line,
    LineOwner const *own, boolean antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    LineDef *other = &cown->line();

    if(other == line)
        return NULL;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!other->hasBackSideDef() || other->frontSectorPtr() != other->backSectorPtr())
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->frontSectorPtr() == sector ||
               (other->hasBackSideDef() && other->backSectorPtr() == sector))
                return other;
        }
        else
        {
            return other;
        }
    }

    // Not suitable, try the next.
    return R_FindLineNeighbor(sector, line, cown, antiClockwise, diff);
}

LineDef *R_FindSolidLineNeighbor(Sector const *sector, LineDef const *line,
    LineOwner const *own, boolean antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    LineDef *other = &cown->line();
    int side;

    if(other == line) return NULL;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!((other->isBspWindow()) && other->frontSectorPtr() != sector))
    {
        if(!other->hasFrontSideDef() || !other->hasBackSideDef())
            return other;

        if(!other->isSelfReferencing() &&
           (other->frontSector().floor().visHeight() >= sector->ceiling().visHeight() ||
            other->frontSector().ceiling().visHeight() <= sector->floor().visHeight() ||
            other->backSector().floor().visHeight() >= sector->ceiling().visHeight() ||
            other->backSector().ceiling().visHeight() <= sector->floor().visHeight() ||
            other->backSector().ceiling().visHeight() <= other->backSector().floor().visHeight()))
            return other;

        // Both front and back MUST be open by this point.

        // Check for mid texture which fills the gap between floor and ceiling.
        // We should not give away the location of false walls (secrets).
        side = (other->frontSectorPtr() == sector? 0 : 1);
        if(other->sideDef(side).SW_middlematerial)
        {
            float oFCeil  = other->frontSector().ceiling().visHeight();
            float oFFloor = other->frontSector().floor().visHeight();
            float oBCeil  = other->backSector().ceiling().visHeight();
            float oBFloor = other->backSector().floor().visHeight();

            if((side == 0 &&
                ((oBCeil > sector->floor().visHeight() &&
                      oBFloor <= sector->floor().visHeight()) ||
                 (oBFloor < sector->ceiling().visHeight() &&
                      oBCeil >= sector->ceiling().visHeight()) ||
                 (oBFloor < sector->ceiling().visHeight() &&
                      oBCeil > sector->floor().visHeight()))) ||
               ( /* side must be 1 */
                ((oFCeil > sector->floor().visHeight() &&
                      oFFloor <= sector->floor().visHeight()) ||
                 (oFFloor < sector->ceiling().visHeight() &&
                      oFCeil >= sector->ceiling().visHeight()) ||
                 (oFFloor < sector->ceiling().visHeight() &&
                      oFCeil > sector->floor().visHeight())))  )
            {

                if(!R_MiddleMaterialCoversLineOpening(other, side, false))
                    return 0;
            }
        }
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

LineDef *R_FindLineBackNeighbor(Sector const *sector, LineDef const *line,
    LineOwner const *own, boolean antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    LineDef *other = &cown->line();

    if(other == line) return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!other->hasBackSideDef() || other->frontSectorPtr() != other->backSectorPtr() ||
       (other->isBspWindow()))
    {
        if(!(other->frontSectorPtr() == sector ||
             (other->hasBackSideDef() && other->backSectorPtr() == sector)))
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineBackNeighbor(sector, line, cown, antiClockwise, diff);
}

LineDef *R_FindLineAlignNeighbor(Sector const *sec, LineDef const *line,
    LineOwner const *own, boolean antiClockwise, int alignment)
{
    int const SEP = 10;

    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    LineDef *other = &cown->line();
    binangle_t diff;

    if(other == line)
        return NULL;

    if(!other->isSelfReferencing())
    {
        diff = line->angle() - other->angle();

        if(alignment < 0)
            diff -= BANG_180;
        if(other->frontSectorPtr() != sec)
            diff -= BANG_180;
        if(diff < SEP || diff > BANG_360 - SEP)
            return other;
    }

    // Can't step over non-twosided lines.
    if(!other->hasFrontSideDef() || !other->hasBackSideDef())
        return NULL;

    // Not suitable, try the next.
    return R_FindLineAlignNeighbor(sec, line, cown, antiClockwise, alignment);
}
#endif // __CLIENT__

static inline void initSurfaceMaterialOffset(Surface *suf)
{
    DENG_ASSERT(suf);
    suf->visOffset[VX] = suf->oldOffset[0][VX] = suf->oldOffset[1][VX] = suf->offset[VX];
    suf->visOffset[VY] = suf->oldOffset[0][VY] = suf->oldOffset[1][VY] = suf->offset[VY];
}

/**
 * Set intial values of various tracked and interpolated properties
 * (lighting, smoothed planes etc).
 */
void R_MapInitSurfaces(boolean forceUpdate)
{
    if(novideo) return;

    for(uint i = 0; i < NUM_SECTORS; ++i)
    {
        Sector *sec = SECTOR_PTR(i);

        R_UpdateSector(sec, forceUpdate);

        foreach(Plane *plane, sec->planes())
        {
            plane->_visHeight = plane->_oldHeight[0] = plane->_oldHeight[1] = plane->_height;
            initSurfaceMaterialOffset(&plane->surface());
        }
    }

    for(uint i = 0; i < NUM_SIDEDEFS; ++i)
    {
        SideDef *si = SIDE_PTR(i);

        initSurfaceMaterialOffset(&si->SW_topsurface);
        initSurfaceMaterialOffset(&si->SW_middlesurface);
        initSurfaceMaterialOffset(&si->SW_bottomsurface);
    }
}

#ifdef __CLIENT__
static void addToSurfaceSets(Surface *suf, Material *material)
{
    if(!suf || !material) return;

    if(material->hasGlow())
    {
        theMap->glowingSurfaces().insert(suf);
    }

    if(material->isDecorated())
    {
        theMap->decoratedSurfaces().insert(suf);
    }
}
#endif // __CLIENT__

void R_MapInitSurfaceLists()
{
#ifdef __CLIENT__

    theMap->decoratedSurfaces().clear();
    theMap->glowingSurfaces().clear();

    for(uint i = 0; i < NUM_SIDEDEFS; ++i)
    {
        SideDef *side = SIDE_PTR(i);

        addToSurfaceSets(&side->SW_middlesurface, side->SW_middlematerial);
        addToSurfaceSets(&side->SW_topsurface,    side->SW_topmaterial);
        addToSurfaceSets(&side->SW_bottomsurface, side->SW_bottommaterial);
    }

    for(uint i = 0; i < NUM_SECTORS; ++i)
    {
        Sector *sec = SECTOR_PTR(i);

        // Skip sectors with no lines as their planes will never be drawn.
        if(!sec->lineCount()) continue;

        foreach(Plane *plane, sec->planes())
        {
            Surface &surface = plane->surface();
            addToSurfaceSets(&surface, surface.material);
        }
    }

#endif // __CLIENT__
}

#undef R_SetupMap
DENG_EXTERN_C void R_SetupMap(int mode, int flags)
{
    DENG_UNUSED(flags);

    switch(mode)
    {
    case DDSMM_INITIALIZE:
        // A new map is about to be setup.
        ddMapSetup = true;

#ifdef __CLIENT__
        App_Materials().purgeCacheQueue();
#endif
        return;

    case DDSMM_AFTER_LOADING:
        DENG_ASSERT(theMap);

        // Update everything again. Its possible that after loading we
        // now have more HOMs to fix, etc..
        GameMap_InitSkyFix(theMap);
        R_MapInitSurfaces(true);
        GameMap_InitPolyobjs(theMap);
        DD_ResetTimer();
        return;

    case DDSMM_FINALIZE: {
        DENG_ASSERT(theMap);

        if(gameTime > 20000000 / TICSPERSEC)
        {
            // In very long-running games, gameTime will become so large that it cannot be
            // accurately converted to 35 Hz integer tics. Thus it needs to be reset back
            // to zero.
            gameTime = 0;
        }

        // We are now finished with the map entity db.
        EntityDatabase_Delete(theMap->entityDatabase);

#ifdef __SERVER__
        // Init server data.
        Sv_InitPools();
#endif

        // Recalculate the light range mod matrix.
        Rend_CalcLightModRange();

        GameMap_InitPolyobjs(theMap);
        P_MapSpawnPlaneParticleGens();

        R_MapInitSurfaces(true);
        R_MapInitSurfaceLists();

#ifdef __CLIENT__
        float startTime = Timer_Seconds();
        Rend_CacheForMap();
        App_Materials().processCacheQueue();
        VERBOSE( Con_Message("Precaching took %.2f seconds.", Timer_Seconds() - startTime) )
#endif

        S_SetupForChangedMap();

        // Map setup has been completed.

        // Run any commands specified in Map Info.
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(GameMap_Uri(theMap));
        if(mapInfo && mapInfo->execute)
        {
            Con_Execute(CMDS_SCRIPT, mapInfo->execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do something useful.
        AutoStr *mapPath = Uri_Resolved(GameMap_Uri(theMap));
        char cmd[80];
        dd_snprintf(cmd, 80, "init-%s", Str_Text(mapPath));
        if(Con_IsValidCommand(cmd))
        {
            Con_Executef(CMDS_SCRIPT, false, "%s", cmd);
        }

#ifdef __CLIENT__
        // Clear any input events that might have accumulated during the
        // setup period.
        DD_ClearEvents();
#endif

        // Now that the setup is done, let's reset the tictimer so it'll
        // appear that no time has passed during the setup.
        DD_ResetTimer();

        // Kill all local commands and determine the invoid status of players.
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t *ddpl = &plr->shared;

            //clients[i].numTics = 0;

            // Determine if the player is in the void.
            ddpl->inVoid = true;
            if(ddpl->mo)
            {
                BspLeaf *bspLeaf = P_BspLeafAtPoint(ddpl->mo->origin);
                if(bspLeaf)
                {
                    /// @todo $nplanes
                    if(ddpl->mo->origin[VZ] >= bspLeaf->sector().floor().visHeight() &&
                       ddpl->mo->origin[VZ] < bspLeaf->sector().ceiling().visHeight() - 4)
                    {
                        ddpl->inVoid = false;
                    }
                }
            }
        }

        // Reset the map tick timer.
        ddMapTime = 0;

        // We've finished setting up the map.
        ddMapSetup = false;

        // Inform the timing system to suspend the starting of the clock.
        firstFrameAfterLoad = true;

        DENG2_FOR_AUDIENCE(MapChange, i) i->currentMapChanged();

        Z_PrintStatus();
        return; }

    default:
        Con_Error("R_SetupMap: Unknown setup mode %i", mode);
        exit(1); // Unreachable.
    }
}

void R_ClearSectorFlags()
{
    for(uint i = 0; i < NUM_SECTORS; ++i)
    {
        Sector *sec = SECTOR_PTR(i);
        // Clear all flags that can be cleared before each frame.
        sec->frameFlags &= ~SIF_FRAME_CLEAR;
    }
}

boolean R_IsGlowingPlane(Plane const *pln)
{
    Material *material = pln->surface().material;
    if(material)
    {
        if(!material->isDrawable() || material->hasGlow()) return true;
    }
    return pln->surface().isSkyMasked();
}

float R_GlowStrength(Plane const *pln)
{
#ifdef __CLIENT__
    Material *material = pln->surface().material;
    if(material)
    {
        if(material->isDrawable() && !pln->surface().isSkyMasked())
        {
            MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());

            float glowStrength = ms.glowStrength();
            if(glowFactor > .0001f)
                glowStrength *= glowFactor; // Global scale factor.
            return glowStrength;
        }
    }
#else
    DENG_UNUSED(pln);
#endif
    return 0;
}

/**
 * Does the specified sector contain any sky surface planes?
 *
 * @return  @c true, if one or more plane surfaces in the given sector use
 * a sky-masked material.
 */
boolean R_SectorContainsSkySurfaces(Sector const *sec)
{
    DENG_ASSERT(sec);

    boolean sectorContainsSkySurfaces = false;
    uint n = 0;
    do
    {
        if(sec->planeSurface(n).isSkyMasked())
            sectorContainsSkySurfaces = true;
        else
            n++;
    } while(!sectorContainsSkySurfaces && n < sec->planeCount());

    return sectorContainsSkySurfaces;
}

#ifdef __CLIENT__

/**
 * Given a sidedef section, look at the neighbouring surfaces and pick the
 * best choice of material used on those surfaces to be applied to "this"
 * surface.
 *
 * Material on back neighbour plane has priority.
 * Non-animated materials are preferred.
 * Sky materials are ignored.
 */
static Material *chooseFixMaterial(SideDef *s, SideDefSection section)
{
    Material *choice1 = 0, *choice2 = 0;
    LineDef *line = s->line;
    byte side = (line->frontSideDefPtr() == s? FRONT : BACK);
    Sector *frontSec = line->sectorPtr(side);
    Sector *backSec  = line->sideDefPtr(side ^ 1)? line->sectorPtr(side ^ 1) : 0;

    if(backSec)
    {
        // Our first choice is a material in the other sector.
        if(section == SS_BOTTOM)
        {
            if(frontSec->floor().height() < backSec->floor().height())
            {
                choice1 = backSec->floorSurface().material;
            }
        }
        else if(section == SS_TOP)
        {
            if(frontSec->ceiling().height()  > backSec->ceiling().height())
            {
                choice1 = backSec->ceilingSurface().material;
            }
        }

        // In the special case of sky mask on the back plane, our best
        // choice is always this material.
        if(choice1 && choice1->isSkyMasked())
        {
            return choice1;
        }
    }
    else
    {
        // Our first choice is a material on an adjacent wall section.
        // Try the left neighbor first.
        LineDef *other = R_FindLineNeighbor(frontSec, line, line->vertexOwner(side),
                                            false /*next clockwise*/, NULL/*angle delta is irrelevant*/);
        if(!other)
            // Try the right neighbor.
            other = R_FindLineNeighbor(frontSec, line, line->vertexOwner(side^1),
                                       true /*next anti-clockwise*/, NULL/*angle delta is irrelevant*/);

        if(other)
        {
            if(!other->hasBackSideDef())
            {
                // Our choice is clear - the middle material.
                choice1 = other->frontSideDef().SW_middlematerial;
            }
            else
            {
                // Compare the relative heights to decide.
                SideDef &otherSide = other->sideDef(&other->frontSector() == frontSec? FRONT : BACK);
                Sector &otherSec   = other->sector(&other->frontSector() == frontSec? BACK  : FRONT);

                if(otherSec.ceiling().height() <= frontSec->floor().height())
                    choice1 = otherSide.SW_topmaterial;
                else if(otherSec.floor().height() >= frontSec->ceiling().height())
                    choice1 = otherSide.SW_bottommaterial;
                else if(otherSec.ceiling().height() < frontSec->ceiling().height())
                    choice1 = otherSide.SW_topmaterial;
                else if(otherSec.floor().height() > frontSec->floor().height())
                    choice1 = otherSide.SW_bottommaterial;
                // else we'll settle for a plane material.
            }
        }
    }

    // Our second choice is a material from this sector.
    choice2 = frontSec->planeSurface(section == SS_BOTTOM? Plane::Floor : Plane::Ceiling).material;

    // Prefer a non-animated, non-masked material.
    if(choice1 && !choice1->isAnimated() && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isAnimated() && !choice2->isSkyMasked())
        return choice2;

    // Prefer a non-masked material.
    if(choice1 && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isSkyMasked())
        return choice2;

    // At this point we'll accept anything if it means avoiding HOM.
    if(choice1) return choice1;
    if(choice2) return choice2;

    // We'll assign the special "missing" material...
    return &App_Materials().find(de::Uri("System", Path("missing"))).material();
}

static void addMissingMaterial(SideDef *s, SideDefSection section)
{
    // A material must be missing for this test to apply.
    Surface *suf = &s->sections[section];
    if(suf->material) return;

    // Look for a suitable replacement.
    suf->setMaterial(chooseFixMaterial(s, section));
    suf->inFlags |= SUIF_FIX_MISSING_MATERIAL;

    // During map load we log missing materials.
    if(ddMapSetup && verbose)
    {
        String path = suf->material? suf->material->manifest().composeUri().asText() : "<null>";
        LOG_WARNING("SideDef #%u is missing a material for the %s section.\n"
                    "  %s was chosen to complete the definition.")
            << s->buildData.index - 1
            << (section == SS_MIDDLE? "middle" : section == SS_TOP? "top" : "bottom")
            << path;
    }
}
#endif // __CLIENT__

#ifdef __CLIENT__
static void updateMissingMaterialsForLinesOfSector(Sector const &sec)
{
    foreach(LineDef *line, sec.lines())
    {
        // Self-referencing lines don't need fixing.
        if(line->isSelfReferencing()) continue;

        // Do not fix BSP "window" lines.
        if(!line->hasFrontSideDef() || (!line->hasBackSideDef() && line->hasBackSector()))
            continue;

        /**
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless it is skymasked),
         * or if there is a midtexture use that instead.
         */
        if(line->hasBackSector())
        {
            Sector const &frontSec = line->frontSector();
            Sector const &backSec  = line->backSector();

            // A potential bottom section fix?
            if(frontSec.floor().height() < backSec.floor().height())
            {
                addMissingMaterial(line->frontSideDefPtr(), SS_BOTTOM);
            }
            else if(frontSec.floor().height() > backSec.floor().height())
            {
                addMissingMaterial(line->backSideDefPtr(), SS_BOTTOM);
            }

            // A potential top section fix?
            if(backSec.ceiling().height() < frontSec.ceiling().height())
            {
                addMissingMaterial(line->frontSideDefPtr(), SS_TOP);
            }
            else if(backSec.ceiling().height() > frontSec.ceiling().height())
            {
                addMissingMaterial(line->backSideDefPtr(), SS_TOP);
            }
        }
        else
        {
            // A potential middle section fix.
            addMissingMaterial(line->frontSideDefPtr(), SS_MIDDLE);
        }
    }
}
#endif // __CLIENT__

boolean R_UpdatePlane(Plane *pln, boolean forceUpdate)
{
    Sector *sec = &pln->sector();
    boolean changed = false;

    // Geometry change?
    if(forceUpdate || pln->height() != pln->_oldHeight[1])
    {
        // Check if there are any camera players in this sector. If their
        // height is now above the ceiling/below the floor they are now in
        // the void.
        for(uint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t *ddpl = &plr->shared;

            if(!ddpl->inGame || !ddpl->mo || !ddpl->mo->bspLeaf)
                continue;

            /// @todo $nplanes
            if((ddpl->flags & DDPF_CAMERA) && ddpl->mo->bspLeaf->sectorPtr() == sec &&
               (ddpl->mo->origin[VZ] > sec->ceiling().height() - 4 || ddpl->mo->origin[VZ] < sec->floor().height()))
            {
                ddpl->inVoid = true;
            }
        }

        // Update the base origins for this plane and all affected wall surfaces.
        pln->surface().updateBaseOrigin();
        foreach(LineDef *line, sec->lines())
        {
            if(line->hasFrontSideDef()) // $degenleaf
            {
                line->frontSideDef().updateBaseOrigins();
            }

            if(line->hasBackSideDef())
            {
                line->backSideDef().updateBaseOrigins();
            }
        }

#ifdef __CLIENT__
        // Inform the shadow bias of changed geometry.
        foreach(BspLeaf *bspLeaf, sec->bspLeafs())
        {
            if(HEdge *base = bspLeaf->firstHEdge())
            {
                HEdge *hedge = base;
                do
                {
                    if(hedge->lineDef)
                    {
                        for(uint i = 0; i < 3; ++i)
                        {
                            SB_SurfaceMoved(hedge->bsuf[i]);
                        }
                    }
                } while((hedge = hedge->next) != base);
            }

            SB_SurfaceMoved(&bspLeaf->biasSurfaceForGeometryGroup(pln->inSectorIndex()));
        }
#endif // __CLIENT__

        // We need the decorations updated.
        pln->surface().update();

        changed = true;
    }

    return changed;
}

boolean R_UpdateSector(Sector *sec, boolean forceUpdate)
{
    DENG_ASSERT(sec);

    boolean changed = false, planeChanged = false;

    // Check if there are any lightlevel or color changes.
    if(forceUpdate ||
       (sec->lightLevel != sec->oldLightLevel ||
        sec->rgb[0] != sec->oldRGB[0] ||
        sec->rgb[1] != sec->oldRGB[1] ||
        sec->rgb[2] != sec->oldRGB[2]))
    {
        sec->frameFlags |= SIF_LIGHT_CHANGED;
        sec->oldLightLevel = sec->lightLevel;
        std::memcpy(sec->oldRGB, sec->rgb, sizeof(sec->oldRGB));

        LG_SectorChanged(static_cast<Sector *>(sec));
        changed = true;
    }
    else
    {
        sec->frameFlags &= ~SIF_LIGHT_CHANGED;
    }

    foreach(Plane *plane, sec->planes())
    {
        if(R_UpdatePlane(plane, forceUpdate))
        {
            planeChanged = true;
        }
    }

    if(forceUpdate || planeChanged)
    {
        sec->updateBaseOrigin();
#ifdef __CLIENT__
        updateMissingMaterialsForLinesOfSector(*sec);
#endif
        S_MarkSectorReverbDirty(sec);
        changed = true;
    }

    return planeChanged;
}

boolean R_UpdateLinedef(LineDef *line, boolean forceUpdate)
{
    // Stub.
    DENG_UNUSED(line); DENG_UNUSED(forceUpdate);
    return false; // Not changed.
}

boolean R_UpdateSidedef(SideDef *side, boolean forceUpdate)
{
    // Stub.
    DENG_UNUSED(side); DENG_UNUSED(forceUpdate);
    return false; // Not changed.
}

boolean R_UpdateSurface(Surface* suf, boolean forceUpdate)
{
    // Stub.
    DENG_UNUSED(suf); DENG_UNUSED(forceUpdate);
    return false; // Not changed.
}

/**
 * All links will be updated every frame (sectorheights may change at
 * any time without notice).
 */
void R_UpdatePlanes()
{
    // Nothing to do.
}

/**
 * The DOOM lighting model applies distance attenuation to sector light
 * levels.
 *
 * @param distToViewer  Distance from the viewer to this object.
 * @param lightLevel    Sector lightLevel at this object's origin.
 * @return              The specified lightLevel plus any attentuation.
 */
float R_DistAttenuateLightLevel(float distToViewer, float lightLevel)
{
    if(distToViewer > 0 && rendLightDistanceAttentuation > 0)
    {
        float real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttentuation *
                (1 - lightLevel);

        float minimum = lightLevel * lightLevel + (lightLevel - .63f) * .5f;
        if(real < minimum)
            real = minimum; // Clamp it.

        return real;
    }

    return lightLevel;
}

float R_ExtraLightDelta()
{
    return extraLightDelta;
}

float R_CheckSectorLight(float lightlevel, float min, float max)
{
    // Has a limit been set?
    if(min == max) return 1;
    Rend_ApplyLightAdaptation(&lightlevel);
    return MINMAX_OF(0, (lightlevel - min) / (float) (max - min), 1);
}

#ifdef __CLIENT__

float const *R_GetSectorLightColor(Sector const *sector)
{
    static vec3f_t skyLightColor, oldSkyAmbientColor = { -1, -1, -1 };
    static float oldRendSkyLight = -1;
    if(rendSkyLight > .001f && R_SectorContainsSkySurfaces(sector))
    {
        ColorRawf const *ambientColor = Sky_AmbientColor();
        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor->red,   oldSkyAmbientColor[CR], .001f) ||
           !INRANGE_OF(ambientColor->green, oldSkyAmbientColor[CG], .001f) ||
           !INRANGE_OF(ambientColor->blue,  oldSkyAmbientColor[CB], .001f))
        {
            vec3f_t white = { 1, 1, 1 };
            V3f_Copy(skyLightColor, ambientColor->rgb);
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            V3f_Lerp(skyLightColor, skyLightColor, white, 1-rendSkyLight);

            // When the sky light color changes we must update the lightgrid.
            LG_MarkAllForUpdate();
            V3f_Copy(oldSkyAmbientColor, ambientColor->rgb);
        }
        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }
    // A non-skylight sector (i.e., everything else!)
    return sector->rgb; // The sector's ambient light color.
}

#endif // __CLIENT__

coord_t R_SkyCapZ(BspLeaf *bspLeaf, int skyCap)
{
    DENG_ASSERT(bspLeaf);
    Plane::Type const plane = (skyCap & SKYCAP_UPPER)? Plane::Ceiling : Plane::Floor;
    if(!bspLeaf->hasSector() || !P_IsInVoid(viewPlayer)) return GameMap_SkyFix(theMap, plane == Plane::Ceiling);
    return bspLeaf->sector().plane(plane).visHeight();
}
