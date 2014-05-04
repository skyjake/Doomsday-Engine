/** @file sectorcluster.cpp  World map sector cluster.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_platform.h"
#include "world/sectorcluster.h"

#include "Face"
#include "BspLeaf"
#include "ConvexSubspace"
#include "Line"
#include "Plane"
#include "Surface"
#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"

#ifdef __CLIENT__
#  include "render/rend_main.h" // useBias
#  include "BiasIllum"
#  include "BiasTracker"
#  include "Shard"
#endif

#include <doomsday/console/var.h>
#include <de/vector1.h>
#include <QtAlgorithms>
#include <QHash>
#include <QMap>
#include <QMutableMapIterator>
#include <QRect>
#include <QSet>

namespace de {

namespace internal
{
    /// Classification flags:
    enum ClusterFlag
    {
        NeverMapped      = 0x01,
        AllMissingBottom = 0x02,
        AllMissingTop    = 0x04,
        AllSelfRef       = 0x08,
        PartSelfRef      = 0x10
    };

    Q_DECLARE_FLAGS(ClusterFlags, ClusterFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ClusterFlags)

    static QRectF qrectFromAABox(AABoxd const &aaBox)
    {
        return QRectF(QPointF(aaBox.minX, aaBox.maxY), QPointF(aaBox.maxX, aaBox.minY));
    }
}
}

using namespace de;
using namespace de::internal;

DENG2_PIMPL(SectorCluster)
, DENG2_OBSERVES(SectorCluster, Deletion)
, DENG2_OBSERVES(Plane,  Deletion)
, DENG2_OBSERVES(Plane,  HeightChange)
#ifdef __CLIENT__
, DENG2_OBSERVES(Plane,  HeightSmoothedChange)
, DENG2_OBSERVES(Sector, LightColorChange)
, DENG2_OBSERVES(Sector, LightLevelChange)
#endif
{
    bool needClassify; ///< @c true= (Re)classification is necessary.
    ClusterFlags flags;
    Subspaces subspaces;
    QScopedPointer<AABoxd> aaBox;

    SectorCluster *mappedVisFloor;
    SectorCluster *mappedVisCeiling;

    struct BoundaryData
    {
        /// Lists of unique exterior clusters which share a boundary edge with
        /// "this" cluster (i.e., one edge per cluster).
        QList<HEdge *> uniqueInnerEdges; /// not owned.
        QList<HEdge *> uniqueOuterEdges; /// not owned.
    };
    QScopedPointer<BoundaryData> boundaryData;

#ifdef __CLIENT__
    struct GeometryData
    {
        MapElement *mapElement;
        int geomId;
        QScopedPointer<Shard> shard;

        GeometryData(MapElement *mapElement, int geomId)
            : mapElement(mapElement), geomId(geomId)
        {}
    };
    /// @todo Avoid two-stage lookup.
    typedef QMap<int, GeometryData *> Shards;
    typedef QMap<MapElement *, Shards> GeometryGroups;
    GeometryGroups geomGroups;

    /// Reverse lookup hash from Shard => GeometryData.
    typedef QHash<Shard *, GeometryData *> ShardGeometryMap;
    ShardGeometryMap shardGeomMap;

    /// Subspaces in the neighborhood effecting environmental audio characteristics.
    typedef QSet<ConvexSubspace *> ReverbSubspaces;
    ReverbSubspaces reverbSubspaces;

    /// Final environmental audio characteristics.
    AudioEnvironmentFactors reverb;
    bool needReverbUpdate;
#endif

    Instance(Public *i)
        : Base            (i)
        , needClassify    (true)
        , flags           (0)
        , mappedVisFloor  (0)
        , mappedVisCeiling(0)
#ifdef __CLIENT__
        , needReverbUpdate(true)
#endif
    {
#ifdef __CLIENT__
        de::zap(reverb);
#endif
    }

    ~Instance()
    {
        observePlane(&self.sector().floor(), false);
        observePlane(&self.sector().ceiling(), false);

#ifdef __CLIENT__
        self.sector().audienceForLightLevelChange -= this;
        self.sector().audienceForLightColorChange -= this;

        DENG2_FOR_EACH(GeometryGroups, geomGroup, geomGroups)
        {
            Shards &shards = *geomGroup;
            qDeleteAll(shards);
        }
#endif

        clearMapping(Sector::Floor);
        clearMapping(Sector::Ceiling);

        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->sectorClusterBeingDeleted(self);
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

    SectorCluster **mappedClusterAdr(int planeIdx)
    {
        if(planeIdx == Sector::Floor)   return &mappedVisFloor;
        if(planeIdx == Sector::Ceiling) return &mappedVisCeiling;
        return 0;
    }

    inline Plane *mappedPlane(int planeIdx)
    {
        SectorCluster **clusterAdr = mappedClusterAdr(planeIdx);
        if(clusterAdr && *clusterAdr)
        {
            return &(*clusterAdr)->plane(planeIdx);
        }
        return 0;
    }

    void observeCluster(SectorCluster *cluster, bool yes = true)
    {
        if(!cluster || cluster == thisPublic)
            return;

        if(yes) cluster->audienceForDeletion += this;
        else    cluster->audienceForDeletion -= this;
    }

    void observePlane(Plane *plane, bool yes = true, bool observeHeight = true)
    {
        if(!plane) return;

        if(yes)
        {
            plane->audienceForDeletion += this;
            if(observeHeight)
            {
                plane->audienceForHeightChange         += this;
#ifdef __CLIENT__
                plane->audienceForHeightSmoothedChange += this;
#endif
            }
        }
        else
        {
            plane->audienceForDeletion             -= this;
            plane->audienceForHeightChange         -= this;
#ifdef __CLIENT__
            plane->audienceForHeightSmoothedChange -= this;
#endif
        }
    }

    void map(int planeIdx, SectorCluster *newCluster, bool permanent = false)
    {
        SectorCluster **clusterAdr = mappedClusterAdr(planeIdx);
        if(!clusterAdr || *clusterAdr == newCluster)
            return;

        if(*clusterAdr != thisPublic)
        {
            observePlane(mappedPlane(planeIdx), false);
        }
        observeCluster(*clusterAdr, false);

        *clusterAdr = newCluster;

        observeCluster(*clusterAdr);
        if(*clusterAdr != thisPublic)
        {
            observePlane(mappedPlane(planeIdx), true, !permanent);
        }
    }

    void clearMapping(int planeIdx)
    {
        map(planeIdx , 0);
    }

    /**
     * To be called when a plane moves to possibly invalidate mapped planes so
     * that they will be re-evaluated later.
     */
    void maybeInvalidateMapping(int planeIdx)
    {
        if(classification() & NeverMapped)
            return;

        SectorCluster **clusterAdr = mappedClusterAdr(planeIdx);
        if(!clusterAdr || *clusterAdr == thisPublic)
            return;

        clearMapping(planeIdx);

        if(classification() & (AllMissingBottom|AllMissingTop))
        {
            // Reclassify incase material visibility has changed.
            needClassify = true;
        }
    }

    /**
     * Returns a copy of the classification flags for the cluster, performing
     * classification of the cluster if necessary.
     */
    ClusterFlags classification()
    {
        if(needClassify)
        {
            needClassify = false;

            flags &= ~(NeverMapped|PartSelfRef);
            flags |= AllSelfRef|AllMissingBottom|AllMissingTop;
            foreach(ConvexSubspace const *subspace, subspaces)
            {
                HEdge const *base  = subspace->poly().hedge();
                HEdge const *hedge = base;
                do
                {
                    if(!hedge->hasMapElement())
                        continue;

                    // This edge defines a section of a map line.

                    // If a back geometry is missing then never map planes.
                    if(!hedge->twin().hasFace())
                    {
                        flags |= NeverMapped;
                        flags &= ~(PartSelfRef|AllSelfRef|AllMissingBottom|AllMissingTop);
                        return flags;
                    }

                    BspLeaf const &backLeaf    = hedge->twin().face().mapElementAs<BspLeaf>();
                    SectorCluster const *backCluster = backLeaf.hasCluster()? &backLeaf.cluster() : 0;

                    // Cluster internal edges are not considered.
                    if(backCluster == thisPublic)
                        continue;

                    LineSide const &frontSide = hedge->mapElementAs<LineSideSegment>().lineSide();
                    LineSide const &backSide  = hedge->twin().mapElementAs<LineSideSegment>().lineSide();

                    // Similarly if no sections are defined for either side then
                    // never map planes. This can happen due to mapping errors
                    // where a group of one-sided lines facing outward in the
                    // void partly form a convex subspace.
                    if(!frontSide.hasSections() || !backSide.hasSections())
                    {
                        flags |= NeverMapped;
                        flags &= ~(PartSelfRef|AllSelfRef|AllMissingBottom|AllMissingTop);
                        return flags;
                    }

                    if(frontSide.line().isSelfReferencing())
                    {
                        flags |= PartSelfRef;
                        continue;
                    }

                    flags &= ~AllSelfRef;

                    if(frontSide.bottom().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingBottom;
                    }

                    if(frontSide.top().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingTop;
                    }

                    if(backCluster->floor().height() < self.sector().floor().height() &&
                       backSide.bottom().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingBottom;
                    }

                    if(backCluster->ceiling().height() > self.sector().ceiling().height() &&
                       backSide.top().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingTop;
                    }
                } while((hedge = &hedge->next()) != base);
            }
        }

        return flags;
    }

    void initBoundaryDataIfNeeded()
    {
        if(!boundaryData.isNull()) return;

        QMap<SectorCluster *, HEdge *> extClusterMap;
        foreach(ConvexSubspace *subspace, subspaces)
        {
            HEdge *base = subspace->poly().hedge();
            HEdge *hedge = base;
            do
            {
                if(!hedge->hasMapElement())
                    continue;

                if(!hedge->twin().hasFace())
                    continue;

                BspLeaf &backLeaf = hedge->twin().face().mapElementAs<BspLeaf>();
                if(!backLeaf.hasCluster())
                    continue;

                if(&backLeaf.cluster() == thisPublic)
                    continue;

                extClusterMap.insert(&backLeaf.cluster(), hedge);

            } while((hedge = &hedge->next()) != base);
        }

        boundaryData.reset(new BoundaryData);
        if(extClusterMap.isEmpty())
            return;

        QRectF boundingRect = qrectFromAABox(self.aaBox());

        // First try to quickly decide by comparing cluster bounding boxes.
        QMutableMapIterator<SectorCluster *, HEdge *> iter(extClusterMap);
        while(iter.hasNext())
        {
            iter.next();
            SectorCluster &extCluster = iter.value()->twin().face().mapElementAs<BspLeaf>().cluster();
            if(!boundingRect.contains(qrectFromAABox(extCluster.aaBox())))
            {
                boundaryData->uniqueOuterEdges.append(iter.value());
                iter.remove();
            }
        }

        if(extClusterMap.isEmpty())
            return;

        // More extensive tests are necessary. At this point we know that all
        // clusters which remain in the map are inside according to the bounding
        // box of "this" cluster.
        QList<HEdge *> const boundaryEdges = extClusterMap.values();
        QList<QRectF> boundaries;
        foreach(HEdge *base, boundaryEdges)
        {
            QRectF bounds;
            SectorClusterCirculator it(base);
            do
            {
                bounds |= QRectF(QPointF(it->origin().x, it->origin().y),
                                 QPointF(it->twin().origin().x, it->twin().origin().y))
                              .normalized();
            } while(&it.next() != base);

            boundaries.append(bounds);
        }

        QRectF const *largest = 0;
        foreach(QRectF const &boundary, boundaries)
        {
            if(!largest || boundary.contains(*largest))
                largest = &boundary;
        }

        for(int i = 0; i < boundaryEdges.count(); ++i)
        {
            HEdge *hedge = boundaryEdges[i];
            QRectF const &boundary = boundaries[i];
            if(&boundary == largest || boundary == *largest)
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
        Sector &sector = self.sector();

        // By default both planes are mapped to the parent sector.
        if(!floorIsMapped())   map(Sector::Floor,   thisPublic);
        if(!ceilingIsMapped()) map(Sector::Ceiling, thisPublic);

        if(classification() & NeverMapped)
            return;

        if(classification() & (AllSelfRef|PartSelfRef))
        {
            // Should we permanently map planes to another cluster?

            initBoundaryDataIfNeeded();

            foreach(HEdge *hedge, boundaryData->uniqueOuterEdges)
            {
                SectorCluster &extCluster = hedge->twin().face().mapElementAs<BspLeaf>().cluster();

                if(!hedge->mapElementAs<LineSideSegment>().line().isSelfReferencing())
                    continue;

                if(!(classification() & AllSelfRef) &&
                   (extCluster.d->classification() & AllSelfRef))
                    continue;

                if(extCluster.d->mappedVisFloor == thisPublic)
                    continue;

                // Setup the mapping and we're done.
                map(Sector::Floor,   &extCluster, true /*permanently*/);
                map(Sector::Ceiling, &extCluster, true /*permanently*/);
                break;
            }

            if(floorIsMapped())
            {
                // Remove the mapping from all inner clusters to this, forcing
                // their re-evaluation (however next time a different cluster
                // will be selected from the boundary).
                foreach(HEdge *hedge, boundaryData->uniqueInnerEdges)
                {
                    SectorCluster &extCluster = hedge->twin().face().mapElementAs<BspLeaf>().cluster();

                    if(!hedge->mapElementAs<LineSideSegment>().line().isSelfReferencing())
                        continue;

                    if(!(classification() & AllSelfRef) &&
                       (extCluster.d->classification() & AllSelfRef))
                        continue;

                    if(extCluster.d->mappedVisFloor == thisPublic)
                    {
                        extCluster.d->clearMapping(Sector::Floor);
                    }
                    if(extCluster.d->mappedVisCeiling == thisPublic)
                    {
                        extCluster.d->clearMapping(Sector::Ceiling);
                    }
                }

                // Permanent mappings won't be remapped.
                return;
            }
        }

        if(classification() & AllSelfRef)
            return;

        /*
         * Dynamic mapping may be needed for one or more planes.
         */

        // The sector must have open space.
        if(sector.ceiling().height() <= sector.floor().height())
            return;

        bool doFloor   =   !floorIsMapped() && classification().testFlag(AllMissingBottom);
        bool doCeiling = !ceilingIsMapped() && classification().testFlag(AllMissingTop);

        if(!doFloor && !doCeiling)
            return;

        initBoundaryDataIfNeeded();

        // Map "this" cluster to the first outer cluster found.
        foreach(HEdge *hedge, boundaryData->uniqueOuterEdges)
        {
            SectorCluster &extCluster = hedge->twin().face().mapElementAs<BspLeaf>().cluster();

            if(doFloor && !floorIsMapped())
            {
                Plane &extVisPlane = extCluster.visFloor();
                if(!extVisPlane.surface().hasSkyMaskedMaterial() &&
                   extVisPlane.height() > sector.floor().height())
                {
                    map(Sector::Floor, &extCluster);
                    if(!doCeiling) break;
                }
            }

            if(doCeiling && !ceilingIsMapped())
            {
                Plane &extVisPlane = extCluster.visCeiling();
                if(!extVisPlane.surface().hasSkyMaskedMaterial() &&
                   extCluster.visCeiling().height() < sector.ceiling().height())
                {
                    map(Sector::Ceiling, &extCluster);
                    if(!doFloor) break;
                }
            }
        }

        if(!floorIsMapped() && !ceilingIsMapped())
            return;

        // Clear mappings for all inner clusters to force re-evaluation (which
        // may in turn lead to their inner clusters being re-evaluated, producing
        // a "ripple effect" that will remap any deeply nested dependents).
        foreach(HEdge *hedge, boundaryData->uniqueInnerEdges)
        {
            SectorCluster &extCluster = hedge->twin().face().mapElementAs<BspLeaf>().cluster();

            if(extCluster.d->classification() & NeverMapped)
                continue;

            if(doFloor && floorIsMapped() &&
               extCluster.visFloor().height() >= sector.floor().height())
            {
                extCluster.d->clearMapping(Sector::Floor);
            }

            if(doCeiling && ceilingIsMapped() &&
               extCluster.visCeiling().height() <= sector.ceiling().height())
            {
                extCluster.d->clearMapping(Sector::Ceiling);
            }
        }
    }

#ifdef __CLIENT__

    void markAllSurfacesForDecorationUpdate(Line &line)
    {
        LineSide &front = line.front();
        DENG2_ASSERT(front.hasSections());
        {
            front.middle().markAsNeedingDecorationUpdate();
            front.bottom().markAsNeedingDecorationUpdate();
            front.   top().markAsNeedingDecorationUpdate();
        }

        LineSide &back = line.back();
        if(back.hasSections())
        {
            back.middle().markAsNeedingDecorationUpdate();
            back.bottom().markAsNeedingDecorationUpdate();
            back   .top().markAsNeedingDecorationUpdate();
        }
    }

    /**
     * To be called when the height changes to update the plotted decoration
     * origins for surfaces whose material offset is dependant upon this.
     */
    void markDependantSurfacesForDecorationUpdate()
    {
        if(ddMapSetup) return;

        initBoundaryDataIfNeeded();

        // Mark surfaces of the outer edge loop.
        /// @todo What about the special case of a cluster with no outer neighbors? -ds
        if(!boundaryInfo->uniqueOuterEdges.isEmpty())
        {
            HEdge *base = boundaryData->uniqueOuterEdges.first();
            SectorClusterCirculator it(base);
            do
            {
                if(it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
                {
                    markAllSurfacesForDecorationUpdate(it->mapElementAs<LineSideSegment>().line());
                }
            } while(&it.next() != base);
        }

        // Mark surfaces of the inner edge loop(s).
        foreach(HEdge *base, boundaryData->uniqueInnerEdges)
        {
            SectorClusterCirculator it(base);
            do
            {
                if(it->hasMapElement()) // BSP errors may fool the circulator wrt interior edges -ds
                {
                    markAllSurfacesForDecorationUpdate(it->mapElementAs<LineSideSegment>().line());
                }
            } while(&it.next() != base);
        }
    }

#endif // __CLIENT__

    /// Observes SectorCluster Deletion.
    void sectorClusterBeingDeleted(SectorCluster const &cluster)
    {
        if(  mappedVisFloor == &cluster) clearMapping(Sector::Floor);
        if(mappedVisCeiling == &cluster) clearMapping(Sector::Ceiling);
    }

    /// Observes Plane Deletion.
    void planeBeingDeleted(Plane const &plane)
    {
        clearMapping(plane.indexInSector());
    }

#ifdef __CLIENT__
    void updateBiasForWallSectionsAfterGeometryMove(HEdge *hedge)
    {
        if(!hedge) return;
        if(!hedge->hasMapElement()) return;

        MapElement *mapElement = &hedge->mapElement();
        if(Shard *shard = self.findShard(*mapElement, LineSide::Middle))
        {
            shard->updateBiasAfterMove();
        }
        if(Shard *shard = self.findShard(*mapElement, LineSide::Bottom))
        {
            shard->updateBiasAfterMove();
        }
        if(Shard *shard = self.findShard(*mapElement, LineSide::Top))
        {
            shard->updateBiasAfterMove();
        }
    }
#endif

    /// Observes Plane HeightChange.
    void planeHeightChanged(Plane &plane)
    {
        if(&plane == mappedPlane(plane.indexInSector()))
        {
            // Check if there are any camera players in this sector. If their height
            // is now above the ceiling/below the floor they are now in the void.
            for(int i = 0; i < DDMAXPLAYERS; ++i)
            {
                player_t *plr = &ddPlayers[i];
                ddplayer_t *ddpl = &plr->shared;

                if(!ddpl->inGame || !ddpl->mo)
                    continue;
                if(Mobj_ClusterPtr(*ddpl->mo) != thisPublic)
                    continue;

                if((ddpl->flags & DDPF_CAMERA) &&
                   (ddpl->mo->origin[VZ] > self.visCeiling().height() - 4 ||
                    ddpl->mo->origin[VZ] < self.visFloor().height()))
                {
                    ddpl->inVoid = true;
                }
            }

#ifdef __CLIENT__
            // We'll need to recalculate environmental audio characteristics.
            needReverbUpdate = true;

            if(!ddMapSetup && useBias)
            {
                // Inform bias surfaces of changed geometry.
                foreach(ConvexSubspace *subspace, subspaces)
                {
                    BspLeaf &bspLeaf = subspace->poly().mapElement().as<BspLeaf>();
                    if(Shard *shard = self.findShard(bspLeaf, plane.indexInSector()))
                    {
                        shard->updateBiasAfterMove();
                    }

                    HEdge *base = subspace->poly().hedge();
                    HEdge *hedge = base;
                    do
                    {
                        updateBiasForWallSectionsAfterGeometryMove(hedge);
                    } while((hedge = &hedge->next()) != base);

                    foreach(Mesh *mesh, subspace->extraMeshes())
                    foreach(HEdge *hedge, mesh->hedges())
                    {
                        updateBiasForWallSectionsAfterGeometryMove(hedge);
                    }
                }
            }

            markDependantSurfacesForDecorationUpdate();
#endif // __CLIENT__
        }

        // We may need to update one or both mapped planes.
        maybeInvalidateMapping(plane.indexInSector());
    }

#ifdef __CLIENT__
    /**
     * Find the GeometryData for a MapElement by the element-unique @a group
     * identifier.
     *
     * @param geomId    Geometry identifier.
     * @param canAlloc  @c true= to allocate if no data exists. Note that the
     *                  number of vertices in the fan geometry must be known
     *                  at this time.
     */
    GeometryData *geomData(MapElement &mapElement, int geomId, bool canAlloc = false)
    {
        GeometryGroups::iterator foundGroup = geomGroups.find(&mapElement);
        if(foundGroup != geomGroups.end())
        {
            Shards &shards = *foundGroup;
            Shards::iterator found = shards.find(geomId);
            if(found != shards.end())
            {
                return *found;
            }
        }

        if(!canAlloc) return 0;

        if(foundGroup == geomGroups.end())
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
        if(shard && shard->cluster() == thisPublic)
        {
            ShardGeometryMap::const_iterator found = shardGeomMap.find(shard);
            if(found != shardGeomMap.end())
                return *found;
        }
        return 0;
    }

    void addReverbSubspace(ConvexSubspace *subspace)
    {
        if(!subspace) return;
        reverbSubspaces.insert(subspace);
    }

    static int addReverbSubspaceWorker(BspLeaf *bspLeaf, void *context)
    {
        static_cast<Instance *>(context)->addReverbSubspace(bspLeaf->subspacePtr());
        return false; // Continue iteration.
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
        AABoxd affectionBounds = self.aaBox();
        affectionBounds.minX -= 128;
        affectionBounds.minY -= 128;
        affectionBounds.maxX += 128;
        affectionBounds.maxY += 128;

        // Link all non-degenerate BspLeafs whose axis-aligned bounding box intersects
        // with the affection bounds to the reverb set.
        self.sector().map().bspLeafBoxIterator(affectionBounds, addReverbSubspaceWorker, this);
    }

    /**
     * Recalculate environmental audio (reverb) for the sector.
     */
    void updateReverb()
    {
        // Need to initialize?
        if(reverbSubspaces.isEmpty())
        {
            findReverbSubspaces();
        }

        needReverbUpdate = false;

        uint spaceVolume = int((self.visCeiling().height() - self.visFloor().height())
                         * self.roughArea());

        reverb[SRD_SPACE] = reverb[SRD_VOLUME] =
            reverb[SRD_DECAY] = reverb[SRD_DAMPING] = 0;

        foreach(ConvexSubspace *subspace, reverbSubspaces)
        {
            if(subspace->updateReverb())
            {
                ConvexSubspace::AudioEnvironmentFactors const &subReverb = subspace->reverb();

                reverb[SRD_SPACE]   += subReverb[SRD_SPACE];

                reverb[SRD_VOLUME]  += subReverb[SRD_VOLUME]  / 255.0f * subReverb[SRD_SPACE];
                reverb[SRD_DECAY]   += subReverb[SRD_DECAY]   / 255.0f * subReverb[SRD_SPACE];
                reverb[SRD_DAMPING] += subReverb[SRD_DAMPING] / 255.0f * subReverb[SRD_SPACE];
            }
        }

        float spaceScatter;
        if(reverb[SRD_SPACE])
        {
            spaceScatter = spaceVolume / reverb[SRD_SPACE];
            // These three are weighted by the space.
            reverb[SRD_VOLUME]  /= reverb[SRD_SPACE];
            reverb[SRD_DECAY]   /= reverb[SRD_SPACE];
            reverb[SRD_DAMPING] /= reverb[SRD_SPACE];
        }
        else
        {
            spaceScatter = 0;
            reverb[SRD_VOLUME]  = .2f;
            reverb[SRD_DECAY]   = .4f;
            reverb[SRD_DAMPING] = 1;
        }

        // If the space is scattered, the reverb effect lessens.
        reverb[SRD_SPACE] /= (spaceScatter > .8 ? 10 : spaceScatter > .6 ? 4 : 1);

        // Normalize the reverb space [0..1]
        //   0= very small
        // .99= very large
        // 1.0= only for open areas (special case).
        reverb[SRD_SPACE] /= 120e6;
        if(reverb[SRD_SPACE] > .99)
            reverb[SRD_SPACE] = .99f;

        if(self.visCeiling().surface().hasSkyMaskedMaterial() ||
           self.visFloor().surface().hasSkyMaskedMaterial())
        {
            // An "open" sector.
            // It can still be small, in which case; reverb is diminished a bit.
            if(reverb[SRD_SPACE] > .5)
                reverb[SRD_VOLUME] = 1; // Full volume.
            else
                reverb[SRD_VOLUME] = .5f; // Small, but still open.

            reverb[SRD_SPACE] = 1;
        }
        else
        {
            // A "closed" sector.
            // Large spaces have automatically a bit more audible reverb.
            reverb[SRD_VOLUME] += reverb[SRD_SPACE] / 4;
        }

        if(reverb[SRD_VOLUME] > 1)
            reverb[SRD_VOLUME] = 1;
    }

    /// Observes Plane HeightSmoothedChange.
    void planeHeightSmoothedChanged(Plane &plane)
    {
        markDependantSurfacesForDecorationUpdate();

        // We may need to update one or both mapped planes.
        maybeInvalidateMapping(plane.indexInSector());
    }

    /// Observes Sector LightLevelChange.
    void sectorLightLevelChanged(Sector &sector)
    {
        DENG2_ASSERT(&sector == &self.sector());
        if(sector.map().hasLightGrid())
        {
            sector.map().lightGrid().blockLightSourceChanged(thisPublic);
        }
    }

    /// Observes Sector LightColorChange.
    void sectorLightColorChanged(Sector &sector)
    {
        DENG2_ASSERT(&sector == &self.sector());
        if(sector.map().hasLightGrid())
        {
            sector.map().lightGrid().blockLightSourceChanged(thisPublic);
        }
    }
#endif // __CLIENT__
};

SectorCluster::SectorCluster(Subspaces const &subspaces)
    : d(new Instance(this))
{
    d->subspaces.append(subspaces);
    foreach(ConvexSubspace *subspace, subspaces)
    {
        // Attribute the subspace to the cluster.
        subspace->setCluster(this);
    }

    // Observe changes to plane heights in "this" sector.
    d->observePlane(&sector().floor());
    d->observePlane(&sector().ceiling());

#ifdef __CLIENT__
    // Observe changes to sector lighting properties.
    sector().audienceForLightLevelChange += d;
    sector().audienceForLightColorChange += d;
#endif
}

SectorCluster::~SectorCluster()
{}

bool SectorCluster::isInternalEdge(HEdge *hedge) // static
{
    if(!hedge) return false;
    if(!hedge->hasFace() || !hedge->twin().hasFace()) return false;
    if(!hedge->face().hasMapElement() || hedge->face().mapElement().type() != DMU_BSPLEAF) return false;
    if(!hedge->twin().face().hasMapElement() || hedge->twin().face().mapElement().type() != DMU_BSPLEAF) return false;

    SectorCluster *frontCluster = hedge->face().mapElementAs<BspLeaf>().clusterPtr();
    if(!frontCluster) return false;
    return frontCluster == hedge->twin().face().mapElementAs<BspLeaf>().clusterPtr();
}

Sector const &SectorCluster::sector() const
{
    return const_cast<ConvexSubspace const *>(d->subspaces.first())->poly().mapElementAs<BspLeaf>().parent().as<Sector>();
}

Sector &SectorCluster::sector()
{
    return d->subspaces.first()->poly().mapElementAs<BspLeaf>().parent().as<Sector>();
}

Plane const &SectorCluster::plane(int planeIndex) const
{
    // Physical planes are never mapped.
    return sector().plane(planeIndex);
}

Plane &SectorCluster::plane(int planeIndex)
{
    // Physical planes are never mapped.
    return sector().plane(planeIndex);
}

Plane &SectorCluster::visPlane(int planeIndex)
{
    return const_cast<Plane &>(const_cast<SectorCluster const *>(this)->visPlane(planeIndex));
}

Plane const &SectorCluster::visPlane(int planeIndex) const
{
    if(planeIndex >= Sector::Floor && planeIndex <= Sector::Ceiling)
    {
        // Time to remap the planes?
        if(d->needRemapVisPlanes())
        {
            d->remapVisPlanes();
        }

        /// @todo Cache this result.
        SectorCluster const *mappedCluster = (planeIndex == Sector::Ceiling? d->mappedVisCeiling : d->mappedVisFloor);
        if(mappedCluster && mappedCluster != this)
        {
            return mappedCluster->visPlane(planeIndex);
        }
    }
    // Not mapped.
    return sector().plane(planeIndex);
}

AABoxd const &SectorCluster::aaBox() const
{
    // If the cluster is comprised of a single subspace we can use the bounding
    // box of the subspace geometry directly.
    if(d->subspaces.count() == 1)
    {
        return d->subspaces.first()->poly().aaBox();
    }

    // Time to determine bounds?
    if(d->aaBox.isNull())
    {
        // Unite the geometry bounding boxes of all subspaces in the cluster.
        foreach(ConvexSubspace const *subspace, d->subspaces)
        {
            AABoxd const &leafAABox = subspace->poly().aaBox();
            if(!d->aaBox.isNull())
            {
                V2d_UniteBox((*d->aaBox).arvec2, leafAABox.arvec2);
            }
            else
            {
                d->aaBox.reset(new AABoxd(leafAABox));
            }
        }
    }

    return *d->aaBox;
}

SectorCluster::Subspaces const &SectorCluster::subspaces() const
{
    return d->subspaces;
}

#ifdef __CLIENT__

bool SectorCluster::hasWorldVolume(bool useSmoothedHeights) const
{
    if(useSmoothedHeights)
    {
        return visCeiling().heightSmoothed() - visFloor().heightSmoothed() > 0;
    }
    else
    {
        return ceiling().height() - floor().height() > 0;
    }
}

coord_t SectorCluster::roughArea() const
{
    AABoxd const &bounds = aaBox();
    return (bounds.maxX - bounds.minX) * (bounds.maxY - bounds.minY);
}

void SectorCluster::markReverbDirty(bool yes)
{
    d->needReverbUpdate = yes;
}

AudioEnvironmentFactors const &SectorCluster::reverb() const
{
    // Perform any scheduled update now.
    if(d->needReverbUpdate)
    {
        d->updateReverb();
    }
    return d->reverb;
}

void SectorCluster::markVisPlanesDirty()
{
    d->maybeInvalidateMapping(Sector::Floor);
    d->maybeInvalidateMapping(Sector::Ceiling);
}

bool SectorCluster::hasSkyMaskedPlane() const
{
    for(int i = 0; i < sector().planeCount(); ++i)
    {
        if(visPlane(i).surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

SectorCluster::LightId SectorCluster::lightSourceId() const
{
    /// @todo Need unique cluster ids.
    return LightId(sector().indexInMap());
}

Vector3f SectorCluster::lightSourceColorf() const
{
    if(Rend_SkyLightIsEnabled() && hasSkyMaskedPlane())
    {
        return Rend_SkyLightColor();
    }

    // A non-skylight sector (i.e., everything else!)
    // Return the sector's ambient light color.
    return sector().lightColor();
}

float SectorCluster::lightSourceIntensity(Vector3d const &/*viewPoint*/) const
{
    return sector().lightLevel();
}

int SectorCluster::blockLightSourceZBias()
{
    int const height = int(visCeiling().height() - visFloor().height());
    bool hasSkyFloor = visFloor().surface().hasSkyMaskedMaterial();
    bool hasSkyCeil  = visCeiling().surface().hasSkyMaskedMaterial();

    if(hasSkyFloor && !hasSkyCeil)
    {
        return -height / 6;
    }
    if(!hasSkyFloor && hasSkyCeil)
    {
        return height / 6;
    }
    if(height > 100)
    {
        return (height - 100) / 2;
    }
    return 0;
}

void SectorCluster::applyBiasDigest(BiasDigest &allChanges)
{
    Instance::ShardGeometryMap::const_iterator it = d->shardGeomMap.constBegin();
    while(it != d->shardGeomMap.constEnd())
    {
        it.key()->biasTracker().applyChanges(allChanges);
        ++it;
    }
}

// Determine the number of bias illumination points needed for this geometry.
// Presently we define a 1:1 mapping to geometry vertices.
static int countIlluminationPoints(MapElement &mapElement, int group)
{
    switch(mapElement.type())
    {
    case DMU_BSPLEAF: {
        BspLeaf &bspLeaf = mapElement.as<BspLeaf>();
        DENG2_ASSERT(group >= 0 && group < bspLeaf.sector().planeCount()); // sanity check
        return bspLeaf.subspace().numFanVertices(); }

    case DMU_SEGMENT:
        DENG2_ASSERT(group >= 0 && group <= LineSide::Top); // sanity check
        return 4;

    default:
        throw Error("SectorCluster::countIlluminationPoints", "Invalid MapElement type");
    }
    return 0;
}

Shard &SectorCluster::shard(MapElement &mapElement, int geomId)
{
    Instance::GeometryData *gdata = d->geomData(mapElement, geomId, true /*create*/);
    if(gdata->shard.isNull())
    {
        gdata->shard.reset(new Shard(countIlluminationPoints(mapElement, geomId), this));
    }
    return *gdata->shard;
}

Shard *SectorCluster::findShard(MapElement &mapElement, int geomId)
{
    if(Instance::GeometryData *gdata = d->geomData(mapElement, geomId))
    {
        return gdata->shard.data();
    }
    return 0;
}

/**
 * @todo This could be enhanced so that only the lights on the right side of the
 * surface are taken into consideration.
 */
bool SectorCluster::updateBiasContributors(Shard *shard)
{
    if(Instance::GeometryData *gdata = d->geomDataForShard(shard))
    {
        Map::BiasSources const &sources = sector().map().biasSources();

        BiasTracker &tracker = shard->biasTracker();
        tracker.clearContributors();

        switch(gdata->mapElement->type())
        {
        case DMU_BSPLEAF: {
            BspLeaf &bspLeaf       = gdata->mapElement->as<BspLeaf>();
            Plane const &plane     = visPlane(gdata->geomId);
            Surface const &surface = plane.surface();

            Vector3d surfacePoint(bspLeaf.poly().center(), plane.heightSmoothed());

            foreach(BiasSource *source, sources)
            {
                // If the source is too weak we will ignore it completely.
                if(source->intensity() <= 0)
                    continue;

                Vector3d sourceToSurface = (source->origin() - surfacePoint).normalize();
                coord_t distance = 0;

                // Calculate minimum 2D distance to the BSP leaf.
                /// @todo This is probably too accurate an estimate.
                HEdge *baseNode = bspLeaf.poly().hedge();
                HEdge *node = baseNode;
                do
                {
                    coord_t len = (Vector2d(source->origin()) - node->origin()).length();
                    if(node == baseNode || len < distance)
                        distance = len;
                } while((node = &node->next()) != baseNode);

                if(sourceToSurface.dot(surface.normal()) < 0)
                    continue;

                tracker.addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
            }
            break; }

        case DMU_SEGMENT: {
            LineSideSegment &seg   = gdata->mapElement->as<LineSideSegment>();
            Surface const &surface = seg.lineSide().middle();
            Vector2d const &from   = seg.hedge().origin();
            Vector2d const &to     = seg.hedge().twin().origin();
            Vector2d const center  = (from + to) / 2;

            foreach(BiasSource *source, sources)
            {
                // If the source is too weak we will ignore it completely.
                if(source->intensity() <= 0)
                    continue;

                Vector3d sourceToSurface = (source->origin() - center).normalize();

                // Calculate minimum 2D distance to the segment.
                coord_t distance = 0;
                for(int k = 0; k < 2; ++k)
                {
                    coord_t len = (Vector2d(source->origin()) - (!k? from : to)).length();
                    if(k == 0 || len < distance)
                        distance = len;
                }

                if(sourceToSurface.dot(surface.normal()) < 0)
                    continue;

                tracker.addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
            }
            break; }

        default:
            throw Error("SectorCluster::updateBiasContributors", "Invalid MapElement type");
        }

        return true;
    }
    return false;
}

uint SectorCluster::biasLastChangeOnFrame() const
{
    return sector().map().biasLastChangeOnFrame();
}

#endif // __CLIENT__

// SectorClusterCirculator -----------------------------------------------------

SectorCluster *SectorClusterCirculator::getCluster(HEdge const &hedge) // static
{
    if(!hedge.hasFace()) return 0;
    if(!hedge.face().hasMapElement()) return 0;
    if(hedge.face().mapElement().type() != DMU_BSPLEAF) return 0;
    return hedge.face().mapElementAs<BspLeaf>().clusterPtr();
}

HEdge &SectorClusterCirculator::getNeighbor(HEdge const &hedge, ClockDirection direction,
    SectorCluster const *cluster) // static
{
    HEdge *neighbor = &hedge.neighbor(direction);
    // Skip over interior edges.
    if(cluster)
    {
        while(neighbor->hasTwin() && cluster == getCluster(neighbor->twin()))
        {
            neighbor = &neighbor->twin().neighbor(direction);
        }
    }
    return *neighbor;
}
