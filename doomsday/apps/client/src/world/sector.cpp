/** @file sector.h  World map sector.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#include "world/sector.h"

#include <QList>
#include <QtAlgorithms>
#include <de/vector1.h>
#include <de/Log>
#include <doomsday/console/cmd.h>
#include "dd_main.h"  // App_World()

#include "world/map.h"
#include "world/p_object.h"
#include "Line"
#include "Plane"
#include "Surface"
#include "SectorCluster"

using namespace de;
using namespace world;

/**
 * Update the sound emitter origin of the plane. This point is determined according to the
 * center point of the parent Sector on the XY plane and Z the height of the plane itself.
 */
static void updateSoundEmitterOrigin(Plane &plane)
{
    SoundEmitter &emitter = plane.soundEmitter();

    emitter.origin[0] = plane.sector().soundEmitter().origin[0];
    emitter.origin[1] = plane.sector().soundEmitter().origin[1];
    emitter.origin[2] = plane.height();
}

/**
 * Update the sound emitter origin of the specified surface section. This point is determined
 * according to the center point of the owning line and the current @em sharp heights of
 * the sector on "this" side of the line.
 */
static void updateSoundEmitterOrigin(LineSide &side, dint sectionId)
{
    if(!side.hasSections()) return;

    SoundEmitter &emitter = side.soundEmitter(sectionId);

    Vector2d lineCenter = side.line().center();
    emitter.origin[0] = lineCenter.x;
    emitter.origin[1] = lineCenter.y;

    DENG2_ASSERT(side.hasSector());
    ddouble const ffloor = side.sector().floor().height();
    ddouble const fceil  = side.sector().ceiling().height();

    /// @todo fixme what if considered one-sided?
    switch(sectionId)
    {
    case LineSide::Middle:
        if(!side.back().hasSections() || side.line().isSelfReferencing())
        {
            emitter.origin[2] = (ffloor + fceil) / 2;
        }
        else
        {
            emitter.origin[2] = (  de::max(ffloor, side.back().sector().floor  ().height())
                + de::min(fceil,  side.back().sector().ceiling().height())) / 2;
        }
        break;

    case LineSide::Bottom:
        if(!side.back().hasSections() || side.line().isSelfReferencing() ||
            side.back().sector().floor().height() <= ffloor)
        {
            emitter.origin[2] = ffloor;
        }
        else
        {
            emitter.origin[2] = (de::min(side.back().sector().floor().height(), fceil) + ffloor) / 2;
        }
        break;

    case LineSide::Top:
        if(!side.back().hasSections() || side.line().isSelfReferencing() ||
            side.back().sector().ceiling().height() >= fceil)
        {
            emitter.origin[2] = fceil;
        }
        else
        {
            emitter.origin[2] = (de::max(side.back().sector().ceiling().height(), ffloor) + fceil) / 2;
        }
        break;
    }
}

static void updateAllSoundEmitterOrigins(Line::Side &side)
{
    if(!side.hasSections()) return;

    updateSoundEmitterOrigin(side, LineSide::Middle);
    updateSoundEmitterOrigin(side, LineSide::Bottom);
    updateSoundEmitterOrigin(side, LineSide::Top);
}

DENG2_PIMPL(Sector)
, DENG2_OBSERVES(Plane, HeightChange)
{
    /**
     * All planes of the sector (owned).
     */
    struct Planes : public QList<Plane *>
    {
        ~Planes() { qDeleteAll(*this); }
    } planes;     

    ThinkerT<SoundEmitter> emitter;   ///< Head of the sound emitter chain.

    mobj_t *mobjList = nullptr;       ///< All mobjs "in" the sector (not owned).
    QList<LineSide *> sides;          ///< All referencing line sides (not owned).

    dfloat lightLevel = 0;            ///< Ambient light level.
    Vector3f lightColor;              ///< Ambient light color.

    std::unique_ptr<AABoxd> bounds;   ///< Bounding box for the whole sector (all clusters).

    dint validCount = 0;

    Instance(Public *i) : Base(i) {}

    /**
     * Calculate the minimum bounding rectangle which encompass the BSP leaf geometry of
     * all the clusters attributed to the given @a sector.
     */
    static AABoxd findBounds(Sector const &sector)
    {
        bool inited = false;
        AABoxd bounds;
        sector.map().forAllClustersOfSector(const_cast<Sector &>(sector), [&bounds, &inited] (SectorCluster &cluster)
        {
            if(inited)
            {
                V2d_UniteBox(bounds.arvec2, cluster.aaBox().arvec2);
            }
            else
            {
                bounds = cluster.aaBox();
                inited = true;
            }
            return LoopContinue;
        });
        return bounds;
    }

    AABoxd &aaBox()
    {
        // Time for an update?
        if(!bounds)
        {
            bounds.reset(new AABoxd(findBounds(self)));
            updateSoundEmitterOriginXY();
        }
        return *bounds;
    }

    void updateSoundEmitterOriginXY()
    {
        emitter->origin[0] = (aaBox().minX + aaBox().maxX) / 2;
        emitter->origin[1] = (aaBox().minY + aaBox().maxY) / 2;
    }

    void updateSoundEmitterOriginZ()
    {
        emitter->origin[2] = (self.floor().height() + self.ceiling().height()) / 2;
    }

    void updateSoundEmitterOrigin()
    {
        updateSoundEmitterOriginXY();
        updateSoundEmitterOriginZ();
    }

#ifdef __CLIENT__

    void fixMissingMaterials()
    {
        for(LineSide *side : sides)
        {
            side->fixMissingMaterials();
            side->back().fixMissingMaterials();
        }
    }

#endif  // __CLIENT__

    void planeHeightChanged(Plane &)
    {
        self.updateSoundEmitterOrigins();
#ifdef __CLIENT__
        fixMissingMaterials();
#endif
    }

    DENG2_PIMPL_AUDIENCE(LightLevelChange)
    DENG2_PIMPL_AUDIENCE(LightColorChange)
};

DENG2_AUDIENCE_METHOD(Sector, LightLevelChange)
DENG2_AUDIENCE_METHOD(Sector, LightColorChange)

Sector::Sector(dfloat lightLevel, Vector3f const &lightColor)
    : MapElement(DMU_SECTOR)
    , d(new Instance(this))
{
    d->lightLevel = de::clamp(0.f, lightLevel, 1.f);
    d->lightColor = lightColor.min(Vector3f(1, 1, 1)).max(Vector3f(0, 0, 0));
}

String Sector::describe() const
{
    return "Sector";
}

/**
 * Two links to update:
 * 1) The link to the mobj from the previous node (sprev, always set) will
 *    be modified to point to the node following it.
 * 2) If there is a node following the mobj, set its sprev pointer to point
 *    to the pointer that points back to it (the mobj's sprev, just modified).
 */
void Sector::unlink(mobj_t *mobj)
{
    if(!mobj || !Mobj_IsSectorLinked(mobj))
        return;

    if((*mobj->sPrev = mobj->sNext))
        mobj->sNext->sPrev = mobj->sPrev;

    // Not linked any more.
    mobj->sNext = nullptr;
    mobj->sPrev = nullptr;

    // Ensure this has been completely unlinked.
#ifdef DENG2_DEBUG
    for(mobj_t *iter = d->mobjList; iter; iter = iter->sNext)
    {
        DENG2_ASSERT(iter != mobj);
    }
#endif
}

void Sector::link(mobj_t *mobj)
{
    if(!mobj) return;

    // Ensure this isn't already linked.
#ifdef DENG2_DEBUG
    for(mobj_t *iter = d->mobjList; iter; iter = iter->sNext)
    {
        DENG2_ASSERT(iter != mobj);
    }
#endif

    // Prev pointers point to the pointer that points back to us.
    // (Which practically disallows traversing the list backwards.)

    if((mobj->sNext = d->mobjList))
        mobj->sNext->sPrev = &mobj->sNext;

    *(mobj->sPrev = &d->mobjList) = mobj;
}

struct mobj_s *Sector::firstMobj() const
{
    return d->mobjList;
}

bool Sector::hasSkyMaskPlane() const
{
    for(Plane *plane : d->planes)
    {
        if(plane->surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

dint Sector::planeCount() const
{
    return d->planes.count();
}

Plane &Sector::plane(dint planeIndex)
{
    if(planeIndex >= 0 && planeIndex < d->planes.count())
    {
        return *d->planes.at(planeIndex);
    }
    /// @throw MissingPlaneError The referenced plane does not exist.
    throw MissingPlaneError("Sector::plane", QString("Missing plane %1").arg(planeIndex));
}

Plane const &Sector::plane(dint planeIndex) const
{
    return const_cast<Sector *>(this)->plane(planeIndex);
}

LoopResult Sector::forAllPlanes(std::function<LoopResult (Plane &)> func) const
{
    for(Plane *plane : d->planes)
    {
        if(auto result = func(*plane)) return result;
    }
    return LoopContinue;
}

Plane *Sector::addPlane(Vector3f const &normal, ddouble height)
{
    auto *plane = new Plane(*this, normal, height);

    plane->setIndexInSector(d->planes.count());
    d->planes.append(plane);

    updateSoundEmitterOrigin(*plane);

    if(plane->isSectorFloor() || plane->isSectorCeiling())
    {
        // We want notification of height changes in order to update sound emitters.
        plane->audienceForHeightChange() += d;
    }

    // Once both floor and ceiling are known we can determine the height our sound emitter.
    /// @todo fixme: Assume planes are defined in order.
    if(planeCount() == 2)
    {
        d->updateSoundEmitterOriginZ();
    }

    return plane;
}

dint Sector::sideCount() const
{
    return d->sides.count();
}

LoopResult Sector::forAllSides(std::function<LoopResult (LineSide &)> func) const
{
    for(LineSide *side : d->sides)
    {
        if(auto result = func(*side)) return result;
    }
    return LoopContinue;
}

void Sector::buildSides()
{
    d->sides.clear();

    dint count = 0;
    map().forAllLines([this, &count] (Line &line)
    {
        if(line.frontSectorPtr() == this || line.backSectorPtr()  == this)
        {
            count += 1;
        }
        return LoopContinue;
    });

    if(!count) return;

    d->sides.reserve(count);
    map().forAllLines([this] (Line &line)
    {
        if(line.frontSectorPtr() == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line.front());
        }
        else if(line.backSectorPtr()  == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line.back());
        }
        return LoopContinue;
    });
}

SoundEmitter &Sector::soundEmitter()
{
    // Emitter origin depends on the axis-aligned bounding box.
    d->aaBox();
    return d->emitter;
}

SoundEmitter const &Sector::soundEmitter() const
{
    return const_cast<SoundEmitter const &>(const_cast<Sector &>(*this).soundEmitter());
}

static void linkSoundEmitter(SoundEmitter &root, SoundEmitter &newEmitter)
{
    // The sector's base is always root of the chain, so link the other after it.
    newEmitter.thinker.prev = &root.thinker;
    newEmitter.thinker.next = root.thinker.next;
    if(newEmitter.thinker.next)
        newEmitter.thinker.next->prev = &newEmitter.thinker;
    root.thinker.next = &newEmitter.thinker;
}

void Sector::chainSoundEmitters()
{
    SoundEmitter &root = d->emitter;

    // Clear the root of the emitter chain.
    root.thinker.next = root.thinker.prev = nullptr;

    // Link plane surface emitters:
    for(Plane *plane : d->planes)
    {
        linkSoundEmitter(root, plane->soundEmitter());
    }

    // Link wall surface emitters:
    for(LineSide *side : d->sides)
    {
        if(side->hasSections())
        {
            linkSoundEmitter(root, side->middleSoundEmitter());
            linkSoundEmitter(root, side->bottomSoundEmitter());
            linkSoundEmitter(root, side->topSoundEmitter());
        }
        if(side->line().isSelfReferencing() && side->back().hasSections())
        {
            LineSide &back = side->back();
            linkSoundEmitter(root, back.middleSoundEmitter());
            linkSoundEmitter(root, back.bottomSoundEmitter());
            linkSoundEmitter(root, back.topSoundEmitter());
        }
    }
}

void Sector::updateSoundEmitterOrigins()
{
    d->updateSoundEmitterOrigin();

    for(Plane *plane : d->planes)
    {
        updateSoundEmitterOrigin(*plane);
    }

    for(LineSide *side : d->sides)
    {
        updateAllSoundEmitterOrigins(*side);
        updateAllSoundEmitterOrigins(side->back());
    }
}

dfloat Sector::lightLevel() const
{
    return d->lightLevel;
}

void Sector::setLightLevel(dfloat newLightLevel)
{
    newLightLevel = de::clamp(0.f, newLightLevel, 1.f);
    if(!de::fequal(d->lightLevel, newLightLevel))
    {
        d->lightLevel = newLightLevel;
        DENG2_FOR_AUDIENCE2(LightLevelChange, i) i->sectorLightLevelChanged(*this);
    }
}

Vector3f const &Sector::lightColor() const
{
    return d->lightColor;
}

void Sector::setLightColor(Vector3f const &newLightColor)
{
    auto newColorClamped = newLightColor.min(Vector3f(1, 1, 1)).max(Vector3f(0, 0, 0));
    if(d->lightColor != newColorClamped)
    {
        d->lightColor = newColorClamped;
        DENG2_FOR_AUDIENCE2(LightColorChange, i) i->sectorLightColorChanged(*this);
    }
}

dint Sector::validCount() const
{
    return d->validCount;
}

void Sector::setValidCount(dint newValidCount)
{
    d->validCount = newValidCount;
}

AABoxd const &Sector::aaBox() const
{
    return d->aaBox();
}

dint Sector::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_LIGHT_LEVEL:
        args.setValue(DMT_SECTOR_LIGHTLEVEL, &d->lightLevel, 0);
        break;
    case DMU_COLOR:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.x, 0);
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.y, 1);
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.z, 2);
        break;
    case DMU_COLOR_RED:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.x, 0);
        break;
    case DMU_COLOR_GREEN:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.y, 0);
        break;
    case DMU_COLOR_BLUE:
        args.setValue(DMT_SECTOR_RGB, &d->lightColor.z, 0);
        break;
    case DMU_EMITTER: {
        SoundEmitter const *emitterAdr = d->emitter;
        args.setValue(DMT_SECTOR_EMITTER, &emitterAdr, 0);
        break; }
    case DMT_MOBJS:
        args.setValue(DMT_SECTOR_MOBJLIST, &d->mobjList, 0);
        break;
    case DMU_VALID_COUNT:
        args.setValue(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    case DMU_FLOOR_PLANE: {
        Plane *pln = d->planes.at(Floor);
        args.setValue(DMT_SECTOR_FLOORPLANE, &pln, 0);
        break; }
    case DMU_CEILING_PLANE: {
        Plane *pln = d->planes.at(Ceiling);
        args.setValue(DMT_SECTOR_CEILINGPLANE, &pln, 0);
        break; }
    default:
        return DmuObject::property(args);
    }

    return false;  // Continue iteration.
}

dint Sector::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_COLOR: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.x, 0);
        args.value(DMT_SECTOR_RGB, &newColor.y, 1);
        args.value(DMT_SECTOR_RGB, &newColor.z, 2);
        setLightColor(newColor);
        break; }
    case DMU_COLOR_RED: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.x, 0);
        setLightColor(newColor);
        break; }
    case DMU_COLOR_GREEN: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.y, 0);
        setLightColor(newColor);
        break; }
    case DMU_COLOR_BLUE: {
        Vector3f newColor = d->lightColor;
        args.value(DMT_SECTOR_RGB, &newColor.z, 0);
        setLightColor(newColor);
        break; }
    case DMU_LIGHT_LEVEL: {
        dfloat newLightLevel;
        args.value(DMT_SECTOR_LIGHTLEVEL, &newLightLevel, 0);
        setLightLevel(newLightLevel);
        break; }
    case DMU_VALID_COUNT:
        args.value(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    default:
        return DmuObject::setProperty(args);
    }

    return false;  // Continue iteration.
}

D_CMD(InspectSector)
{
    DENG2_UNUSED(src);

    LOG_AS("inspectsector (Cmd)");

    if(argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (sector-id)") << argv[0];
        return true;
    }

    if(!App_World().hasMap())
    {
        LOG_SCR_ERROR("No map is currently loaded");
        return false;
    }

    // Find the sector.
    dint const index  = String(argv[1]).toInt();
    Sector const *sec = App_World().map().sectorPtr(index);
    if(!sec)
    {
        LOG_SCR_ERROR("Sector #%i not found") << index;
        return false;
    }

    LOG_SCR_MSG(_E(b) "Sector %i" _E(.) " [%p]")
            << sec->indexInMap() << sec;
    LOG_SCR_MSG(_E(l)  "Light Level: " _E(.)_E(i) "%f" _E(.)
                _E(l) " Light Color: " _E(.)_E(i) "%s")
            << sec->lightLevel()
            << sec->lightColor().asText();
    sec->forAllPlanes([](Plane &plane)
    {
        LOG_SCR_MSG("") << plane.description();
        return LoopContinue;
    });

    return true;
}

void Sector::consoleRegister()  // static
{
    C_CMD("inspectsector", "i", InspectSector);
}
