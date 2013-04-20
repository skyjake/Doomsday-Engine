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

float rendLightWallAngle      = 1.2f; // Intensity of angle-based wall lighting.
byte rendLightWallAngleSmooth = true;

float rendSkyLight            = .2f; // Intensity factor.
byte rendSkyLightAuto         = true;

boolean firstFrameAfterLoad;
boolean ddMapSetup;

/// Notified when the current map changes.
MapChangeAudience audienceForMapChange;

/**
 * @return  Lineowner for this line for this vertex; otherwise @c 0.
 */
LineOwner *R_GetVtxLineOwner(Vertex const *v, Line const *line)
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

void R_OrderVertices(Line *line, Sector const *sector, Vertex *verts[2])
{
    byte edge = (sector == line->frontSectorPtr()? 0:1);
    verts[0] = &line->vertex(edge);
    verts[1] = &line->vertex(edge^1);
}

bool R_SideSectionCoords(Line::Side const &side, int section,
    Sector const *frontSec, Sector const *backSec,
    coord_t *retBottom, coord_t *retTop, Vector2f *retMaterialOrigin)
{
    Line const &line = side.line();
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
            bottom = de::max(bfloor->visHeight(), ffloor->visHeight());
            top    = de::min(bceil->visHeight(),  fceil->visHeight());

            if(retMaterialOrigin)
            {
                retMaterialOrigin->x = surface->visMaterialOrigin().x;
                retMaterialOrigin->y = 0;
            }

            if(surface->hasMaterial() && !stretchMiddle)
            {
                bool const clipBottom = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && ffloor->surface().hasSkyMaskedMaterial() && bfloor->surface().hasSkyMaskedMaterial());
                bool const clipTop    = !(!(devRendSkyMode || P_IsInVoid(viewPlayer)) && fceil->surface().hasSkyMaskedMaterial()  && bceil->surface().hasSkyMaskedMaterial());

                coord_t const openBottom = bottom;
                coord_t const openTop    = top;
                int const matHeight      = surface->material().height();
                coord_t const matYOffset = surface->visMaterialOrigin()[VY];

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

    return /*is_visible=*/ top > bottom;
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
bool R_MiddleMaterialCoversOpening(Line::Side const &side,
    Sector const *frontSec, Sector const *backSec, bool ignoreOpacity)
{
    if(!frontSec) return false; // Never.

    if(!side.hasSections()) return false;
    if(!side.middle().hasMaterial()) return false;

    // Ensure we have up to date info about the material.
    MaterialSnapshot const &ms = side.middle().material().prepare(Rend_MapSurfaceMaterialSpec());

    if(ignoreOpacity || (ms.isOpaque() && !side.middle().blendMode() && side.middle().opacity() >= 1))
    {
        coord_t openRange, openBottom, openTop;

        // Stretched middles always cover the opening.
        if(side.isFlagged(SDF_MIDDLE_STRETCH))
            return true;

        // Might the material cover the opening?
        openRange = R_VisOpenRange(side, frontSec, backSec, &openBottom, &openTop);
        if(ms.height() >= openRange)
        {
            // Possibly; check the placement.
            coord_t bottom, top;
            if(R_SideSectionCoords(side, Line::Side::Middle, frontSec, backSec,
                                   &bottom, &top))
            {
                return (top >= openTop && bottom <= openBottom);
            }
        }
    }

    return false;
}

/**
 * @param side  Line::Side instance.
 * @param ignoreOpacity  @c true= do not consider Material opacity.
 *
 * @return  @c true if this side is considered "closed" (i.e., there is no opening
 * through which the relative back Sector can be seen). Tests consider all Planes
 * which interface with this and the "middle" Material used on the "this" side.
 */
bool R_SideBackClosed(Line::Side const &side, bool ignoreOpacity)
{
    if(!side.hasSections()) return false;
    if(!side.back().hasSections()) return true;
    if(side.line().isSelfReferencing()) return false; // Never.

    if(side.hasSector() && side.back().hasSector())
    {
        Sector const &frontSec = side.sector();
        Sector const &backSec  = side.back().sector();

        if(backSec.floor().visHeight()   >= backSec.ceiling().visHeight())  return true;
        if(backSec.ceiling().visHeight() <= frontSec.floor().visHeight())   return true;
        if(backSec.floor().visHeight()   >= frontSec.ceiling().visHeight()) return true;
    }

    return R_MiddleMaterialCoversOpening(side, ignoreOpacity);
}

Line *R_FindLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line)
        return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!other->hasBackSections() || !other->isSelfReferencing())
    {
        if(sector) // Must one of the sectors match?
        {
            if(other->frontSectorPtr() == sector ||
               (other->hasBackSections() && other->backSectorPtr() == sector))
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

Line *R_FindSolidLineNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line) return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!((other->isBspWindow()) && other->frontSectorPtr() != sector))
    {
        if(!other->hasFrontSections()) return other;
        if(!other->hasBackSections()) return other;

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
        int side = (other->frontSectorPtr() == sector? 0 : 1);
        if(other->side(side).middle().hasMaterial())
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
                if(!R_MiddleMaterialCoversOpening(other->side(side)))
                    return 0;
            }
        }
    }

    // Not suitable, try the next.
    return R_FindSolidLineNeighbor(sector, line, cown, antiClockwise, diff);
}

Line *R_FindLineBackNeighbor(Sector const *sector, Line const *line,
    LineOwner const *own, bool antiClockwise, binangle_t *diff)
{
    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();

    if(other == line) return 0;

    if(diff) *diff += (antiClockwise? cown->angle() : own->angle());

    if(!other->hasBackSections() || !other->isSelfReferencing() || other->isBspWindow())
    {
        if(!(other->frontSectorPtr() == sector ||
             (other->hasBackSections() && other->backSectorPtr() == sector)))
            return other;
    }

    // Not suitable, try the next.
    return R_FindLineBackNeighbor(sector, line, cown, antiClockwise, diff);
}

Line *R_FindLineAlignNeighbor(Sector const *sec, Line const *line,
    LineOwner const *own, bool antiClockwise, int alignment)
{
    int const SEP = 10;

    LineOwner const *cown = antiClockwise? &own->prev() : &own->next();
    Line *other = &cown->line();
    binangle_t diff;

    if(other == line)
        return 0;

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
    if(!other->hasFrontSections() || !other->hasBackSections())
        return 0;

    // Not suitable, try the next.
    return R_FindLineAlignNeighbor(sec, line, cown, antiClockwise, alignment);
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

static void resetAllMapSurfaceVisMaterialOrigins(GameMap &map)
{
    foreach(Sector *sector, map.sectors())
    foreach(Plane *plane, sector->planes())
    {
        plane->surface().resetVisMaterialOrigin();
    }

    foreach(Line *line, map.lines())
    for(int i = 0; i < 2; ++i)
    {
        if(!line->hasSections(i))
            continue;

        Line::Side &side = line->side(i);
        side.top().resetVisMaterialOrigin();
        side.middle().resetVisMaterialOrigin();
        side.bottom().resetVisMaterialOrigin();
    }
}

#ifdef __CLIENT__

/**
 * Given a side section, look at the neighbouring surfaces and pick the
 * best choice of material used on those surfaces to be applied to "this"
 * surface.
 *
 * Material on back neighbour plane has priority.
 * Non-animated materials are preferred.
 * Sky materials are ignored.
 */
static Material *chooseFixMaterial(Line::Side &side, int section)
{
    Material *choice1 = 0, *choice2 = 0;

    Sector *frontSec = side.sectorPtr();
    Sector *backSec  = side.back().hasSections()? side.back().sectorPtr() : 0;

    if(backSec)
    {
        // Our first choice is a material in the other sector.
        if(section == Line::Side::Bottom)
        {
            if(frontSec->floor().height() < backSec->floor().height())
            {
                choice1 = backSec->floorSurface().materialPtr();
            }
        }
        else if(section == Line::Side::Top)
        {
            if(frontSec->ceiling().height()  > backSec->ceiling().height())
            {
                choice1 = backSec->ceilingSurface().materialPtr();
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
        Line *other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.lineSideId()),
                                         false /*next clockwise*/);
        if(!other)
            // Try the right neighbor.
            other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.lineSideId()^1),
                                       true /*next anti-clockwise*/);

        if(other)
        {
            if(!other->hasBackSections())
            {
                // Our choice is clear - the middle material.
                choice1 = other->front().middle().materialPtr();
            }
            else
            {
                // Compare the relative heights to decide.
                Line::Side &otherSide = other->side(&other->frontSector() == frontSec? Line::Front : Line::Back);
                Sector &otherSec = other->sector(&other->frontSector() == frontSec? Line::Back : Line::Front);

                if(otherSec.ceiling().height() <= frontSec->floor().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() >= frontSec->ceiling().height())
                    choice1 = otherSide.bottom().materialPtr();
                else if(otherSec.ceiling().height() < frontSec->ceiling().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() > frontSec->floor().height())
                    choice1 = otherSide.bottom().materialPtr();
                // else we'll settle for a plane material.
            }
        }
    }

    // Our second choice is a material from this sector.
    choice2 = frontSec->planeSurface(section == Line::Side::Bottom? Plane::Floor : Plane::Ceiling).materialPtr();

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

static void addMissingMaterial(Line::Side &side, int section)
{
    // A material must be missing for this test to apply.
    Surface &surface = side.surface(section);
    if(surface.hasMaterial()) return;

    // Look for a suitable replacement.
    surface.setMaterial(chooseFixMaterial(side, section), true/* is missing fix */);

    // During map load we log missing materials.
    if(ddMapSetup && verbose)
    {
        String path = surface.hasMaterial()? surface.material().manifest().composeUri().asText() : "<null>";

        LOG_WARNING("%s of Line #%u is missing a material for the %s section.\n"
                    "  %s was chosen to complete the definition.")
            << (side.isBack()? "Back" : "Front") << side.line().origIndex() - 1
            << (section == Line::Side::Middle? "middle" : section == Line::Side::Top? "top" : "bottom")
            << path;
    }
}

void R_UpdateMissingMaterialsForLinesOfSector(Sector const &sec)
{
    foreach(Line *line, sec.lines())
    {
        // Self-referencing lines don't need fixing.
        if(line->isSelfReferencing()) continue;

        // Do not fix BSP "window" lines.
        if(!line->hasFrontSections() || (!line->hasBackSections() && line->hasBackSector()))
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
                addMissingMaterial(line->front(), Line::Side::Bottom);
            }
            else if(frontSec.floor().height() > backSec.floor().height())
            {
                addMissingMaterial(line->back(), Line::Side::Bottom);
            }

            // A potential top section fix?
            if(backSec.ceiling().height() < frontSec.ceiling().height())
            {
                addMissingMaterial(line->front(), Line::Side::Top);
            }
            else if(backSec.ceiling().height() > frontSec.ceiling().height())
            {
                addMissingMaterial(line->back(), Line::Side::Top);
            }
        }
        else
        {
            // A potential middle section fix.
            addMissingMaterial(line->front(), Line::Side::Middle);
        }
    }
}
#endif // __CLIENT__

static void updateAllMapSectors(GameMap &map)
{
    foreach(Sector *sector, map.sectors())
    {
        sector->updateSoundEmitterOrigin();
#ifdef __CLIENT__
        R_UpdateMissingMaterialsForLinesOfSector(*sector);
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
        resetAllMapSurfaceVisMaterialOrigins(*theMap);

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

        // Recalculate the light range mod matrix.
        Rend_CalcLightModRange();

        theMap->initPolyobjs();
        P_MapSpawnPlaneParticleGens();

        updateAllMapSectors(*theMap);
        resetAllMapPlaneVisHeights(*theMap);
        resetAllMapSurfaceVisMaterialOrigins(*theMap);

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

void R_ClearSectorFlags()
{
    if(!theMap) return;

    foreach(Sector *sector, theMap->sectors())
    {
        // Clear all flags that can be cleared before each frame.
        sector->_frameFlags &= ~SIF_FRAME_CLEAR;
    }
}

float R_GlowStrength(Plane const *plane)
{
#ifdef __CLIENT__
    Surface const &surface = plane->surface();
    if(surface.hasMaterial() && glowFactor > .0001f)
    {
        if(surface.material().isDrawable() && !surface.hasSkyMaskedMaterial())
        {
            MaterialSnapshot const &ms = surface.material().prepare(Rend_MapSurfaceMaterialSpec());

            float glowStrength = ms.glowStrength() * glowFactor; // Global scale factor.
            return glowStrength;
        }
    }
#else
    DENG_UNUSED(plane);
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
        if(sec->planeSurface(n).hasSkyMaskedMaterial())
            sectorContainsSkySurfaces = true;
        else
            n++;
    } while(!sectorContainsSkySurfaces && n < sec->planeCount());

    return sectorContainsSkySurfaces;
}

void R_UpdateSector(Sector &sector, bool forceUpdate)
{
#ifdef __CLIENT__
    // Check if there are any lightlevel or color changes.
    if(forceUpdate ||
       (sector._lightLevel != sector._oldLightLevel ||
        sector._lightColor[0] != sector._oldLightColor[0] ||
        sector._lightColor[1] != sector._oldLightColor[1] ||
        sector._lightColor[2] != sector._oldLightColor[2]))
    {
        sector._frameFlags |= SIF_LIGHT_CHANGED;
        sector._oldLightLevel = sector._lightLevel;
        sector._oldLightColor = sector._lightColor;

        LG_SectorChanged(&sector);
    }
    else
    {
        sector._frameFlags &= ~SIF_LIGHT_CHANGED;
    }
#endif
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
    if(distToViewer > 0 && rendLightDistanceAttenuation > 0)
    {
        float real = lightLevel -
            (distToViewer - 32) / rendLightDistanceAttenuation *
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

Vector3f const &R_GetSectorLightColor(Sector const &sector)
{
    static Vector3f skyLightColor;
    static Vector3f oldSkyAmbientColor(-1.f, -1.f, -1.f);
    static float oldRendSkyLight = -1;

    if(rendSkyLight > .001f && R_SectorContainsSkySurfaces(&sector))
    {
        ColorRawf const *ambientColor = Sky_AmbientColor();

        if(rendSkyLight != oldRendSkyLight ||
           !INRANGE_OF(ambientColor->red,   oldSkyAmbientColor.x, .001f) ||
           !INRANGE_OF(ambientColor->green, oldSkyAmbientColor.y, .001f) ||
           !INRANGE_OF(ambientColor->blue,  oldSkyAmbientColor.z, .001f))
        {
            skyLightColor = Vector3f(ambientColor->rgb);
            R_AmplifyColor(skyLightColor);

            // Apply the intensity factor cvar.
            for(int i = 0; i < 3; ++i)
            {
                skyLightColor[i] = skyLightColor[i] + (1 - rendSkyLight) * (1.f - skyLightColor[i]);
            }

            // When the sky light color changes we must update the lightgrid.
            LG_MarkAllForUpdate();
            oldSkyAmbientColor = Vector3f(ambientColor->rgb);
        }

        oldRendSkyLight = rendSkyLight;
        return skyLightColor;
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector.lightColor();
}

#endif // __CLIENT__

coord_t R_SkyCapZ(BspLeaf *bspLeaf, int skyCap)
{
    DENG_ASSERT(bspLeaf);
    Plane::Type const plane = (skyCap & SKYCAP_UPPER)? Plane::Ceiling : Plane::Floor;
    if(!bspLeaf->hasSector() || !P_IsInVoid(viewPlayer))
        return theMap->skyFix(plane == Plane::Ceiling);
    return bspLeaf->sector().plane(plane).visHeight();
}
