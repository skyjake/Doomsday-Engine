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

#ifdef __CLIENT__
#  include "clientapp.h"
#endif
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
#  include "gl/gl_texmanager.h"
#  include "render/rend_main.h"
#  include "render/billboard.h"
#  include "render/projector.h"
#  include "render/r_main.h"
#  include "render/rend_clip.h"       /// @todo remove me
#  include "render/rend_fakeradio.h"
#  include "BiasIllum"
#  include "BiasTracker"
#  include "MaterialSnapshot"
#  include "Shard"
#  include "TriangleStripBuilder"
#  include "WallEdge"
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

#ifdef __CLIENT__
/**
 * @defgroup skyCapFlags  Sky Cap Flags
 * @ingroup flags
 */
///@{
#define SKYCAP_LOWER 0x1
#define SKYCAP_UPPER 0x2
///@}

    static coord_t skyPlaneZ(SectorCluster &cluster, int skyCap)
    {
        if(!P_IsInVoid(viewPlayer))
        {
            Map &map = cluster.sector().map();
            return map.skyPlane((skyCap & SKYCAP_UPPER)? Map::SkyCeiling : Map::SkyFloor).height();
        }
        else
        {
            return cluster.visPlane((skyCap & SKYCAP_UPPER)? Sector::Ceiling : Sector::Floor).heightSmoothed();
        }
    }

    static Lumobj::LightmapSemantic lightmapForSurface(Surface const &surface)
    {
        if(surface.parent().type() == DMU_SIDE) return Lumobj::Side;
        // Must be a plane then.
        Plane const &plane = surface.parent().as<Plane>();
        return plane.isSectorFloor()? Lumobj::Down : Lumobj::Up;
    }

    static void quadTexCoords(Vector2f *tc, Vector3f const *posCoords,
        coord_t wallLength, Vector3d const &topLeft)
    {
        tc[0].x = tc[1].x = posCoords[0].x - topLeft.x;
        tc[3].y = tc[1].y = posCoords[0].y - topLeft.y;
        tc[3].x = tc[2].x = tc[0].x + wallLength;
        tc[2].y = tc[3].y + (posCoords[1].z - posCoords[0].z);
        tc[0].y = tc[3].y + (posCoords[3].z - posCoords[2].z);
    }

    static void quadLightCoords(Vector2f *tc, Vector2f const &topLeft, Vector2f const &bottomRight)
    {
        tc[1].x = tc[0].x = topLeft.x;
        tc[1].y = tc[3].y = topLeft.y;
        tc[3].x = tc[2].x = bottomRight.x;
        tc[2].y = tc[0].y = bottomRight.y;
    }

    static float shinyVertical(float dy, float dx)
    {
        return ((atan(dy/dx) / (PI/2)) + 1) / 2;
    }

    static void quadShinyTexCoords(Vector2f *tc, Vector3f const *posCoords, coord_t wallLength)
    {
        Vector3f const *topLeft     = &posCoords[1];
        Vector3f const *bottomRight = &posCoords[2];

        vec2f_t surface, normal, projected, s, reflected, view;
        float distance, angle, prevAngle = 0;
        uint i;

        // Quad surface vector.
        V2f_Set(surface, (bottomRight->x - topLeft->x) / wallLength,
                         (bottomRight->y - topLeft->y) / wallLength);

        V2f_Set(normal, surface[VY], -surface[VX]);

        // Calculate coordinates based on viewpoint and surface normal.
        for(i = 0; i < 2; ++i)
        {
            // View vector.
            V2f_Set(view, vOrigin.x - (i == 0? topLeft->x : bottomRight->x),
                          vOrigin.z - (i == 0? topLeft->y : bottomRight->y));

            distance = V2f_Normalize(view);

            V2f_Project(projected, view, normal);
            V2f_Subtract(s, projected, view);
            V2f_Scale(s, 2);
            V2f_Sum(reflected, view, s);

            angle = acos(reflected[VY]) / PI;
            if(reflected[VX] < 0)
            {
                angle = 1 - angle;
            }

            if(i == 0)
            {
                prevAngle = angle;
            }
            else
            {
                if(angle > prevAngle)
                    angle -= 1;
            }

            // Horizontal coordinates.
            tc[ (i == 0 ? 1 : 2) ].x =
                tc[ (i == 0 ? 0 : 3) ].x = angle + .3f; /*acos(-dot)/PI*/

            // Vertical coordinates.
            tc[ (i == 0 ? 0 : 2) ].y = shinyVertical(vOrigin.y - bottomRight->z, distance);
            tc[ (i == 0 ? 1 : 3) ].y = shinyVertical(vOrigin.y - topLeft->z, distance);
        }
    }

    /**
     * The DOOM lighting model applies a sector light level delta when drawing
     * walls based on their 2D world angle.
     *
     * @todo WallEdge should encapsulate.
     */
    static float calcLightLevelDelta(Vector3f const &normal)
    {
        return (1.0f / 255) * (normal.x * 18) * rendLightWallAngle;
    }

    static void wallSectionLightLevelDeltas(WallEdgeSection const &sectionLeft,
        WallEdgeSection const &sectionRight, float &leftDelta, float &rightDelta)
    {
        leftDelta = calcLightLevelDelta(sectionLeft.normal());

        if(sectionLeft.normal() == sectionRight.normal())
        {
            rightDelta = leftDelta;
        }
        else
        {
            rightDelta = calcLightLevelDelta(sectionRight.normal());

            // Linearly interpolate to find the light level delta values for the
            // vertical edges of this wall section.
            coord_t const lineLength    = sectionLeft.edge().lineSide().line().length();
            coord_t const sectionOffset = sectionLeft.edge().lineSideOffset();
            coord_t const sectionWidth  = de::abs(Vector2d(sectionRight.edge().origin() - sectionLeft.edge().origin()).length());

            float deltaDiff = rightDelta - leftDelta;
            rightDelta = leftDelta + ((sectionOffset + sectionWidth) / lineLength) * deltaDiff;
            leftDelta += (sectionOffset / lineLength) * deltaDiff;
        }
    }

#endif // __CLIENT__
}
}

using namespace de;
using namespace de::internal;

#ifdef __CLIENT__
static Vector3f curSectorLightColor;
static float curSectorLightLevel;
#endif

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

    WallEdges wallEdges;
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

        qDeleteAll(wallEdges);

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

                    if(!hedge->twin().face().hasMapElement())
                        continue;

                    ConvexSubspace const &backSubspace = hedge->twin().face().mapElementAs<ConvexSubspace>();
                    // Cluster internal edges are not considered.
                    if(&backSubspace.cluster() == thisPublic)
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

                    SectorCluster const &backCluster = backSubspace.cluster();
                    if(backCluster.floor().height() < self.sector().floor().height() &&
                       backSide.bottom().hasDrawableNonFixMaterial())
                    {
                        flags &= ~AllMissingBottom;
                    }

                    if(backCluster.ceiling().height() > self.sector().ceiling().height() &&
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

                if(!hedge->twin().hasFace() || !hedge->twin().face().hasMapElement())
                    continue;

                SectorCluster &backCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().cluster();
                if(&backCluster == thisPublic)
                    continue;

                extClusterMap.insert(&backCluster, hedge);

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
            SectorCluster &extCluster = iter.value()->twin().face().mapElementAs<ConvexSubspace>().cluster();
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
                SectorCluster &extCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().cluster();

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
                    SectorCluster &extCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().cluster();

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
            SectorCluster &extCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().cluster();

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
            SectorCluster &extCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().cluster();

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
        if(!boundaryData->uniqueOuterEdges.isEmpty())
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
                    if(Shard *shard = self.findShard(*subspace, plane.indexInSector()))
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

    static int addReverbSubspaceWorker(ConvexSubspace *subspace, void *context)
    {
        static_cast<Instance *>(context)->addReverbSubspace(subspace);
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

        // Link all convex subspaces whose axis-aligned bounding box intersects
        // with the affection bounds to the reverb set.
        self.sector().map().subspaceBoxIterator(affectionBounds, addReverbSubspaceWorker, this);
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

    WallEdge *findWallEdge(HEdge &hedge, int side)
    {
        // Verify that the hedge is one of "ours".
        DENG2_ASSERT(hedge.face().mapElementAs<ConvexSubspace>().clusterPtr() == thisPublic);
        // Verify that the hedge represents a drawable wall.
        DENG2_ASSERT(hedge.mapElementAs<LineSideSegment>().lineSide().hasSections());

        // Perhaps we already have a wall edge for this?
        WallEdges::iterator it = wallEdges.find(&hedge);
        while(it != wallEdges.end() && it.key() == &hedge)
        {
            WallEdge *wedge = *it;
            if(side == wedge->side())
            {
                return wedge; // Found it.
            }
            it++;
        }

        // Allocate this now.
        /// @todo Do this during cluster init.
        return wallEdges.insert(&hedge, new WallEdge(hedge, side)).value();
    }

    struct preparedynlightshards_params_t
    {
        Instance *inst;
        ConvexSubspace *subspace;
        uint lastIdx;
        WorldVBuf::Index vertCount;
        union {
            de::Vector3f const *posCoords;
            WorldVBuf::Index const *indices;
        };
        Vector3d const *topLeft;
        Vector3d const *bottomRight;

        // Wall section edges:
        // Both are provided or none at all. If present then this is a wall geometry.
        WallEdgeSection const *leftSection;
        WallEdgeSection const *rightSection;
    };

    void prepareDynlightShards(TexProjection const &tp, preparedynlightshards_params_t &p)
    {
        WorldVBuf &vbuf = ClientApp::renderSystem().worldVBuf();

        // If multitexturing is in use we skip the first.
        if(!(Rend_IsMTexLights() && p.lastIdx == 0))
        {
            DrawListSpec listSpec;
            listSpec.group = LightGeom;
            listSpec.texunits[TU_PRIMARY] = GLTextureUnit(tp.texture, gl::ClampToEdge, gl::ClampToEdge);

            if(p.leftSection) // A wall.
            {
                WallEdgeSection const &leftSection  = *p.leftSection;
                WallEdgeSection const &rightSection = *p.rightSection;
                bool const mustSubdivide            = (leftSection.divisionCount() || rightSection.divisionCount());

                if(mustSubdivide) // Draw as two triangle fans.
                {
                    WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();
                    WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();

                    Vector2f const quadCoords[4] = {
                        Vector2f(tp.topLeft.x,     tp.bottomRight.y),
                        Vector2f(tp.topLeft.x,     tp.topLeft.y    ),
                        Vector2f(tp.bottomRight.x, tp.bottomRight.y),
                        Vector2f(tp.bottomRight.x, tp.topLeft.y    )
                    };

                    Shard::Geom *shard = new Shard::Geom(listSpec);
                    p.subspace->shards() << shard;

                    shard->indices.resize(leftFanSize + rightFanSize);

                    vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                    Rend_DivPosCoords(shard->indices.data(), p.posCoords, leftSection, rightSection);
                    Rend_DivTexCoords(shard->indices.data(), quadCoords, leftSection, rightSection,
                                      WorldVBuf::PrimaryTex);

                    for(WorldVBuf::Index i = 0; i < leftFanSize + rightFanSize; ++i)
                    {
                        WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                        //vertex.pos  = vbuf[p.indices[i]].pos;
                        vertex.rgba = tp.color;
                    }

                    Shard::Geom::Primitive leftFan;
                    leftFan.type             = gl::TriangleFan;
                    leftFan.vertCount        = leftFanSize;
                    leftFan.indices          = shard->indices.data();
                    leftFan.texScale         = Vector2f(1, 1);
                    leftFan.texOffset        = Vector2f(0, 0);
                    leftFan.detailTexScale   = Vector2f(1, 1);
                    leftFan.detailTexOffset  = Vector2f(0, 0);
                    shard->primitives.append(leftFan);

                    Shard::Geom::Primitive rightFan;
                    rightFan.type            = gl::TriangleFan;
                    rightFan.vertCount       = rightFanSize;
                    rightFan.indices         = shard->indices.data() + leftFan.vertCount;
                    rightFan.texScale        = Vector2f(1, 1);
                    rightFan.texOffset       = Vector2f(0, 0);
                    rightFan.detailTexScale  = Vector2f(1, 1);
                    rightFan.detailTexOffset = Vector2f(0, 0);
                    shard->primitives.append(rightFan);
                }
                else // Draw as one quad.
                {
                    WorldVBuf::Index vertCount = p.vertCount;

                    Shard::Geom *shard = new Shard::Geom(listSpec);
                    p.subspace->shards() << shard;

                    shard->indices.resize(vertCount);

                    vbuf.reserveElements(vertCount, shard->indices);
                    for(int i = 0; i < vertCount; ++i)
                    {
                        WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                        vertex.pos  = p.posCoords[i];// vbuf[p.indices[i]].pos;
                        vertex.rgba = tp.color;
                    }

                    vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].x =
                    vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].x = tp.topLeft.x;

                    vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].y =
                    vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].y = tp.topLeft.y;

                    vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].x =
                    vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].x = tp.bottomRight.x;

                    vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].y =
                    vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].y = tp.bottomRight.y;

                    Shard::Geom::Primitive prim;
                    prim.type            = gl::TriangleStrip;
                    prim.vertCount       = vertCount;
                    prim.indices         = shard->indices.data();
                    prim.texScale        = Vector2f(1, 1);
                    prim.texOffset       = Vector2f(0, 0);
                    prim.detailTexScale  = Vector2f(1, 1);
                    prim.detailTexOffset = Vector2f(0, 0);
                    shard->primitives.append(prim);
                }
            }
            else // A flat.
            {
                Vector2f const pDimensions = p.bottomRight->xy() - p.topLeft->xy();
                WorldVBuf::Index vertCount = p.vertCount;

                Shard::Geom *shard = new Shard::Geom(listSpec);
                p.subspace->shards() << shard;

                shard->indices.resize(vertCount);

                vbuf.reserveElements(vertCount, shard->indices);
                for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];

                    vertex.pos  = vbuf[p.indices[i]].pos;
                    vertex.rgba = tp.color;

                    vertex.texCoord[WorldVBuf::PrimaryTex] =
                        Vector2f(((p.bottomRight->x - vertex.pos.x) / pDimensions.x * tp.topLeft.x) +
                                 ((vertex.pos.x     - p.topLeft->x) / pDimensions.x * tp.bottomRight.x)
                                 ,
                                 ((p.bottomRight->y - vertex.pos.y) / pDimensions.y * tp.topLeft.y) +
                                 ((vertex.pos.y     - p.topLeft->y) / pDimensions.y * tp.bottomRight.y));
                }

                Shard::Geom::Primitive prim;
                prim.type            = gl::TriangleFan;
                prim.vertCount       = vertCount;
                prim.indices         = shard->indices.data();
                prim.texScale        = Vector2f(1, 1);
                prim.texOffset       = Vector2f(0, 0);
                prim.detailTexScale  = Vector2f(1, 1);
                prim.detailTexOffset = Vector2f(0, 0);
                shard->primitives.append(prim);
            }
        }
        p.lastIdx++;
    }

    /// Generates a new primitive for each light projection.
    static int prepareDynlightShardsWorker(TexProjection const *tp, void *context)
    {
        preparedynlightshards_params_t &parm = *static_cast<preparedynlightshards_params_t *>(context);
        parm.inst->prepareDynlightShards(*tp, parm);
        return 0; // Continue iteration.
    }

    /**
     * Render all dynlights in projection list @a listIdx according to @a parm
     * writing them to the renderering lists for the current frame.
     *
     * @note If multi-texturing is being used for the first light; it is skipped.
     *
     * @return  Number of lights rendered.
     */
    uint prepareAllDynlightShards(uint listIdx, preparedynlightshards_params_t &parm)
    {
        uint numRendered = parm.lastIdx;

        Rend_IterateProjectionList(listIdx, prepareDynlightShardsWorker, (void *)&parm);

        numRendered = parm.lastIdx - numRendered;
        if(Rend_IsMTexLights())
        {
            numRendered -= 1;
        }
        return numRendered;
    }

    struct preparealldynshadowshards_params_t
    {
        ConvexSubspace *subspace;
        uint lastIdx;
        WorldVBuf::Index vertCount;
        union {
            Vector3f const *posCoords;
            WorldVBuf::Index const *indices;
        };
        Vector3d const *topLeft;
        Vector3d const *bottomRight;

        // Wall section edges:
        // Both are provided or none at all. If present then this is a wall geometry.
        WallEdgeSection const *leftSection;
        WallEdgeSection const *rightSection;
    };

    /**
     * Generates a new primitive for the shadow projection.
     *
     * @param drawListSpec  Draw list specififcation for the projected geometry.
     * @param tp            The projected texture.
     * @param parm          Shadow drawer parameters.
     */
    void prepareDynshadowShards(DrawListSpec &drawListSpec, TexProjection const &tp,
        preparealldynshadowshards_params_t &p)
    {
        WorldVBuf &vbuf = ClientApp::renderSystem().worldVBuf();

        if(p.leftSection) // A wall.
        {
            WallEdgeSection const &leftSection  = *p.leftSection;
            WallEdgeSection const &rightSection = *p.rightSection;
            bool const mustSubdivide            = (leftSection.divisionCount() || rightSection.divisionCount());

            if(mustSubdivide) // Draw as two triangle fans.
            {
                WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();
                WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();

                Vector2f quadCoords[4] = {
                    Vector2f(tp.topLeft.x,     tp.bottomRight.y),
                    Vector2f(tp.topLeft.x,     tp.topLeft.y    ),
                    Vector2f(tp.bottomRight.x, tp.bottomRight.y),
                    Vector2f(tp.bottomRight.x, tp.topLeft.y    )
                };

                Shard::Geom *shard = new Shard::Geom(drawListSpec);
                shard->indices.resize(leftFanSize + rightFanSize);

                vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                Rend_DivPosCoords(shard->indices.data(), p.posCoords, leftSection, rightSection);
                Rend_DivTexCoords(shard->indices.data(), quadCoords, leftSection, rightSection,
                                  WorldVBuf::PrimaryTex);

                for(WorldVBuf::Index i = 0; i < leftFanSize + rightFanSize; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                    //vertex.pos  = vbuf[p.indices[i]].pos;
                    vertex.rgba = tp.color;
                }

                Shard::Geom::Primitive leftFan;
                leftFan.type             = gl::TriangleFan;
                leftFan.vertCount        = leftFanSize;
                leftFan.indices          = shard->indices.data();
                leftFan.texScale         = Vector2f(1, 1);
                leftFan.texOffset        = Vector2f(0, 0);
                leftFan.detailTexScale   = Vector2f(1, 1);
                leftFan.detailTexOffset  = Vector2f(0, 0);
                shard->primitives.append(leftFan);

                Shard::Geom::Primitive rightFan;
                rightFan.type            = gl::TriangleFan;
                rightFan.vertCount       = rightFanSize;
                rightFan.indices         = shard->indices.data() + leftFan.vertCount;
                rightFan.texScale        = Vector2f(1, 1);
                rightFan.texOffset       = Vector2f(0, 0);
                rightFan.detailTexScale  = Vector2f(1, 1);
                rightFan.detailTexOffset = Vector2f(0, 0);
                shard->primitives.append(rightFan);

                p.subspace->shards() << shard;
            }
            else
            {
                WorldVBuf::Index vertCount = p.vertCount;

                Shard::Geom *shard = new Shard::Geom(drawListSpec);
                shard->indices.resize(vertCount);

                vbuf.reserveElements(vertCount, shard->indices);
                for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                    vertex.pos  = p.posCoords[i];//vbuf[p.indices[i]].pos;
                    vertex.rgba = tp.color;
                }

                vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].x =
                vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].x = tp.topLeft.x;

                vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].y =
                vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].y = tp.topLeft.y;

                vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].x =
                vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].x = tp.bottomRight.x;

                vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].y =
                vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].y = tp.bottomRight.y;

                Shard::Geom::Primitive prim;
                prim.type            = gl::TriangleStrip;
                prim.vertCount       = vertCount;
                prim.indices         = shard->indices.data();
                prim.texScale        = Vector2f(1, 1);
                prim.texOffset       = Vector2f(0, 0);
                prim.detailTexScale  = Vector2f(1, 1);
                prim.detailTexOffset = Vector2f(0, 0);
                shard->primitives.append(prim);

                p.subspace->shards() << shard;
            }
        }
        else // A flat.
        {
            Vector2f const pDimensions = p.bottomRight->xy() - p.topLeft->xy();

            WorldVBuf::Index vertCount = p.vertCount;

            Shard::Geom *shard = new Shard::Geom(drawListSpec);
            shard->indices.resize(vertCount);

            vbuf.reserveElements(vertCount, shard->indices);
            for(WorldVBuf::Index i = 0; i < vertCount; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[shard->indices[i]];

                vertex.pos  = vbuf[p.indices[i]].pos;
                vertex.rgba =    tp.color;

                vertex.texCoord[WorldVBuf::PrimaryTex] =
                    Vector2f(((p.bottomRight->x - vertex.pos.x) / pDimensions.x * tp.topLeft.x) +
                             ((vertex.pos.x     - p.topLeft->x) / pDimensions.x * tp.bottomRight.x)
                             ,
                             ((p.bottomRight->y - vertex.pos.y) / pDimensions.y * tp.topLeft.y) +
                             ((vertex.pos.y     - p.topLeft->y) / pDimensions.y * tp.bottomRight.y));
            }

            Shard::Geom::Primitive prim;
            prim.type            = gl::TriangleFan;
            prim.vertCount       = vertCount;
            prim.indices         = shard->indices.data();
            prim.texScale        = Vector2f(1, 1);
            prim.texOffset       = Vector2f(0, 0);
            prim.detailTexScale  = Vector2f(1, 1);
            prim.detailTexOffset = Vector2f(0, 0);
            shard->primitives.append(prim);

            p.subspace->shards() << shard;
        }
    }

    struct drawshadowworker_params_t
    {
        Instance *inst;
        DrawListSpec drawListSpec;
        preparealldynshadowshards_params_t *drawShadowParms;
    };

    static int prepareDynshadowShardsWorker(TexProjection const *tp, void *context)
    {
        drawshadowworker_params_t &parm = *static_cast<drawshadowworker_params_t *>(context);
        parm.inst->prepareDynshadowShards(parm.drawListSpec, *tp, *parm.drawShadowParms);
        return 0; // Continue iteration.
    }

    /**
     * Render all shadows in projection list @a listIdx according to @a parm
     * writing them to the renderering lists for the current frame.
     */
    void prepareAllDynshadowShards(uint listIdx, preparealldynshadowshards_params_t &p)
    {
        // Write shadows to the render lists.
        drawshadowworker_params_t parm;
        parm.inst                              = this;
        parm.drawListSpec.group                = ShadowGeom;
        parm.drawListSpec.texunits[TU_PRIMARY] = GLTextureUnit(GL_PrepareLSTexture(LST_DYNAMIC), gl::ClampToEdge, gl::ClampToEdge);
        parm.drawShadowParms = &p;

        Rend_IterateProjectionList(listIdx, prepareDynshadowShardsWorker, &parm);
    }

    void projectDynamics(ConvexSubspace &subspace, Surface const &surface,
        float glowStrength, Vector3d const &topLeft, Vector3d const &bottomRight,
        bool noLights, bool noShadows, bool sortLights,
        uint &lightListIdx, uint &shadowListIdx)
    {
        if(glowStrength >= 1 || levelFullBright)
            return;

        // lights?
        if(!noLights)
        {
            float const blendFactor = 1;

            if(useDynLights)
            {
                Rend_ProjectLumobjs(&subspace, topLeft, bottomRight,
                                    surface.tangentMatrix(), blendFactor,
                                    lightmapForSurface(surface), sortLights,
                                    lightListIdx);
            }

            if(useGlowOnWalls && surface.parent().type() == DMU_SIDE)
            {
                Rend_ProjectPlaneGlows(&subspace, topLeft, bottomRight,
                                       surface.tangentMatrix(), blendFactor,
                                       sortLights, lightListIdx);
            }
        }

        // Shadows?
        if(!noShadows && useShadows)
        {
            // Glow inversely diminishes shadow strength.
            float const blendFactor = 1 - glowStrength;

            Rend_ProjectMobjShadows(&subspace, topLeft, bottomRight,
                                    surface.tangentMatrix(), blendFactor,
                                    shadowListIdx);
        }
    }

    /// @param skyCap  @ref skyCapFlags.
    void prepareSkyMaskCapShards(ConvexSubspace &subspace, int skyCap)
    {
        RenderSystem &rendSys = ClientApp::renderSystem();
        WorldVBuf &vbuf       = rendSys.worldVBuf();

        // Caps are unnecessary in sky debug mode (will be drawn as regular planes).
        if(devRendSkyMode) return;
        if(!skyCap) return;

        ClockDirection const direction = (skyCap & SKYCAP_UPPER)? Anticlockwise : Clockwise;
        coord_t const height           = skyPlaneZ(self, skyCap);
        Face const &poly               = subspace.poly();

        HEdge *fanBase = subspace.fanBase();
        WorldVBuf::Index const vertCount = poly.hedgeCount() + (!fanBase? 2 : 0);

        Shard::Geom *shard = new Shard::Geom(DrawListSpec(SkyMaskGeom));
        shard->indices.resize(vertCount);

        vbuf.reserveElements(vertCount, shard->indices);
        WorldVBuf::Index n = 0;
        if(!fanBase)
        {
            vbuf[shard->indices[n]].pos = Vector3f(poly.center(), height);
            n++;
        }

        // Add the vertices for each hedge.
        HEdge *base  = fanBase? fanBase : poly.hedge();
        HEdge *hedge = base;
        do
        {
            vbuf[shard->indices[n]].pos = Vector3f(hedge->origin(), height);
            n++;
        } while((hedge = &hedge->neighbor(direction)) != base);

        // The last vertex is always equal to the first.
        if(!fanBase)
        {
            vbuf[shard->indices[n]].pos = Vector3f(poly.hedge()->origin(), height);
        }

        Shard::Geom::Primitive prim;
        prim.type            = gl::TriangleFan;
        prim.vertCount       = vertCount;
        prim.indices         = shard->indices.data();
        prim.texScale        = Vector2f(1, 1);
        prim.texOffset       = Vector2f(0, 0);
        prim.detailTexScale  = Vector2f(1, 1);
        prim.detailTexOffset = Vector2f(0, 0);
        shard->primitives.append(prim);

        subspace.shards() << shard;
    }

    void prepareSkyMaskWallShardStrip(ConvexSubspace &subspace, int vertCount,
        Vector3f const *posCoords, Vector2f const *texCoords, Material *material)
    {
        DENG2_ASSERT(posCoords != 0);

        WorldVBuf &vbuf = ClientApp::renderSystem().worldVBuf();

        DrawListSpec listSpec;
        listSpec.group = (devRendSkyMode? UnlitGeom : SkyMaskGeom);
        if(devRendSkyMode && renderTextures != 2)
        {
            // Map RTU configuration from the sky surface material.
            DENG2_ASSERT(material != 0);
            MaterialSnapshot const &ms = material->prepare(Rend_MapSurfaceMaterialSpec());
            listSpec.texunits[TU_PRIMARY]        = ms.unit(RTU_PRIMARY);
            listSpec.texunits[TU_PRIMARY_DETAIL] = ms.unit(RTU_PRIMARY_DETAIL);
            listSpec.texunits[TU_INTER]          = ms.unit(RTU_INTER);
            listSpec.texunits[TU_INTER_DETAIL]   = ms.unit(RTU_INTER_DETAIL);
        }

        Shard::Geom *shard = new Shard::Geom(listSpec);
        shard->indices.resize(vertCount);

        vbuf.reserveElements(vertCount, shard->indices);
        for(int i = 0; i < vertCount; ++i)
        {
            WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
            vertex.pos = posCoords[i];
            if(devRendSkyMode)
            {
                vertex.rgba = Vector4f(1, 1, 1, 1); // This geometry is always fully lit.
                DENG2_ASSERT(texCoords != 0);
                vertex.texCoord[WorldVBuf::PrimaryTex] = texCoords[i];
            }
        }

        Shard::Geom::Primitive prim;
        prim.type      = gl::TriangleStrip;
        prim.vertCount = vertCount;
        prim.indices   = shard->indices.data();
        if(!devRendSkyMode)
        {
            prim.texScale        = Vector2f(1, 1);
            prim.texOffset       = Vector2f(0, 0);
            prim.detailTexScale  = Vector2f(1, 1);
            prim.detailTexOffset = Vector2f(0, 0);
        }
        else
        {
            prim.texScale        = shard->listSpec.unit(TU_PRIMARY       ).scale;
            prim.texOffset       = shard->listSpec.unit(TU_PRIMARY       ).offset;
            prim.detailTexScale  = shard->listSpec.unit(TU_PRIMARY_DETAIL).scale;
            prim.detailTexOffset = shard->listSpec.unit(TU_PRIMARY_DETAIL).offset;
        }
        shard->primitives.append(prim);

        subspace.shards() << shard;
    }

    void prepareSkyMaskWallShards(ConvexSubspace &subspace, WallEdge::SectionId sectionId)
    {
        // Determine strip generation behavior.
        ClockDirection const direction   = Clockwise;
        bool const buildTexCoords        = CPP_BOOL(devRendSkyMode);
        bool const splitOnMaterialChange = (devRendSkyMode && renderTextures != 2);

        // Configure the strip builder wrt vertex attributes.
        TriangleStripBuilder stripBuilder(buildTexCoords);

        // Configure the strip build state (we'll most likely need to break edge loop
        // into multiple strips).
        HEdge *startNode          = 0;
        coord_t startZBottom      = 0;
        coord_t startZTop         = 0;
        Material *startMaterial   = 0;
        float startMaterialOffset = 0;

        // Determine the relative sky plane (for monitoring material changes).
        int relPlane = sectionId == WallEdge::SkyTop? Sector::Ceiling : Sector::Floor;

        // Begin generating geometry.
        HEdge *base  = subspace.poly().hedge();
        HEdge *hedge = base;
        forever
        {
            // Are we monitoring material changes?
            Material *skyMaterial = 0;
            if(splitOnMaterialChange)
            {
                skyMaterial = hedge->face().mapElementAs<ConvexSubspace>()
                                  .cluster().visPlane(relPlane).surface().materialPtr();
            }

            // Add a first (left) edge to the current strip?
            if(startNode == 0 && hedge->hasMapElement())
            {
                LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
                if(seg.lineSide().hasSections())
                {
                    startMaterialOffset = seg.lineSideOffset();

                    // Prepare the edge geometry.
                    WallEdge left(*hedge, (direction == Anticlockwise)? Line::To : Line::From, startMaterialOffset);
                    WallEdgeSection &sectionLeft = left.section(sectionId);

                    if(sectionLeft.isValid())// && sectionLeft.bottom().z() < sectionLeft.top().z())
                    {
                        // A new strip begins.
                        stripBuilder.begin(direction);
                        stripBuilder << sectionLeft;

                        // Update the strip build state.
                        startNode     = hedge;
                        startZBottom  = sectionLeft.bottom().z();
                        startZTop     = sectionLeft.top().z();
                        startMaterial = skyMaterial;
                    }
                }
            }

            bool beginNewStrip = false;

            // Add the i'th (right) edge to the current strip?
            if(startNode != 0)
            {
                // Stop if we've reached a "null" edge.
                bool endStrip = false;
                if(hedge->hasMapElement())
                {
                    LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
                    if(seg.lineSide().hasSections())
                    {
                        startMaterialOffset += seg.length() * (direction == Anticlockwise? -1 : 1);

                        // Prepare the edge geometry
                        WallEdge left(*hedge, (direction == Anticlockwise)? Line::From : Line::To, startMaterialOffset);
                        WallEdgeSection &sectionLeft = left.section(sectionId);

                        if(!sectionLeft.isValid())// && sectionLeft.bottom().z() < sectionLeft.top().z()))
                        {
                            endStrip = true;
                        }
                        // Must we split the strip here?
                        else if(hedge != startNode &&
                                (!de::fequal(sectionLeft.bottom().z(), startZBottom) ||
                                 !de::fequal(sectionLeft.top().z(), startZTop) ||
                                 (splitOnMaterialChange && skyMaterial != startMaterial)))
                        {
                            endStrip = true;
                            beginNewStrip = true; // We'll continue from here.
                        }
                        else
                        {
                            // Extend the strip geometry.
                            stripBuilder << sectionLeft;
                        }
                    }
                    else
                    {
                        endStrip = true;
                    }
                }
                else
                {
                    endStrip = true;
                }

                if(endStrip || &hedge->neighbor(direction) == base)
                {
                    // End the current strip.
                    startNode = 0;

                    // Take ownership of the built geometry.
                    PositionBuffer *positions = 0;
                    TexCoordBuffer *texcoords = 0;
                    int numVerts = stripBuilder.take(&positions, &texcoords);

                    // Write the strip geometry to the render lists.
                    prepareSkyMaskWallShardStrip(subspace, numVerts, positions->constData(),
                                                 texcoords? texcoords->constData() : 0,
                                                 startMaterial);

                    delete positions;
                    delete texcoords;
                }
            }

            // Start a new strip from the current node?
            if(beginNewStrip) continue;

            // On to the next node.
            hedge = &hedge->neighbor(direction);

            // Are we done?
            if(hedge == base) break;
        }
    }

    /// @param skyCap  @ref skyCapFlags
    void prepareAllSkyMaskShards(ConvexSubspace &subspace, int skyCap = SKYCAP_LOWER|SKYCAP_UPPER)
    {
        // Any work to do?
        // Sky caps are only necessary in sectors with sky-masked planes.
        if((skyCap & SKYCAP_LOWER) &&
           !self.visFloor().surface().hasSkyMaskedMaterial())
        {
            skyCap &= ~SKYCAP_LOWER;
        }
        if((skyCap & SKYCAP_UPPER) &&
           !self.visCeiling().surface().hasSkyMaskedMaterial())
        {
            skyCap &= ~SKYCAP_UPPER;
        }

        if(!skyCap) return;

        // Lower?
        if(skyCap & SKYCAP_LOWER)
        {
            prepareSkyMaskWallShards(subspace, WallEdge::SkyBottom);
            prepareSkyMaskCapShards(subspace, SKYCAP_LOWER);
        }

        // Upper?
        if(skyCap & SKYCAP_UPPER)
        {
            prepareSkyMaskWallShards(subspace, WallEdge::SkyTop);
            prepareSkyMaskCapShards(subspace, SKYCAP_UPPER);
        }
    }

    void prepareWallSectionShard(ConvexSubspace &subspace,
        rendworldpoly_params_t const &p, MaterialSnapshot const &matSnapshot)
    {
        DENG2_ASSERT(p.leftSection != 0 && p.rightSection != 0);

        WorldVBuf &vbuf = ClientApp::renderSystem().worldVBuf();

        bool useLights = false, useShadows = false;

        bool const skyMaskedMaterial = (p.skyMasked || (matSnapshot.material().isSkyMasked()));

        // Map RTU configuration from prepared MaterialSnapshot(s).
        GLTextureUnit const *primaryRTU       = (!p.skyMasked)? &matSnapshot.unit(RTU_PRIMARY) : NULL;
        GLTextureUnit const *primaryDetailRTU = (r_detail && !p.skyMasked && matSnapshot.unit(RTU_PRIMARY_DETAIL).hasTexture())? &matSnapshot.unit(RTU_PRIMARY_DETAIL) : NULL;
        GLTextureUnit const *interRTU         = (!p.skyMasked && matSnapshot.unit(RTU_INTER).hasTexture())? &matSnapshot.unit(RTU_INTER) : NULL;
        GLTextureUnit const *interDetailRTU   = (r_detail && !p.skyMasked && matSnapshot.unit(RTU_INTER_DETAIL).hasTexture())? &matSnapshot.unit(RTU_INTER_DETAIL) : NULL;
        GLTextureUnit const *shineRTU         = (useShinySurfaces && !p.skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture())? &matSnapshot.unit(RTU_REFLECTION) : NULL;
        GLTextureUnit const *shineMaskRTU     = (useShinySurfaces && !p.skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture() && matSnapshot.unit(RTU_REFLECTION_MASK).hasTexture())? &matSnapshot.unit(RTU_REFLECTION_MASK) : NULL;

        DGLuint modTex = 0;
        Vector2f modTexSt[2]; // [topLeft, bottomRight]
        Vector3f modColor;

        if(!skyMaskedMaterial && p.glowing < 1)
        {
            useLights  = (p.lightListIdx  > 0);
            useShadows = (p.shadowListIdx > 0);

            // If multitexturing is enabled and there is at least one dynlight
            // affecting this surface, grab the parameters needed to draw it.
            if(useLights && Rend_IsMTexLights())
            {
                TexProjection *dyn = 0;
                Rend_IterateProjectionList(p.lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

                modTex       = dyn->texture;
                modColor     = dyn->color;
                modTexSt[0]  = dyn->topLeft;
                modTexSt[1]  = dyn->bottomRight;
            }
        }

        Vector3f const posCoords[4] = {
            Vector3f( p.leftSection->bottom().origin()),
            Vector3f( p.leftSection->top   ().origin()),
            Vector3f(p.rightSection->bottom().origin()),
            Vector3f(p.rightSection->top   ().origin())
        };

        Vector2f primaryTexCoords[4];
        quadTexCoords(primaryTexCoords, posCoords, p.sectionWidth, *p.topLeft);

        Vector2f interTexCoords[4];
        if(interRTU)
        {
            quadTexCoords(interTexCoords, posCoords, p.sectionWidth, *p.topLeft);
        }

        Vector2f shineTexCoords[4];
        if(shineRTU)
        {
            quadShinyTexCoords(shineTexCoords, posCoords, p.sectionWidth);
        }

        Vector2f modTexCoords[4];
        if(modTex && Rend_IsMTexLights())
        {
            quadLightCoords(modTexCoords, modTexSt[0], modTexSt[1]);
        }

        // Light this polygon.
        Vector4f colorCoords[4];
        Vector4f shineColorCoords[4];
        if(!skyMaskedMaterial)
        {
            if(levelFullBright || !(p.glowing < 1))
            {
                // Uniform color. Apply to all vertices.
                float ll = de::clamp(0.f, curSectorLightLevel + (levelFullBright? 1 : p.glowing), 1.f);
                Vector4f *colorIt = colorCoords;
                for(duint16 i = 0; i < 4; ++i, colorIt++)
                {
                    colorIt->x = colorIt->y = colorIt->z = ll;
                }
            }
            else
            {
                // Non-uniform color.
                if(useBias)
                {
                    Map &map     = self.sector().map();
                    Shard &shard = self.shard(*p.mapElement, p.geomGroup);

                    // Apply the ambient light term from the grid (if available).
                    if(map.hasLightGrid())
                    {
                        Vector3f const *posIt = posCoords;
                        Vector4f *colorIt     = colorCoords;
                        for(duint16 i = 0; i < 4; ++i, posIt++, colorIt++)
                        {
                            *colorIt = map.lightGrid().evaluate(*posIt);
                        }
                    }

                    // Apply bias light source contributions.
                    shard.lightWithBiasSources(posCoords, colorCoords, *p.surfaceTangentMatrix,
                                               map.biasCurrentTime());

                    // Apply surface glow.
                    if(p.glowing > 0)
                    {
                        Vector4f const glow(p.glowing, p.glowing, p.glowing, 0);
                        Vector4f *colorIt = colorCoords;
                        for(duint16 i = 0; i < 4; ++i, colorIt++)
                        {
                            *colorIt += glow;
                        }
                    }

                    // Apply light range compression and clamp.
                    Vector3f const *posIt = posCoords;
                    Vector4f *colorIt     = colorCoords;
                    for(duint16 i = 0; i < 4; ++i, posIt++, colorIt++)
                    {
                        for(int k = 0; k < 3; ++k)
                        {
                            (*colorIt)[k] = de::clamp(0.f, (*colorIt)[k] + Rend_LightAdaptationDelta((*colorIt)[k]), 1.f);
                        }
                    }
                }
                else
                {
                    float llL = de::clamp(0.f, curSectorLightLevel + p.surfaceLightLevelDL + p.glowing, 1.f);
                    float llR = de::clamp(0.f, curSectorLightLevel + p.surfaceLightLevelDR + p.glowing, 1.f);

                    // Calculate the color for each vertex, blended with plane color?
                    if(p.surfaceColor->x < 1 || p.surfaceColor->y < 1 || p.surfaceColor->z < 1)
                    {
                        // Blend sector light+color+surfacecolor
                        Vector3f vColor = (*p.surfaceColor) * curSectorLightColor;

                        if(llL != llR)
                        {
                            Rend_LightVertex(colorCoords[0], posCoords[0], llL, vColor);
                            Rend_LightVertex(colorCoords[1], posCoords[1], llL, vColor);
                            Rend_LightVertex(colorCoords[2], posCoords[2], llR, vColor);
                            Rend_LightVertex(colorCoords[3], posCoords[3], llR, vColor);
                        }
                        else
                        {
                            Rend_LightVertices(4, colorCoords, posCoords, llL, vColor);
                        }
                    }
                    else
                    {
                        // Use sector light+color only.
                        if(llL != llR)
                        {
                            Rend_LightVertex(colorCoords[0], posCoords[0], llL, curSectorLightColor);
                            Rend_LightVertex(colorCoords[1], posCoords[1], llL, curSectorLightColor);
                            Rend_LightVertex(colorCoords[2], posCoords[2], llR, curSectorLightColor);
                            Rend_LightVertex(colorCoords[3], posCoords[3], llR, curSectorLightColor);
                        }
                        else
                        {
                            Rend_LightVertices(4, colorCoords, posCoords, llL, curSectorLightColor);
                        }
                    }

                    // Bottom color (if different from top)?
                    if(p.surfaceColor2)
                    {
                        // Blend sector light+color+surfacecolor
                        Vector3f vColor = (*p.surfaceColor2) * curSectorLightColor;

                        Rend_LightVertex(colorCoords[0], posCoords[0], llL, vColor);
                        Rend_LightVertex(colorCoords[2], posCoords[2], llR, vColor);
                    }
                }

                // Apply torch light?
                if(viewPlayer->shared.fixedColorMap)
                {
                    Vector3f const *posIt = posCoords;
                    Vector4f *colorIt = colorCoords;
                    for(duint16 i = 0; i < 4; ++i, colorIt++, posIt++)
                    {
                        Rend_ApplyTorchLight(*colorIt, Rend_PointDist2D(*posIt));
                    }
                }
            }

            if(shineRTU)
            {
                // Strength of the shine.
                Vector3f const &minColor = matSnapshot.shineMinColor();
                for(duint16 i = 0; i < 4; ++i)
                {
                    Vector4f &color = shineColorCoords[i];
                    color = Vector3f(colorCoords[i]).max(minColor);
                    color.w = shineRTU->opacity;
                }
            }

            // Apply uniform alpha (overwritting luminance factors).
            Vector4f *colorIt = colorCoords;
            for(duint16 i = 0; i < 4; ++i, colorIt++)
            {
                colorIt->w = p.opacity;
            }
        }

        if(useLights || useShadows)
        {
            // Surfaces lit by dynamic lights may need to be rendered differently
            // than non-lit surfaces. Determine the average light level of this rend
            // poly, if too bright; do not bother with lights.
            float avgLightlevel = 0;
            for(duint16 i = 0; i < 4; ++i)
            {
                avgLightlevel += colorCoords[i].x;
                avgLightlevel += colorCoords[i].y;
                avgLightlevel += colorCoords[i].z;
            }
            avgLightlevel /= 4 * 3;

            if(avgLightlevel > 0.98f)
            {
                useLights = false;
            }
            if(avgLightlevel < 0.02f)
            {
                useShadows = false;
            }
        }

        bool hasDynlights = false;
        if(useLights)
        {
            // Render all lights projected onto this surface.
            preparedynlightshards_params_t parm; de::zap(parm);

            parm.inst         = this;
            parm.subspace     = &subspace;
            parm.vertCount    = 4;
            parm.posCoords    = posCoords;
            parm.topLeft      = p.topLeft;
            parm.bottomRight  = p.bottomRight;
            parm.leftSection  = p.leftSection;
            parm.rightSection = p.rightSection;

            hasDynlights = (0 != prepareAllDynlightShards(p.lightListIdx, parm));
        }

        if(useShadows)
        {
            // Render all shadows projected onto this surface.
            preparealldynshadowshards_params_t parm; de::zap(parm);

            parm.subspace     = &subspace;
            parm.vertCount    = 4;
            parm.posCoords    = posCoords;
            parm.topLeft      = p.topLeft;
            parm.bottomRight  = p.bottomRight;
            parm.leftSection  = p.leftSection;
            parm.rightSection = p.rightSection;

            prepareAllDynshadowShards(p.shadowListIdx, parm);
        }

        if(!p.skyMasked)
        {
            bool const mustSubdivide = (p.leftSection->divisionCount() || p.rightSection->divisionCount());

            DrawListSpec listSpec((modTex || hasDynlights)? LitGeom : UnlitGeom);
            if(primaryRTU)
            {
                listSpec.texunits[TU_PRIMARY] = *primaryRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY].offset += *p.materialOrigin;
                }
                if(p.materialScale)
                {
                    listSpec.texunits[TU_PRIMARY].scale  *= *p.materialScale;
                    listSpec.texunits[TU_PRIMARY].offset *= *p.materialScale;
                }
            }
            if(primaryDetailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *primaryDetailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *p.materialOrigin;
                }
            }
            if(interRTU)
            {
                listSpec.texunits[TU_INTER] = *interRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                }
                if(p.materialScale)
                {
                    listSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                    listSpec.texunits[TU_INTER].offset *= *p.materialScale;
                }
            }
            if(interDetailRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *interDetailRTU;
                if(p.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *p.materialOrigin;
                }
            }

            if(mustSubdivide) // Generate two triangle fans.
            {
                WorldVBuf::Index const leftFanSize  = 3 + p.leftSection->divisionCount();
                WorldVBuf::Index const rightFanSize = 3 + p.rightSection->divisionCount();

                Shard::Geom *shard = new Shard::Geom(listSpec, BM_NORMAL, modTex, modColor, hasDynlights);
                subspace.shards() << shard;

                shard->indices.resize(leftFanSize + rightFanSize);

                vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                Rend_DivPosCoords(shard->indices.data(), posCoords, *p.leftSection, *p.rightSection);
                Rend_DivColorCoords(shard->indices.data(), colorCoords, *p.leftSection, *p.rightSection);
                if(primaryRTU)
                {
                    Rend_DivTexCoords(shard->indices.data(), primaryTexCoords, *p.leftSection, *p.rightSection,
                                      WorldVBuf::PrimaryTex);
                }
                if(interRTU)
                {
                    Rend_DivTexCoords(shard->indices.data(), interTexCoords, *p.leftSection, *p.rightSection,
                                      WorldVBuf::InterTex);
                }
                if(modTex && Rend_IsMTexLights())
                {
                    Rend_DivTexCoords(shard->indices.data(), modTexCoords, *p.leftSection, *p.rightSection,
                                      WorldVBuf::ModTex);
                }

                Shard::Geom::Primitive leftFan;
                leftFan.type             = gl::TriangleFan;
                leftFan.vertCount        = leftFanSize;
                leftFan.indices          = shard->indices.data();
                leftFan.texScale         = listSpec.unit(TU_PRIMARY       ).scale;
                leftFan.texOffset        = listSpec.unit(TU_PRIMARY       ).offset;
                leftFan.detailTexScale   = listSpec.unit(TU_PRIMARY_DETAIL).scale;
                leftFan.detailTexOffset  = listSpec.unit(TU_PRIMARY_DETAIL).offset;
                shard->primitives.append(leftFan);

                Shard::Geom::Primitive rightFan;
                rightFan.type            = gl::TriangleFan;
                rightFan.vertCount       = rightFanSize;
                rightFan.indices         = shard->indices.data() + leftFan.vertCount;
                rightFan.texScale        = listSpec.unit(TU_PRIMARY       ).scale;
                rightFan.texOffset       = listSpec.unit(TU_PRIMARY       ).offset;
                rightFan.detailTexScale  = listSpec.unit(TU_PRIMARY_DETAIL).scale;
                rightFan.detailTexOffset = listSpec.unit(TU_PRIMARY_DETAIL).offset;
                shard->primitives.append(rightFan);

                if(shineRTU)
                {
                    DrawListSpec shineListSpec(ShineGeom);
                    shineListSpec.texunits[TU_PRIMARY] = *shineRTU;
                    if(shineMaskRTU)
                    {
                        shineListSpec.texunits[TU_INTER] = *shineMaskRTU;
                        if(p.materialOrigin)
                        {
                            shineListSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                        }
                        if(p.materialScale)
                        {
                            shineListSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                            shineListSpec.texunits[TU_INTER].offset *= *p.materialScale;
                        }
                    }

                    Shard::Geom *shineShard = new Shard::Geom(shineListSpec, matSnapshot.shineBlendMode());
                    subspace.shards() << shineShard;

                    shineShard->indices.resize(leftFanSize + rightFanSize);

                    vbuf.reserveElements(leftFanSize + rightFanSize, shineShard->indices);
                    Rend_DivPosCoords(shineShard->indices.data(), posCoords, *p.leftSection, *p.rightSection);
                    Rend_DivColorCoords(shineShard->indices.data(), shineColorCoords, *p.leftSection, *p.rightSection);
                    Rend_DivTexCoords(shineShard->indices.data(), shineTexCoords, *p.leftSection, *p.rightSection,
                                      WorldVBuf::PrimaryTex);
                    if(shineMaskRTU)
                    {
                        Rend_DivTexCoords(shineShard->indices.data(), primaryTexCoords, *p.leftSection, *p.rightSection,
                                          WorldVBuf::InterTex);
                    }

                    Shard::Geom::Primitive leftFan;
                    leftFan.type             = gl::TriangleFan;
                    leftFan.vertCount        = leftFanSize;
                    leftFan.indices          = shineShard->indices.data();
                    leftFan.texScale         = shineListSpec.unit(TU_INTER).scale;
                    leftFan.texOffset        = shineListSpec.unit(TU_INTER).offset;
                    leftFan.detailTexScale   = Vector2f(1, 1);
                    leftFan.detailTexOffset  = Vector2f(0, 0);
                    shineShard->primitives.append(leftFan);

                    Shard::Geom::Primitive rightFan;
                    rightFan.type            = gl::TriangleFan;
                    rightFan.vertCount       = rightFanSize;
                    rightFan.indices         = shineShard->indices.data() + leftFan.vertCount;
                    rightFan.texScale        = shineListSpec.unit(TU_INTER).scale;
                    rightFan.texOffset       = shineListSpec.unit(TU_INTER).offset;
                    rightFan.detailTexScale  = Vector2f(1, 1);
                    rightFan.detailTexOffset = Vector2f(0, 0);
                    shineShard->primitives.append(rightFan);
                }
            }
            else // Generate one triangle strip.
            {
                Shard::Geom *shard = new Shard::Geom(listSpec, BM_NORMAL, modTex, modColor, hasDynlights);
                subspace.shards() << shard;

                shard->indices.resize(4);

                vbuf.reserveElements(4, shard->indices);
                for(WorldVBuf::Index i = 0; i < 4; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                    vertex.pos  =   posCoords[i];
                    vertex.rgba = colorCoords[i];
                    if(primaryRTU)
                    {
                        vertex.texCoord[WorldVBuf::PrimaryTex] = primaryTexCoords[i];
                    }
                    if(interRTU)
                    {
                        vertex.texCoord[WorldVBuf::InterTex] = interTexCoords[i];
                    }
                    if(modTex && Rend_IsMTexLights())
                    {
                        vertex.texCoord[WorldVBuf::ModTex] = modTexCoords[i];
                    }
                }

                Shard::Geom::Primitive prim;
                prim.type            = gl::TriangleStrip;
                prim.vertCount       = 4;
                prim.indices         = shard->indices.data();
                prim.texScale        = listSpec.unit(TU_PRIMARY       ).scale;
                prim.texOffset       = listSpec.unit(TU_PRIMARY       ).offset;
                prim.detailTexScale  = listSpec.unit(TU_PRIMARY_DETAIL).scale;
                prim.detailTexOffset = listSpec.unit(TU_PRIMARY_DETAIL).offset;
                shard->primitives.append(prim);

                if(shineRTU)
                {
                    DrawListSpec shineListSpec(ShineGeom);
                    shineListSpec.texunits[TU_PRIMARY] = *shineRTU;
                    if(shineMaskRTU)
                    {
                        shineListSpec.texunits[TU_INTER] = *shineMaskRTU;
                        if(p.materialOrigin)
                        {
                            shineListSpec.texunits[TU_INTER].offset += *p.materialOrigin;
                        }
                        if(p.materialScale)
                        {
                            shineListSpec.texunits[TU_INTER].scale  *= *p.materialScale;
                            shineListSpec.texunits[TU_INTER].offset *= *p.materialScale;
                        }
                    }

                    Shard::Geom *shineShard = new Shard::Geom(shineListSpec, matSnapshot.shineBlendMode());
                    subspace.shards() << shineShard;

                    shineShard->indices.resize(4);

                    vbuf.reserveElements(4, shineShard->indices);
                    for(WorldVBuf::Index i = 0; i < 4; ++i)
                    {
                        WorldVBuf::Type &vertex = vbuf[shineShard->indices[i]];
                        vertex.pos  =   posCoords[i];
                        vertex.rgba = shineColorCoords[i];
                        vertex.texCoord[WorldVBuf::PrimaryTex] = shineTexCoords[i];
                        if(shineMaskRTU)
                        {
                            vertex.texCoord[WorldVBuf::InterTex] = primaryTexCoords[i];
                        }
                    }

                    Shard::Geom::Primitive prim;
                    prim.type            = gl::TriangleStrip;
                    prim.vertCount       = 4;
                    prim.indices         = shard->indices.data();
                    prim.texScale        = shineListSpec.unit(TU_INTER         ).scale;
                    prim.texOffset       = shineListSpec.unit(TU_INTER         ).offset;
                    prim.detailTexScale  = shineListSpec.unit(TU_PRIMARY_DETAIL).scale;
                    prim.detailTexOffset = shineListSpec.unit(TU_PRIMARY_DETAIL).offset;
                    shineShard->primitives.append(prim);
                }
            }
        }
        else // Sky-masked
        {
            bool const mustSubdivide = (p.leftSection->divisionCount() || p.rightSection->divisionCount());

            Shard::Geom *shard = new Shard::Geom(DrawListSpec(SkyMaskGeom));
            subspace.shards() << shard;

            if(mustSubdivide) // Generate two triangle fans.
            {
                WorldVBuf::Index const leftFanSize  = 3 + p.leftSection->divisionCount();
                WorldVBuf::Index const rightFanSize = 3 + p.rightSection->divisionCount();

                shard->indices.resize(leftFanSize + rightFanSize);

                vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                Rend_DivPosCoords(shard->indices.data(), posCoords, *p.leftSection, *p.rightSection);

                Shard::Geom::Primitive leftFan;
                leftFan.type             = gl::TriangleFan;
                leftFan.vertCount        = leftFanSize;
                leftFan.indices          = shard->indices.data();
                leftFan.texScale         = Vector2f(1, 1);
                leftFan.texOffset        = Vector2f(0, 0);
                leftFan.detailTexScale   = Vector2f(1, 1);
                leftFan.detailTexOffset  = Vector2f(0, 0);
                shard->primitives.append(leftFan);

                Shard::Geom::Primitive rightFan;
                rightFan.type            = gl::TriangleFan;
                rightFan.vertCount       = rightFanSize;
                rightFan.indices         = shard->indices.data() + leftFan.vertCount;
                rightFan.texScale        = Vector2f(1, 1);
                rightFan.texOffset       = Vector2f(0, 0);
                rightFan.detailTexScale  = Vector2f(1, 1);
                rightFan.detailTexOffset = Vector2f(0, 0);
                shard->primitives.append(rightFan);
            }
            else // Generate one triangle strip.
            {
                shard->indices.resize(4);

                vbuf.reserveElements(4, shard->indices);
                for(WorldVBuf::Index i = 0; i < 4; ++i)
                {
                    vbuf[shard->indices[i]].pos = posCoords[i];
                }

                Shard::Geom::Primitive prim;
                prim.type            = gl::TriangleStrip;
                prim.vertCount       = 4;
                prim.indices         = shard->indices.data();
                prim.texScale        = Vector2f(1, 1);
                prim.texOffset       = Vector2f(0, 0);
                prim.detailTexScale  = Vector2f(1, 1);
                prim.detailTexOffset = Vector2f(0, 0);
                shard->primitives.append(prim);
            }
        }
    }

    void prepareWallSectionShards(ConvexSubspace &subspace,
        WallEdgeSection &leftSection, WallEdgeSection &rightSection,
        bool *retWroteOpaque = 0, coord_t *retBottomZ = 0, coord_t *retTopZ = 0)
    {
        DENG2_ASSERT(leftSection.edge().hedge().mapElementAs<LineSideSegment>().isFrontFacing());

        LineSide &side            = leftSection.edge().lineSide();
        Surface &surface          = *leftSection.surfacePtr();
        bool const twoSidedMiddle = leftSection.id() == WallEdge::WallMiddle && !side.considerOneSided();
        int const section         = leftSection.id() == WallEdge::WallMiddle? LineSide::Middle :
                                    leftSection.id() == WallEdge::WallBottom? LineSide::Bottom :
                                                                              LineSide::Top;

        if(retWroteOpaque) *retWroteOpaque = false;
        if(retBottomZ)     *retBottomZ     = 0;
        if(retTopZ)        *retTopZ        = 0;

        // Skip nearly transparent surfaces.
        float opacity = surface.opacity();
        if(opacity < .001f)
            return;

        // Do the edge geometries describe a valid polygon?
        if(!leftSection.isValid() || !rightSection.isValid() ||
           de::fequal(leftSection.bottom().z(), rightSection.top().z()))
            return;

        // Determine which Material to use (a drawable material is required).
        Material *material = Rend_ChooseMapSurfaceMaterial(surface);
        if(!material || !material->isDrawable())
            return;

        // Should we apply a fade out when the viewer is near to this geometry?
        bool const useNearFade = Rend_NearFadeOpacity(leftSection, rightSection, opacity);


        MaterialSnapshot const &matSnapshot = material->prepare(Rend_MapSurfaceMaterialSpec());

        rendworldpoly_params_t parm; de::zap(parm);

        Vector3d topLeft          = leftSection.top().origin();
        Vector3d bottomRight      = rightSection.bottom().origin();
        Vector3f materialOrigin   = leftSection.materialOrigin();
        Vector2f materialScale    = Vector2f((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                                             (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        parm.skyMasked            = (material->isSkyMasked() && !devRendSkyMode);
        parm.mapElement           = &leftSection.edge().hedge().mapElementAs<LineSideSegment>();
        parm.geomGroup            = section;
        parm.topLeft              = &topLeft;
        parm.bottomRight          = &bottomRight;
        parm.forceOpaque          = leftSection.flags().testFlag(WallEdgeSection::ForceOpaque);
        parm.opacity                = parm.forceOpaque? 1 : opacity;
        parm.surfaceTangentMatrix = &surface.tangentMatrix();

        parm.blendmode            = BM_NORMAL;
        parm.materialOrigin       = &materialOrigin;
        parm.materialScale        = &materialScale;

        parm.leftSection          = &leftSection;
        parm.rightSection         = &rightSection;
        parm.sectionWidth         = de::abs(Vector2d(rightSection.edge().origin() - leftSection.edge().origin()).length());

        // Calculate the light level deltas for this wall section?
        if(!leftSection.flags().testFlag(WallEdgeSection::NoLightDeltas))
        {
            wallSectionLightLevelDeltas(leftSection, rightSection,
                                        parm.surfaceLightLevelDL, parm.surfaceLightLevelDR);
        }

        if(!parm.skyMasked)
        {
            if(glowFactor > .0001f)
            {
                if(material == surface.materialPtr())
                {
                    parm.glowing = matSnapshot.glowStrength();
                }
                else
                {
                    Material *actualMaterial =
                        surface.hasMaterial()? surface.materialPtr()
                                             : &ClientApp::resourceSystem().material(de::Uri("System", Path("missing")));

                    MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                    parm.glowing = ms.glowStrength();
                }

                parm.glowing *= glowFactor; // Global scale factor.
            }

            projectDynamics(subspace, surface, parm.glowing, *parm.topLeft, *parm.bottomRight,
                            leftSection.flags().testFlag(WallEdgeSection::NoDynLights),
                            leftSection.flags().testFlag(WallEdgeSection::NoDynShadows),
                            leftSection.flags().testFlag(WallEdgeSection::SortDynLights),
                            parm.lightListIdx, parm.shadowListIdx);

            if(twoSidedMiddle)
            {
                parm.blendmode = surface.blendMode();
                if(parm.blendmode == BM_NORMAL && noSpriteTrans)
                    parm.blendmode = BM_ZEROALPHA; // "no translucency" mode
            }

            side.chooseSurfaceTintColors(section, &parm.surfaceColor, &parm.surfaceColor2);
        }

        /*
         * Geometry write/drawing begins.
         */

        if(twoSidedMiddle && side.sectorPtr() != &self.sector())
        {
            // Temporarily modify the draw state.
            curSectorLightColor = Rend_AmbientLightColor(side.sector());
            curSectorLightLevel = side.sector().lightLevel();
        }

        // Draw this section.
        bool wroteOpaque = true;
        if(!Rend_MustDrawAsVissprite(parm, matSnapshot))
        {
            prepareWallSectionShard(subspace, parm, matSnapshot);

            // Render FakeRadio for this section?
            if(!leftSection.flags().testFlag(WallEdgeSection::NoFakeRadio) &&
               !parm.skyMasked && !(parm.glowing > 0))
            {
                Rend_RadioWallSection(leftSection, rightSection, curSectorLightLevel);
            }
        }
        else
        {
            Rend_PrepareWallSectionVissprite(parm, matSnapshot, subspace, curSectorLightLevel, curSectorLightColor);
            wroteOpaque = false; /// We @em had to use a vissprite; clearly not opaque.
        }

        if(twoSidedMiddle && side.sectorPtr() != &self.sector())
        {
            // Undo temporary draw state changes.
            Vector4f const color = self.lightSourceColorfIntensity();
            curSectorLightColor = color.toVector3f();
            curSectorLightLevel = color.w;
        }

        if(retWroteOpaque) *retWroteOpaque = wroteOpaque && !useNearFade;
        if(retBottomZ)     *retBottomZ     = leftSection.bottom().z();
        if(retTopZ)        *retTopZ        = rightSection.top().z();
    }

    void prepareWallShards(ConvexSubspace &subspace, HEdge *hedge)
    {
        // Edges without a map line segment implicitly have no surfaces.
        if(!hedge || !hedge->hasMapElement())
            return;

        // We are only interested in front facing segments with sections.
        LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
        if(!seg.isFrontFacing() || !seg.lineSide().hasSections())
            return;

        Rend_ReportWallSectionDrawn(seg.line()); // Here because of doom.exe automap logic.

        // Generate and write the wall section geometries to the draw lists.
        bool wroteOpaqueMiddle = false;
        coord_t middleBottomZ  = 0;
        coord_t middleTopZ     = 0;

        WallEdge leftEdge(*hedge, Line::From);// = wallEdge(*hedge, Line::From);
        WallEdge rightEdge(*hedge, Line::To); // = wallEdge(*hedge, Line::To);

        prepareWallSectionShards(subspace, leftEdge.wallBottom(), rightEdge.wallBottom());
        prepareWallSectionShards(subspace, leftEdge.wallTop(),    rightEdge.wallTop());
        prepareWallSectionShards(subspace, leftEdge.wallMiddle(), rightEdge.wallMiddle(),
                                 &wroteOpaqueMiddle, &middleBottomZ, &middleTopZ);

        // We can occlude the angle range defined by the X|Y origins of the
        // line segment if the open range has been covered (when the viewer
        // is not in the void).
        if(!P_IsInVoid(viewPlayer) &&
           Rend_CoveredOpenRange(*hedge, middleBottomZ, middleTopZ, wroteOpaqueMiddle))
        {
            C_AddRangeFromViewRelPoints(hedge->origin(), hedge->twin().origin());
        }
    }

    void prepareAllWallShards(ConvexSubspace &subspace)
    {
        HEdge *base = subspace.poly().hedge();
        HEdge *hedge = base;
        do
        {
            prepareWallShards(subspace, hedge);
        } while((hedge = &hedge->next()) != base);

        foreach(Mesh *mesh, subspace.extraMeshes())
        foreach(HEdge *hedge, mesh->hedges())
        {
            prepareWallShards(subspace, hedge);
        }

        foreach(Polyobj *po, subspace.polyobjs())
        foreach(HEdge *hedge, po->mesh().hedges())
        {
            prepareWallShards(subspace, hedge);
        }
    }

    void prepareFlatShard(ConvexSubspace &subspace, Plane &plane)
    {
        RenderSystem &rendSys  = ClientApp::renderSystem();
        WorldVBuf &vbuf        = rendSys.worldVBuf();
        Face const &poly       = subspace.poly();
        Surface const &surface = plane.surface();

        // Skip nearly transparent surfaces.
        float const opacity = surface.opacity();
        if(opacity < .001f) return;

        // Determine which Material to use (a drawable material is required).
        Material *material = Rend_ChooseMapSurfaceMaterial(surface);
        if(!material || !material->isDrawable())
            return;

        // Skip planes with a sky-masked material?
        if(!devRendSkyMode)
        {
            if(surface.hasSkyMaskedMaterial() && plane.indexInSector() <= Sector::Ceiling)
            {
                return; // Not handled here (drawn with the mask geometry).
            }
        }

        MaterialSnapshot const &matSnapshot = material->prepare(Rend_MapSurfaceMaterialSpec());

        Vector2f materialOrigin = subspace.worldGridOffset() // Align to the worldwide grid.
                                + surface.materialOriginSmoothed();

        // Add the Y offset to orient the Y flipped material.
        /// @todo fixme: What is this meant to do? -ds
        if(plane.isSectorCeiling())
        {
            materialOrigin.y -= poly.aaBox().maxY - poly.aaBox().minY;
        }
        materialOrigin.y = -materialOrigin.y;

        Vector2f const materialScale((surface.flags() & DDSUF_MATERIAL_FLIPH)? -1 : 1,
                                     (surface.flags() & DDSUF_MATERIAL_FLIPV)? -1 : 1);

        // Set the texture origin, Y is flipped for the ceiling.
        Vector3d topLeft(poly.aaBox().minX,
                         poly.aaBox().arvec2[plane.isSectorFloor()? 1 : 0][VY],
                         plane.heightSmoothed());
        Vector3d bottomRight(poly.aaBox().maxX,
                             poly.aaBox().arvec2[plane.isSectorFloor()? 0 : 1][VY],
                             plane.heightSmoothed());

        rendworldpoly_params_t parm; zap(parm);

        parm.mapElement           = &subspace;
        parm.geomGroup            = plane.indexInSector();
        parm.topLeft              = &topLeft;
        parm.bottomRight          = &bottomRight;
        parm.materialOrigin       = &materialOrigin;
        parm.materialScale        = &materialScale;
        parm.surfaceLightLevelDL  = parm.surfaceLightLevelDR = 0;
        parm.surfaceColor         = &surface.tintColor();
        parm.surfaceTangentMatrix = &surface.tangentMatrix();

        if(material->isSkyMasked())
        {
            // In devRendSkyMode mode we render all polys destined for the
            // skymask as regular world polys (with a few obvious properties).
            if(devRendSkyMode)
            {
                parm.blendmode   = BM_NORMAL;
                parm.forceOpaque = true;
            }
            else
            {
                // We'll mask this.
                parm.skyMasked = true;
            }
        }
        else if(plane.indexInSector() <= Sector::Ceiling)
        {
            parm.blendmode   = BM_NORMAL;
            parm.forceOpaque = true;
        }
        else
        {
            parm.blendmode = surface.blendMode();
            if(parm.blendmode == BM_NORMAL && noSpriteTrans)
            {
                parm.blendmode = BM_ZEROALPHA; // "no translucency" mode
            }

            parm.opacity = surface.opacity();
        }

        if(!parm.skyMasked)
        {
            if(glowFactor > .0001f)
            {
                if(material == surface.materialPtr())
                {
                    parm.glowing = matSnapshot.glowStrength();
                }
                else
                {
                    Material *actualMaterial =
                        surface.hasMaterial()? surface.materialPtr()
                                             : &ClientApp::resourceSystem().material(de::Uri("System", Path("missing")));

                    MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                    parm.glowing = ms.glowStrength();
                }

                parm.glowing *= glowFactor; // Global scale factor.
            }

            projectDynamics(subspace, surface, parm.glowing, *parm.topLeft, *parm.bottomRight,
                            false /*do light*/, false /*do shadow*/, false /*don't sort*/,
                            parm.lightListIdx, parm.shadowListIdx);
        }

        /*
         * Geometry write/drawing begins.
         */

        if(&plane.sector() != &subspace.sector())
        {
            // Temporarily modify the draw state.
            curSectorLightColor = Rend_AmbientLightColor(plane.sector());
            curSectorLightLevel = plane.sector().lightLevel();
        }

        ClockDirection const direction = (plane.isSectorCeiling())? Anticlockwise : Clockwise;
        coord_t const height           = plane.heightSmoothed();

        HEdge *fanBase = subspace.fanBase();
        WorldVBuf::Index const vertCount = poly.hedgeCount() + (!fanBase? 2 : 0);

        WorldVBuf::Indices indices(vertCount);

        vbuf.reserveElements(vertCount, indices);

        WorldVBuf::Index n = 0;
        if(!fanBase)
        {
            vbuf[indices[n]].pos = Vector3f(poly.center(), height);
            n++;
        }

        // Add the vertices for each hedge.
        HEdge *base  = fanBase? fanBase : poly.hedge();
        HEdge *hedge = base;
        do
        {
            vbuf[indices[n]].pos = Vector3f(hedge->origin(), height);
            n++;
        } while((hedge = &hedge->neighbor(direction)) != base);

        // The last vertex is always equal to the first.
        if(!fanBase)
        {
            vbuf[indices[n]].pos = Vector3f(poly.hedge()->origin(), height);
        }

        bool const skyMaskedMaterial = (parm.skyMasked || (matSnapshot.material().isSkyMasked()));

        bool useLights = false, useShadows = false, hasDynlights = false;

        // Map RTU configuration from prepared MaterialSnapshot(s).
        GLTextureUnit const *primaryRTU       = (!parm.skyMasked)? &matSnapshot.unit(RTU_PRIMARY) : NULL;
        GLTextureUnit const *primaryDetailRTU = (r_detail && !parm.skyMasked && matSnapshot.unit(RTU_PRIMARY_DETAIL).hasTexture())? &matSnapshot.unit(RTU_PRIMARY_DETAIL) : NULL;
        GLTextureUnit const *interRTU         = (!parm.skyMasked && matSnapshot.unit(RTU_INTER).hasTexture())? &matSnapshot.unit(RTU_INTER) : NULL;
        GLTextureUnit const *interDetailRTU   = (r_detail && !parm.skyMasked && matSnapshot.unit(RTU_INTER_DETAIL).hasTexture())? &matSnapshot.unit(RTU_INTER_DETAIL) : NULL;
        GLTextureUnit const *shineRTU         = (useShinySurfaces && !parm.skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture())? &matSnapshot.unit(RTU_REFLECTION) : NULL;
        GLTextureUnit const *shineMaskRTU     = (useShinySurfaces && !parm.skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture() && matSnapshot.unit(RTU_REFLECTION_MASK).hasTexture())? &matSnapshot.unit(RTU_REFLECTION_MASK) : NULL;

        WorldVBuf::Indices shineIndices;

        DGLuint modTex = 0;
        Vector2f modTexSt[2]; // [topLeft, bottomRight]
        Vector3f modColor;

        if(!skyMaskedMaterial)
        {
            // ShinySurface?
            if(shineRTU)
            {
                shineIndices.resize(vertCount);
                vbuf.reserveElements(vertCount, shineIndices);
                for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                {
                    vbuf[shineIndices[i]].pos = vbuf[indices[i]].pos;
                }
            }

            if(parm.glowing < 1)
            {
                useLights  = (parm.lightListIdx ? true : false);
                useShadows = (parm.shadowListIdx? true : false);

                // If multitexturing is enabled and there is at least one dynlight
                // affecting this surface, grab the parameters needed to draw it.
                if(useLights && Rend_IsMTexLights())
                {
                    TexProjection *dyn = 0;
                    Rend_IterateProjectionList(parm.lightListIdx, RIT_FirstDynlightIterator, (void *)&dyn);

                    modTex      = dyn->texture;
                    modColor    = dyn->color;
                    modTexSt[0] = dyn->topLeft;
                    modTexSt[1] = dyn->bottomRight;
                }
            }
        }

        for(WorldVBuf::Index i = 0; i < vertCount; ++i)
        {
            WorldVBuf::Type &vertex = vbuf[indices[i]];
            Vector3f const delta(vertex.pos - *parm.topLeft);

            // Primary texture coordinates.
            if(primaryRTU)
            {
                vertex.texCoord[WorldVBuf::PrimaryTex] = Vector2f(delta.x, -delta.y);
            }

            // Blend primary texture coordinates.
            if(interRTU)
            {
                vertex.texCoord[WorldVBuf::InterTex] = Vector2f(delta.x, -delta.y);
            }

            if(shineRTU)
            {
                WorldVBuf::Type &shineVertex = vbuf[shineIndices[i]];

                // Determine distance to viewer. If too small it will result in an
                // ugly 'crunch' below and above the viewpoint (so clamp it).
                float distToEye = (vOrigin.xz() - vertex.pos.xy()).normalize().length();
                if(distToEye < 10) distToEye = 10;

                // Offset from the normal view plane.
                Vector2f start(vOrigin.x, vOrigin.z);
                float offset = ((start.y - vertex.pos.y) * sin(.4f)/*viewFrontVec[VX]*/ -
                                (start.x - vertex.pos.x) * cos(.4f)/*viewFrontVec[VZ]*/);

                shineVertex.texCoord[WorldVBuf::PrimaryTex] =
                        Vector2f(.5f + (shinyVertical(offset, distToEye) - .5f) * 2
                                 ,
                                 shinyVertical(vOrigin.y - vertex.pos.z, distToEye));

                if(shineMaskRTU)
                {
                    shineVertex.texCoord[WorldVBuf::InterTex] = Vector2f(delta.x, -delta.y);
                }
            }

            // First light texture coordinates.
            if(modTex && Rend_IsMTexLights())
            {
                float const width  = parm.bottomRight->x - parm.topLeft->x;
                float const height = parm.bottomRight->y - parm.topLeft->y;

                vertex.texCoord[WorldVBuf::ModTex] =
                    Vector2f(((parm.bottomRight->x - vertex.pos.x) / width  * modTexSt[0].x) + (delta.x / width  * modTexSt[1].x)
                             ,
                             ((parm.bottomRight->y - vertex.pos.y) / height * modTexSt[0].y) + (delta.y / height * modTexSt[1].y));
            }
        }

        // Light this polygon.
        if(!skyMaskedMaterial)
        {
            if(levelFullBright || !(parm.glowing < 1))
            {
                // Uniform color. Apply to all vertices.
                float ll = de::clamp(0.f, curSectorLightLevel + (levelFullBright? 1 : parm.glowing), 1.f);
                for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[indices[i]];
                    vertex.rgba.x = vertex.rgba.y = vertex.rgba.z = ll;
                }
            }
            else
            {
                // Non-uniform color.
                if(useBias)
                {
                    Map &map     = self.sector().map();
                    Shard &shard = self.shard(*parm.mapElement, parm.geomGroup);

                    // Apply the ambient light term from the grid (if available).
                    if(map.hasLightGrid())
                    {
                        for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                        {
                            WorldVBuf::Type &vertex = vbuf[indices[i]];
                            vertex.rgba = map.lightGrid().evaluate(vertex.pos);
                        }
                    }

                    // Apply bias light source contributions.
                    shard.lightWithBiasSources(indices.data(), *parm.surfaceTangentMatrix,
                                               map.biasCurrentTime());

                    // Apply surface glow.
                    if(parm.glowing > 0)
                    {
                        Vector4f const glow(parm.glowing, parm.glowing, parm.glowing, 0);
                        for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                        {
                            vbuf[indices[i]].rgba += glow;
                        }
                    }

                    // Apply light range compression and clamparm.
                    for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                    {
                        WorldVBuf::Type &vertex = vbuf[indices[i]];
                        for(int k = 0; k < 3; ++k)
                        {
                            vertex.rgba[k] = de::clamp(0.f, vertex.rgba[k] + Rend_LightAdaptationDelta(vertex.rgba[k]), 1.f);
                        }
                    }
                }
                else
                {
                    float llL = de::clamp(0.f, curSectorLightLevel + parm.surfaceLightLevelDL + parm.glowing, 1.f);

                    // Calculate the color for each vertex, blended with plane color?
                    if(parm.surfaceColor->x < 1 || parm.surfaceColor->y < 1 || parm.surfaceColor->z < 1)
                    {
                        // Blend sector light+color+surfacecolor
                        Vector3f vColor = (*parm.surfaceColor) * curSectorLightColor;
                        Rend_LightVertices(vertCount, indices.data(), llL, vColor);
                    }
                    else
                    {
                        // Use sector light+color only.
                        Rend_LightVertices(vertCount, indices.data(), llL, curSectorLightColor);
                    }
                }

                // Apply torch light?
                if(viewPlayer->shared.fixedColorMap)
                {
                    for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                    {
                        WorldVBuf::Type &vertex = vbuf[indices[i]];
                        Rend_ApplyTorchLight(vertex.rgba, Rend_PointDist2D(vertex.pos));
                    }
                }
            }

            if(shineRTU)
            {
                // Strength of the shine.
                Vector3f const &minColor = matSnapshot.shineMinColor();
                for(WorldVBuf::Index i = 0; i < vertCount; ++i)
                {
                    Vector4f &color = vbuf[shineIndices[i]].rgba;
                    color = Vector3f(vbuf[indices[i]].rgba).max(minColor);
                    color.w = shineRTU->opacity;
                }
            }

            // Apply uniform alpha (overwritting luminance factors).
            for(WorldVBuf::Index i = 0; i < vertCount; ++i)
            {
                vbuf[indices[i]].rgba.w = parm.opacity;
            }
        }
        else
        {
            // Uniform color. Apply to all vertices.
            for(WorldVBuf::Index i = 0; i < vertCount; ++i)
            {
                vbuf[indices[i]].rgba = Vector4f(1, 1, 1, 1);
            }
        }

        if(useLights || useShadows)
        {
            // Surfaces lit by dynamic lights may need to be rendered differently
            // than non-lit surfaces. Determine the average light level of this rend
            // poly, if too bright; do not bother with lights.
            float avgLightlevel = 0;
            for(WorldVBuf::Index i = 0; i < vertCount; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[indices[i]];
                avgLightlevel += vertex.rgba.x;
                avgLightlevel += vertex.rgba.y;
                avgLightlevel += vertex.rgba.z;
            }
            avgLightlevel /= vertCount * 3;

            if(avgLightlevel > 0.98f)
            {
                useLights = false;
            }
            if(avgLightlevel < 0.02f)
            {
                useShadows = false;
            }
        }

        if(useLights)
        {
            // Render all lights projected onto this surface.
            preparedynlightshards_params_t plparm; de::zap(plparm);

            plparm.subspace    = &subspace;
            plparm.vertCount   = vertCount;
            plparm.indices     = indices.data();
            plparm.topLeft     = parm.topLeft;
            plparm.bottomRight = parm.bottomRight;

            hasDynlights = (0 != prepareAllDynlightShards(parm.lightListIdx, plparm));
        }

        if(useShadows)
        {
            // Render all shadows projected onto this surface.
            preparealldynshadowshards_params_t psparm; de::zap(psparm);

            psparm.subspace    = &subspace;
            psparm.vertCount   = vertCount;
            psparm.indices     = indices.data();
            psparm.topLeft     = parm.topLeft;
            psparm.bottomRight = parm.bottomRight;

            prepareAllDynshadowShards(parm.shadowListIdx, psparm);
        }

        if(!parm.skyMasked)
        {
            DrawListSpec listSpec((modTex || hasDynlights)? LitGeom : UnlitGeom);
            if(primaryRTU)
            {
                listSpec.texunits[TU_PRIMARY] = *primaryRTU;
                if(parm.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY].offset += *parm.materialOrigin;
                }
                if(parm.materialScale)
                {
                    listSpec.texunits[TU_PRIMARY].scale  *= *parm.materialScale;
                    listSpec.texunits[TU_PRIMARY].offset *= *parm.materialScale;
                }
            }
            if(primaryDetailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *primaryDetailRTU;
                if(parm.materialOrigin)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += *parm.materialOrigin;
                }
            }
            if(interRTU)
            {
                listSpec.texunits[TU_INTER] = *interRTU;
                if(parm.materialOrigin)
                {
                    listSpec.texunits[TU_INTER].offset += *parm.materialOrigin;
                }
                if(parm.materialScale)
                {
                    listSpec.texunits[TU_INTER].scale  *= *parm.materialScale;
                    listSpec.texunits[TU_INTER].offset *= *parm.materialScale;
                }
            }
            if(interDetailRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *interDetailRTU;
                if(parm.materialOrigin)
                {
                    listSpec.texunits[TU_INTER_DETAIL].offset += *parm.materialOrigin;
                }
            }

            Shard::Geom *shard = new Shard::Geom(listSpec, BM_NORMAL, modTex, modColor, hasDynlights);
            shard->indices = indices;

            Shard::Geom::Primitive prim;
            prim.type            = gl::TriangleFan;
            prim.vertCount       = vertCount;
            prim.indices         = shard->indices.data();
            prim.texScale        = listSpec.unit(TU_PRIMARY       ).scale;
            prim.texOffset       = listSpec.unit(TU_PRIMARY       ).offset;
            prim.detailTexScale  = listSpec.unit(TU_PRIMARY_DETAIL).scale;
            prim.detailTexOffset = listSpec.unit(TU_PRIMARY_DETAIL).offset;
            shard->primitives.append(prim);

            subspace.shards() << shard;

            if(shineRTU)
            {
                DrawListSpec shineListSpec(ShineGeom);
                shineListSpec.texunits[TU_PRIMARY] = *shineRTU;
                if(shineMaskRTU)
                {
                    shineListSpec.texunits[TU_INTER] = *shineMaskRTU;
                    if(parm.materialOrigin)
                    {
                        shineListSpec.texunits[TU_INTER].offset += *parm.materialOrigin;
                    }
                    if(parm.materialScale)
                    {
                        shineListSpec.texunits[TU_INTER].scale  *= *parm.materialScale;
                        shineListSpec.texunits[TU_INTER].offset *= *parm.materialScale;
                    }
                }

                Shard::Geom *shineShard = new Shard::Geom(shineListSpec, matSnapshot.shineBlendMode());
                shard->indices = shineIndices;

                Shard::Geom::Primitive shinePrim;
                shinePrim.type            = gl::TriangleFan;
                shinePrim.vertCount       = vertCount;
                shinePrim.indices         = shard->indices.data();
                shinePrim.texScale        = shineListSpec.unit(TU_INTER         ).scale;
                shinePrim.texOffset       = shineListSpec.unit(TU_INTER         ).offset;
                shinePrim.detailTexScale  = shineListSpec.unit(TU_PRIMARY_DETAIL).scale;
                shinePrim.detailTexOffset = shineListSpec.unit(TU_PRIMARY_DETAIL).offset;
                shineShard->primitives.append(shinePrim);

                subspace.shards() << shineShard;
            }
        }
        else // Sky-masked.
        {
            Shard::Geom *shard = new Shard::Geom(DrawListSpec(SkyMaskGeom));
            shard->indices = indices;

            Shard::Geom::Primitive prim;
            prim.type            = gl::TriangleFan;
            prim.vertCount       = vertCount;
            prim.indices         = shard->indices.data();
            prim.texScale        = Vector2f(1, 1);
            prim.texOffset       = Vector2f(0, 0);
            prim.detailTexScale  = Vector2f(1, 1);
            prim.detailTexOffset = Vector2f(0, 0);
            shard->primitives.append(prim);

            subspace.shards() << shard;
        }

        if(&plane.sector() != &subspace.sector())
        {
            // Undo temporary draw state changes.
            Vector4f const color = subspace.cluster().lightSourceColorfIntensity();
            curSectorLightColor = color.toVector3f();
            curSectorLightLevel = color.w;
        }
    }

    void prepareAllFlatShards(ConvexSubspace &subspace)
    {
        for(int i = 0; i < self.visPlaneCount(); ++i)
        {
            Plane &plane = self.visPlane(i);

            // Skip planes facing away from the viewer.
            Vector3d const pointOnPlane(self.center(), plane.heightSmoothed());
            if((Rend_ViewerOrigin() - pointOnPlane).dot(plane.surface().normal()) < 0)
                continue;

            prepareFlatShard(subspace, plane);
        }
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
    if(!hedge->face().hasMapElement() || hedge->face().mapElement().type() != DMU_SUBSPACE) return false;
    if(!hedge->twin().face().hasMapElement() || hedge->twin().face().mapElement().type() != DMU_SUBSPACE) return false;

    SectorCluster *frontCluster = hedge->face().mapElementAs<ConvexSubspace>().clusterPtr();
    if(!frontCluster) return false;
    return frontCluster == hedge->twin().face().mapElementAs<ConvexSubspace>().clusterPtr();
}

Sector const &SectorCluster::sector() const
{
    return *const_cast<ConvexSubspace const *>(d->subspaces.first())->bspLeaf().sectorPtr();
}

Sector &SectorCluster::sector()
{
    return *d->subspaces.first()->bspLeaf().sectorPtr();
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

WallEdge &SectorCluster::wallEdge(HEdge &hedge, int side)
{
    if(WallEdge *wedge = d->findWallEdge(hedge, side))
    {
        return *wedge;
    }
    /// @throw MissingWallEdgeError  No matching WallEdge exists.
    throw MissingWallEdgeError("SectorCluster::wallEdge", "The referenced WallEdge does not exist");
}

SectorCluster::WallEdges const &SectorCluster::allWallEdges() const
{
    return d->wallEdges;
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
    case DMU_SUBSPACE: {
        ConvexSubspace &subspace = mapElement.as<ConvexSubspace>();
        DENG2_ASSERT(group >= 0 && group < subspace.sector().planeCount()); // sanity check
        return subspace.numFanVertices(); }

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
        case DMU_SUBSPACE: {
            ConvexSubspace &subspace = gdata->mapElement->as<ConvexSubspace>();
            Plane const &plane       = visPlane(gdata->geomId);
            Surface const &surface   = plane.surface();

            Vector3d surfacePoint(subspace.poly().center(), plane.heightSmoothed());

            foreach(BiasSource *source, sources)
            {
                // If the source is too weak we will ignore it completely.
                if(source->intensity() <= 0)
                    continue;

                Vector3d sourceToSurface = (source->origin() - surfacePoint).normalize();
                coord_t distance = 0;

                // Calculate minimum 2D distance to the subspace.
                /// @todo This is probably too accurate an estimate.
                HEdge *baseNode = subspace.poly().hedge();
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

void SectorCluster::prepareShards(ConvexSubspace &subspace)
{
    DENG2_ASSERT(subspace.clusterPtr() == this);

    Vector4f const color = lightSourceColorfIntensity();
    curSectorLightColor = color.toVector3f();
    curSectorLightLevel = color.w;

    d->prepareAllSkyMaskShards(subspace);
    d->prepareAllWallShards(subspace);
    d->prepareAllFlatShards(subspace);
}
#endif // __CLIENT__

// SectorClusterCirculator -----------------------------------------------------

SectorCluster *SectorClusterCirculator::getCluster(HEdge const &hedge) // static
{
    if(!hedge.hasFace()) return 0;
    if(!hedge.face().hasMapElement()) return 0;
    if(hedge.face().mapElement().type() != DMU_SUBSPACE) return 0;
    return hedge.face().mapElementAs<ConvexSubspace>().clusterPtr();
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
