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

#include "de_base.h"
#include "world/map.h"
#include "api_console.h"

#include "world/bsp/partitioner.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "BspLeaf"
#include "ConvexSubspace"
#include "Face"
#include "Line"
#include "Polyobj"
#include "Subsector"
#include "Surface"
#include "Vertex"

#ifdef __CLIENT__
#  include "clientapp.h"
#  include "client/cl_mobj.h"
#  include "api_sound.h"
#  include "client/clientsubsector.h"
#  include "client/clskyplane.h"
#  include "Contact"
#  include "ContactSpreader"
#  include "LightDecoration"
#  include "Lumobj"
#  include "WallEdge"
#  include "render/viewports.h"
#  include "render/rend_main.h"
#  include "render/rend_particle.h"
#endif

#include <doomsday/defs/mapinfo.h>
#include <doomsday/defs/sky.h>
#include <doomsday/EntityDatabase>
#include <doomsday/BspNode>
#include <doomsday/world/blockmap.h>
#include <doomsday/world/lineblockmap.h>
#include <doomsday/world/lineowner.h>
#include <doomsday/world/polyobjdata.h>
#include <doomsday/world/materials.h>
#include <doomsday/world/sector.h>
#include <doomsday/world/sky.h>
#include <doomsday/world/thinkers.h>

#include <de/BitArray>
#include <de/LogBuffer>
#include <de/Hash>
#include <de/Rectangle>
#include <de/charsymbols.h>
#include <de/legacy/aabox.h>
#include <de/legacy/nodepile.h>
#include <de/legacy/vector1.h>
#include <de/legacy/timer.h>

#include <array>
#include <map>

using namespace de;

#ifdef __CLIENT__
/// Milliseconds it takes for Unpredictable and Hidden mobjs to be
/// removed from the hash. Under normal circumstances, the special
/// status should be removed fairly quickly.
#define CLMOBJ_TIMEOUT  4000
#endif

DE_PIMPL(::Map)
#ifdef __CLIENT__
, DE_OBSERVES(ThinkerData, Deletion)
#endif
#ifdef __SERVER__
, DE_OBSERVES(world::Thinkers, Removal)
#endif
{
    using Blockmap = world::Blockmap;
    using Sector   = world::Sector;

#ifdef __CLIENT__
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
        static const dint LINKSTORE_SIZE = 4 * MAX_GENERATORS;
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
        void link(world::Contact &contact)
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
#endif

    Impl(Public *i)
        : Base(i)
    {}

    ~Impl()
    {
#ifdef __CLIENT__
        self().removeAllLumobjs();
#endif
    }

#ifdef __SERVER__
    void thinkerRemoved(thinker_t &th) override
    {
        auto *mob = reinterpret_cast<mobj_t *>(&th);

        // If the state of the mobj is the NULL state, this is a
        // predictable mobj removal (result of animation reaching its
        // end) and shouldn't be included in netGame deltas.
        if (!mob->state || !runtimeDefs.states.indexOf(mob->state))
        {
            Sv_MobjRemoved(th.id);
        }
    }
#endif

#ifdef __CLIENT__

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
        R_ForAllContacts([this] (const Contact &contact)
        {
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
        for (Sector *sector : sectors)
        {
            for (mobj_t *iter = sector->firstMobj(); iter; iter = iter->sNext)
            {
                R_AddContact(*iter);
            }
        }
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
            generators->resize(sectors.count());
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

        for (dint i = 0; i < DED_Definitions()->ptcGens.size(); ++i)
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

        for (dint i = 0; i < defs.ptcGens.size(); ++i)
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

    dint findDefForGenerator(Generator *gen)
    {
        DE_ASSERT(gen);

        auto &defs = *DED_Definitions();

        // Search for a suitable definition.
        for (dint i = 0; i < defs.ptcGens.size(); ++i)
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

                    Material *mat = gen->plane->surface().materialPtr();
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
                catch (const MaterialManifest::MissingMaterialError &)
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

            if (dint defIndex = findDefForGenerator(gen))
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
        //LOG_AS("Map::linkAllParticles");

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
                for (dint i = 0; i < gen->count; ++i, pInfo++)
                {
                    if (pInfo->stage < 0 || !pInfo->bspLeaf)
                        continue;

                    dint listIndex = pInfo->bspLeaf->sectorPtr()->indexInMap();
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

#endif  // __CLIENT__
};

Map::Map(res::MapManifest *manifest)
    : world::Map(manifest)
    , d(new Impl(this))
{}

#if defined(__CLIENT__)

SkyDrawable::Animator &Map::skyAnimator() const
{
    return d->skyAnimator;
}

void Map::initRadio()
{
    LOG_AS("Map::initRadio");

    Time begunAt;

    for (Vertex *vtx : d->mesh.vertexs())
    {
        vtx->updateShadowOffsets();
    }

    /// The algorithm:
    ///
    /// 1. Use the subspace blockmap to look for all the blocks that are within the line's shadow
    ///    bounding box.
    /// 2. Check the ConvexSubspaces whose sector is the same as the line.
    /// 3. If any of the shadow points are in the subspace, or any of the shadow edges cross one
    ///    of the subspace's edges (not parallel), link the line to the ConvexSubspace.
    for (Line *line : d->lines)
    {
        if (!line->isShadowCaster()) continue;

        // For each side of the line.
        for (dint i = 0; i < 2; ++i)
        {
            LineSide &side = line->side(i);

            if (!side.hasSector()) continue;
            if (!side.hasSections()) continue;

            // Skip sides which share one or more edge with malformed geometry.
            if (!side.leftHEdge() || !side.rightHEdge()) continue;

            const Vertex &vtx0   = line->vertex(i);
            const Vertex &vtx1   = line->vertex(i ^ 1);
            const LineOwner *vo0 = line->vertexOwner(i)->next();
            const LineOwner *vo1 = line->vertexOwner(i ^ 1)->prev();

            AABoxd bounds = line->bounds();

            // Use the extended points, they are wider than inoffsets.
            const Vec2d sv0 = vtx0.origin() + vo0->extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, sv0.x, sv0.y);

            const Vec2d sv1 = vtx1.origin() + vo1->extendedShadowOffset();
            V2d_AddToBoxXY(bounds.arvec2, sv1.x, sv1.y);

            // Link the shadowing line to all the subspaces whose axis-aligned bounding box
            // intersects 'bounds'.
            ::validCount++;
            const dint localValidCount = ::validCount;
            subspaceBlockmap().forAllInBox(bounds, [&bounds, &side, &localValidCount] (void *object)
            {
                auto &sub = *(ConvexSubspace *)object;
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
                            sub.addShadowLine(side);
                        }
                    }
                }
                return LoopContinue;
            });
        }
    }

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

    for (Sector *sector : d->sectors)
    {
        Plane &floor = sector->floor();
        floor.spawnParticleGen(Def_GetGenerator(floor.surface().composeMaterialUri()));

        Plane &ceiling = sector->ceiling();
        ceiling.spawnParticleGen(Def_GetGenerator(ceiling.surface().composeMaterialUri()));
    }
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

    d->thinkers->setMobjId(id);  // Mark this ID as used.

    // Client mobjs are full-fludged game mobjs as well.
    d->thinkers->add(*(thinker_t *)mob);

    return mob.take();
}

dint Map::clMobjIterator(dint (*callback)(mobj_t *, void *), void *context)
{
    ClMobjHash::const_iterator next;
    for (auto i = d->clMobjHash.cbegin(); i != d->clMobjHash.cend(); i = next)
    {
        next = i;
        ++next;

        DE_ASSERT(THINKER_DATA(i->second->thinker, ClientMobjThinkerData).hasRemoteSync());

        // Callback returns zero to continue.
        if (dint result = callback(i->second, context))
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
    const BspLeaf &bspLeaf = bspLeafAt(point);
    if (bspLeaf.hasSubspace() && bspLeaf.subspace().contains(point) && bspLeaf.subspace().hasSubsector())
    {
        const auto &subsec = bspLeaf.subspace().subsector().as<ClientSubsector>();
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
    for (Sector *sector : d->sectors)
    {
        if (!sector->sideCount()) continue;

        const bool skyFloor = sector->floor  ().surface().hasSkyMaskedMaterial();
        const bool skyCeil  = sector->ceiling().surface().hasSkyMaskedMaterial();

        if (!skyFloor && !skyCeil) continue;

        if (skyCeil)
        {
            // Adjust for the plane height.
            if (sector->ceiling().heightSmoothed() > d->skyCeiling.height())
            {
                // Must raise the skyfix ceiling.
                d->skyCeiling.setHeight(sector->ceiling().heightSmoothed());
            }

            // Check that all the mobjs in the sector fit in.
            for (mobj_t *mob = sector->firstMobj(); mob; mob = mob->sNext)
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
            if (sector->floor().heightSmoothed() < d->skyFloor.height())
            {
                // Must lower the skyfix floor.
                d->skyFloor.setHeight(sector->floor().heightSmoothed());
            }
        }

        // Update for middle materials on lines which intersect the floor and/or ceiling
        // on the front (i.e., sector) side.
        sector->forAllSides([this, &skyCeil, &skyFloor] (LineSide &side)
                            {
                                if (!side.hasSections()) return LoopContinue;
                                if (!side.middle().hasMaterial()) return LoopContinue;

                                // There must be a sector on both sides.
                                if (!side.hasSector() || !side.back().hasSector())
                                    return LoopContinue;

                                // Possibility of degenerate BSP leaf.
                                if (!side.leftHEdge()) return LoopContinue;

                                WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                                              *side.leftHEdge(), Line::From);

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
    }

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
    d->thinkers->add(gen->thinker, false /*not public*/);

    // Link the generator into the collection.
    gens.activeGens[id - 1] = gen;

    return gen;
}

dint Map::generatorCount() const
{
    if (!d->generators) return 0;
    dint count = 0;
    for (Generator *gen : d->getGenerators().activeGens)
    {
        if (gen) count += 1;
    }
    return count;
}

void Map::unlink(Generator &generator)
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

LoopResult Map::forAllGeneratorsInSector(const Sector &sector, const std::function<LoopResult (Generator &)>& func) const
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

dint Map::lumobjCount() const
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
    lumobj->bspLeafAtOrigin().subspace().link(*lumobj);
    R_AddContact(*lumobj);  // For spreading purposes.

    return *lumobj;
}

void Map::removeLumobj(dint which)
{
    if (which >= 0 && which < lumobjCount())
    {
        delete d->lumobjs.takeAt(which);
    }
}

void Map::removeAllLumobjs()
{
    for (ConvexSubspace *subspace : d->subspaces)
    {
        subspace->unlinkAllLumobjs();
    }
    deleteAll(d->lumobjs); d->lumobjs.clear();
}

Lumobj &Map::lumobj(dint index) const
{
    if (Lumobj *lum = lumobjPtr(index)) return *lum;
    /// @throw MissingObjectError  Invalid Lumobj reference specified.
    throw MissingObjectError("Map::lumobj", "Unknown Lumobj index:" + String::asText(index));
}

Lumobj *Map::lumobjPtr(dint index) const
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
    for (Sector *sector : d->sectors)
    {
        sector->forAllSubsectors([] (Subsector &subsector)
                                 {
                                     subsector.as<ClientSubsector>().markForDecorationUpdate();
                                     return LoopContinue;
                                 });
    }

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

String Map::objectsDescription() const
{
    String str;
    if (gx.MobjStateAsInfo)
    {
        // Print out a state description for each thinker.
        thinkers().forAll(0x3, [&str](thinker_t *th) {
            if (Thinker_IsMobj(th))
            {
                str += gx.MobjStateAsInfo(reinterpret_cast<const mobj_t *>(th));
            }
            return LoopContinue;
        });
    }
    return str;
}

void Map::restoreObjects(const Info &objState, const IThinkerMapping &thinkerMapping) const
{
    /// @todo Generalize from mobjs to all thinkers?
    LOG_AS("Map::restoreObjects");

    if (!gx.MobjStateAsInfo || !gx.MobjRestoreState) return;

    bool problemsDetected = false;

    // Look up all the mobjs.
    List<const thinker_t *> mobjs;
    thinkers().forAll(0x3, [&mobjs] (thinker_t *th) {
        if (Thinker_IsMobj(th)) mobjs << th;
        return LoopContinue;
    });

    // Check that all objects are found in the state description.
    if (objState.root().contents().size() != mobjs.size())
    {
        LOGDEV_MAP_WARNING("Different number of objects: %i in map, but got %i in restore data")
            << mobjs.size()
            << objState.root().contents().size();
    }

    // Check the cross-references.
    for (auto i  = objState.root().contentsInOrder().begin();
         i != objState.root().contentsInOrder().end();
         ++i)
    {
        const Info::BlockElement &state = (*i)->as<Info::BlockElement>();
        const Id::Type privateId = state.name().toUInt32();
        DE_ASSERT(privateId != 0);

        if (thinker_t *th = thinkerMapping.thinkerForPrivateId(privateId))
        {
            if (ThinkerData *found = ThinkerData::find(privateId))
            {
                DE_ASSERT(&found->thinker() == th);

                // Restore the state according to the serialized info.
                gx.MobjRestoreState(found->as<MobjThinkerData>().mobj(), state);

#if defined (DE_DEBUG)
                {
                    // Verify that the state is now correct.
                    Info const currentDesc(gx.MobjStateAsInfo(found->as<MobjThinkerData>().mobj()));
                    const Info::BlockElement &currentState =
                        currentDesc.root().contentsInOrder().first()->as<Info::BlockElement>();
                    DE_ASSERT(currentState.name() == state.name());
                    for (const auto &i : state.contents())
                    {
                        if (state.keyValue(i.first).text != currentState.keyValue(i.first).text)
                        {
                            problemsDetected = true;
                            const String msg =
                                Stringf("Object %u has mismatching '%s' (current:%s != arch:%s)",
                                        privateId,
                                        i.first.c_str(),
                                        currentState.keyValue(i.first).text.c_str(),
                                        state.keyValue(i.first).text.c_str());
                            LOGDEV_MAP_WARNING("%s") << msg;
                        }
                    }
                }
#endif
            }
            else
            {
                LOGDEV_MAP_ERROR("Map does not have a thinker matching ID 0x%x")
                    << privateId;
            }
        }
        else
        {
            LOGDEV_MAP_ERROR("Thinker mapping does not have a thinker matching ID 0x%x")
                << privateId;
        }
    }

    if (problemsDetected)
    {
        LOG_MAP_WARNING("Map objects were not fully restored " DE_CHAR_MDASH
                        " gameplay may be affected (enable Developer log entries for details)");
    }
    else
    {
        LOGDEV_MAP_MSG("State of map objects has been restored");
    }
}

void Map::serializeInternalState(Writer &to) const
{
    BaseMap::serializeInternalState(to);

    // Internal state of thinkers.
    thinkers().forAll(0x3, [&to] (thinker_t *th)
                      {
                          if (th->d)
                          {
                              const ThinkerData &thinkerData = THINKER_DATA(*th, ThinkerData);
                              if (const ISerializable *serial = THINKER_DATA_MAYBE(*th, ISerializable))
                              {
                                  to << thinkerData.id()
                                     << Writer::BeginSpan
                                     << *serial
                                     << Writer::EndSpan;
                              }
                          }
                          return LoopContinue;
                      });

    // Terminator.
    to << Id(Id::None);
}

void Map::deserializeInternalState(Reader &from, const IThinkerMapping &thinkerMapping)
{
    BaseMap::deserializeInternalState(from, thinkerMapping);

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
    forAllSectors([](Sector &sector) {
        sector.forAllSubsectors([](Subsector &subsec) {
            subsec.as<ClientSubsector>().markForDecorationUpdate();
            return LoopContinue;
        });
        return LoopContinue;
    });
}

void Map::worldSystemFrameBegins(bool resetNextViewer)
{
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
            for (Sector *sector : d->sectors)
            {
                sector->forAllSubsectors([] (Subsector &ssec)
                                         {
                                             auto &clSubsector = ssec.as<ClientSubsector>();

                                             // Perform scheduled redecoration.
                                             clSubsector.decorate();

                                             // Generate lumobjs for all decorations who want them.
                                             clSubsector.generateLumobjs();
                                             return LoopContinue;
                                         });
            }
        }

        // Spawn omnilights for mobjs?
        if (useDynLights)
        {
            for (Sector *sector : d->sectors)
                for (mobj_t *iter = sector->firstMobj(); iter; iter = iter->sNext)
                {
                    Mobj_GenerateLumobjs(iter);
                }
        }

        d->generateMobjContacts();

        d->linkAllParticles();
        d->linkAllContacts();
    }
}

/// @return  @c false= Continue iteration.
static dint expireClMobjsWorker(mobj_t *mob, void *context)
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

#endif // __CLIENT__

const AABoxd &Map::bounds() const
{
    return d->bounds;
}

coord_t Map::gravity() const
{
    return _effectiveGravity;
}

void Map::setGravity(coord_t newGravity)
{
    if (!de::fequal(_effectiveGravity, newGravity))
    {
        _effectiveGravity = newGravity;
        LOG_MAP_VERBOSE("Effective gravity for %s now %.1f")
            << (hasManifest() ? manifest().gets("id") : "(unknown map)") << _effectiveGravity;
    }
}

Thinkers &Map::thinkers() const
{
    if (bool( d->thinkers )) return *d->thinkers;
    /// @throw MissingThinkersError  The thinker lists are not yet initialized.
    throw MissingThinkersError("Map::thinkers", "Thinkers not initialized");
}

Sky &Map::sky() const
{
    return d->sky;
}

dint Map::vertexCount() const
{
    return d->mesh.vertexCount();
}

Vertex &Map::vertex(dint index) const
{
    if (Vertex *vtx = vertexPtr(index)) return *vtx;
    /// @throw MissingElementError  Invalid Vertex reference specified.
    throw MissingElementError("Map::vertex", "Unknown Vertex index:" + String::asText(index));
}

Vertex *Map::vertexPtr(dint index) const
{
    if (index >= 0 && index < d->mesh.vertexCount())
    {
        return d->mesh.vertexs().at(index);
    }
    return nullptr;
}

LoopResult Map::forAllVertexs(const std::function<LoopResult (Vertex &)>& func) const
{
    for (Vertex *vtx : d->mesh.vertexs())
    {
        if (auto result = func(*vtx)) return result;
    }
    return LoopContinue;
}

dint Map::sectorCount() const
{
    return d->sectors.count();
}

dint Map::subspaceCount() const
{
    return d->subspaces.count();
}

ConvexSubspace &Map::subspace(dint index) const
{
    if (ConvexSubspace *sub = subspacePtr(index)) return *sub;
    /// @throw MissingElementError  Invalid ConvexSubspace reference specified.
    throw MissingElementError("Map::subspace", "Unknown subspace index:" + String::asText(index));
}

ConvexSubspace *Map::subspacePtr(dint index) const
{
    if (index >= 0 && index < d->subspaces.count())
    {
        return d->subspaces.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllSubspaces(const std::function<LoopResult (ConvexSubspace &)>& func) const
{
    for (ConvexSubspace *sub : d->subspaces)
    {
        if (auto result = func(*sub)) return result;
    }
    return LoopContinue;
}

void Map::initPolyobjs()
{
    LOG_AS("Map::initPolyobjs");

    for (Polyobj *po : d->polyobjs)
    {
        /// @todo Is this still necessary? -ds
        /// (This data is updated automatically when moving/rotating).
        po->updateBounds();
        po->updateSurfaceTangents();

        po->unlink();
        po->link();
    }
}

int Map::ambientLightLevel() const
{
    return _ambientLightLevel;
}

LineSide &Map::side(dint index) const
{
    if (LineSide *side = sidePtr(index)) return *side;
    /// @throw MissingElementError  Invalid LineSide reference specified.
    throw MissingElementError("Map::side", stringf("Unknown LineSide index: %i", index));
}

LineSide *Map::sidePtr(dint index) const
{
    if (index < 0) return nullptr;
    return &d->lines.at(index / 2)->side(index % 2);
}

dint Map::toSideIndex(dint lineIndex, dint backSide) // static
{
    DE_ASSERT(lineIndex >= 0);
    return lineIndex * 2 + (backSide? 1 : 0);
}

bool Map::identifySoundEmitter(const SoundEmitter &emitter, Sector **sector,
    Polyobj **poly, Plane **plane, Surface **surface) const
{
    *sector  = nullptr;
    *poly    = nullptr;
    *plane   = nullptr;
    *surface = nullptr;

    /// @todo Optimize: All sound emitters in a sector are linked together forming
    /// a chain. Make use of the chains instead.

    *poly = d->polyobjBySoundEmitter(emitter);
    if (!*poly)
    {
        // Not a polyobj. Try the sectors next.
        *sector = d->sectorBySoundEmitter(emitter);
        if (!*sector)
        {
            // Not a sector. Try the planes next.
            *plane = d->planeBySoundEmitter(emitter);
            if (!*plane)
            {
                // Not a plane. Try the surfaces next.
                *surface = d->surfaceBySoundEmitter(emitter);
            }
        }
    }

    return (*sector != 0 || *poly != 0|| *plane != 0|| *surface != 0);
}

void Map::initNodePiles()
{
    LOG_AS("Map");

    Time begunAt;

    // Initialize node piles and line rings.
    NP_Init(&d->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&d->lineNodes, lineCount() + 1000);

    // Allocate the rings.
    DE_ASSERT(d->lineLinks == nullptr);
    d->lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*d->lineLinks) * lineCount(), PU_MAPSTATIC, 0);

    for (dint i = 0; i < lineCount(); ++i)
    {
        d->lineLinks[i] = NP_New(&d->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    LOGDEV_MAP_MSG("Initialized node piles in %.2f seconds") << begunAt.since();
}

Sector &Map::sector(dint index) const
{
    if (Sector *sec = sectorPtr(index)) return *sec;
    /// @throw MissingElementError  Invalid Sector reference specified.
    throw MissingElementError("Map::sector", "Unknown Sector index:" + String::asText(index));
}

Sector *Map::sectorPtr(dint index) const
{
    if (index >= 0 && index < d->sectors.count())
    {
        return d->sectors.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllSectors(const std::function<LoopResult (Sector &)> &func) const
{
    for (Sector *sec : d->sectors)
    {
        if (auto result = func(*sec)) return result;
    }
    return LoopContinue;
}

Subsector *Map::subsectorAt(const Vec2d &point) const
{
    BspLeaf &bspLeaf = bspLeafAt(point);
    if (bspLeaf.hasSubspace() && bspLeaf.subspace().contains(point))
    {
        return bspLeaf.subspace().subsectorPtr();
    }
    return nullptr;
}

Subsector &Map::subsector(de::Id id) const
{
    if (Subsector *subsec = subsectorPtr(id)) return *subsec;
    /// @throw MissingElementError  Invalid Sector reference specified.
    throw MissingSubsectorError("Map::subsector", "Unknown Subsector \"" + id.asText() + "\"");
}

Subsector *Map::subsectorPtr(de::Id id) const
{
    auto found = d->subsectorsById.find(id);
    if (found != d->subsectorsById.end())
    {
        return found->second;
    }
    return nullptr;
}

const Blockmap &Map::mobjBlockmap() const
{
    if (bool(d->mobjBlockmap)) return *d->mobjBlockmap;
    /// @throw MissingBlockmapError  The mobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::mobjBlockmap", "Mobj blockmap is not initialized");
}

const Blockmap &Map::polyobjBlockmap() const
{
    if (bool(d->polyobjBlockmap)) return *d->polyobjBlockmap;
    /// @throw MissingBlockmapError  The polyobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::polyobjBlockmap", "Polyobj blockmap is not initialized");
}

const LineBlockmap &Map::lineBlockmap() const
{
    if (bool(d->lineBlockmap)) return *d->lineBlockmap;
    /// @throw MissingBlockmapError  The line blockmap is not yet initialized.
    throw MissingBlockmapError("Map::lineBlockmap", "Line blockmap is not initialized");
}

const Blockmap &Map::subspaceBlockmap() const
{
    if (bool(d->subspaceBlockmap)) return *d->subspaceBlockmap;
    /// @throw MissingBlockmapError  The subspace blockmap is not yet initialized.
    throw MissingBlockmapError("Map::subspaceBlockmap", "Convex subspace blockmap is not initialized");
}

LoopResult Map::forAllLinesTouchingMobj(mobj_t &mob, const std::function<LoopResult (Line &)>& func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&Mobj_Map(mob) == this && Mobj_IsLinked(mob) && mob.lineRoot)
    {
        List<Line *> linkStore;

        linknode_t *tn = d->mobjNodes.nodes;
        for (nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
        {
            linkStore.append(reinterpret_cast<Line *>(tn[nix].ptr));
        }

        for (dint i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

LoopResult Map::forAllSectorsTouchingMobj(mobj_t &mob, const std::function<LoopResult (Sector &)>& func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if (&Mobj_Map(mob) == this && Mobj_IsLinked(mob))
    {
        List<Sector *> linkStore;

        // Always process the mobj's own sector first.
        Sector &ownSec = *Mobj_BspLeafAtOrigin(mob).sectorPtr();
        ownSec.setValidCount(validCount);
        linkStore << &ownSec;

        // Any good lines around here?
        if (mob.lineRoot)
        {
            linknode_t *tn = d->mobjNodes.nodes;
            for (nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
            {
                auto *ld = reinterpret_cast<Line *>(tn[nix].ptr);

                // All these lines have sectors on both sides.
                // First, try the front.
                Sector &frontSec = ld->front().sector();
                if (frontSec.validCount() != validCount)
                {
                    frontSec.setValidCount(validCount);
                    linkStore.append(&frontSec);
                }

                // And then the back.
                /// @todo Above comment suggest always twosided, which is it? -ds
                if (ld->back().hasSector())
                {
                    Sector &backSec = ld->back().sector();
                    if (backSec.validCount() != validCount)
                    {
                        backSec.setValidCount(validCount);
                        linkStore.append(&backSec);
                    }
                }
            }
        }

        for (dint i = 0; i < linkStore.count(); ++i)
        {
            if (auto result = func(*linkStore[i]))
                return result;
        }
    }

    return LoopContinue;
}



void Map::unlink(Polyobj &polyobj)
{
    d->polyobjBlockmap->unlink(polyobj.bounds, &polyobj);
}

void Map::link(Polyobj &polyobj)
{
    d->polyobjBlockmap->link(polyobj.bounds, &polyobj);
}

LoopResult Map::forAllLinesInBox(const AABoxd &box, dint flags, std::function<LoopResult (Line &line)> func) const
{
    LoopResult result = LoopContinue;

    // Process polyobj lines?
    if ((flags & LIF_POLYOBJ) && polyobjCount())
    {
        const dint localValidCount = validCount;
        result = polyobjBlockmap().forAllInBox(box, [&func, &localValidCount] (void *object)
        {
            auto &pob = *reinterpret_cast<Polyobj *>(object);
            if (pob.validCount != localValidCount) // not yet processed
            {
                pob.validCount = localValidCount;
                for (Line *line : pob.lines())
                {
                    if (line->validCount() != localValidCount) // not yet processed
                    {
                        line->setValidCount(localValidCount);
                        if (auto result = func(*line))
                            return result;
                    }
                }
            }
            return LoopResult(); // continue
        });
    }

    // Process sector lines?
    if (!result && (flags & LIF_SECTOR))
    {
        const dint localValidCount = validCount;
        result = lineBlockmap().forAllInBox(box, [&func, &localValidCount] (void *object)
        {
            auto &line = *reinterpret_cast<Line *>(object);
            if (line.validCount() != localValidCount) // not yet processed
            {
                line.setValidCount(localValidCount);
                return func(line);
            }
            return LoopResult(); // continue
        });
    }

    return result;
}

void Map::link(mobj_t &mob, dint flags)
{
    world::Map::link(mob, flags);

    // If this is a player - perform additional tests to see if they have either entered or exited the void.
    if (mob.dPlayer && mob.dPlayer->mo)
    {
        auto &client = ClientApp::player(P_GetDDPlayerIdx(mob.dPlayer));
        client.inVoid = true;
        if (Mobj_HasSubsector(mob))
        {
            auto &subsec = Mobj_Subsector(mob).as<world::ClientSubsector>();
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

BspLeaf &Map::bspLeafAt(const Vec2d &point) const
{
    if (!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt", "No BSP data available");

    const BspTree *bspTree = d->bsp.tree;
    while (!bspTree->isLeaf())
    {
        auto &bspNode = bspTree->userData()->as<BspNode>();
        dint side     = bspNode.pointOnSide(point) < 0;

        // Descend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

BspLeaf &Map::bspLeafAt_FixedPrecision(const Vec2d &point) const
{
    if (!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt_FixedPrecision", "No BSP data available");

    fixed_t pointX[2] = { DBL2FIX(point.x), DBL2FIX(point.y) };

    const BspTree *bspTree = d->bsp.tree;
    while (!bspTree->isLeaf())
    {
        const auto &bspNode = bspTree->userData()->as<BspNode>();

        fixed_t lineOriginX[2]    = { DBL2FIX(bspNode.origin.x),    DBL2FIX(bspNode.origin.y) };
        fixed_t lineDirectionX[2] = { DBL2FIX(bspNode.direction.x), DBL2FIX(bspNode.direction.y) };
        dint side = V2x_PointOnLineSide(pointX, lineOriginX, lineDirectionX);

        // Decend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

String Map::elementSummaryAsStyledText() const
{
    String str;

#define TABBED(count, label) Stringf(_E(Ta) "  %i " _E(Tb) "%s\n", count, label)
    if (lineCount())    str += TABBED(lineCount(),    "Lines");
    //if (sideCount())    str += TABBED(sideCount(),    "Sides");
    if (sectorCount())  str += TABBED(sectorCount(),  "Sectors");
    if (vertexCount())  str += TABBED(vertexCount(),  "Vertexes");
    if (polyobjCount()) str += TABBED(polyobjCount(), "Polyobjs");
#undef TABBED

    return str.rightStrip();
}

String Map::objectSummaryAsStyledText() const
{
    dint thCountInStasis = 0;
    dint thCount = thinkers().count(&thCountInStasis);
    String str;

#define TABBED(count, label) Stringf(_E(Ta) "  %i " _E(Tb) "%s\n", count, label)
    if (thCount)           str += TABBED(thCount,            stringf("Thinkers (%i in stasis)", thCountInStasis).c_str());
#ifdef __CLIENT__
    if (generatorCount())  str += TABBED(generatorCount(),   "Generators");
    if (lumobjCount())     str += TABBED(lumobjCount(),      "Lumobjs");
#endif
#undef TABBED

    return str.rightStrip();
}

void Map::consoleRegister() // static
{
    world::Map::consoleRegister();

    Mobj_ConsoleRegister();
}

bool Map::endEditing()
{
    if (!world::Map::endEditing())
    {
        return false;
    }

    forAllSectors([](Sector &sector) {
        if (sector.visPlaneLinkTargetSector() != MapElement::NoIndex)
        {
            if (Sector *target = sectorsByArchiveIndex[sector.visPlaneLinkTargetSector()])
            {
                // Use the first subsector as the target.
                auto &targetSub = target->subsector(0).as<ClientSubsector>();

                // Linking is done for each subsector separately. (Necessary, though?)
                sector.forAllSubsectors([&targetSub, &sector](Subsector &sub) {
                    auto &clsub = sub.as<ClientSubsector>();
                    for (int plane = 0; plane < 2; ++plane)
                    {
                        if (sector.visPlaneLinked(plane))
                        {
                            clsub.linkVisPlane(plane, targetSub);
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
