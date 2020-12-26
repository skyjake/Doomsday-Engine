/** @file subsector.cpp  Client-side world map subsector.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "world/subsector.h"

#include "world/map.h"
#include "world/convexsubspace.h"
#include "world/p_object.h"
#include "world/p_players.h"
#include "world/surface.h"
#include "client/clskyplane.h"
#include "client/cledgeloop.h"
#include "render/rend_main.h" // Rend_SkyLightColor(), useBias
#include "render/lightdecoration.h"
#include "resource/materialanimator.h"
#include "render/walledge.h"
#include "dd_main.h"  // verbose

#include <doomsday/mesh/face.h>
#include <doomsday/world/blockmap.h>
#include <de/logbuffer.h>
#include <de/keymap.h>
#include <de/set.h>

using namespace de;
using mesh::HEdge;
using world::Material;
using world::MapElement;
using world::Sector;
using world::SubsectorCirculator;
using world::World;

/// Classification flags:
enum SubsectorFlag
{
    NeverMapped      = 0x01,
    AllMissingBottom = 0x02,
    AllMissingTop    = 0x04,
    AllSelfRef       = 0x08,
    PartSelfRef      = 0x10
};

using SubsectorFlags = Flags;

#ifdef DE_DEBUG
/**
 * Returns a textual, map-relative path for the given @a surface.
 */
static DotPath composeSurfacePath(const Surface &surface)
{
    DE_ASSERT(surface.hasParent());
    const auto &owner = surface.parent();

    switch (owner.type())
    {
    case DMU_PLANE:
        return Stringf("sector#%i.%s",
                              owner.as<Plane>().sector().indexInMap(),
                              world::Sector::planeIdAsText(owner.as<Plane>().indexInSector()).c_str());

    case DMU_SIDE:
        return Stringf(
            "line#%i.%s.%s",
            owner.as<LineSide>().line().indexInMap(),
            Line::sideIdAsText(owner.as<LineSide>().sideId()).c_str(),
            LineSide::sectionIdAsText(
                &surface == &owner.as<LineSide>().middle()
                    ? LineSide::Middle
                    : &surface == &owner.as<LineSide>().bottom() ? LineSide::Bottom : LineSide::Top)
                .c_str());

    default: return "";
    }
}
#endif // DE_DEBUG

/**
 * @todo optimize: Translation of decorations on the world up axis would be a trivial
 * operation to perform, which, would not require plotting decorations again. This
 * frequent case should be designed for. -ds
 */
DE_PIMPL(Subsector)

//, DE_OBSERVES(Subsector, Deletion)
//, DE_OBSERVES(Plane,  Deletion)
//, DE_OBSERVES(Sector, LightColorChange)
//, DE_OBSERVES(Sector, LightLevelChange)

, DE_OBSERVES(Line,    FlagsChange)
, DE_OBSERVES(Plane,   HeightChange)
, DE_OBSERVES(Plane,   HeightSmoothedChange)
, DE_OBSERVES(Surface, MaterialChange)
, DE_OBSERVES(Surface, OriginChange)
, DE_OBSERVES(Surface, OriginSmoothedChange)

, DE_OBSERVES(Material,  DimensionsChange)
, DE_OBSERVES(MaterialAnimator, DecorationStageChange)
{
    struct BoundaryData
    {
        struct EdgeLoops : public List<ClEdgeLoop *>
        {
            ~EdgeLoops() { clear(); }

            void clear()
            {
                deleteAll(*this);
                List<ClEdgeLoop *>::clear();
            }
        };

        std::unique_ptr<ClEdgeLoop> outerLoop;
        EdgeLoops innerLoops;

        void clear()
        {
            outerLoop.release();
            innerLoops.clear();
        }

        /**
         * Add @a loop to the data set (ownership is given).
         */
        BoundaryData &addEdgeLoop(ClEdgeLoop *loop)
        {
            if (loop)
            {
                if (loop->isOuter()) outerLoop.reset(loop);
                else                 innerLoops.append(loop);
            }
            return *this;
        }
    };

    struct GeometryData
    {
        MapElement *mapElement;
        int geomId;

        GeometryData(MapElement *mapElement, int geomId)
            : mapElement(mapElement)
            , geomId(geomId)
        {}
    };
    /// @todo Avoid two-stage lookup.
    typedef de::KeyMap<int, GeometryData *> Shards;
    struct GeometryGroups : public de::KeyMap<MapElement *, Shards>
    {
        ~GeometryGroups()
        {
            for (auto &g : *this)
            {
                g.second.deleteAll();
            }
        }
    };

    struct DecoratedSurface : public Surface::IDecorationState
    {
        List<Decoration *> decorations;
        bool               needUpdate = true;

        DecoratedSurface() {}

        ~DecoratedSurface() { deleteAll(decorations); }

        void markForUpdate(bool yes = true)
        {
            if (World::ddMapSetup) return;
            needUpdate = yes;
        }

        void clear()
        {
            markForUpdate(false);
            deleteAll(decorations);
            decorations.clear();
        }
    };

    int validFrame;
    bool hasWorldVolumeInValidFrame;
    bool hasInvisibleTop = false;

    bool           needClassify = true; ///< @c true= (Re)classification is necessary.
    SubsectorFlags flags        = 0;

    struct VisPlaneLink
    {
        VisPlaneLinkMode linkMode = LinkAlways;
        Subsector *      target   = nullptr;
        Subsector *      current  = nullptr; // current link in effect, perhaps nullptr
    };
    VisPlaneLink visPlaneLinks[2];

    std::unique_ptr<BoundaryData> boundaryData;
    GeometryGroups geomGroups;

    /// Subspaces in the neighborhood effecting environmental audio characteristics.
    typedef Set<ConvexSubspace *> ReverbSubspaces;
    ReverbSubspaces reverbSubspaces;

    /// Environmental audio config.
    AudioEnvironment reverb;
    bool needReverbUpdate = true;

    // Per surface lists of light decoration info and state.
    Set<Surface *> decorSurfaces;

    Impl(Public *i) : Base(i)
    {}

//    ~Impl() override
//    {
//        unlinkVisPlane(Sector::Floor);
//        unlinkVisPlane(Sector::Ceiling);
//    }

#if 0
    inline bool floorIsMapped()
    {
        return visPlaneLinks[Sector::Floor].current &&
               visPlaneLinks[Sector::Floor].current != thisPublic;
    }

    inline bool ceilingIsMapped()
    {
        return visPlaneLinks[Sector::Ceiling].current &&
               visPlaneLinks[Sector::Ceiling].current != thisPublic;
    }

    inline bool needRemapVisPlanes()
    {
        return visPlaneLinks[Sector::Floor].target == 0 || mappedVisCeiling == 0;
    }

    Subsector **mappedSubsectorAdr(int planeIdx)
    {
        if (planeIdx == Sector::Floor)   return &mappedVisFloor;
        if (planeIdx == Sector::Ceiling) return &mappedVisCeiling;
        return nullptr;
    }
#endif

    Plane *linkedPlane(dint planeIdx)
    {
        DE_ASSERT(planeIdx >= Sector::Floor && planeIdx <= Sector::Ceiling);
        const auto &visp = visPlaneLinks[planeIdx];
        if (visp.current)
        {
            return &visp.current->sector().plane(planeIdx).as<Plane>();
        }
        return nullptr;
    }

    void observeMaterial(Material *material, bool yes = true)
    {
        if (!material) return;

        MaterialAnimator &materialAnimator = material->as<ClientMaterial>().getAnimator(Rend_MapSurfaceMaterialSpec());
        if (yes)
        {
            material->audienceForDimensionsChange() += this;
            materialAnimator.audienceForDecorationStageChange += this;
        }
        else
        {
            materialAnimator.audienceForDecorationStageChange -= this;
            material->audienceForDimensionsChange() -= this;
        }
    }

    static bool hasDecoratedMaterial(const Surface &surface)
    {
        return surface.hasMaterial() && surface.material().as<ClientMaterial>().hasDecorations();
    }

    void observeSurface(Surface *surface, bool yes = true)
    {
        if (!surface) return;

        if (yes)
        {
            surface->audienceForMaterialChange()     += this;
            surface->audienceForOriginChange()       += this;
            surface->audienceForOriginSmoothedChange += this;

            if (hasDecoratedMaterial(*surface))
            {
                allocDecorationState(*surface);
            }
        }
        else
        {
            surface->audienceForOriginSmoothedChange -= this;
            surface->audienceForOriginChange()       -= this;
            surface->audienceForMaterialChange()     -= this;

            decorSurfaces.remove(surface);
        }
    }

    void observePlane(Plane *plane, bool yes = true, bool observeHeight = true)
    {
        if (!plane) return;

        if (yes)
        {
//            plane->audienceForDeletion() += this;
            if (observeHeight)
            {
                plane->audienceForHeightChange() += this;
                plane->audienceForHeightSmoothedChange += this;
            }
        }
        else
        {
            plane->audienceForHeightSmoothedChange -= this;
            plane->audienceForHeightChange() -= this;
//            plane->audienceForDeletion() -= this;
        }
    }

#if 0
    void observeSubsector(Subsector *subsec, bool yes = true)
    {
        if (!subsec || subsec == thisPublic)
            return;

        if (yes) subsec->audienceForDeletion += this;
        else     subsec->audienceForDeletion -= this;
    }
#endif

    void linkVisPlane(dint planeIdx, Subsector *newSubsector)
    {
        auto *visp = &visPlaneLinks[planeIdx];

        if (visp->current == newSubsector)
        {
            // Already linked there.
            return;
        }

        if (visp->current != thisPublic)
        {
            if (Plane *oldPlane = linkedPlane(planeIdx))
            {
                observeMaterial(oldPlane->surface().materialPtr(), false);
                observeSurface(&oldPlane->surface(), false);
                observePlane(oldPlane, false);
            }
        }
//        observeSubsector(visp->current, false);

        visp->current = newSubsector;

//        observeSubsector(visp->current);
        if (visp->current != thisPublic)
        {
            if (Plane *newPlane = linkedPlane(planeIdx))
            {
                observePlane(newPlane, true, true); // !permanent);
                observeSurface(&newPlane->surface(), true);
                observeMaterial(newPlane->surface().materialPtr(), true);
            }
        }
    }

    void unlinkVisPlane(dint planeIdx)
    {
        linkVisPlane(planeIdx, nullptr);
    }

    void relinkVisPlanes()
    {
        for (int planeIdx = 0; planeIdx < 2; ++planeIdx)
        {
            auto *visp = &visPlaneLinks[planeIdx];
            if (!visp->target) continue; // Not linked to anything.

            // The actual height of the plane.
            const double planeZ = self().sector().plane(planeIdx).as<Plane>().heightSmoothed();
            const coord_t targetZ = visp->target->visPlane(planeIdx).heightSmoothed();

            if ( visp->linkMode == LinkAlways ||
                (visp->linkMode == LinkWhenDifferentThanTarget && !fequal(planeZ, targetZ)) ||
                (visp->linkMode == LinkWhenLowerThanTarget     && planeZ < targetZ) ||
                (visp->linkMode == LinkWhenHigherThanTarget    && planeZ > targetZ))
            {
                linkVisPlane(planeIdx, visp->target);
            }
            else
            {
                unlinkVisPlane(planeIdx);
            }
        }
    }

#if 0
    /*
     * To be called when a plane moves to possibly invalidate mapped planes so
     * that they will be re-evaluated later.
     */
    void maybeInvalidateMapping(int planeIdx)
    {
        if (classification() & NeverMapped)
            return;

        Subsector **subsecAdr = mappedSubsectorAdr(planeIdx);
        if (!subsecAdr || *subsecAdr == thisPublic)
            return;

        unlinkVisPlane(planeIdx);

        if (classification() & (AllMissingBottom|AllMissingTop))
        {
            // Reclassify incase material visibility has changed.
            needClassify = true;
        }
    }
#endif

    /**
     * Returns a copy of the classification flags for the subsector, performing
     * classification of the subsector if necessary.
     */
    SubsectorFlags classification()
    {
        if (needClassify)
        {
            needClassify = false;

            flags &= ~(NeverMapped | PartSelfRef);
            flags |= AllSelfRef | AllMissingBottom | AllMissingTop;
            self().forAllSubspaces([this] (world::ConvexSubspace &subspace)
            {
                const auto *base  = subspace.poly().hedge();
                const auto *hedge = base;
                do
                {
                    if (!hedge->hasMapElement())
                        continue;

                    // This edge defines a section of a map line.

                    // If a back geometry is missing then never map planes.
                    if (!hedge->twin().hasFace())
                    {
                        flags |= NeverMapped;
                        flags &= ~(PartSelfRef | AllSelfRef | AllMissingBottom | AllMissingTop);
                        return LoopAbort;
                    }

                    if (!hedge->twin().face().hasMapElement())
                        continue;

                    const auto &backSpace = hedge->twin().face().mapElementAs<ConvexSubspace>();
                    // Subsector internal edges are not considered.
                    if (&backSpace.subsector() == thisPublic)
                        continue;

                    const LineSide &frontSide = hedge->mapElementAs<LineSideSegment>().lineSide().as<LineSide>();
                    const LineSide &backSide  = hedge->twin().mapElementAs<LineSideSegment>().lineSide().as<LineSide>();

                    // Similarly if no sections are defined for either side then
                    // never map planes. This can happen due to mapping errors
                    // where a group of one-sided lines facing outward in the
                    // void partly form a convex subspace.
                    if (!frontSide.hasSections() || !backSide.hasSections())
                    {
                        flags |= NeverMapped;
                        flags &= ~(PartSelfRef | AllSelfRef | AllMissingBottom | AllMissingTop);
                        return LoopAbort;
                    }

                    if (frontSide.line().isSelfReferencing())
                    {
                        flags |= PartSelfRef;
                        continue;
                    }

                    flags &= ~AllSelfRef;

                    if (frontSide.bottom().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingBottom;
                    }

                    if (frontSide.top().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingTop;
                    }

                    if (backSpace.subsector().sector().floor().height() < self().sector().floor().height()
                        && backSide.bottom().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingBottom;
                    }

                    if (backSpace.subsector().sector().ceiling().height() > self().sector().ceiling().height()
                        && backSide.top().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingTop;
                    }
                } while ((hedge = &hedge->next()) != base);

                return LoopContinue;
            });
        }

        return flags;
    }

    void initBoundaryDataIfNeeded()
    {
        if (boundaryData) return;

        boundaryData.reset(new BoundaryData);
        List<HEdge *> neighbors = self().listUniqueBoundaryEdges();
        if (neighbors.count() == 1)
        {
            // Single neighbor => one implicit loop.
            boundaryData->addEdgeLoop(new ClEdgeLoop(self(), *neighbors.first()));
        }
        else
        {
            // Multiple neighbors require testing of bounding boxes.
            DE_ASSERT(!neighbors.isEmpty());

            List<Rectanglef> boundaries;
            for (auto *base : neighbors)
            {
                Rectanglef bounds;
                SubsectorCirculator it(base);
                do
                {
                    bounds |= Rectanglef(Vec2f(it->origin().x, it->origin().y),
                                         Vec2f(it->twin().origin().x, it->twin().origin().y))
                        .normalized();
                } while (&it.next() != base);

                boundaries << bounds;
            }

            const Rectanglef *largest = nullptr;
            for (const Rectanglef &boundary : boundaries)
            {
                if (!largest || boundary.contains(*largest))
                    largest = &boundary;
            }

            for (int i = 0; i < neighbors.count(); ++i)
            {
                Rectanglef &boundary = boundaries[i];
                HEdge *     hedge    = neighbors[i];

                boundaryData->addEdgeLoop(new ClEdgeLoop(
                    self(), *hedge, !(&boundary == largest || (largest && boundary == *largest))));
            }
        }
    }

#if 0
    void remapVisPlanes()
    {
        /// @todo Has performance issues -- disabled; needs reworking.

        // By default both planes are mapped to the parent sector.
        if (!floorIsMapped())   map(Sector::Floor,   thisPublic);
        if (!ceilingIsMapped()) map(Sector::Ceiling, thisPublic);

        if (classification() & NeverMapped)
            return;

        if (classification() & (AllSelfRef | PartSelfRef))
        {
            // Should we permanently map one or both planes to those of another sector?
            self().forAllEdgeLoops([this] (const ClEdgeLoop &loop)
            {
                auto &backSubsec = loop.backSubsector().as<Subsector>();
                if (loop.isSelfReferencing()
                    && (classification() & AllSelfRef)
                    && !(backSubsec.d->classification() & AllSelfRef)
                    && backSubsec.d->mappedVisFloor != thisPublic)
                {
                    // Setup the mapping and we're done.
                    map(Sector::Floor  , &backSubsec, true /*permanently*/);
                    map(Sector::Ceiling, &backSubsec, true /*permanently*/);
                    return LoopAbort;
                }
                else
                {
                    return LoopContinue;
                }
            });

            if (floorIsMapped())
            {
                // Remove the mapping from all inner subsectors to this, forcing
                // their re-evaluation (however next time a different subsector
                // will be selected from the boundary).
                for (ClEdgeLoop *loop : boundaryData->innerLoops)
                {
                    if (loop->isSelfReferencing())
                    {
                        auto &backSubsec = loop->backSubsector().as<Subsector>();
                        if ((classification() & AllSelfRef)
                            && !(backSubsec.d->classification() & AllSelfRef))
                        {
                            if (backSubsec.d->mappedVisFloor == thisPublic)
                            {
                                backSubsec.d->unlinkVisPlane(Sector::Floor);
                            }
                            if (backSubsec.d->mappedVisCeiling == thisPublic)
                            {
                                backSubsec.d->unlinkVisPlane(Sector::Ceiling);
                            }
                        }
                    }
                }

                // Permanent mappings won't be remapped.
                return;
            }
        }

        if (classification() & AllSelfRef)
            return;

        //
        // Dynamic mapping may be needed for one or more planes.
        //

        // The sector must have open space.
        if (self().sector().ceiling().height() <= self().sector().floor().height())
            return;

        bool doFloor   =   !floorIsMapped() && classification().testFlag(AllMissingBottom);
        bool doCeiling = !ceilingIsMapped() && classification().testFlag(AllMissingTop);

        if (!doFloor && !doCeiling)
            return;

        // Map "this" subsector to the first outer subsector found.
        initBoundaryDataIfNeeded();
        DE_ASSERT(boundaryData->outerLoop);
        {
            const ClEdgeLoop &loop = *boundaryData->outerLoop;
            if (loop.hasBackSubsector())
            {
                auto &backSubsec = loop.backSubsector().as<Subsector>();
                SubsectorCirculator it(&loop.first());
                do
                {
                    if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
                    {
                        if (doFloor && !floorIsMapped()
                            && !backSubsec.hasSkyFloor()
                            && backSubsec.visFloor().height() > self().sector().floor().height())
                        {
                            map(Sector::Floor, &backSubsec);
                            if (!doCeiling) break;
                        }
                        if (doCeiling && !ceilingIsMapped()
                            && !backSubsec.hasSkyCeiling()
                            && backSubsec.visCeiling().height() < self().sector().ceiling().height())
                        {
                            map(Sector::Ceiling, &backSubsec);
                            if (!doFloor) break;
                        }
                    }
                } while (&it.next() != &loop.first());
            }
        }

        if (!floorIsMapped() && !ceilingIsMapped())
            return;

        // Clear mappings for all inner subsectors to force re-evaluation (which
        // may in turn lead to their inner subsectors being re-evaluated, producing
        // a "ripple effect" that will remap any deeply nested dependents).
        for (const ClEdgeLoop *loop : boundaryData->innerLoops)
        {
            if (loop->hasBackSubsector())
            {
                auto &backSubsec = loop->backSubsector().as<Subsector>();
                SubsectorCirculator it(&loop->first());
                do
                {
                    if (it->hasMapElement() // BSP errors may fool the circulator wrt interior edges -ds
                        && !(backSubsec.d->classification() & NeverMapped))
                    {
                        if (doFloor && floorIsMapped()
                            && backSubsec.visFloor().height() >= self().sector().floor().height())
                        {
                            backSubsec.d->unlinkVisPlane(Sector::Floor);
                        }
                        if (doCeiling && ceilingIsMapped()
                            && backSubsec.visCeiling().height() <= self().sector().ceiling().height())
                        {
                            backSubsec.d->unlinkVisPlane(Sector::Ceiling);
                        }
                    }
                } while (&it.next() != &loop->first());
            }
        }
    }
#endif

#if 0
    void updateBiasForWallSectionsAfterGeometryMove(HEdge *hedge)
    {
        if (!hedge) return;
        if (!hedge->hasMapElement()) return;

        MapElement *mapElement = &hedge->mapElement();
        if (Shard *shard = self().findShard(*mapElement, LineSide::Middle))
        {
            shard->updateBiasAfterMove();
        }
        if (Shard *shard = self().findShard(*mapElement, LineSide::Bottom))
        {
            shard->updateBiasAfterMove();
        }
        if (Shard *shard = self().findShard(*mapElement, LineSide::Top))
        {
            shard->updateBiasAfterMove();
        }
    }
#endif

#if 0
    /**
     * Find the GeometryData for a MapElement by the element-unique @a group identifier.
     *
     * @param geomId    Geometry identifier.
     *
     * @param canAlloc  @c true= to allocate if no data exists. Note that the number of
     * vertices in the fan geometry must be known at this time.
     */
    GeometryData *geomData(MapElement &mapElement, int geomId, bool canAlloc = false)
    {
        auto foundGroup = geomGroups.find(&mapElement);
        if (foundGroup != geomGroups.end())
        {
            Shards &shards = foundGroup->second;
            auto found = shards.find(geomId);
            if (found != shards.end())
            {
                return found->second;
            }
        }

        if (!canAlloc) return nullptr;

        if (foundGroup == geomGroups.end())
        {
            foundGroup = geomGroups.insert(&mapElement, Shards());
        }

        return foundGroup->second.insert(geomId, new GeometryData(&mapElement, geomId))->second;
    }

    /**
     * Find the GeometryData for the given @a shard.
     */
    GeometryData *geomDataForShard(Shard *shard)
    {
        if (shard && shard->subsector() == thisPublic)
        {
            auto found = shardToGeomData.find(shard);
            if (found != shardToGeomData.end()) return *found;
        }
        return nullptr;
    }
#endif

    void addReverbSubspace(ConvexSubspace *subspace)
    {
        if (!subspace) return;
        reverbSubspaces.insert(subspace);
    }

    /**
     * Perform environmental audio (reverb) initialization.
     *
     * Determines the subspaces which contribute to the environmental audio
     * characteristics. Given that subspaces do not change shape (on the XY plane,
     * that is), they do not move and are not created/destroyed once the map has
     * been loaded; this step can be pre-processed.
     *
     * @pre The Map's BSP leaf blockmap must be ready for use.
     */
    void findReverbSubspaces()
    {
        const Map &map = self().sector().map().as<Map>();

        AABoxd box = self().bounds();
        box.minX -= 128;
        box.minY -= 128;
        box.maxX += 128;
        box.maxY += 128;

        // Link all convex subspaces whose axis-aligned bounding box intersects
        // with the affection bounds to the reverb set.
        const int localValidCount = ++World::validCount;
        map.subspaceBlockmap().forAllInBox(box, [this, &box, &localValidCount] (void *object)
        {
            auto &sub = *(ConvexSubspace *)object;
            if (sub.validCount() != localValidCount) // not yet processed
            {
                sub.setValidCount(localValidCount);

                // Check the bounds.
                const AABoxd &polyBounds = sub.poly().bounds();
                if (!(   polyBounds.maxX < box.minX
                      || polyBounds.minX > box.maxX
                      || polyBounds.minY > box.maxY
                      || polyBounds.maxY < box.minY))
                {
                    addReverbSubspace(&sub);
                }
            }
            return LoopContinue;
        });
    }

    /**
     * Recalculate environmental audio (reverb) for the sector.
     */
    void updateReverb()
    {
        // Need to initialize?
        if (reverbSubspaces.isEmpty())
        {
            findReverbSubspaces();
        }

        needReverbUpdate = false;

        duint spaceVolume = int((self().visCeiling().height() - self().visFloor().height())
                          * self().roughArea());

        reverb.reset();

        for (ConvexSubspace *subspace : reverbSubspaces)
        {
            if (subspace->updateAudioEnvironment())
            {
                const auto &aenv = subspace->audioEnvironment();

                reverb.space   += aenv.space;

                reverb.volume  += aenv.volume  / 255.0f * aenv.space;
                reverb.decay   += aenv.decay   / 255.0f * aenv.space;
                reverb.damping += aenv.damping / 255.0f * aenv.space;
            }
        }

        float spaceScatter;

        if (reverb.space)
        {
            spaceScatter = spaceVolume / reverb.space;

            // These three are weighted by the space.
            reverb.volume  /= reverb.space;
            reverb.decay   /= reverb.space;
            reverb.damping /= reverb.space;
        }
        else
        {
            spaceScatter = 0;

            reverb.volume  = .2f;
            reverb.decay   = .4f;
            reverb.damping = 1;
        }

        // If the space is scattered, the reverb effect lessens.
        reverb.space /= (spaceScatter > .8 ? 10 : spaceScatter > .6 ? 4 : 1);

        // Normalize the reverb space [0..1]
        //   0= very small
        // .99= very large
        // 1.0= only for open areas (special case).
        reverb.space /= 120e6;
        if (reverb.space > .99)
            reverb.space = .99f;

        if (self().hasSkyPlane())
        {
            // An "exterior" space.
            // It can still be small, in which case; reverb is diminished a bit.
            if (reverb.space > .5)
                reverb.volume = 1;    // Full volume.
            else
                reverb.volume = .5f;  // Small, but still open.

            reverb.space = 1;
        }
        else
        {
            // An "interior" space.
            // Large spaces have automatically a bit more audible reverb.
            reverb.volume += reverb.space / 4;
        }

        if (reverb.volume > 1)
            reverb.volume = 1;
    }

    bool prepareGeometry(Surface &surface, Vec3d &topLeft, Vec3d &bottomRight,
                         Vec2f &materialOrigin) const
    {
        if (surface.parent().type() == DMU_SIDE)
        {
            auto &side = surface.parent().as<LineSide>();
            int section = &side.middle() == &surface ? LineSide::Middle
                         : &side.bottom() == &surface ? LineSide::Bottom
                         :                              LineSide::Top;

            if (!side.hasSections()) return false;

            HEdge *leftHEdge = side.leftHEdge();
            HEdge *rightHEdge = side.rightHEdge();

            if (!leftHEdge || !rightHEdge) return false;

            // Is the wall section potentially visible?
            const WallSpec wallSpec = WallSpec::fromMapSide(side, section);
            WallEdge leftEdge(wallSpec, *leftHEdge, Line::From);
            WallEdge rightEdge(wallSpec, *rightHEdge, Line::To);

            if (!leftEdge.isValid() || !rightEdge.isValid()
                || de::fequal(leftEdge.bottom().z(), rightEdge.top().z()))
                return false;

            topLeft = leftEdge.top().origin();
            bottomRight = rightEdge.bottom().origin();
            materialOrigin = -leftEdge.materialOrigin();

            return true;
        }

        if (surface.parent().type() == DMU_PLANE)
        {
            auto &plane = surface.parent().as<Plane>();
            const AABoxd &sectorBounds = plane.sector().bounds();

            topLeft = Vec3d(sectorBounds.minX,
                               plane.isSectorFloor() ? sectorBounds.maxY : sectorBounds.minY,
                               plane.heightSmoothed());

            bottomRight = Vec3d(sectorBounds.maxX,
                                   plane.isSectorFloor() ? sectorBounds.minY : sectorBounds.maxY,
                                   plane.heightSmoothed());

            materialOrigin = Vec2f(-fmod(sectorBounds.minX, 64), -fmod(sectorBounds.minY, 64))
                           - surface.originSmoothed();

            return true;
        }

        return false;
    }

    void projectDecorations(Surface &suf, MaterialAnimator &matAnimator,
                            const Vec2f &materialOrigin, const Vec3d &topLeft,
                            const Vec3d &bottomRight)
    {
        Vec3d delta = bottomRight - topLeft;
        if (de::fequal(delta.length(), 0)) return;

        ClientMaterial &material = matAnimator.material();
        const int axis = suf.normal().maxAxis();

        Vec2d sufDimensions;
        if (axis == 0 || axis == 1)
        {
            sufDimensions.x = std::sqrt(de::squared(delta.x) + de::squared(delta.y));
            sufDimensions.y = delta.z;
        }
        else
        {
            sufDimensions.x = std::sqrt(de::squared(delta.x));
            sufDimensions.y = delta.y;
        }

        if (sufDimensions.x < 0) sufDimensions.x = -sufDimensions.x;
        if (sufDimensions.y < 0) sufDimensions.y = -sufDimensions.y;

        // Generate a number of decorations.
        int decorIndex = 0;

        material.forAllDecorations([this, &suf, &matAnimator, &materialOrigin
                                   , &topLeft
                                   , &delta, &axis, &sufDimensions, &decorIndex]
                                   (MaterialDecoration &decor)
        {
            const Vec2ui &matDimensions = matAnimator.material().dimensions();
            const MaterialAnimator::Decoration &decorSS = matAnimator.decoration(decorIndex);

            // Skip values must be at least one.
            Vec2i skip = Vec2i(decor.patternSkip().x + 1, decor.patternSkip().y + 1)
                .max(Vec2i(1, 1));

            Vec2f repeat = skip.toVec2ui() * matDimensions;
            if (repeat == Vec2f(0, 0))
                return LoopAbort;

            Vec3d origin = topLeft + suf.normal() * decorSS.elevation();

            float s = de::wrap(decorSS.origin().x - matDimensions.x * decor.patternOffset().x + materialOrigin.x,
                                0.f, repeat.x);

            // Plot decorations.
            for (; s < sufDimensions.x; s += repeat.x)
            {
                // Determine the topmost point for this row.
                float t = de::wrap(decorSS.origin().y - matDimensions.y * decor.patternOffset().y + materialOrigin.y,
                                    0.f, repeat.y);

                for (; t < sufDimensions.y; t += repeat.y)
                {
                    const auto offset = Vec2f(s, t) / sufDimensions;
                    Vec3d patternOffset(offset.x,
                                           axis == 2 ? offset.y : offset.x,
                                           axis == 2 ? offset.x : offset.y);

                    Vec3d decorOrigin = origin + delta * patternOffset;
                    // The point must be in the correct subsector.
                    if (suf.map().subsectorAt(decorOrigin) == thisPublic)
                    {
                        std::unique_ptr<LightDecoration> decor(new LightDecoration(decorSS, decorOrigin));
                        decor->setSurface(&suf);
                        if (self().sector().hasMap()) decor->setMap(&self().sector().map());
                        static_cast<DecoratedSurface *>(suf.decorationState())->
                                decorations.append(decor.get()); // take ownership.
                        decor.release();
                    }
                }
            }

            decorIndex += 1;
            return LoopContinue;
        });
    }

    DecoratedSurface &allocDecorationState(Surface &surface)
    {
        auto *ds = static_cast<DecoratedSurface *>(surface.decorationState());
        if (!ds)
        {
            surface.setDecorationState(ds = new DecoratedSurface);
            decorSurfaces.insert(&surface);
        }
        return *ds;
    }

    void decorate(Surface &surface)
    {       
        if (!hasDecoratedMaterial(surface))
        {
            // Just clear the state.
            if (surface.decorationState())
            {
                static_cast<DecoratedSurface *>(surface.decorationState())->clear();
            }
            return;
        }

        // Has a decorated material, so needs decoration state.
        auto &ds = allocDecorationState(surface);

        if (!ds.needUpdate) return;

        LOGDEV_MAP_XVERBOSE_DEBUGONLY("  decorating %s%s"
            , composeSurfacePath(surface)
                << (surface.parent().type() == DMU_PLANE
                    && &surface.parent() == linkedPlane(surface.parent().as<Plane>().indexInSector()) ? " (mapped)" : "")
        );

        ds.clear();

        // Clear any existing decorations.
        deleteAll(ds.decorations);
        ds.decorations.clear();

        if (surface.hasMaterial())
        {
            Vec2f materialOrigin;
            Vec3d bottomRight, topLeft;
            if (prepareGeometry(surface, topLeft, bottomRight, materialOrigin))
            {
                MaterialAnimator &animator = *surface.materialAnimator();
                projectDecorations(surface, animator, materialOrigin, topLeft, bottomRight);
            }
        }

        ds.needUpdate = false;
    }

    void markDependentSurfacesForRedecoration(Plane &plane, bool yes = true)
    {
        if (World::ddMapSetup) return;

        LOGDEV_MAP_XVERBOSE_DEBUGONLY("Marking [%p] (sector: %i) for redecoration..."
            , thisPublic << self().sector().indexInMap()
        );

        // Mark surfaces of the edge loops.
        self().forAllEdgeLoops([this, &plane, &yes] (const ClEdgeLoop &loop)
        {
            SubsectorCirculator it(&loop.first());
            do
            {
                if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
                {
                    if (    &plane == &self().visPlane(plane.indexInSector())
                        || (&plane == (it->hasTwin() && it->twin().hasFace()
                                       ? &it->twin().face().mapElementAs<ConvexSubspace>()
                                                  .subsector().as<Subsector>().visPlane(plane.indexInSector())
                                       : nullptr)))
                    {
                        auto &side = it->mapElementAs<LineSideSegment>().lineSide();
                        side.forAllSurfaces([&yes] (world::Surface &surface)
                        {
                            LOGDEV_MAP_XVERBOSE_DEBUGONLY("  ", composeSurfacePath(surface));
                            if (auto *decor = surface.decorationState())
                            {
                                static_cast<DecoratedSurface *>(decor)->markForUpdate(yes);
                            }
                            return LoopContinue;
                        });
                    }
                }
            } while (&it.next() != &loop.first());
            return LoopContinue;
        });

        if (&plane == &self().visPlane(plane.indexInSector()))
        {
            LOGDEV_MAP_XVERBOSE_DEBUGONLY("  ", composeSurfacePath(plane.surface()));
            if (auto *decor = plane.surface().decorationState())
            {
                static_cast<DecoratedSurface *>(decor)->markForUpdate(yes);
            }
        }
    }

    void markDependentSurfacesForRedecoration(Material &material, bool yes = true)
    {
        if (World::ddMapSetup) return;

        LOGDEV_MAP_XVERBOSE_DEBUGONLY("Marking [%p] (sector: %i) for redecoration..."
            , thisPublic << self().sector().indexInMap()
        );

        // Surfaces of the edge loops.
        self().forAllEdgeLoops([&material, &yes] (const ClEdgeLoop &loop)
        {
            SubsectorCirculator it(&loop.first());
            do
            {
                if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
                {
                    auto &side = it->mapElementAs<LineSideSegment>().lineSide();
                    side.forAllSurfaces([&material, &yes] (world::Surface &surface)
                    {
                        if (surface.materialPtr() == &material)
                        {
                            LOGDEV_MAP_XVERBOSE_DEBUGONLY("  ", composeSurfacePath(surface));
                            if (auto *decor = surface.decorationState())
                            {
                                static_cast<DecoratedSurface *>(decor)->markForUpdate(yes);
                            }
                        }
                        return LoopContinue;
                    });
                }
            } while (&it.next() != &loop.first());
            return LoopContinue;
        });

        // Surfaces of the visual planes.
        Plane &floor = self().visFloor();
        if (floor.surface().materialPtr() == &material)
        {
            LOGDEV_MAP_XVERBOSE_DEBUGONLY("  ", composeSurfacePath(floor.surface()));
            if (auto *decor = floor.surface().decorationState())
            {
                static_cast<DecoratedSurface *>(decor)->markForUpdate(yes);
            }
        }
        Plane &ceiling = self().visCeiling();
        if (ceiling.surface().materialPtr() == &material)
        {
            LOGDEV_MAP_XVERBOSE_DEBUGONLY("  ", composeSurfacePath(ceiling.surface()));
            if (auto *decor = ceiling.surface().decorationState())
            {
                static_cast<DecoratedSurface *>(decor)->markForUpdate(yes);
            }
        }
    }

    void lineFlagsChanged(world::Line &line, int oldFlags) override
    {
        LOG_AS("Subsector");
        line.forAllSides([this, &oldFlags] (world::LineSide &side)
        {
            if (side.sectorPtr() == &self().sector())
            {
                if ((side.line().flags() & DDLF_DONTPEGTOP) != (oldFlags & DDLF_DONTPEGTOP))
                {
                    if (auto *decor = side.top().decorationState())
                    {
                        static_cast<DecoratedSurface *>(decor)->markForUpdate();
                    }
                }
                if ((side.line().flags() & DDLF_DONTPEGBOTTOM) != (oldFlags & DDLF_DONTPEGBOTTOM))
                {
                    if (auto *decor = side.bottom().decorationState())
                    {
                        static_cast<DecoratedSurface *>(decor)->markForUpdate();
                    }
                }
            }
            return LoopContinue;
        });
    }

    void materialDimensionsChanged(Material &material) override
    {
        LOG_AS("Subsector");
        markDependentSurfacesForRedecoration(material);
    }

    void materialAnimatorDecorationStageChanged(MaterialAnimator &animator) override
    {
        LOG_AS("Subsector");
        markDependentSurfacesForRedecoration(animator.material());
    }

#if 0
    void planeBeingDeleted(const world::Plane &plane) override
    {
        LOG_AS("Subsector");
        if (&plane == linkedPlane(plane.indexInSector()))
        {
            unlinkVisPlane(plane.indexInSector());
            decorSurfaces.remove(maybeAs<Surface>(plane.surfacePtr()));
        }
    }
#endif

    void planeHeightChanged(world::Plane &plane) override
    {
        LOG_AS("Subsector");

        // We may need to update one or both mapped planes.
//        maybeInvalidateMapping(plane.indexInSector());

        relinkVisPlanes();

        /*
        // We may need to fix newly revealed missing materials.
        self().forAllEdgeLoops([] (ClEdgeLoop &loop)
        {
            loop.fixSurfacesMissingMaterials();
            return LoopContinue;
        });
        */

        // We may need to project new decorations.
        markDependentSurfacesForRedecoration(plane.as<Plane>());

        const bool planeIsInterior = (&plane == &self().visPlane(plane.indexInSector()));
        if (planeIsInterior)
        {
            // We'll need to recalculate environmental audio characteristics.
            needReverbUpdate = true;

            // Check if there are any camera players in the subsector. If their height
            // is now above the ceiling/below the floor they are now in the void.
            DoomsdayApp::players().forAll([this] (Player &plr)
            {
                const ddplayer_t &ddpl = plr.publicData();
                if (plr.isInGame()
                    && (ddpl.flags & DDPF_CAMERA)
                    && Mobj_SubsectorPtr(*ddpl.mo) == thisPublic
                    && (   ddpl.mo->origin[2] > self().visCeiling().height() - 4
                        || ddpl.mo->origin[2] < self().visFloor  ().height()))
                {
                    plr.as<ClientPlayer>().inVoid = true;
                }
                return LoopContinue;
            });
        }
    }

    void planeHeightSmoothedChanged(Plane &plane) override
    {
        LOG_AS("Subsector");

        // We may need to update one or both mapped planes.
//        maybeInvalidateMapping(plane.indexInSector());

        relinkVisPlanes();

        // We may need to project new decorations.
        markDependentSurfacesForRedecoration(plane);
    }

    /// Observes Sector LightLevelChange.
    void surfaceMaterialChanged(world::Surface &surface) override
    {
        LOG_AS("Subsector");

        if (auto *ds = static_cast<DecoratedSurface *>(surface.decorationState()))
        {
            // Clear any existing decorations (now invalid).
            ds->clear();
//            qDeleteAll(ds->decorations);
//            ds->decorations.clear();
//            ds->markForUpdate();
        }

        if (hasDecoratedMaterial(surface))
        {
            auto &ds = allocDecorationState(surface.as<Surface>());
            ds.markForUpdate();
        }

        // Begin observing the new material (if any).
        //
        // Note that the subsector keeps observing all the materials of all surfaces.
        // To stop observing old ones, one needs to make sure that no surface in the
        // subsector is no longer using the material.
        //
        /// @todo Stop observing unused materials.
        //
        observeMaterial(surface.materialPtr());
    }

    void surfaceOriginChanged(world::Surface &surface) override
    {
        LOG_AS("Subsector");
        if (surface.hasMaterial())
        {
            allocDecorationState(surface.as<Surface>()).markForUpdate();
        }
    }

    void surfaceOriginSmoothedChanged(Surface &surface) override
    {
        LOG_AS("Subsector");
        if (surface.hasMaterial())
        {
            allocDecorationState(surface).markForUpdate();
        }
    }

#if 0
    void subsectorBeingDeleted(const world::Subsector &subsec) override
    {
        if (visPlaneLinks[Sector::Floor  ].current == &subsec) unlinkVisPlane(Sector::Floor);
        if (visPlaneLinks[Sector::Ceiling].current == &subsec) unlinkVisPlane(Sector::Ceiling);
    }
#endif
};

Subsector::Subsector(const List<world::ConvexSubspace *> &subspaces)
    : world::Subsector(subspaces)
    , d(new Impl(this))
{
    // Observe changes to surfaces in the subsector.
    forAllSubspaces([this] (world::ConvexSubspace &subspace)
    {
        auto *hedge = subspace.poly().hedge();
        do
        {
            if (hedge->hasMapElement())
            {
                auto &front = hedge->mapElementAs<LineSideSegment>().lineSide();

                // Line flags affect material offsets so observe those, too.
                front.line().audienceForFlagsChange += d;

                front.forAllSurfaces([this] (world::Surface &surface)
                {
                    d->observeSurface(&surface.as<Surface>());
                    d->observeMaterial(surface.materialPtr());
                    return LoopContinue;
                });

                /// @todo Ignorant of mappings -ds
                if (front.back().hasSector())
                {
                    Sector &backsec = front.back().sector();
                    d->observePlane(&backsec.floor().as<Plane>());
                    d->observePlane(&backsec.ceiling().as<Plane>());
                }
            }
        } while ((hedge = &hedge->next()) != subspace.poly().hedge());

        return LoopContinue;
    });

    // Observe changes to planes in the sector.
    Plane &floor = sector().floor().as<Plane>();
    d->observePlane(&floor);
    d->observeSurface(&floor.surface());
    d->observeMaterial(floor.surface().materialPtr());

    Plane &ceiling = sector().ceiling().as<Plane>();
    d->observePlane(&ceiling);
    d->observeSurface(&ceiling.surface());
    d->observeMaterial(ceiling.surface().materialPtr());
}

String Subsector::description() const
{
    auto desc = Stringf(_E(l) "%s: " _E(.) _E(i) "Sector %i%s" _E(.) " " _E(l) "%s: " _E(.)
                                   _E(i) "Sector %i%s" _E(.),
                               Sector::planeIdAsText(Sector::Floor).upperFirstChar().c_str(),
                               visFloor().sector().indexInMap(),
                               &visFloor() != &sector().floor() ? " (mapped)" : "",
                               Sector::planeIdAsText(Sector::Ceiling).upperFirstChar().c_str(),
                               visCeiling().sector().indexInMap(),
                               &visCeiling() != &sector().ceiling() ? " (mapped)" : "");

    if (d->boundaryData)
    {
        desc += Stringf(_E(D) "\nEdge loops (%i):" _E(.), edgeLoopCount());
        int index = 0;
        forAllEdgeLoops([&desc, &index] (const ClEdgeLoop &loop)
        {
            desc += Stringf("\n[%i]: ", index) + _E(>) + loop.description() + _E(<);
            index += 1;
            return LoopContinue;
        });
    }

    if (hasDecorations())
    {
        desc += String(_E(D) "\nDecorations:" _E(.));
        int decorIndex = 0;
        for (Surface *surface : d->decorSurfaces)
        {
            for (Decoration *decor : static_cast<Impl::DecoratedSurface *>
                                     (surface->decorationState())->decorations)
            {
                desc += Stringf("\n[%i]: ", decorIndex) + _E(>) + decor->description() + _E(<);
                decorIndex += 1;
            }
        }
    }

    DE_DEBUG_ONLY(
        desc.prepend(Stringf(_E(b) "Subsector " _E(.) "[%p]\n", this));
    )
    return world::Subsector::description() + "\n" + desc;
}

String Subsector::edgeLoopIdAsText(int loopId) // static
{
    switch (loopId)
    {
    case OuterLoop: return "outer";
    case InnerLoop: return "inner";

    default:
        DE_ASSERT_FAIL("Subsector::edgeLoopIdAsText: Invalid loopId");
        throw Error("Subsector::edgeLoopIdAsText", stringf("Unknown loop ID %i", loopId));
    }
}

int Subsector::edgeLoopCount() const
{
    d->initBoundaryDataIfNeeded();
    return (bool(d->boundaryData->outerLoop) ? 1 : 0) + d->boundaryData->innerLoops.count();
}

LoopResult Subsector::forAllEdgeLoops(const std::function<LoopResult (ClEdgeLoop &)> &func)
{
    d->initBoundaryDataIfNeeded();
    DE_ASSERT(d->boundaryData->outerLoop);
    {
        if (auto result = func(*d->boundaryData->outerLoop))
            return result;
    }
    for (ClEdgeLoop *loop : d->boundaryData->innerLoops)
    {
        if (auto result = func(*loop))
            return result;
    }
    return LoopContinue;
}

LoopResult Subsector::forAllEdgeLoops(const std::function<LoopResult (const ClEdgeLoop &)> &func) const
{
    d->initBoundaryDataIfNeeded();
    DE_ASSERT(d->boundaryData->outerLoop);
    {
        if (auto result = func(*d->boundaryData->outerLoop))
            return result;
    }
    for (ClEdgeLoop *loop : d->boundaryData->innerLoops)
    {
        if (auto result = func(*loop))
            return result;
    }
    return LoopContinue;
}

bool Subsector::hasSkyPlane(int planeIndex) const
{
    if (planeIndex < 0)
    {
        for (int i = 0; i < sector().planeCount(); ++i)
        {
            if (visPlane(i).surface().hasSkyMaskedMaterial())
                return true;
        }
        return false;
    }
    else
    {
        return visPlane(planeIndex).surface().hasSkyMaskedMaterial();
    }
}

bool Subsector::hasSkyFloor() const
{
    return hasSkyPlane(Sector::Floor);
}

bool Subsector::hasSkyCeiling() const
{
    return hasSkyPlane(Sector::Ceiling);
}

void Subsector::linkVisPlane(int planeIndex, Subsector &target, VisPlaneLinkMode linkMode)
{
    DE_ASSERT(planeIndex >= Sector::Floor && planeIndex <= Sector::Ceiling);

    d->visPlaneLinks[planeIndex].target   = &target;
    d->visPlaneLinks[planeIndex].linkMode = linkMode;

    d->relinkVisPlanes();
}

int Subsector::visPlaneCount() const
{
    return sector().planeCount();
}

Plane &Subsector::visPlane(int planeIndex)
{
    return const_cast<Plane &>(const_cast<const Subsector *>(this)->visPlane(planeIndex));
}

const Plane &Subsector::visPlane(int planeIndex) const
{
    if (planeIndex >= Sector::Floor && planeIndex <= Sector::Ceiling)
    {
#if 0
        // Time to remap the planes?
        if (d->needRemapVisPlanes())
        {
            /// @todo This is broken. For instance in ICARUS.WAD map09, locks up for several
            /// seconds while looping through the map. A better algorithm is needed.
            //d->remapVisPlanes();
        }
#endif

        auto *mapping = d->visPlaneLinks[planeIndex].current;
        if (mapping && mapping != this)
        {
            return mapping->visPlane(planeIndex);
        }
    }
    // Not mapped.
    return sector().plane(planeIndex).as<Plane>();
}

LoopResult Subsector::forAllVisPlanes(const std::function<LoopResult (Plane &)>& func)
{
    for (int i = 0; i < visPlaneCount(); ++i)
    {
        if (auto result = func(visPlane(i)))
            return result;
    }
    return LoopContinue;
}

LoopResult Subsector::forAllVisPlanes(const std::function<LoopResult (const Plane &)>& func) const
{
    for (int i = 0; i < visPlaneCount(); ++i)
    {
        if (auto result = func(visPlane(i)))
            return result;
    }
    return LoopContinue;
}

bool Subsector::isHeightInVoid(double height) const
{
    // Check the mapped planes.
    if (visCeiling().surface().hasSkyMaskedMaterial())
    {
        const ClSkyPlane &skyCeil = sector().map().as<Map>().skyCeiling();
        if (skyCeil.height() < DDMAXFLOAT && height > skyCeil.height())
            return true;
    }
    else if (height > visCeiling().heightSmoothed())
    {
        return true;
    }

    if (visFloor().surface().hasSkyMaskedMaterial())
    {
        const ClSkyPlane &skyFloor = sector().map().as<Map>().skyFloor();
        if (skyFloor.height() > DDMINFLOAT && height < skyFloor.height())
            return true;
    }
    else if (height < visFloor().heightSmoothed())
    {
        return true;
    }

    return false;  // Not in the void.
}

bool Subsector::hasWorldVolume(bool useSmoothedHeights) const
{
    const auto currentFrame = R_FrameCount();
    if (d->validFrame != currentFrame)
    {
        d->validFrame = currentFrame;
        if (useSmoothedHeights)
        {
//            debug("ceiling %f -- floor %f (%f, %f)", visCeiling().heightSmoothed(), visFloor().heightSmoothed(),
//                  sector().ceiling().height(), sector().floor().height());
            d->hasWorldVolumeInValidFrame = visCeiling().heightSmoothed() > visFloor().heightSmoothed();
        }
        else
        {
            d->hasWorldVolumeInValidFrame = sector().ceiling().height() > sector().floor().height();
        }
    }
    return d->hasWorldVolumeInValidFrame;
}

void Subsector::markReverbDirty(bool yes)
{
    d->needReverbUpdate = yes;
}

const Subsector::AudioEnvironment &Subsector::reverb() const
{
    // Perform any scheduled update now.
    if (d->needReverbUpdate)
    {
        d->updateReverb();
    }
    return d->reverb;
}

#if 0
void Subsector::markVisPlanesDirty()
{
    d->maybeInvalidateMapping(Sector::Floor);
    d->maybeInvalidateMapping(Sector::Ceiling);
}
#endif

Subsector::LightId Subsector::lightSourceId() const
{
    /// @todo Need unique Subsector ids.
    return LightId(sector().indexInMap());
}

Vec3f Subsector::lightSourceColorf() const
{
    if (Rend_SkyLightIsEnabled() && hasSkyPlane())
    {
        return Rend_SkyLightColor();
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector().lightColor();
}

float Subsector::lightSourceIntensity(const Vec3d &/*viewPoint*/) const
{
    return sector().lightLevel();
}

int Subsector::blockLightSourceZBias()
{
    int height      = int(visCeiling().height() - visFloor().height());
    bool hasSkyFloor = visFloor().surface().hasSkyMaskedMaterial();
    bool hasSkyCeil  = visCeiling().surface().hasSkyMaskedMaterial();

    if (hasSkyFloor && !hasSkyCeil)
    {
        return -height / 6;
    }
    if (!hasSkyFloor && hasSkyCeil)
    {
        return height / 6;
    }
    if (height > 100)
    {
        return (height - 100) / 2;
    }
    return 0;
}

void Subsector::decorate()
{
    LOG_AS("Subsector::decorate");

    if (!hasDecorations()) return;

#if 0
    // Surfaces of the edge loops.
    forAllEdgeLoops([this](const ClEdgeLoop &loop)
    {
        SubsectorCirculator it(&loop.first());
        do
        {
            if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
            {
                auto &side = it->mapElementAs<LineSideSegment>().lineSide();
                if (side.hasSections())
                {
                    for (int i = LineSide::Middle; i <= LineSide::Top; ++i)
                    {
                        d->decorate(side.surface(i).as<Surface>());
                    }
                }
            }
        }
        while (&it.next() != &loop.first());
        return LoopContinue;
    });

    // Surfaces of the visual planes.
    d->decorate(visFloor  ().surface());
    d->decorate(visCeiling().surface());
#endif

    for (Surface *surface : d.getConst()->decorSurfaces)
    {
        d->decorate(*surface);
    }
}

bool Subsector::hasDecorations() const
{
    return !d.getConst()->decorSurfaces.isEmpty();
}

void Subsector::generateLumobjs()
{
    Map &map = sector().map().as<Map>();
    for (Surface *surface : d.getConst()->decorSurfaces)
    {
        for (Decoration *decor : static_cast<Impl::DecoratedSurface *>(surface->decorationState())->decorations)
        {
            if (const auto *lightDecor = maybeAs<LightDecoration>(decor))
            {
                if (Lumobj *lum = lightDecor->generateLumobj())
                {
                    map.addLumobj(lum);
                }
            }
        }
    }
}

void Subsector::markForDecorationUpdate(bool yes)
{
    for (Surface *surface : d.getConst()->decorSurfaces)
    {
        if (auto *decor = static_cast<Impl::DecoratedSurface *>(surface->decorationState()))
        {
            decor->markForUpdate(yes);
        }
    }
}
