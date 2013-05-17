/** @file r_world.cpp World Setup/Refresh.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include "map/lineowner.h"
#include "Plane"
#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "MaterialVariantSpec"
#endif

#include <de/Observers>

using namespace de;

boolean ddMapSetup;

/// Notified when the current map changes.
MapChangeAudience audienceForMapChange;

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

void R_SetRelativeHeights(Sector const *front, Sector const *back, int planeIndex,
    coord_t *fz, coord_t *bz, coord_t *bhz)
{
    if(fz)
    {
        if(front)
        {
            *fz = front->plane(planeIndex).visHeight();
            if(planeIndex != Plane::Floor)
                *fz = -(*fz);
        }
        else
        {
            *fz = 0;
        }
    }
    if(bz)
    {
        if(back)
        {
            *bz = back->plane(planeIndex).visHeight();
            if(planeIndex != Plane::Floor)
                *bz = -(*bz);
        }
        else
        {
            *bz = 0;
        }
    }
    if(bhz)
    {
        if(back)
        {
            int otherPlaneIndex = planeIndex == Plane::Floor? Plane::Ceiling : Plane::Floor;
            *bhz = back->plane(otherPlaneIndex).visHeight();
            if(planeIndex != Plane::Floor)
                *bhz = -(*bhz);
        }
        else
        {
            *bhz = 0;
        }
    }
}

void R_SideSectionCoords(Line::Side const &side, int section,
    coord_t *retBottom, coord_t *retTop, Vector2f *retMaterialOrigin)
{
    Sector const *frontSec = side.line().definesPolyobj()? side.line().polyobj().bspLeaf().sectorPtr() : side.sectorPtr();
    Sector const *backSec  = (side.line().definesPolyobj() || (side.leftHEdge()->twin().hasBspLeaf() && !side.leftHEdge()->twin().bspLeaf().isDegenerate()))? side.back().sectorPtr() : 0;

    Line const &line       = side.line();
    bool const unpegBottom = (line.flags() & DDLF_DONTPEGBOTTOM) != 0;
    bool const unpegTop    = (line.flags() & DDLF_DONTPEGTOP) != 0;

    coord_t bottom = 0, top = 0; // Shutup compiler.

    // Single sided?
    if(!frontSec || !backSec || !side.back().hasSections()/*front side of a "window"*/)
    {
        bottom = frontSec->floor().visHeight();
        top    = frontSec->ceiling().visHeight();

        if(retMaterialOrigin)
        {
            *retMaterialOrigin = side.middle().visMaterialOrigin();
            if(unpegBottom)
            {
                retMaterialOrigin->y -= top - bottom;
            }
        }
    }
    else
    {
        bool const stretchMiddle = side.isFlagged(SDF_MIDDLE_STRETCH);
        Surface const *surface = &side.surface(section);
        Plane const *ffloor = &frontSec->floor();
        Plane const *fceil  = &frontSec->ceiling();
        Plane const *bfloor = &backSec->floor();
        Plane const *bceil  = &backSec->ceiling();

        switch(section)
        {
        case Line::Side::Top:
            // Can't go over front ceiling (would induce geometry flaws).
            if(bceil->visHeight() < ffloor->visHeight())
                bottom = ffloor->visHeight();
            else
                bottom = bceil->visHeight();
            top = fceil->visHeight();

            if(retMaterialOrigin)
            {
                *retMaterialOrigin = surface->visMaterialOrigin();
                if(!unpegTop)
                {
                    // Align with normal middle texture.
                    retMaterialOrigin->y -= fceil->visHeight() - bceil->visHeight();
                }
            }
            break;

        case Line::Side::Bottom: {
            bool const raiseToBackFloor = (fceil->surface().hasSkyMaskedMaterial() && bceil->surface().hasSkyMaskedMaterial() &&
                                           fceil->visHeight() < bceil->visHeight() &&
                                           bfloor->visHeight() > fceil->visHeight());
            coord_t t = bfloor->visHeight();

            bottom = ffloor->visHeight();
            // Can't go over the back ceiling, would induce polygon flaws.
            if(bfloor->visHeight() > bceil->visHeight())
                t = bceil->visHeight();

            // Can't go over front ceiling, would induce polygon flaws.
            // In the special case of a sky masked upper we must extend the bottom
            // section up to the height of the back floor.
            if(t > fceil->visHeight() && !raiseToBackFloor)
                t = fceil->visHeight();
            top = t;

            if(retMaterialOrigin)
            {
                *retMaterialOrigin = surface->visMaterialOrigin();
                if(bfloor->visHeight() > fceil->visHeight())
                {
                    retMaterialOrigin->y -= (raiseToBackFloor? t : fceil->visHeight()) - bfloor->visHeight();
                }

                if(unpegBottom)
                {
                    // Align with normal middle texture.
                    retMaterialOrigin->y += (raiseToBackFloor? t : fceil->visHeight()) - bfloor->visHeight();
                }
            }
            break; }

        case Line::Side::Middle:
            if(!side.line().isSelfReferencing())
            {
                bottom = de::max(bfloor->visHeight(), ffloor->visHeight());
                top    = de::min(bceil->visHeight(),  fceil->visHeight());
            }
            else
            {
                bottom = ffloor->visHeight();
                top    = bceil->visHeight();
            }

            if(retMaterialOrigin)
            {
                retMaterialOrigin->x = surface->visMaterialOrigin().x;
                retMaterialOrigin->y = 0;
            }

            if(surface->hasMaterial() && !stretchMiddle)
            {
#ifdef __CLIENT__
                bool const clipBottom = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial());
                bool const clipTop    = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && fceil->surface().hasSkyMaskedMaterial()  && bceil->surface().hasSkyMaskedMaterial());
#else
                bool const clipBottom = !(ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial());
                bool const clipTop    = !(fceil->surface().hasSkyMaskedMaterial()  && bceil->surface().hasSkyMaskedMaterial());
#endif

                HEdge &hedge = *side.leftHEdge();

                coord_t openBottom, openTop;
                if(!side.line().isSelfReferencing())
                {
                    openBottom = bottom;
                    openTop    = top;
                }
                else
                {
                    openBottom = hedge.sector().floor().visHeight();
                    openTop    = hedge.sector().ceiling().visHeight();
                }

                int const matHeight      = surface->material().height();
                coord_t const matYOffset = surface->visMaterialOrigin().y;

                if(openTop > openBottom)
                {
                    if(unpegBottom)
                    {
                        bottom += matYOffset;
                        top = bottom + matHeight;
                    }
                    else
                    {
                        top += matYOffset;
                        bottom = top - matHeight;
                    }

                    if(retMaterialOrigin && top > openTop)
                    {
                        retMaterialOrigin->y = top - openTop;
                    }

                    // Clip it?
                    if(clipTop || clipBottom)
                    {
                        if(clipBottom && bottom < openBottom)
                            bottom = openBottom;

                        if(clipTop && top > openTop)
                            top = openTop;
                    }

                    if(retMaterialOrigin && !clipTop)
                    {
                        retMaterialOrigin->y = 0;
                    }
                }
            }
            break;
        }
    }

    if(retBottom) *retBottom = bottom;
    if(retTop)    *retTop    = top;
}

coord_t R_OpenRange(Line::Side const &side, Sector const *frontSec,
    Sector const *backSec, coord_t *retBottom, coord_t *retTop)
{
    DENG_UNUSED(side); // Don't remove (present for API symmetry) -ds
    DENG_ASSERT(frontSec != 0);

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

coord_t R_VisOpenRange(Line::Side const &side, Sector const *frontSec,
    Sector const *backSec, coord_t *retBottom, coord_t *retTop)
{
    DENG_UNUSED(side); // Don't remove (present for API symmetry) -ds
    DENG_ASSERT(frontSec != 0);

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

bool R_SideBackClosed(Line::Side const &side, bool ignoreOpacity)
{
    if(!side.hasSections()) return false;
    if(!side.hasSector()) return false;
    if(side.line().isSelfReferencing()) return false; // Never.

    if(!side.line().definesPolyobj())
    {
        HEdge const &hedge = *side.leftHEdge();
        if(!hedge.twin().hasBspLeaf() || hedge.twin().bspLeaf().isDegenerate()) return true;
    }

    if(!side.back().hasSector()) return true;

    Sector const &frontSec = side.sector();
    Sector const &backSec  = side.back().sector();

    if(backSec.floor().visHeight()   >= backSec.ceiling().visHeight())  return true;
    if(backSec.ceiling().visHeight() <= frontSec.floor().visHeight())   return true;
    if(backSec.floor().visHeight()   >= frontSec.ceiling().visHeight()) return true;

    // Perhaps a middle material completely covers the opening?
    if(side.middle().hasMaterial())
    {
        // Ensure we have up to date info about the material.
        MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

        if(ignoreOpacity || (ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1))
        {
            // Stretched middles always cover the opening.
            if(side.isFlagged(SDF_MIDDLE_STRETCH))
                return true;

            coord_t openRange, openBottom, openTop;
            openRange = R_VisOpenRange(side, &openBottom, &openTop);
            if(ms.height() >= openRange)
            {
                // Possibly; check the placement.
                coord_t bottom, top;
                R_SideSectionCoords(side, Line::Side::Middle, &bottom, &top);
                return (top > bottom && top >= openTop && bottom <= openBottom);
            }
        }
    }

    return false;
}

Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line)
        return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!other->hasBackSector() || !other->isSelfReferencing())
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->frontSectorPtr() == sector ||
               (other->hasBackSector() && other->backSectorPtr() == sector))
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

/**
 * @param side          Line side for which to determine covered opening status.
 * @param frontSec      Sector in front of the wall.
 * @param backSec       Sector behind the wall. Can be @c 0.
 *
 * @return  @c true iff Line::Side @a front has a "middle" Material which completely
 *     covers the open range defined by sectors @a frontSec and @a backSec.
 *
 * @note Anything calling this is likely working at the wrong level (should work with
 * half-edges instead).
 */
static bool middleMaterialCoversOpening(Line::Side const &side)
{
    if(!side.hasSector()) return false; // Never.

    if(!side.hasSections()) return false;
    if(!side.middle().hasMaterial()) return false;

    // Stretched middles always cover the opening.
    if(side.isFlagged(SDF_MIDDLE_STRETCH))
        return true;

    // Ensure we have up to date info about the material.
    MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

    if(ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1)
    {
        coord_t openRange, openBottom, openTop;
        openRange = R_VisOpenRange(side, &openBottom, &openTop);
        if(ms.height() >= openRange)
        {
            // Possibly; check the placement.
            coord_t bottom, top;
            R_SideSectionCoords(side, Line::Side::Middle, &bottom, &top);
            return (top > bottom && top >= openTop && bottom <= openBottom);
        }
    }

    return false;
}

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line) return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!((other->isBspWindow()) && other->frontSectorPtr() != sector) &&
       !other->isSelfReferencing())
    {
        if(!other->hasFrontSector()) return other;
        if(!other->hasBackSector()) return other;

        if(other->frontSector().floor().visHeight() >= sector->ceiling().visHeight() ||
           other->frontSector().ceiling().visHeight() <= sector->floor().visHeight() ||
           other->backSector().floor().visHeight() >= sector->ceiling().visHeight() ||
           other->backSector().ceiling().visHeight() <= sector->floor().visHeight() ||
           other->backSector().ceiling().visHeight() <= other->backSector().floor().visHeight())
            return other;

        // Both front and back MUST be open by this point.

        // Perhaps a middle material completely covers the opening?
        // We should not give away the location of false walls (secrets).
        Line::Side &otherSide = other->side(other->frontSectorPtr() == sector? Line::Front : Line::Back);
        if(middleMaterialCoversOpening(otherSide))
            return other;
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

#endif // __CLIENT__

static void resetAllMapPlaneVisHeights(GameMap &map)
{
    foreach(Sector *sector, map.sectors())
    foreach(Plane *plane, sector->planes())
    {
        plane->resetVisHeight();
    }
}

static void updateAllMapSectors(GameMap &map)
{
    foreach(Sector *sector, map.sectors())
    {
        sector->updateSoundEmitterOrigin();
#ifdef __CLIENT__
        map.updateMissingMaterialsForLinesOfSector(*sector);
        S_MarkSectorReverbDirty(sector);
#endif
    }
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
        theMap->initSkyFix();

        updateAllMapSectors(*theMap);
        resetAllMapPlaneVisHeights(*theMap);

        theMap->initPolyobjs();
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

#ifdef __CLIENT__
        // Recalculate the light range mod matrix.
        Rend_UpdateLightModMatrix();
#endif

        theMap->initPolyobjs();
        P_MapSpawnPlaneParticleGens();

        updateAllMapSectors(*theMap);
        resetAllMapPlaneVisHeights(*theMap);

#ifdef __CLIENT__
        theMap->buildSurfaceLists();

        float startTime = Timer_Seconds();
        Rend_CacheForMap();
        App_Materials().processCacheQueue();
        VERBOSE( Con_Message("Precaching took %.2f seconds.", Timer_Seconds() - startTime) )
#endif

        S_SetupForChangedMap();

        // Map setup has been completed.

        // Run any commands specified in Map Info.
        de::Uri mapUri = theMap->uri();
        ded_mapinfo_t *mapInfo = Def_GetMapInfo(reinterpret_cast<uri_s *>(&mapUri));
        if(mapInfo && mapInfo->execute)
        {
            Con_Execute(CMDS_SCRIPT, mapInfo->execute, true, false);
        }

        // Run the special map setup command, which the user may alias to do something useful.
        String cmd = "init-" + mapUri.resolved();
        if(Con_IsValidCommand(cmd.toUtf8().constData()))
        {
            Con_Executef(CMDS_SCRIPT, false, "%s", cmd.toUtf8().constData());
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
                BspLeaf *bspLeaf = theMap->bspLeafAtPoint(ddpl->mo->origin);
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
