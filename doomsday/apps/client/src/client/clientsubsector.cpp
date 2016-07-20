/** @file clientsubsector.cpp  Client-side world map subsector.
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

#include "client/clientsubsector.h"

#include "Surface"
#include "world/blockmap.h"
#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"

#include "render/rend_main.h" // Rend_SkyLightColor(), useBias
#include "BiasIllum"
#include "BiasTracker"
#include "Shard"

#include "Face"

#include <QtAlgorithms>
#include <QHash>
#include <QMap>
#include <QMutableMapIterator>
#include <QRect>
#include <QSet>

using namespace de;

namespace internal {

/// Classification flags:
enum SubsectorFlag
{
    NeverMapped      = 0x01,
    AllMissingBottom = 0x02,
    AllMissingTop    = 0x04,
    AllSelfRef       = 0x08,
    PartSelfRef      = 0x10
};

Q_DECLARE_FLAGS(SubsectorFlags, SubsectorFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(SubsectorFlags)

static QRectF qrectFromAABox(AABoxd const &b)
{
    return QRectF(QPointF(b.minX, b.maxY), QPointF(b.maxX, b.minY));
}

}  // namespace internal
using namespace ::internal;

namespace world {

DENG2_PIMPL(ClientSubsector)
, DENG2_OBSERVES(Subsector, Deletion)
, DENG2_OBSERVES(Plane,     Deletion)
, DENG2_OBSERVES(Plane,     HeightChange)
, DENG2_OBSERVES(Plane,     HeightSmoothedChange)
, DENG2_OBSERVES(Sector,    LightColorChange)
, DENG2_OBSERVES(Sector,    LightLevelChange)
{
    bool needClassify = true;  ///< @c true= (Re)classification is necessary.
    SubsectorFlags flags = 0;
    ClientSubsector *mappedVisFloor   = nullptr;
    ClientSubsector *mappedVisCeiling = nullptr;

    struct BoundaryData
    {
        /// Lists of unique exterior subsectors which share a boundary edge with
        /// "this" subsector (i.e., one edge per subsec).
        QList<HEdge *> uniqueInnerEdges; /// not owned.
        QList<HEdge *> uniqueOuterEdges; /// not owned.
    };
    std::unique_ptr<BoundaryData> boundaryData;

    struct GeometryData
    {
        MapElement *mapElement;
        dint geomId;
        std::unique_ptr<Shard> shard;

        GeometryData(MapElement *mapElement, dint geomId)
            : mapElement(mapElement)
            , geomId(geomId)
        {}
    };
    /// @todo Avoid two-stage lookup.
    typedef QMap<dint, GeometryData *> Shards;
    struct GeometryGroups : public QMap<MapElement *, Shards>
    {
        ~GeometryGroups()
        {
            DENG2_FOR_EACH(GeometryGroups, g, *this) qDeleteAll(*g);
        }
    };
    GeometryGroups geomGroups;

    /// Reverse lookup hash from Shard => GeometryData.
    typedef QHash<Shard *, GeometryData *> ShardGeometryMap;
    ShardGeometryMap shardGeomMap;

    /// Subspaces in the neighborhood effecting environmental audio characteristics.
    typedef QSet<ConvexSubspace *> ReverbSubspaces;
    ReverbSubspaces reverbSubspaces;

    /// Environmental audio config.
    AudioEnvironment reverb;
    bool needReverbUpdate = true;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        //observePlane(&sector().floor(), false);
        //observePlane(&sector().ceiling(), false);

        //sector().audienceForLightLevelChange() -= this;
        //sector().audienceForLightColorChange() -= this;

        clearMapping(Sector::Floor);
        clearMapping(Sector::Ceiling);
    }

    inline bool floorIsMapped()
    {
        return mappedVisFloor != 0 && mappedVisFloor != thisPublic;
    }

    inline bool ceilingIsMapped()
    {
        return mappedVisCeiling != 0 && mappedVisCeiling != thisPublic;
    }

    inline bool needRemapVisPlanes()
    {
        return mappedVisFloor == 0 || mappedVisCeiling == 0;
    }

    ClientSubsector **mappedSubsectorAdr(dint planeIdx)
    {
        if (planeIdx == Sector::Floor)   return &mappedVisFloor;
        if (planeIdx == Sector::Ceiling) return &mappedVisCeiling;
        return nullptr;
    }

    inline Plane *mappedPlane(dint planeIdx)
    {
        ClientSubsector **subsecAdr = mappedSubsectorAdr(planeIdx);
        if (subsecAdr && *subsecAdr)
        {
            return &(*subsecAdr)->plane(planeIdx);
        }
        return nullptr;
    }

    void observeSubsector(ClientSubsector *subsec, bool yes = true)
    {
        if (!subsec || subsec == thisPublic)
            return;

        if (yes) subsec->audienceForDeletion += this;
        else     subsec->audienceForDeletion -= this;
    }

    void observePlane(Plane *plane, bool yes = true, bool observeHeight = true)
    {
        if (!plane) return;

        if (yes)
        {
            plane->audienceForDeletion() += this;
            if (observeHeight)
            {
                plane->audienceForHeightChange()         += this;
                plane->audienceForHeightSmoothedChange() += this;
            }
        }
        else
        {
            plane->audienceForDeletion()             -= this;
            plane->audienceForHeightChange()         -= this;
            plane->audienceForHeightSmoothedChange() -= this;
        }
    }

    void map(dint planeIdx, ClientSubsector *newSubsector, bool permanent = false)
    {
        ClientSubsector **subsecAdr = mappedSubsectorAdr(planeIdx);
        if (!subsecAdr || *subsecAdr == newSubsector)
            return;

        if (*subsecAdr != thisPublic)
        {
            observePlane(mappedPlane(planeIdx), false);
        }
        observeSubsector(*subsecAdr, false);

        *subsecAdr = newSubsector;

        observeSubsector(*subsecAdr);
        if (*subsecAdr != thisPublic)
        {
            observePlane(mappedPlane(planeIdx), true, !permanent);
        }
    }

    void clearMapping(dint planeIdx)
    {
        map(planeIdx , 0);
    }

    /**
     * To be called when a plane moves to possibly invalidate mapped planes so
     * that they will be re-evaluated later.
     */
    void maybeInvalidateMapping(dint planeIdx)
    {
        if (classification() & NeverMapped)
            return;

        ClientSubsector **subsecAdr = mappedSubsectorAdr(planeIdx);
        if (!subsecAdr || *subsecAdr == thisPublic)
            return;

        clearMapping(planeIdx);

        if (classification() & (AllMissingBottom|AllMissingTop))
        {
            // Reclassify incase material visibility has changed.
            needClassify = true;
        }
    }

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
            self.forAllSubspaces([this] (ConvexSubspace &subspace)
            {
                HEdge const *base  = subspace.poly().hedge();
                HEdge const *hedge = base;
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

                    auto const &backSpace = hedge->twin().face().mapElementAs<ConvexSubspace>();
                    // ClientSubsector internal edges are not considered.
                    if (&backSpace.subsector() == thisPublic)
                        continue;

                    LineSide const &frontSide = hedge->mapElementAs<LineSideSegment>().lineSide();
                    LineSide const &backSide  = hedge->twin().mapElementAs<LineSideSegment>().lineSide();

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

                    auto const &backSubsec = backSpace.subsector().as<ClientSubsector>();
                    if (backSubsec.floor().height() < self.sector().floor().height() &&
                        backSide.bottom().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingBottom;
                    }

                    if (backSubsec.ceiling().height() > self.sector().ceiling().height() &&
                        backSide.top().hasDrawableNonFixMaterial())
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

        QMap<ClientSubsector *, HEdge *> extSubsectorMap;
        self.forAllSubspaces([this, &extSubsectorMap] (ConvexSubspace &subspace)
        {
            HEdge *base = subspace.poly().hedge();
            HEdge *hedge = base;
            do
            {
                if (!hedge->hasMapElement())
                    continue;

                if (!hedge->twin().hasFace() || !hedge->twin().face().hasMapElement())
                    continue;

                auto &backSubsec = hedge->twin().face().mapElementAs<ConvexSubspace>()
                                       .subsector().as<ClientSubsector>();
                if (&backSubsec == thisPublic)
                    continue;

                extSubsectorMap.insert(&backSubsec, hedge);

            } while ((hedge = &hedge->next()) != base);

            return LoopContinue;
        });

        boundaryData.reset(new BoundaryData);
        if (extSubsectorMap.isEmpty())
            return;

        QRectF boundingRect = qrectFromAABox(self.bounds());

        // First try to quickly decide by comparing subsector bounding boxes.
        QMutableMapIterator<ClientSubsector *, HEdge *> iter(extSubsectorMap);
        while (iter.hasNext())
        {
            iter.next();
            auto &extSubsec = iter.value()->twin().face().mapElementAs<ConvexSubspace>()
                                  .subsector().as<ClientSubsector>();
            if (!boundingRect.contains(qrectFromAABox(extSubsec.bounds())))
            {
                boundaryData->uniqueOuterEdges.append(iter.value());
                iter.remove();
            }
        }

        if (extSubsectorMap.isEmpty())
            return;

        // More extensive tests are necessary. At this point we know that all
        // subsectors which remain in the map are inside according to the bounding
        // box of "this" subsector.
        QList<HEdge *> const boundaryEdges = extSubsectorMap.values();
        QList<QRectF> boundaries;
        for (HEdge *base : boundaryEdges)
        {
            QRectF bounds;
            SubsectorCirculator it(base);
            do
            {
                bounds |= QRectF(QPointF(it->origin().x, it->origin().y),
                                 QPointF(it->twin().origin().x, it->twin().origin().y))
                              .normalized();
            } while (&it.next() != base);

            boundaries.append(bounds);
        }

        QRectF const *largest = nullptr;
        for (QRectF const &boundary : boundaries)
        {
            if (!largest || boundary.contains(*largest))
                largest = &boundary;
        }

        for (dint i = 0; i < boundaryEdges.count(); ++i)
        {
            HEdge *hedge = boundaryEdges[i];
            QRectF const &boundary = boundaries[i];
            if (&boundary == largest || (largest && boundary == *largest))
            {
                boundaryData->uniqueOuterEdges.append(hedge);
            }
            else
            {
                boundaryData->uniqueInnerEdges.append(hedge);
            }
        }
    }

    void remapVisPlanes()
    {
        // By default both planes are mapped to the parent sector.
        if (!floorIsMapped())   map(Sector::Floor,   thisPublic);
        if (!ceilingIsMapped()) map(Sector::Ceiling, thisPublic);

        if (classification() & NeverMapped)
            return;

        if (classification() & (AllSelfRef | PartSelfRef))
        {
            // Should we permanently map one or both planes to those of another sector?

            initBoundaryDataIfNeeded();

            for (HEdge *hedge : boundaryData->uniqueOuterEdges)
            {
                auto &extSubsec = hedge->twin().face().mapElementAs<ConvexSubspace>()
                                     .subsector().as<ClientSubsector>();

                if (!hedge->mapElementAs<LineSideSegment>().line().isSelfReferencing())
                    continue;

                if (!(classification() & AllSelfRef)
                    && (extSubsec.d->classification() & AllSelfRef))
                    continue;

                if (extSubsec.d->mappedVisFloor == thisPublic)
                    continue;

                // Setup the mapping and we're done.
                map(Sector::Floor,   &extSubsec, true /*permanently*/);
                map(Sector::Ceiling, &extSubsec, true /*permanently*/);
                break;
            }

            if (floorIsMapped())
            {
                // Remove the mapping from all inner subsectors to this, forcing
                // their re-evaluation (however next time a different subsector
                // will be selected from the boundary).
                for (HEdge *hedge : boundaryData->uniqueInnerEdges)
                {
                    auto &extSubsec = hedge->twin().face().mapElementAs<ConvexSubspace>()
                                         .subsector().as<ClientSubsector>();

                    if (!hedge->mapElementAs<LineSideSegment>().line().isSelfReferencing())
                        continue;

                    if (!(classification() & AllSelfRef)
                        && (extSubsec.d->classification() & AllSelfRef))
                        continue;

                    if (extSubsec.d->mappedVisFloor == thisPublic)
                    {
                        extSubsec.d->clearMapping(Sector::Floor);
                    }
                    if (extSubsec.d->mappedVisCeiling == thisPublic)
                    {
                        extSubsec.d->clearMapping(Sector::Ceiling);
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
        if (self.sector().ceiling().height() <= self.sector().floor().height())
            return;

        bool doFloor   =   !floorIsMapped() && classification().testFlag(AllMissingBottom);
        bool doCeiling = !ceilingIsMapped() && classification().testFlag(AllMissingTop);

        if (!doFloor && !doCeiling)
            return;

        initBoundaryDataIfNeeded();

        // Map "this" subsector to the first outer subsector found.
        for (HEdge *hedge : boundaryData->uniqueOuterEdges)
        {
            auto &extSubsec = hedge->twin().face().mapElementAs<ConvexSubspace>()
                                 .subsector().as<ClientSubsector>();

            if (doFloor && !floorIsMapped())
            {
                Plane &extVisPlane = extSubsec.visFloor();
                if (!extVisPlane.surface().hasSkyMaskedMaterial()
                    && extVisPlane.height() > self.sector().floor().height())
                {
                    map(Sector::Floor, &extSubsec);
                    if (!doCeiling) break;
                }
            }

            if (doCeiling && !ceilingIsMapped())
            {
                Plane &extVisPlane = extSubsec.visCeiling();
                if (!extVisPlane.surface().hasSkyMaskedMaterial()
                    && extSubsec.visCeiling().height() < self.sector().ceiling().height())
                {
                    map(Sector::Ceiling, &extSubsec);
                    if (!doFloor) break;
                }
            }
        }

        if (!floorIsMapped() && !ceilingIsMapped())
            return;

        // Clear mappings for all inner subsectors to force re-evaluation (which
        // may in turn lead to their inner subsectors being re-evaluated, producing
        // a "ripple effect" that will remap any deeply nested dependents).
        for (HEdge *hedge : boundaryData->uniqueInnerEdges)
        {
            auto &extSubsec = hedge->twin().face().mapElementAs<ConvexSubspace>()
                                 .subsector().as<ClientSubsector>();

            if (extSubsec.d->classification() & NeverMapped)
                continue;

            if (doFloor && floorIsMapped()
                && extSubsec.visFloor().height() >= self.sector().floor().height())
            {
                extSubsec.d->clearMapping(Sector::Floor);
            }

            if (doCeiling && ceilingIsMapped()
                && extSubsec.visCeiling().height() <= self.sector().ceiling().height())
            {
                extSubsec.d->clearMapping(Sector::Ceiling);
            }
        }
    }

    void markAllSurfacesForDecorationUpdate(Line &line)
    {
        LineSide &front = line.front();
        DENG2_ASSERT(front.hasSections());
        {
            front.middle().markForDecorationUpdate();
            front.bottom().markForDecorationUpdate();
            front.   top().markForDecorationUpdate();
        }

        LineSide &back = line.back();
        if (back.hasSections())
        {
            back.middle().markForDecorationUpdate();
            back.bottom().markForDecorationUpdate();
            back   .top().markForDecorationUpdate();
        }
    }

    /**
     * To be called when the height changes to update the plotted decoration
     * origins for surfaces whose material offset is dependant upon this.
     */
    void markDependantSurfacesForDecorationUpdate()
    {
        if (ddMapSetup) return;

        initBoundaryDataIfNeeded();

        // Mark surfaces of the outer edge loop.
        /// @todo What about the special case of a subsector with no outer neighbors? -ds
        if (!boundaryData->uniqueOuterEdges.isEmpty())
        {
            HEdge *base = boundaryData->uniqueOuterEdges.first();
            SubsectorCirculator it(base);
            do
            {
                if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
                {
                    markAllSurfacesForDecorationUpdate(it->mapElementAs<LineSideSegment>().line());
                }
            } while (&it.next() != base);
        }

        // Mark surfaces of the inner edge loop(s).
        for (HEdge *base : boundaryData->uniqueInnerEdges)
        {
            SubsectorCirculator it(base);
            do
            {
                if (it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
                {
                    markAllSurfacesForDecorationUpdate(it->mapElementAs<LineSideSegment>().line());
                }
            } while (&it.next() != base);
        }
    }

    /// Observes Subsector Deletion.
    void subsectorBeingDeleted(Subsector const &subsec)
    {
        if (  mappedVisFloor == &subsec) clearMapping(Sector::Floor);
        if (mappedVisCeiling == &subsec) clearMapping(Sector::Ceiling);
    }

    /// Observes Plane Deletion.
    void planeBeingDeleted(Plane const &plane)
    {
        clearMapping(plane.indexInSector());
    }

    void updateBiasForWallSectionsAfterGeometryMove(HEdge *hedge)
    {
        if (!hedge) return;
        if (!hedge->hasMapElement()) return;

        MapElement *mapElement = &hedge->mapElement();
        if (Shard *shard = self.findShard(*mapElement, LineSide::Middle))
        {
            shard->updateBiasAfterMove();
        }
        if (Shard *shard = self.findShard(*mapElement, LineSide::Bottom))
        {
            shard->updateBiasAfterMove();
        }
        if (Shard *shard = self.findShard(*mapElement, LineSide::Top))
        {
            shard->updateBiasAfterMove();
        }
    }

    /// Observes Plane HeightChange.
    void planeHeightChanged(Plane &plane)
    {
        if (&plane == mappedPlane(plane.indexInSector()))
        {
            // Check if there are any camera players in this sector. If their height
            // is now above the ceiling/below the floor they are now in the void.
            DoomsdayApp::players().forAll([this] (Player &plr)
            {
                ddplayer_t const &ddpl = plr.publicData();
                if (plr.isInGame()
                    && (ddpl.flags & DDPF_CAMERA)
                    && Mobj_SubsectorPtr(*ddpl.mo) == thisPublic
                    && (   ddpl.mo->origin[2] > self.visCeiling().height() - 4
                        || ddpl.mo->origin[2] < self.visFloor  ().height()))
                {
                    plr.as<ClientPlayer>().inVoid = true;
                }
                return LoopContinue;
            });

            // We'll need to recalculate environmental audio characteristics.
            needReverbUpdate = true;

            if (!ddMapSetup && useBias)
            {
                // Inform bias surfaces of changed geometry.
                self.forAllSubspaces([this, &plane] (ConvexSubspace &subspace)
                {
                    if (Shard *shard = self.findShard(subspace, plane.indexInSector()))
                    {
                        shard->updateBiasAfterMove();
                    }

                    HEdge *base = subspace.poly().hedge();
                    HEdge *hedge = base;
                    do
                    {
                        updateBiasForWallSectionsAfterGeometryMove(hedge);
                    } while ((hedge = &hedge->next()) != base);

                    return subspace.forAllExtraMeshes([this] (Mesh &mesh)
                    {
                        for (HEdge *hedge : mesh.hedges())
                        {
                            updateBiasForWallSectionsAfterGeometryMove(hedge);
                        }
                        return LoopContinue;
                    });
                });
            }

            markDependantSurfacesForDecorationUpdate();
        }

        // We may need to update one or both mapped planes.
        maybeInvalidateMapping(plane.indexInSector());
    }

    /**
     * Find the GeometryData for a MapElement by the element-unique @a group identifier.
     *
     * @param geomId    Geometry identifier.
     *
     * @param canAlloc  @c true= to allocate if no data exists. Note that the number of
     * vertices in the fan geometry must be known at this time.
     */
    GeometryData *geomData(MapElement &mapElement, dint geomId, bool canAlloc = false)
    {
        GeometryGroups::iterator foundGroup = geomGroups.find(&mapElement);
        if (foundGroup != geomGroups.end())
        {
            Shards &shards = *foundGroup;
            Shards::iterator found = shards.find(geomId);
            if (found != shards.end())
            {
                return *found;
            }
        }

        if (!canAlloc) return nullptr;

        if (foundGroup == geomGroups.end())
        {
            foundGroup = geomGroups.insert(&mapElement, Shards());
        }

        return *foundGroup->insert(geomId, new GeometryData(&mapElement, geomId));
    }

    /**
     * Find the GeometryData for the given @a shard.
     */
    GeometryData *geomDataForShard(Shard *shard)
    {
        if (shard && shard->subsector() == thisPublic)
        {
            ShardGeometryMap::const_iterator found = shardGeomMap.find(shard);
            if (found != shardGeomMap.end())
                return *found;
        }
        return nullptr;
    }

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
        Map const &map = self.sector().map();

        AABoxd box = self.bounds();
        box.minX -= 128;
        box.minY -= 128;
        box.maxX += 128;
        box.maxY += 128;

        // Link all convex subspaces whose axis-aligned bounding box intersects
        // with the affection bounds to the reverb set.
        dint const localValidCount = ++validCount;
        map.subspaceBlockmap().forAllInBox(box, [this, &box, &localValidCount] (void *object)
        {
            auto &sub = *(ConvexSubspace *)object;
            if (sub.validCount() != localValidCount) // not yet processed
            {
                sub.setValidCount(localValidCount);

                // Check the bounds.
                AABoxd const &polyBounds = sub.poly().bounds();
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

        duint spaceVolume = dint((self.visCeiling().height() - self.visFloor().height())
                          * self.roughArea());

        reverb.reset();

        for (ConvexSubspace *subspace : reverbSubspaces)
        {
            if (subspace->updateAudioEnvironment())
            {
                auto const &aenv = subspace->audioEnvironment();

                reverb.space   += aenv.space;

                reverb.volume  += aenv.volume  / 255.0f * aenv.space;
                reverb.decay   += aenv.decay   / 255.0f * aenv.space;
                reverb.damping += aenv.damping / 255.0f * aenv.space;
            }
        }

        dfloat spaceScatter;

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

        if (self.hasSkyMaskPlane())
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

    /// Observes Plane HeightSmoothedChange.
    void planeHeightSmoothedChanged(Plane &plane)
    {
        markDependantSurfacesForDecorationUpdate();

        // We may need to update one or both mapped planes.
        maybeInvalidateMapping(plane.indexInSector());
    }

    /// Observes Sector LightLevelChange.
    void sectorLightLevelChanged(Sector &changed)
    {
        DENG2_ASSERT(&changed == &self.sector());
        DENG2_UNUSED(changed);
        if (self.sector().map().hasLightGrid())
        {
            self.sector().map().lightGrid().blockLightSourceChanged(thisPublic);
        }
    }

    /// Observes Sector LightColorChange.
    void sectorLightColorChanged(Sector &changed)
    {
        DENG2_ASSERT(&changed == &self.sector());
        DENG2_UNUSED(changed);
        if (self.sector().map().hasLightGrid())
        {
            self.sector().map().lightGrid().blockLightSourceChanged(thisPublic);
        }
    }
};

ClientSubsector::ClientSubsector(QList<ConvexSubspace *> const &subspaces)
    : Subsector(subspaces)
    , d(new Impl(this))
{
    // Observe changes to plane heights in "this" sector.
    d->observePlane(&sector().floor  ());
    d->observePlane(&sector().ceiling());

    // Observe changes to sector lighting properties.
    sector().audienceForLightLevelChange() += d;
    sector().audienceForLightColorChange() += d;
}

Plane &ClientSubsector::visPlane(dint planeIndex)
{
    return const_cast<Plane &>(const_cast<ClientSubsector const *>(this)->visPlane(planeIndex));
}

Plane const &ClientSubsector::visPlane(dint planeIndex) const
{
    if (planeIndex >= Sector::Floor && planeIndex <= Sector::Ceiling)
    {
        // Time to remap the planes?
        if (d->needRemapVisPlanes())
        {
            d->remapVisPlanes();
        }

        ClientSubsector *mapping = (planeIndex == Sector::Ceiling ? d->mappedVisCeiling
                                                                  : d->mappedVisFloor);
        if (mapping && mapping != this)
        {
            return mapping->visPlane(planeIndex);
        }
    }
    // Not mapped.
    return sector().plane(planeIndex);
}

bool ClientSubsector::isHeightInVoid(ddouble height) const
{
    // Check the mapped planes.
    if (visCeiling().surface().hasSkyMaskedMaterial())
    {
        ddouble const skyCeil = sector().map().skyFixCeiling();
        if (skyCeil < DDMAXFLOAT && height > skyCeil)
            return true;
    }
    else if (height > visCeiling().heightSmoothed())
    {
        return true;
    }

    if (visFloor().surface().hasSkyMaskedMaterial())
    {
        ddouble const skyFloor = sector().map().skyFixFloor();
        if (skyFloor > DDMINFLOAT && height < skyFloor)
            return true;
    }
    else if (height < visFloor().heightSmoothed())
    {
        return true;
    }

    return false;  // Not in the void.
}

bool ClientSubsector::hasWorldVolume(bool useSmoothedHeights) const
{
    if (useSmoothedHeights)
    {
        return visCeiling().heightSmoothed() - visFloor().heightSmoothed() > 0;
    }
    else
    {
        return ceiling().height() - floor().height() > 0;
    }
}

void ClientSubsector::markReverbDirty(bool yes)
{
    d->needReverbUpdate = yes;
}

ClientSubsector::AudioEnvironment const &ClientSubsector::reverb() const
{
    // Perform any scheduled update now.
    if (d->needReverbUpdate)
    {
        d->updateReverb();
    }
    return d->reverb;
}

void ClientSubsector::markVisPlanesDirty()
{
    d->maybeInvalidateMapping(Sector::Floor);
    d->maybeInvalidateMapping(Sector::Ceiling);
}

bool ClientSubsector::hasSkyMaskPlane() const
{
    for (dint i = 0; i < sector().planeCount(); ++i)
    {
        if (visPlane(i).surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

ClientSubsector::LightId ClientSubsector::lightSourceId() const
{
    /// @todo Need unique ClientSubsector ids.
    return LightId(sector().indexInMap());
}

Vector3f ClientSubsector::lightSourceColorf() const
{
    if (Rend_SkyLightIsEnabled() && hasSkyMaskPlane())
    {
        return Rend_SkyLightColor();
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector().lightColor();
}

dfloat ClientSubsector::lightSourceIntensity(Vector3d const &/*viewPoint*/) const
{
    return sector().lightLevel();
}

dint ClientSubsector::blockLightSourceZBias()
{
    dint height      = dint(visCeiling().height() - visFloor().height());
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

void ClientSubsector::applyBiasChanges(QBitArray &allChanges)
{
    Impl::ShardGeometryMap::const_iterator it = d->shardGeomMap.constBegin();
    while (it != d->shardGeomMap.constEnd())
    {
        it.key()->biasTracker().applyChanges(allChanges);
        ++it;
    }
}

// Determine the number of bias illumination points needed for this geometry.
// Presently we define a 1:1 mapping to geometry vertices.
static dint countIlluminationPoints(MapElement &mapElement, dint group)
{
    DENG2_UNUSED(group); // just assert
    switch (mapElement.type())
    {
    case DMU_SUBSPACE: {
        auto &space = mapElement.as<ConvexSubspace>();
        DENG2_ASSERT(group >= 0 && group < space.subsector().sector().planeCount()); // sanity check
        return space.fanVertexCount(); }

    case DMU_SEGMENT:
        DENG2_ASSERT(group >= 0 && group <= LineSide::Top); // sanity check
        return 4;

    default:
        throw Error("ClientSubsector::countIlluminationPoints", "Invalid MapElement type");
    }
    return 0;
}

Shard &ClientSubsector::shard(MapElement &mapElement, dint geomId)
{
    auto *gdata = d->geomData(mapElement, geomId, true /*create*/);
    if (!gdata->shard)
    {
        gdata->shard.reset(new Shard(countIlluminationPoints(mapElement, geomId), this));
    }
    return *gdata->shard;
}

Shard *ClientSubsector::findShard(MapElement &mapElement, dint geomId)
{
    if (auto *gdata = d->geomData(mapElement, geomId))
    {
        return gdata->shard.get();
    }
    return nullptr;
}

/**
 * @todo This could be enhanced so that only the lights on the right side of the
 * surface are taken into consideration.
 */
bool ClientSubsector::updateBiasContributors(Shard *shard)
{
    if (Impl::GeometryData *gdata = d->geomDataForShard(shard))
    {
        Map const &map = sector().map();

        BiasTracker &tracker = shard->biasTracker();
        tracker.clearContributors();

        switch (gdata->mapElement->type())
        {
        case DMU_SUBSPACE: {
            auto &subspace         = gdata->mapElement->as<ConvexSubspace>();
            Plane const &plane     = visPlane(gdata->geomId);
            Surface const &surface = plane.surface();

            Vector3d const surfacePoint(subspace.poly().center(), plane.heightSmoothed());

            map.forAllBiasSources([&tracker, &subspace, &surface, &surfacePoint] (BiasSource &source)
            {
                // If the source is too weak we will ignore it completely.
                if (source.intensity() <= 0)
                    return LoopContinue;

                Vector3d sourceToSurface = (source.origin() - surfacePoint).normalize();
                ddouble distance = 0;

                // Calculate minimum 2D distance to the subspace.
                /// @todo This is probably too accurate an estimate.
                HEdge *baseNode = subspace.poly().hedge();
                HEdge *node = baseNode;
                do
                {
                    ddouble len = (Vector2d(source.origin()) - node->origin()).length();
                    if (node == baseNode || len < distance)
                        distance = len;
                } while ((node = &node->next()) != baseNode);

                if (sourceToSurface.dot(surface.normal()) < 0)
                    return LoopContinue;

                tracker.addContributor(&source, source.evaluateIntensity() / de::max(distance, 1.0));
                return LoopContinue;
            });
            break; }

        case DMU_SEGMENT: {
            auto &seg              = gdata->mapElement->as<LineSideSegment>();
            Surface const &surface = seg.lineSide().middle();
            Vector2d const &from   = seg.hedge().origin();
            Vector2d const &to     = seg.hedge().twin().origin();
            Vector2d const center  = (from + to) / 2;

            map.forAllBiasSources([&tracker, &surface, &from, &to, &center] (BiasSource &source)
            {
                // If the source is too weak we will ignore it completely.
                if (source.intensity() <= 0)
                    return LoopContinue;

                Vector3d sourceToSurface = (source.origin() - center).normalize();

                // Calculate minimum 2D distance to the segment.
                ddouble distance = 0;
                for (dint k = 0; k < 2; ++k)
                {
                    ddouble len = (Vector2d(source.origin()) - (!k? from : to)).length();
                    if (k == 0 || len < distance)
                        distance = len;
                }

                if (sourceToSurface.dot(surface.normal()) < 0)
                    return LoopContinue;

                tracker.addContributor(&source, source.evaluateIntensity() / de::max(distance, 1.0));
                return LoopContinue;
            });
            break; }

        default:
            throw Error("ClientSubsector::updateBiasContributors", "Invalid MapElement type");
        }

        return true;
    }
    return false;
}

duint ClientSubsector::biasLastChangeOnFrame() const
{
    return sector().map().biasLastChangeOnFrame();
}

} // namespace world
