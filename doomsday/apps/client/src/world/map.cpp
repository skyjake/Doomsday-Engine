/** @file map.cpp  World map.
 *
 * @todo This file has grown far too large. It should be split up through the
 * introduction of new abstractions / collections.
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

#if defined(__SERVER__)
#  error "map.cpp is only for the Client"
#endif

#include "de_base.h"
#include "world/map.h"
#include "world/contact.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/convexsubspace.h"
#include "world/line.h"
#include "world/subsector.h"
#include "world/surface.h"
#include "world/vertex.h"
#include "api_console.h"

#include "clientapp.h"
#include "client/cl_mobj.h"
#include "api_sound.h"
#include "client/clskyplane.h"
#include "world/contact.h"
#include "world/contactspreader.h"
#include "render/lightdecoration.h"
#include "render/lumobj.h"
#include "render/rend_main.h"
#include "render/rend_particle.h"
#include "render/viewports.h"
#include "render/walledge.h"

#include <doomsday/defs/mapinfo.h>
#include <doomsday/defs/sky.h>
#include <doomsday/world/entitydatabase.h>
#include <doomsday/world/bspnode.h>
#include <doomsday/mesh/face.h>
#include <doomsday/world/blockmap.h>
#include <doomsday/world/bspleaf.h>
#include <doomsday/world/convexsubspace.h>
#include <doomsday/world/lineblockmap.h>
#include <doomsday/world/lineowner.h>
#include <doomsday/world/polyobjdata.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/mobjthinker.h>
#include <doomsday/world/polyobj.h>
#include <doomsday/world/sector.h>
#include <doomsday/world/sky.h>
#include <doomsday/world/thinkers.h>
#include <doomsday/world/bsp/partitioner.h>

#include <de/bitarray.h>
#include <de/logbuffer.h>
#include <de/hash.h>
#include <de/rectangle.h>
#include <de/charsymbols.h>
#include <de/legacy/aabox.h>
#include <de/legacy/nodepile.h>
#include <de/legacy/vector1.h>
#include <de/legacy/timer.h>

#include <array>
#include <map>

using namespace de;
using world::World;

/// Milliseconds it takes for Unpredictable and Hidden mobjs to be
/// removed from the hash. Under normal circumstances, the special
/// status should be removed fairly quickly.
#define CLMOBJ_TIMEOUT  4000

DE_PIMPL(Map)
, DE_OBSERVES(ThinkerData, Deletion)
#ifdef __SERVER__
, DE_OBSERVES(world::Thinkers, Removal)
#endif
{
    using Blockmap = world::Blockmap;
    using Sector   = world::Sector;

    /**
     * All (particle) generators.
     */
    struct Generators
    {
        struct ListNode
        {
            ListNode *next;
            Generator *gen;
        };

        std::array<Generator *, MAX_GENERATORS> activeGens;

        // We can link 64 generators each into four lists each before running out of links.
        static const int LINKSTORE_SIZE = 4 * MAX_GENERATORS;
        ListNode *linkStore = nullptr;
        duint linkStoreCursor = 0;

        duint listsSize = 0;
        // Array of list heads containing links from linkStore to generators in activeGens.
        ListNode **lists = nullptr;

        ~Generators()
        {
            Z_Free(lists);
            Z_Free(linkStore);
        }

        /**
         * Resize the collection.
         *
         * @param listCount  Number of lists the collection must support.
         */
        void resize(duint listCount)
        {
            if (!linkStore)
            {
                linkStore = (ListNode *) Z_Malloc(sizeof(*linkStore) * LINKSTORE_SIZE, PU_MAP, 0);
                linkStoreCursor = 0;
                activeGens.fill(nullptr);
            }

            listsSize = listCount;
            lists = (ListNode **) Z_Realloc(lists, sizeof(ListNode *) * listsSize, PU_MAP);
        }

        /**
         * Returns an unused link from the linkStore.
         */
        ListNode *newLink()
        {
            if (linkStoreCursor < (unsigned)LINKSTORE_SIZE)
            {
                return &linkStore[linkStoreCursor++];
            }
            LOG_MAP_WARNING("Exhausted generator link storage");
            return nullptr;
        }
    };

    struct ContactBlockmap : public Blockmap
    {
        BitArray spreadBlocks;  ///< Used to prevent repeat processing.

        /**
         * Construct a new contact blockmap.
         *
         * @param bounds    Map space boundary.
         * @param cellSize  Width and height of a cell in map space units.
         */
        ContactBlockmap(const AABoxd &bounds, duint cellSize = 128)
            : Blockmap(bounds, cellSize)
            , spreadBlocks(width() * height())
        {}

        void clear()
        {
            spreadBlocks.fill(false);
            unlinkAll();
        }

        /**
         * @param contact  Contact to be linked. Note that if the object's origin
         *                 lies outside the blockmap it will not be linked!
         */
        void link(Contact &contact)
        {
            bool outside;
            world::BlockmapCell cell = toCell(contact.objectOrigin(), &outside);
            if (!outside)
            {
                Blockmap::link(cell, &contact);
            }
        }

        void spread(const AABoxd &region)
        {
            spreadContacts(*this, region, &spreadBlocks);
        }
    };

    std::unique_ptr<ContactBlockmap> mobjContactBlockmap;  /// @todo Redundant?
    std::unique_ptr<ContactBlockmap> lumobjContactBlockmap;

    PlaneSet trackedPlanes;
    SurfaceSet scrollingSurfaces;
    SkyDrawable::Animator skyAnimator;
    std::unique_ptr<Generators> generators;
    List<Lumobj *> lumobjs;  ///< All lumobjs (owned).

    ClSkyPlane skyFloor{Sector::Floor, DDMAXFLOAT};
    ClSkyPlane skyCeiling{Sector::Ceiling, DDMINFLOAT};

    ClMobjHash clMobjHash;

    Impl(Public *i)
        : Base(i)
    {}

    ~Impl()
    {
        self().removeAllLumobjs();
        self().clearData();
    }

    void initContactBlockmaps(ddouble margin = 8)
    {
        const auto &bounds = self().bounds();

        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        AABoxd expandedBounds(bounds.minX - margin, bounds.minY - margin,
                              bounds.maxX + margin, bounds.maxY + margin);

        mobjContactBlockmap.reset(new ContactBlockmap(expandedBounds));
        lumobjContactBlockmap.reset(new ContactBlockmap(expandedBounds));
    }

    /**
     * Returns the appropriate contact blockmap for the specified contact @a type.
     */
    ContactBlockmap &contactBlockmap(ContactType type)
    {
        switch (type)
        {
        case ContactMobj:   return *mobjContactBlockmap;
        case ContactLumobj: return *lumobjContactBlockmap;

        default: throw Error("Map::contactBlockmap", "Invalid contact type");
        }
    }

    /**
     * To be called to link all contacts into the contact blockmaps.
     *
     * @todo Why don't we link contacts immediately? -ds
     */
    void linkAllContacts()
    {
        R_ForAllContacts([this](const Contact &contact) {
            contactBlockmap(contact.type()).link(const_cast<Contact &>(contact));
            return LoopContinue;
        });
    }

    // Clear the "contact" blockmaps (BSP leaf => object).
    void removeAllContacts()
    {
        mobjContactBlockmap->clear();
        lumobjContactBlockmap->clear();

        R_ClearContactLists(*thisPublic);
    }

    /**
     * Interpolate the smoothed height of planes.
     */
    void lerpTrackedPlanes(bool resetNextViewer)
    {
        if (resetNextViewer)
        {
            // Reset the plane height trackers.
            for (Plane *plane : trackedPlanes)
            {
                plane->resetSmoothedHeight();
            }

            // Tracked movement is now all done.
            trackedPlanes.clear();
        }
        // While the game is paused there is no need to smooth.
        else //if (!clientPaused)
        {
            for (auto iter = trackedPlanes.begin(); iter != trackedPlanes.end(); )
            {
                Plane *plane = *iter;

                plane->lerpSmoothedHeight();

                // Has this plane reached its destination?
                if (de::fequal(plane->heightSmoothed(), plane->height()))
                {
                    iter = trackedPlanes.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }

    /**
     * Interpolate the smoothed material origin of surfaces.
     */
    void lerpScrollingSurfaces(bool resetNextViewer)
    {
        if (resetNextViewer)
        {
            // Reset the surface material origin trackers.
            for (Surface *surface : scrollingSurfaces)
            {
                surface->resetSmoothedOrigin();
            }

            // Tracked movement is now all done.
            scrollingSurfaces.clear();
        }
        // While the game is paused there is no need to smooth.
        else //if (!clientPaused)
        {
            for (auto iter = scrollingSurfaces.begin(); iter != scrollingSurfaces.end(); )
            {
                Surface *surface = *iter;
                surface->lerpSmoothedOrigin();
                // Has this material reached its destination?
                if (surface->originSmoothed() == surface->origin())
                {
                    iter = scrollingSurfaces.erase(iter);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }

    /**
     * Create new mobj => BSP leaf contacts.
     */
    void generateMobjContacts()
    {
        self().forAllSectors([](Sector &sector) {
            for (mobj_t *iter = sector.firstMobj(); iter; iter = iter->sNext)
            {
                R_AddContact(*iter);
            }
            return LoopContinue;
        });
    }

    /**
     * Perform lazy initialization of the generator collection.
     */
    Generators &getGenerators()
    {
        // Time to initialize a new collection?
        if (!generators)
        {
            generators.reset(new Generators);
            generators->resize(self().sectorCount());
        }
        return *generators;
    }

    /**
     * Lookup the next available generator id.
     *
     * @todo Optimize: Cache this result.
     * @todo Optimize: We could maintain an age-sorted list of generators.
     *
     * @return  The next available id else @c 0 iff there are no unused ids.
     */
    Generator::Id findIdForNewGenerator()
    {
        Generators &gens = getGenerators();

        // Prefer allocating a new generator if we've a spare id.
        duint unused = 0;
        for (; unused < gens.activeGens.size(); ++unused)
        {
            if (!gens.activeGens[unused]) break;
        }
        if (unused < gens.activeGens.size()) return Generator::Id(unused + 1);

        // See if there is an active, non-static generator we can supplant.
        Generator *oldest = nullptr;
        for (Generator *gen : gens.activeGens)
        {
            if (!gen || gen->isStatic()) continue;

            if (!oldest || gen->age() > oldest->age())
            {
                oldest = gen;
            }
        }

        return (oldest? oldest->id() : 0);
    }

    void spawnMapParticleGens()
    {
        if (!self().hasManifest()) return;

        for (int i = 0; i < DED_Definitions()->ptcGens.size(); ++i)
        {
            ded_ptcgen_t *genDef = &DED_Definitions()->ptcGens[i];

            if (!genDef->map) continue;

            if (*genDef->map != self().manifest().composeUri())
                continue;

            // Are we still spawning using this generator?
            if (genDef->spawnAge > 0 && App_World().time() > genDef->spawnAge)
                continue;

            Generator *gen = self().newGenerator();
            if (!gen) return;  // No more generators.

            // Initialize the particle generator.
            gen->count = genDef->particles;
            gen->spawnRateMultiplier = 1;

            gen->configureFromDef(genDef);
            gen->setUntriggered();

            // Is there a need to pre-simulate?
            gen->presimulate(genDef->preSim);
        }
    }

    /**
     * Spawns all type-triggered particle generators, regardless of whether
     * the type of mobj exists in the map or not (mobjs might be dynamically
     * created).
     */
    void spawnTypeParticleGens()
    {
        auto &defs = *DED_Definitions();

        for (int i = 0; i < defs.ptcGens.size(); ++i)
        {
            ded_ptcgen_t *def = &defs.ptcGens[i];

            if (def->typeNum != DED_PTCGEN_ANY_MOBJ_TYPE && def->typeNum < 0)
                continue;

            Generator *gen = self().newGenerator();
            if (!gen) return;  // No more generators.

            // Initialize the particle generator.
            gen->count = def->particles;
            gen->spawnRateMultiplier = 1;

            gen->configureFromDef(def);
            gen->type  = def->typeNum;
            gen->type2 = def->type2Num;

            // Is there a need to pre-simulate?
            gen->presimulate(def->preSim);
        }
    }

    int findDefForGenerator(Generator *gen)
    {
        DE_ASSERT(gen);

        auto &defs = *DED_Definitions();

        // Search for a suitable definition.
        for (int i = 0; i < defs.ptcGens.size(); ++i)
        {
            ded_ptcgen_t *def = &defs.ptcGens[i];

            // A type generator?
            if (def->typeNum == DED_PTCGEN_ANY_MOBJ_TYPE && gen->type == DED_PTCGEN_ANY_MOBJ_TYPE)
            {
                return i + 1;  // Stop iteration.
            }
            if (def->typeNum >= 0 &&
               (gen->type == def->typeNum || gen->type2 == def->type2Num))
            {
                return i + 1;  // Stop iteration.
            }

            // A damage generator?
            if (gen->source && gen->source->type == def->damageNum)
            {
                return i + 1;  // Stop iteration.
            }

            // A flat generator?
            if (gen->plane && def->material)
            {
                try
                {
                    world::Material &defMat = world::Materials::get().material(*def->material);

                    auto *mat = gen->plane->surface().materialPtr();
                    if (def->flags & Generator::SpawnFloor)
                        mat = gen->plane->sector().floor().surface().materialPtr();
                    if (def->flags & Generator::SpawnCeiling)
                        mat = gen->plane->sector().ceiling().surface().materialPtr();

                    // Is this suitable?
                    if (mat == &defMat)
                    {
                        return i + 1; // 1-based index.
                    }

#if 0 /// @todo $revise-texture-animation
                    if (def->flags & PGF_GROUP)
                    {
                        // Generator triggered by all materials in the animation.
                        if (Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat)
                            && &Material_AnimGroup(defMat) == &Material_AnimGroup(mat))
                        {
                            // Both are in this animation! This def will do.
                            return i + 1;  // 1-based index.
                        }
                    }
#endif
                }
                catch (const world::MaterialManifest::MissingMaterialError &)
                {}  // Ignore this error.
                catch (const Resources::MissingResourceManifestError &)
                {}  // Ignore this error.
            }

            // A state generator?
            if (gen->source && def->state[0]
                && ::runtimeDefs.states.indexOf(gen->source->state) == DED_Definitions()->getStateNum(def->state))
            {
                return i + 1;  // 1-based index.
            }
        }

        return 0;  // Not found.
    }

    /**
     * Update existing generators in the map following an engine reset.
     */
    void updateParticleGens()
    {
        for (Generator *gen : getGenerators().activeGens)
        {
            // Only consider active generators.
            if (!gen) continue;

            // Map generators cannot be updated (we have no means to reliably
            // identify them), so destroy them.
            if (gen->isUntriggered())
            {
                Generator_Delete(gen);
                continue;  // Continue iteration.
            }

            if (int defIndex = findDefForGenerator(gen))
            {
                // Update the generator using the new definition.
                gen->def = &DED_Definitions()->ptcGens[defIndex - 1];
            }
            else
            {
                // Nothing else we can do, destroy it.
                Generator_Delete(gen);
            }
        }

        // Re-spawn map generators.
        spawnMapParticleGens();
    }

    /**
     * Link all generated particles into the map so that they will be drawn.
     *
     * @todo Overkill?
     */
    void linkAllParticles()
    {
        Generators &gens = getGenerators();

        // Empty all generator lists.
        std::memset(gens.lists, 0, sizeof(*gens.lists) * gens.listsSize);
        gens.linkStoreCursor = 0;

        if (useParticles)
        {
            for (Generator *gen : gens.activeGens)
            {
                if (!gen) continue;

                const ParticleInfo *pInfo = gen->particleInfo();
                for (int i = 0; i < gen->count; ++i, pInfo++)
                {
                    if (pInfo->stage < 0 || !pInfo->bspLeaf)
                        continue;

                    int listIndex = pInfo->bspLeaf->sectorPtr()->indexInMap();
                    DE_ASSERT((unsigned)listIndex < gens.listsSize);

                    // Must check that it isn't already there...
                    bool found = false;
                    for (Generators::ListNode *it = gens.lists[listIndex]; it; it = it->next)
                    {
                        if (it->gen == gen)
                        {
                            // Warning message disabled as these are occuring so thick and fast
                            // that logging is pointless (and negatively affecting performance).
                            //LOGDEV_MAP_VERBOSE("Attempted repeat link of generator %p to list %u.")
                            //        << gen << listIndex;
                            found = true; // No, no...
                        }
                    }

                    if (found) continue;

                    // We need a new link.
                    if (Generators::ListNode *link = gens.newLink())
                    {
                        link->gen  = gen;
                        link->next = gens.lists[listIndex];
                        gens.lists[listIndex] = link;
                    }
                }
            }
        }
    }

    void thinkerBeingDeleted(thinker_s &th)
    {
        clMobjHash.remove(th.id);
    }
};

Map::Map(res::MapManifest *manifest)
    : world::Map(manifest)
    , d(new Impl(this))
{}

SkyDrawable::Animator &Map::skyAnimator() const
{
    return d->skyAnimator;
}

void Map::initRadio()
{
    LOG_AS("Map::initRadio");

    Time begunAt;

    forAllVertices([](world::Vertex &vtx) {
        vtx.as<Vertex>().updateShadowOffsets();
        return LoopContinue;
    });

    /// The algorithm:
    ///
    /// 1. Use the subspace blockmap to look for all the blocks that are within the line's shadow
    ///    bounding box.
    /// 2. Check the ConvexSubspaces whose sector is the same as the line.
    /// 3. If any of the shadow points are in the subspace, or any of the shadow edges cross one
    ///    of the subspace's edges (not parallel), link the line to the ConvexSubspace.
    forAllLines([this](world::Line &wline)
    {
        Line &line = wline.as<Line>();

        if (!line.isShadowCaster())
        {
            return LoopContinue;
        }

        // For each side of the line.
        for (int i = 0; i < 2; ++i)
        {
            LineSide &side = line.side(i).as<LineSide>();

            if (!side.hasSector()) continue;
            if (!side.hasSections()) continue;

            // Skip sides which share one or more edge with malformed geometry.
            if (!side.leftHEdge() || !side.rightHEdge()) continue;

            const auto &vtx0 = line.vertex(i);
            const auto &vtx1 = line.vertex(i ^ 1);
            const auto *vo0  = line.vertexOwner(i) -> next();
            const auto *vo1  = line.vertexOwner(i ^ 1) -> prev();

            AABoxd bounds = line.bounds();

            // Use the extended points, they are wider than inoffsets.
            const Vec2d sv0 = vtx0.origin() + vo0->extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, sv0.x, sv0.y);

            const Vec2d sv1 = vtx1.origin() + vo1->extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, sv1.x, sv1.y);

            // Link the shadowing line to all the subspaces whose axis-aligned bounding box
            // intersects 'bounds'.
            const int localValidCount = ++World::validCount;
            subspaceBlockmap().forAllInBox(bounds, [&bounds, &side, &localValidCount] (void *object)
            {
                auto &sub = *reinterpret_cast<world::ConvexSubspace *>(object);
                if (sub.validCount() != localValidCount)  // not yet processed
                {
                    sub.setValidCount(localValidCount);
                    if (&sub.subsector().sector() == side.sectorPtr())
                    {
                        // Check the bounds.
                        const AABoxd &polyBox = sub.poly().bounds();
                        if (!(   polyBox.maxX < bounds.minX
                              || polyBox.minX > bounds.maxX
                              || polyBox.minY > bounds.maxY
                              || polyBox.maxY < bounds.minY))
                        {
                            sub.as<ConvexSubspace>().addShadowLine(side);
                        }
                    }
                }
                return LoopContinue;
            });
        }
        return LoopContinue;
    });

    LOGDEV_GL_MSG("Completed in %.2f seconds") << begunAt.since();
}

void Map::initContactBlockmaps()
{
    d->initContactBlockmaps();
}

void Map::spreadAllContacts(const AABoxd &region)
{
    // Expand the region according by the maxium radius of each contact type.
    d->mobjContactBlockmap->
        spread(AABoxd(region.minX - DDMOBJ_RADIUS_MAX, region.minY - DDMOBJ_RADIUS_MAX,
                      region.maxX + DDMOBJ_RADIUS_MAX, region.maxY + DDMOBJ_RADIUS_MAX));

    d->lumobjContactBlockmap->
        spread(AABoxd(region.minX - Lumobj::radiusMax(), region.minY - Lumobj::radiusMax(),
                      region.maxX + Lumobj::radiusMax(), region.maxY + Lumobj::radiusMax()));
}

void Map::initGenerators()
{
    LOG_AS("Map::initGenerators");
    Time begunAt;
    d->spawnTypeParticleGens();
    d->spawnMapParticleGens();
    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

void Map::spawnPlaneParticleGens()
{
    //if (!useParticles) return;

    forAllSectors([](world::Sector &sector) {
        auto &floor = sector.floor().as<Plane>();
        floor.spawnParticleGen(Def_GetGenerator(floor.surface().composeMaterialUri()));

        auto &ceiling = sector.ceiling().as<Plane>();
        ceiling.spawnParticleGen(Def_GetGenerator(ceiling.surface().composeMaterialUri()));
        return LoopContinue;
    });
}

void Map::clearClMobjs()
{
    d->clMobjHash.clear();
}

mobj_t *Map::clMobjFor(thid_t id, bool canCreate) const
{
    LOG_AS("Map::clMobjFor");

    auto found = d->clMobjHash.find(id);
    if (found != d->clMobjHash.end())
    {
        return found->second;
    }

    if (!canCreate) return nullptr;

    // Create a new client mobj. This is a regular mobj that has network state
    // associated with it.

    MobjThinker mob(Thinker::AllocateMemoryZone);
    mob.id       = id;
    mob.function = reinterpret_cast<thinkfunc_t>(gx.MobjThinker);

    auto *data = new ClientMobjThinkerData;
    data->remoteSync().flags = DDMF_REMOTE;
    mob.setData(data);

    d->clMobjHash.insert(id, mob);
    data->audienceForDeletion() += d;  // for removing from the hash

    thinkers().setMobjId(id);  // Mark this ID as used.

    // Client mobjs are full-fludged game mobjs as well.
    thinkers().add(*(thinker_t *)mob);

    return mob.take();
}

int Map::clMobjIterator(int (*callback)(mobj_t *, void *), void *context)
{
    ClMobjHash::const_iterator next;
    for (auto i = d->clMobjHash.cbegin(); i != d->clMobjHash.cend(); i = next)
    {
        next = i;
        ++next;

        DE_ASSERT(THINKER_DATA(i->second->thinker, ClientMobjThinkerData).hasRemoteSync());

        // Callback returns zero to continue.
        if (int result = callback(i->second, context))
            return result;
    }
    return 0;
}

const Map::ClMobjHash &Map::clMobjHash() const
{
    return d->clMobjHash;
}

bool Map::isPointInVoid(const de::Vec3d &point) const
{
    const auto &bspLeaf = bspLeafAt(point);
    if (bspLeaf.hasSubspace() && bspLeaf.subspace().contains(point) && bspLeaf.subspace().hasSubsector())
    {
        const auto &subsec = bspLeaf.subspace().subsector().as<Subsector>();
        return subsec.isHeightInVoid(point.z);
    }
    return true; // In the void.
}

void Map::updateScrollingSurfaces()
{
    for (Surface *surface : d->scrollingSurfaces)
    {
        surface->updateOriginTracking();
    }
}

Map::SurfaceSet &Map::scrollingSurfaces()
{
    return d->scrollingSurfaces;
}

void Map::updateTrackedPlanes()
{
    for (Plane *plane : d->trackedPlanes)
    {
        plane->updateHeightTracking();
    }
}

Map::PlaneSet &Map::trackedPlanes()
{
    return d->trackedPlanes;
}

void Map::initSkyFix()
{
    Time begunAt;

    LOG_AS("Map::initSkyFix");

    d->skyFloor  .setHeight(DDMAXFLOAT);
    d->skyCeiling.setHeight(DDMINFLOAT);

    // Update for sector plane heights and mobjs which intersect the ceiling.
    /// @todo Can't we defer this?
    forAllSectors([this](world::Sector &sector)
    {
        if (!sector.sideCount()) return LoopContinue;

        const bool skyFloor = sector.floor  ().surface().hasSkyMaskedMaterial();
        const bool skyCeil  = sector.ceiling().surface().hasSkyMaskedMaterial();

        if (!skyFloor && !skyCeil) return LoopContinue;

        if (skyCeil)
        {
            // Adjust for the plane height.
            if (sector.ceiling().as<Plane>().heightSmoothed() > d->skyCeiling.height())
            {
                // Must raise the skyfix ceiling.
                d->skyCeiling.setHeight(sector.ceiling().as<Plane>().heightSmoothed());
            }

            // Check that all the mobjs in the sector fit in.
            for (mobj_t *mob = sector.firstMobj(); mob; mob = mob->sNext)
            {
                ddouble extent = mob->origin[2] + mob->height;
                if (extent > d->skyCeiling.height())
                {
                    // Must raise the skyfix ceiling.
                    d->skyCeiling.setHeight(extent);
                }
            }
        }

        if (skyFloor)
        {
            // Adjust for the plane height.
            if (sector.floor().as<Plane>().heightSmoothed() < d->skyFloor.height())
            {
                // Must lower the skyfix floor.
                d->skyFloor.setHeight(sector.floor().as<Plane>().heightSmoothed());
            }
        }

        // Update for middle materials on lines which intersect the floor and/or ceiling
        // on the front (i.e., sector) side.
        sector.forAllSides([this, &skyCeil, &skyFloor](world::LineSide &side) {
            if (!side.hasSections()) return LoopContinue;
            if (!side.middle().hasMaterial()) return LoopContinue;

            // There must be a sector on both sides.
            if (!side.hasSector() || !side.back().hasSector()) return LoopContinue;

            // Possibility of degenerate BSP leaf.
            if (!side.leftHEdge()) return LoopContinue;

            WallEdge edge(WallSpec::fromMapSide(side.as<LineSide>(), LineSide::Middle), *side.leftHEdge(), Line::From);

            if (edge.isValid() && edge.top().z() > edge.bottom().z())
            {
                if (skyCeil && edge.top().z() + edge.origin().y > d->skyCeiling.height())
                {
                    // Must raise the skyfix ceiling.
                    d->skyCeiling.setHeight(edge.top().z() + edge.origin().y);
                }
                if (skyFloor && edge.bottom().z() + edge.origin().y < d->skyFloor.height())
                {
                    // Must lower the skyfix floor.
                    d->skyFloor.setHeight(edge.bottom().z() + edge.origin().y);
                }
            }
            return LoopContinue;
        });

        return LoopContinue;
    });

    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

ClSkyPlane &Map::skyFloor()
{
    return d->skyFloor;
}

const ClSkyPlane &Map::skyFloor() const
{
    return d->skyFloor;
}

ClSkyPlane &Map::skyCeiling()
{
    return d->skyCeiling;
}

const ClSkyPlane &Map::skyCeiling() const
{
    return d->skyCeiling;
}

Generator *Map::newGenerator()
{
    Generator::Id id = d->findIdForNewGenerator();  // 1-based
    if (!id) return nullptr;  // Failed; too many generators?

    Impl::Generators &gens = d->getGenerators();

    // If there is already a generator with that id - remove it.
    if (id > 0 && (unsigned)id <= gens.activeGens.size())
    {
        Generator_Delete(gens.activeGens[id - 1]);
    }

    /// @todo Linear allocation when in-game is not good...
    auto *gen = (Generator *) Z_Calloc(sizeof(Generator), PU_MAP, 0);

    gen->setId(id);

    // Link the thinker to the list of (private) thinkers.
    gen->thinker.function = (thinkfunc_t) Generator_Thinker;
    thinkers().add(gen->thinker, false /*not public*/);

    // Link the generator into the collection.
    gens.activeGens[id - 1] = gen;

    return gen;
}

int Map::generatorCount() const
{
    if (!d->generators) return 0;
    int count = 0;
    for (Generator *gen : d->getGenerators().activeGens)
    {
        if (gen) count += 1;
    }
    return count;
}

void Map::unlinkGenerator(Generator &generator)
{
    Impl::Generators &gens = d->getGenerators();
    for (duint i = 0; i < gens.activeGens.size(); ++i)
    {
        if (gens.activeGens[i] == &generator)
        {
            gens.activeGens[i] = nullptr;
            break;
        }
    }
}

LoopResult Map::forAllGenerators(const std::function<LoopResult (Generator &)>& func) const
{
    for (Generator *gen : d->getGenerators().activeGens)
    {
        if (!gen) continue;

        if (auto result = func(*gen))
            return result;
    }
    return LoopContinue;
}

LoopResult Map::forAllGeneratorsInSector(const world::Sector &sector,
                                         const std::function<LoopResult (Generator &)>& func) const
{
    if (sector.mapPtr() == this)  // Ignore 'alien' sectors.
    {
        const duint listIndex = sector.indexInMap();

        Impl::Generators &gens = d->getGenerators();
        for (Impl::Generators::ListNode *it = gens.lists[listIndex]; it; it = it->next)
        {
            if (auto result = func(*it->gen))
                return result;
        }
    }
    return LoopContinue;
}

int Map::lumobjCount() const
{
    return d->lumobjs.count();
}

Lumobj &Map::addLumobj(Lumobj *lumobj)
{
    DE_ASSERT(lumobj != nullptr);

    d->lumobjs.append(lumobj);

    lumobj->setMap(this);
    lumobj->setIndexInMap(d->lumobjs.count() - 1);
    DE_ASSERT(lumobj->bspLeafAtOrigin().hasSubspace());
    lumobj->bspLeafAtOrigin().subspace().as<ConvexSubspace>().link(*lumobj);
    R_AddContact(*lumobj);  // For spreading purposes.

    return *lumobj;
}

void Map::removeLumobj(int which)
{
    if (which >= 0 && which < lumobjCount())
    {
        delete d->lumobjs.takeAt(which);
    }
}

void Map::removeAllLumobjs()
{
    forAllSubspaces([](world::ConvexSubspace &subspace) {
        subspace.as<ConvexSubspace>().unlinkAllLumobjs();
        return LoopContinue;
    });
    deleteAll(d->lumobjs); d->lumobjs.clear();
}

Lumobj &Map::lumobj(int index) const
{
    if (Lumobj *lum = lumobjPtr(index)) return *lum;
    /// @throw MissingObjectError  Invalid Lumobj reference specified.
    throw MissingObjectError("Map::lumobj", "Unknown Lumobj index:" + String::asText(index));
}

Lumobj *Map::lumobjPtr(int index) const
{
    if (index >= 0 && index < d->lumobjs.count())
    {
        return d->lumobjs.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllLumobjs(const std::function<LoopResult (Lumobj &)>& func) const
{
    for (Lumobj *lob : d->lumobjs)
    {
        if (auto result = func(*lob)) return result;
    }
    return LoopContinue;
}

void Map::update()
{
    world::Map::update();

    d->updateParticleGens();  // Defs might've changed.

    // Update all surfaces.
    forAllSectors([](world::Sector &sector) {
        sector.forAllSubsectors([](world::Subsector &subsector) {
             subsector.as<Subsector>().markForDecorationUpdate();
             return LoopContinue;
        });
        return LoopContinue;
    });

    const Record &inf = mapInfo();
    
    // Reconfigure the sky.
    /// @todo Sky needs breaking up into multiple components. There should be
    /// a representation on server side and a logical entity which the renderer
    /// visualizes. We also need multiple concurrent skies for BOOM support.
    defn::Sky skyDef;
    if (const Record *def = DED_Definitions()->skies.tryFind("id", inf.gets("skyId")))
    {
        skyDef = *def;
    }
    else
    {
        skyDef = inf.subrecord("sky");
    }
    sky().configure(&skyDef);
}

void Map::serializeInternalState(Writer &to) const
{
    world::Map::serializeInternalState(to);

    // Internal state of thinkers.
    thinkers().forAll(0x3, [&to](thinker_t *th) {
        if (th->d)
        {
            const ThinkerData &thinkerData = THINKER_DATA(*th, ThinkerData);
            if (const ISerializable *serial = THINKER_DATA_MAYBE(*th, ISerializable))
            {
                to << thinkerData.id() << Writer::BeginSpan << *serial << Writer::EndSpan;
            }
        }
        return LoopContinue;
    });

    // Terminator.
    to << Id(Id::None);
}

void Map::deserializeInternalState(Reader &from, const world::IThinkerMapping &thinkerMapping)
{
    world::Map::deserializeInternalState(from, thinkerMapping);

    try
    {
        // Internal state of thinkers.
        for (;;)
        {
            Id id { Id::None };
            from >> id;
            if (!id) break; // Zero ID terminates the sequence.

            // Span length.
            duint32 size = 0;
            from >> size;

            const auto nextOffset = from.offset() + size;

            //qDebug() << "Found serialized internal state for private ID" << id.asText() << "size" << size;

            try
            {
                if (thinker_t *th = thinkerMapping.thinkerForPrivateId(id))
                {
                    // The identifier is changed if necessary.
                    Thinker_InitPrivateData(th, id);
                    if (ISerializable *serial = THINKER_DATA_MAYBE(*th, ISerializable))
                    {
                        from >> *serial;
                    }
                    else
                    {
                        LOG_MAP_WARNING("State for thinker %i is not deserializable "
                                        DE_CHAR_MDASH " internal representation may have "
                                        "changed, or save data is corrupt") << id;
                    }
                }
            }
            catch (const Error &er)
            {
                LOG_MAP_WARNING("Error when reading state of object %s: %s")
                    << id << er.asText();
            }

            from.setOffset(nextOffset);
        }
    }
    catch (const Error &er)
    {
        LOG_MAP_WARNING("Error when reading state: %s") << er.asText();
    }
}

void Map::redecorate()
{
    forAllSectors([](world::Sector &sector)
    {
        sector.forAllSubsectors([](world::Subsector &subsec)
        {
            subsec.as<Subsector>().markForDecorationUpdate();
            return LoopContinue;
        });
        return LoopContinue;
    });
}

void Map::worldFrameState(world::World::FrameState frameState)
{
    if (frameState != world::World::FrameBegins) return;

    const bool resetNextViewer = R_IsViewerResetPending();

    DE_ASSERT(&App_World().map() == this); // Sanity check.

    // Interpolate the map ready for drawing view(s) of it.
    d->lerpTrackedPlanes(resetNextViewer);
    d->lerpScrollingSurfaces(resetNextViewer);

    if (!freezeRLs)
    {
#if 0
        // Initialize and/or update the LightGrid.
        initLightGrid();

        d->biasBeginFrame();
#endif

        removeAllLumobjs();

        d->removeAllContacts();

        // Generate surface decorations for the frame.
        if (useLightDecorations)
        {
            forAllSectors([](world::Sector &sector)
            {
                sector.forAllSubsectors([] (world::Subsector &wsub)
                {
                    auto &sub = wsub.as<Subsector>();

                    // Perform scheduled redecoration.
                    sub.decorate();

                    // Generate lumobjs for all decorations who want them.
                    sub.generateLumobjs();
                    return LoopContinue;
                });
                return LoopContinue;
            });
        }

        // Spawn omnilights for mobjs?
        if (useDynLights)
        {
            forAllSectors([](world::Sector &sector)
            {
                for (mobj_t *iter = sector.firstMobj(); iter; iter = iter->sNext)
                {
                    Mobj_GenerateLumobjs(iter);
                }
                return LoopContinue;
            });
        }

        d->generateMobjContacts();
        d->linkAllParticles();
        d->linkAllContacts();
    }
}

/// @return  @c false= Continue iteration.
static int expireClMobjsWorker(mobj_t *mob, void *context)
{
    const duint nowTime = *static_cast<duint *>(context);

    // Already deleted?
    if (mob->thinker.function == (thinkfunc_t)-1)
        return 0;

    // Don't expire player mobjs.
    if (mob->dPlayer) return 0;

    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mob);
    DE_ASSERT(info);

    if ((info->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN | CLMF_NULLED)) || !mob->info)
    {
        // Has this mobj timed out?
        if (nowTime - info->time > CLMOBJ_TIMEOUT)
        {
            LOGDEV_MAP_VERBOSE("Mobj %i has expired (%i << %i), in state %s [%c%c%c]")
                << mob->thinker.id
                << info->time << nowTime
                << Def_GetStateName(mob->state)
                << (info->flags & CLMF_UNPREDICTABLE? 'U' : '_')
                << (info->flags & CLMF_HIDDEN?        'H' : '_')
                << (info->flags & CLMF_NULLED?        '0' : '_');

            // Too long. The server will probably never send anything for this map-object,
            // so get rid of it. (Both unpredictable and hidden mobjs are not visible or
            // bl/seclinked.)
            Mobj_Destroy(mob);
        }
    }

    return 0;
}

void Map::expireClMobjs()
{
    duint nowTime = Timer_RealMilliseconds();
    clMobjIterator(expireClMobjsWorker, &nowTime);
}

void Map::link(mobj_t &mob, int flags)
{
    world::Map::link(mob, flags);

    // If this is a player - perform additional tests to see if they have either entered or exited the void.
    if (mob.dPlayer && mob.dPlayer->mo)
    {
        auto &client = ClientApp::player(P_GetDDPlayerIdx(mob.dPlayer));
        client.inVoid = true;
        if (Mobj_HasSubsector(mob))
        {
            auto &subsec = Mobj_Subsector(mob).as<Subsector>();
            if (Mobj_BspLeafAtOrigin(mob).subspace().contains(Mobj_Origin(mob)))
            {
                if (   mob.origin[2] <  subsec.visCeiling().heightSmoothed() + 4
                    && mob.origin[2] >= subsec.  visFloor().heightSmoothed())
                {
                    client.inVoid = false;
                }
            }
        }
    }
}

String Map::objectSummaryAsStyledText() const
{
    String str = world::Map::objectSummaryAsStyledText();

#define TABBED(count, label) Stringf(_E(Ta) "  %i " _E(Tb) "%s\n", count, label)
    if (generatorCount()) str += TABBED(generatorCount(), "Generators");
    if (lumobjCount())    str += TABBED(lumobjCount(),    "Lumobjs");
#undef TABBED

    return str.rightStrip();
}

void Map::consoleRegister() // static
{
    world::Map::consoleRegister();

    Mobj_ConsoleRegister();
}

enum {
    LinkFloorBit           = 0x1,
    LinkCeilingBit         = 0x2,
    FlatBleedingFloorBit   = 0x4,
    FlatBleedingCeilingBit = 0x8,
    InvisibleFloorBit      = 0x10,
    InvisibleCeilingBit    = 0x20,
};

void Map::applySectorHacks(world::Sector &sector, const struct de_api_sector_hacks_s *hacks)
{
    world::Map::applySectorHacks(sector, hacks);

    int linkFlags = 0;

    // Which planes to link.
    if (hacks->flags.linkFloorPlane)   linkFlags |= LinkFloorBit;
    if (hacks->flags.linkCeilingPlane) linkFlags |= LinkCeilingBit;

    // When to link the planes.
    if (hacks->flags.missingInsideBottom)  linkFlags |= FlatBleedingFloorBit;
    if (hacks->flags.missingInsideTop)     linkFlags |= FlatBleedingCeilingBit;
    if (hacks->flags.missingOutsideBottom) linkFlags |= InvisibleFloorBit;
    if (hacks->flags.missingOutsideTop)    linkFlags |= InvisibleCeilingBit;

    sector.setVisPlaneLinks(hacks->visPlaneLinkTargetSector, linkFlags);
}

bool Map::endEditing()
{
    if (!world::Map::endEditing())
    {
        return false;
    }

    // Client mobjs use server-assigned IDs so we don't need to assign them locally.
    thinkers().setIdAssignmentFunc([this](thinker_t &th){
        if (!Cl_IsClientMobj(reinterpret_cast<mobj_t *>(&th)))
        {
            th.id = thinkers().newMobjId();
        }
    });
    
    std::map<int, world::Sector *> sectorsByArchiveIndex;
    forAllSectors([&sectorsByArchiveIndex](world::Sector &sector) {
        sectorsByArchiveIndex[sector.indexInArchive()] = &sector;
        return LoopContinue;
    });

    forAllSectors([&sectorsByArchiveIndex](world::Sector &sector) {
        if (sector.visPlaneLinkTargetSector() != world::MapElement::NoIndex)
        {
            if (auto *target = sectorsByArchiveIndex[sector.visPlaneLinkTargetSector()])
            {
                // Use the first subsector as the target.
                auto &targetSub = target->subsector(0).as<Subsector>();

                int linkModes[2]{};
                if (sector.visPlaneBits() & FlatBleedingFloorBit)
                {
                    linkModes[world::Sector::Floor] |= Subsector::LinkWhenLowerThanTarget;
                }
                if (sector.visPlaneBits() & FlatBleedingCeilingBit)
                {
                    linkModes[world::Sector::Ceiling] |= Subsector::LinkWhenHigherThanTarget;
                }
                if (sector.visPlaneBits() & InvisibleFloorBit)
                {
                    linkModes[world::Sector::Floor] |= Subsector::LinkWhenHigherThanTarget;
                }
                if (sector.visPlaneBits() & InvisibleCeilingBit)
                {
                    linkModes[world::Sector::Ceiling] |= Subsector::LinkWhenLowerThanTarget;
                }

                // Fallback is to link always.
                for (auto &lm : linkModes)
                {
                    if (lm == 0) lm = Subsector::LinkAlways;
                }

                // Linking is done for each subsector separately. (Necessary, though?)
                sector.forAllSubsectors([&targetSub, &sector, linkModes](world::Subsector &wsub) {
                    auto &sub = wsub.as<Subsector>();
                    for (int plane = 0; plane < 2; ++plane)
                    {
                        if (sector.isVisPlaneLinked(plane))
                        {
                            sub.linkVisPlane(plane,
                                             targetSub,
                                             Subsector::VisPlaneLinkMode(linkModes[plane]));
                        }
                    }
                    return LoopContinue;
                });
            }
        }
        return LoopContinue;
    });

    return true;
}
