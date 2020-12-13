/** @file sector.h  World map sector.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/map.h"
#include "world/p_object.h"
#include "ConvexSubspace"
#include "Line"
#include "Plane"
#include "Subsector"
#include "Surface"

#include "dd_main.h"  // App_World()

#include <doomsday/console/cmd.h>
#include <de/LogBuffer>
#include <de/vector1.h>
#include <de/Rectangle>
#include <QList>
#include <QtAlgorithms>

using namespace de;
using namespace world;

static Sector::SubsectorConstructor subsectorConstructor;

DENG2_PIMPL(Sector)
, DENG2_OBSERVES(Plane, HeightChange)
{
    /**
     * POD: Metrics describing the geometry of the sector (the subsectors).
     */
    struct GeomData
    {
        AABoxd bounds;              ///< Bounding box for the whole sector (all subsectors).
        ddouble roughArea = 0;      ///< Rough approximation.
    };

    struct MapObjects
    {
        mobj_t *head = nullptr;     ///< The list of map objects.

        /**
         * Returns @c true if the map-object @a mob is linked.
         */
        bool contains(mobj_t const *mob) const
        {
            if (mob)
            {
                for (mobj_t const *it = head; it; it = it->sNext)
                {
                    if (it == mob) return true;
                }
            }
            return false;
        }

        void add(mobj_t *mob)
        {
            if (!mob) return;

            // Ensure this isn't already included.
            DENG2_ASSERT(!contains(mob));

            // Prev pointers point to the pointer that points back to us.
            // (Which practically disallows traversing the list backwards.)
            if ((mob->sNext = head))
            {
                mob->sNext->sPrev = &mob->sNext;
            }
            *(mob->sPrev = &head) = mob;
        }

        /**
         * Two links to update:
         * 1) The link to the mobj from the previous node (sprev, always set) will
         *    be modified to point to the node following it.
         * 2) If there is a node following the mobj, set its sprev pointer to point
         *    to the pointer that points back to it (the mobj's sprev, just modified).
         */
        void remove(mobj_t *mob)
        {
            if (!mob || !Mobj_IsSectorLinked(*mob)) return;

            if ((*mob->sPrev = mob->sNext))
            {
                mob->sNext->sPrev = mob->sPrev;
            }
            // Not linked any more.
            mob->sNext = nullptr;
            mob->sPrev = nullptr;

            // Ensure this has been completely unlinked.
            DENG2_ASSERT(!contains(mob));
        }
    };

    struct Planes : public QVector<Plane *>
    {
        ~Planes() { clear(); }

        void clear() {
            qDeleteAll(*this);
            QVector<Plane *>::clear();
        }
    };

    struct Subsectors : public QVector<Subsector *>
    {
        ~Subsectors() { clear(); }

        void clear() {
            qDeleteAll(*this);
            QVector<Subsector *>::clear();
        }
    };

    Planes planes;                   ///< Planes of the sector.
    MapObjects mapObjects;           ///< All map-objects "in" one of the subsectors (not owned).
    QVector<LineSide *> sides;       ///< All line sides referencing the sector (not owned).
    Subsectors subsectors;           ///< Traversable subsectors of the sector.
    ThinkerT<SoundEmitter> emitter;  ///< Head of the sound emitter chain.
    int visPlaneLinkSector = MapElement::NoIndex;
    int visPlaneLinkBits = 0;
    dfloat lightLevel = 0;           ///< Ambient light level.
    Vector3f lightColor;             ///< Ambient light color.

    std::unique_ptr<GeomData> gdata; ///< Additional geometry info/metrics (cache).

    dint validCount = 0;             ///< Used by legacy algorithms to prevent repeated processing.

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        // Ensure planes are cleared first (subsectors may include mappings).
        planes.clear();

        delete [] self()._lookupPlanes;
    }

    /**
     * Returns the additional geometry info/metrics from the cache.
     */
    GeomData &geom()
    {
        if (!gdata)
        {
            // Time to prepare this info.
            std::unique_ptr<GeomData> gd(new GeomData);
            gd->bounds    = findBounds();
            gd->roughArea = findRoughArea();
            gdata.reset(gd.get());
            gd.release();

            // As the bounds are now known; update the origin of the primary SoundEmitter.
            emitter->origin[0] = (gdata->bounds.minX + gdata->bounds.maxX) / 2;
            emitter->origin[1] = (gdata->bounds.minY + gdata->bounds.maxY) / 2;
        }
        return *gdata;
    }

    /**
     * Calculate the minimum bounding rectangle containing all the subsector geometries.
     */
    AABoxd findBounds() const
    {
        bool inited = false;
        AABoxd bounds;
        for (Subsector const *subsec : subsectors)
        {
            if (inited)
            {
                V2d_UniteBox(bounds.arvec2, subsec->bounds().arvec2);
            }
            else
            {
                bounds = subsec->bounds();
                inited = true;
            }
        }
        return bounds;
    }

    /**
     * Approximate the total area of all the subsector geometries.
     */
    ddouble findRoughArea() const
    {
        ddouble roughArea = 0;
        for (Subsector const *subsec : subsectors)
        {
            roughArea += subsec->roughArea();
        }
        return roughArea;
    }

    void updateEmitterOriginZ()
    {
        emitter->origin[2] = (self().floor().height() + self().ceiling().height()) / 2;
    }

    void updateSideEmitterOrigins()
    {
        for (LineSide *side : sides)
        {
            side->updateAllSoundEmitterOrigins();
            side->back().updateAllSoundEmitterOrigins();
        }
    }

    void updateAllEmitterOrigins()
    {
        updateEmitterOriginZ();
        updateSideEmitterOrigins();
    }

    void planeHeightChanged(Plane &)
    {
        updateAllEmitterOrigins();
    }

    void updatePlanesLookup()
    {
        delete [] self()._lookupPlanes;

        self()._lookupPlanes = new Plane *[planes.size()];
        Plane **ptr = self()._lookupPlanes;
        for (Plane *p : planes)
        {
            *ptr++ = p;
        }
    }

    DENG2_PIMPL_AUDIENCE(LightLevelChange)
    DENG2_PIMPL_AUDIENCE(LightColorChange)
};

DENG2_AUDIENCE_METHOD(Sector, LightLevelChange)
DENG2_AUDIENCE_METHOD(Sector, LightColorChange)

Sector::Sector(dfloat lightLevel, Vector3f const &lightColor)
    : MapElement(DMU_SECTOR)
    , d(new Impl(this))
    , _lookupPlanes(nullptr)
{
    d->lightLevel = de::clamp(0.f, lightLevel, 1.f);
    d->lightColor = lightColor.min(Vector3f(1, 1, 1)).max(Vector3f(0, 0, 0));
}

void Sector::unlink(mobj_t *mob)
{
    d->mapObjects.remove(mob);
}

void Sector::link(mobj_t *mob)
{
    d->mapObjects.add(mob);
}

struct mobj_s *Sector::firstMobj() const
{
    return d->mapObjects.head;
}

bool Sector::hasSkyMaskPlane() const
{
    for (Plane *plane : d->planes)
    {
        if (plane->surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

dint Sector::planeCount() const
{
    return d->planes.count();
}

LoopResult Sector::forAllPlanes(std::function<LoopResult (Plane &)> func)
{
    for (Plane *plane : d->planes)
    {
        if(auto result = func(*plane)) return result;
    }
    return LoopContinue;
}

LoopResult Sector::forAllPlanes(std::function<LoopResult (Plane const &)> func) const
{
    for (Plane const *plane : d->planes)
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
    d->updatePlanesLookup();

    if (plane->isSectorFloor() || plane->isSectorCeiling())
    {
        // We want notification of height changes so that we can update sound emitter
        // origins of all the dependent surfaces.
        plane->audienceForHeightChange() += d;
    }

    // Once both floor and ceiling are known we can determine the z-height origin
    // of our sound emitter.
    /// @todo fixme: Assume planes are defined in order.
    if (planeCount() == 2)
    {
        d->updateEmitterOriginZ();
    }

    return plane;
}

void Sector::setVisPlaneLinks(int sectorArchiveIndex, int planeBits)
{
    d->visPlaneLinkSector = sectorArchiveIndex;
    d->visPlaneLinkBits   = planeBits;
}

int Sector::visPlaneLinkTargetSector() const
{
    return d->visPlaneLinkSector;
}

bool Sector::isVisPlaneLinked(int planeIndex) const
{
    return (d->visPlaneLinkBits & (1 << planeIndex)) != 0;
}

int Sector::visPlaneBits() const
{
    return d->visPlaneLinkBits;
}

bool Sector::hasSubsectors() const
{
    return !d->subsectors.isEmpty();
}

dint Sector::subsectorCount() const
{
    return d->subsectors.count();
}

Subsector &Sector::subsector(int index) const
{
    DENG2_ASSERT(index >= 0 && index < d->subsectors.count());
    return *d->subsectors.at(index);
}

LoopResult Sector::forAllSubsectors(const std::function<LoopResult(Subsector &)> &callback) const
{
    for (Subsector *subsec : d->subsectors)
    {
        if (auto result = callback(*subsec)) return result;
    }
    return LoopContinue;
}

Subsector *Sector::addSubsector(QVector<ConvexSubspace *> const &subspaces)
{
    DENG2_ASSERT(subsectorConstructor);
    /// @todo Add/move debug logic for ensuring the set is valid here. -ds
    std::unique_ptr<Subsector> subsec(subsectorConstructor(subspaces));
    d->subsectors << subsec.get();
    LOG_MAP_XVERBOSE("New Subsector %s (sector-%s)", subsec->id().asText() << indexInMap());
    return subsec.release();
}

dint Sector::sideCount() const
{
    return d->sides.count();
}

LoopResult Sector::forAllSides(std::function<LoopResult (LineSide &)> func) const
{
    for (LineSide *side : d->sides)
    {
        if (auto result = func(*side)) return result;
    }
    return LoopContinue;
}

void Sector::buildSides()
{
    d->sides.clear();

    dint count = 0;
    map().forAllLines([this, &count] (Line &line)
    {
        if (line.front().sectorPtr() == this || line.back().sectorPtr() == this)
        {
            count += 1;
        }
        return LoopContinue;
    });

    if (!count) return;

    d->sides.reserve(count);

    map().forAllLines([this] (Line &line)
    {
        if (line.front().sectorPtr() == this)
        {
            d->sides.append(&line.front()); // Ownership not given.
        }
        else if (line.back().sectorPtr()  == this)
        {
            d->sides.append(&line.back()); // Ownership not given.
        }
        return LoopContinue;
    });

    d->updateAllEmitterOrigins();
}

SoundEmitter &Sector::soundEmitter()
{
    // Emitter origin depends on the axis-aligned bounding box.
    (void) d->geom();
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
    if (newEmitter.thinker.next)
        newEmitter.thinker.next->prev = &newEmitter.thinker;
    root.thinker.next = &newEmitter.thinker;
}

void Sector::chainSoundEmitters()
{
    SoundEmitter &root = d->emitter;

    // Clear the root of the emitter chain.
    root.thinker.next = root.thinker.prev = nullptr;

    // Link emitters for planes.
    for (Plane *plane : d->planes)
    {
        linkSoundEmitter(root, plane->soundEmitter());
    }

    // Link emitters for LineSide sections.
    for (LineSide *side : d->sides)
    {
        if (side->hasSections())
        {
            linkSoundEmitter(root, side->middleSoundEmitter());
            linkSoundEmitter(root, side->bottomSoundEmitter());
            linkSoundEmitter(root, side->topSoundEmitter   ());
        }
        if (side->line().isSelfReferencing() && side->back().hasSections())
        {
            LineSide &back = side->back();
            linkSoundEmitter(root, back.middleSoundEmitter());
            linkSoundEmitter(root, back.bottomSoundEmitter());
            linkSoundEmitter(root, back.topSoundEmitter   ());
        }
    }
}

dfloat Sector::lightLevel() const
{
    return d->lightLevel;
}

void Sector::setLightLevel(dfloat newLightLevel)
{
    newLightLevel = de::clamp(0.f, newLightLevel, 1.f);
    if (!de::fequal(d->lightLevel, newLightLevel))
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
    if (d->lightColor != newColorClamped)
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

AABoxd const &Sector::bounds() const
{
    return d->geom().bounds;
}

#ifdef __CLIENT__
ddouble Sector::roughArea() const
{
    return d->geom().roughArea;
}
#endif

dint Sector::property(DmuArgs &args) const
{
    switch (args.prop)
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
        args.setValue(DMT_SECTOR_MOBJLIST, &d->mapObjects.head, 0);
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
        return MapElement::property(args);
    }

    return false;  // Continue iteration.
}

dint Sector::setProperty(DmuArgs const &args)
{
    switch (args.prop)
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
        return MapElement::setProperty(args);
    }

    return false;  // Continue iteration.
}

D_CMD(InspectSector)
{
    DENG2_UNUSED(src);

    LOG_AS("inspectsector (Cmd)");

    if (argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (sector-id)") << argv[0];
        return true;
    }

    if (!App_World().hasMap())
    {
        LOG_SCR_ERROR("No map is currently loaded");
        return false;
    }

    // Find the sector.
    dint const index  = String(argv[1]).toInt();
    Sector const *sec = App_World().map().sectorPtr(index);
    if (!sec)
    {
        LOG_SCR_ERROR("Sector #%i not found") << index;
        return false;
    }

    LOG_SCR_MSG(_E(b) "Sector %s" _E(.) " [%p]")
            << Id(sec->indexInMap()).asText() << sec;
    LOG_SCR_MSG(    _E(l) "Bounds: "      _E(.)_E(i) "%s" _E(.)
                " " _E(l) "Light Color: " _E(.)_E(i) "%s" _E(.)
                " " _E(l) "Light Level: " _E(.)_E(i) "%f")
            << Rectangled(sec->bounds().min, sec->bounds().max).asText()
            << sec->lightColor().asText()
            << sec->lightLevel();
    if (sec->planeCount())
    {
        LOG_SCR_MSG(_E(D) "Planes (%i):") << sec->planeCount();
        sec->forAllPlanes([] (Plane const &plane)
        {
            LOG_SCR_MSG("%s: " _E(>))
                << Sector::planeIdAsText(plane.indexInSector()).upperFirstChar()
                << plane.description();
            return LoopContinue;
        });
    }
    if (sec->subsectorCount())
    {
        LOG_SCR_MSG(_E(D) "Subsectors (%i):") << sec->subsectorCount();
        dint subsectorIndex = 0;
        sec->forAllSubsectors([&subsectorIndex] (Subsector const &subsec)
        {
            LOG_SCR_MSG("%i: " _E(>))
                << subsectorIndex
                << subsec.description();
            subsectorIndex += 1;
            return LoopContinue;
        });
    }

    return true;
}

String Sector::planeIdAsText(dint planeId)
{
    switch (planeId)
    {
    case Floor:   return "floor";
    case Ceiling: return "ceiling";

    default:      return "plane-" + String::number(planeId);
    }
}

void Sector::consoleRegister()  // static
{
    C_CMD("inspectsector", "i", InspectSector);
}

void Sector::setSubsectorConstructor(SubsectorConstructor func) // static
{
    subsectorConstructor = func;
}

