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
    static Lumobj::LightmapSemantic lightmapForSurface(Surface const &surface)
    {
        if(surface.parent().type() == DMU_SIDE) return Lumobj::Side;
        // Must be a plane then.
        Plane const &plane = surface.parent().as<Plane>();
        return plane.isSectorFloor()? Lumobj::Down : Lumobj::Up;
    }

    static Vector2f calcFlatMaterialOrigin(ConvexSubspace const &subspace, Plane const &plane)
    {
        Vector2f origin = subspace.worldGridOffset() // Align to the worldwide grid.
                          + plane.surface().materialOriginSmoothed();

        // Add the Y offset to orient the Y flipped material.
        /// @todo fixme: What is this meant to do? -ds
        if(plane.isSectorCeiling())
        {
            origin.y -= subspace.poly().aaBox().maxY - subspace.poly().aaBox().minY;
        }
        origin.y = -origin.y;

        return origin;
    }

#if 0
    static blendmode_t chooseFlatBlendmode(Plane const &plane)
    {
        if(plane.indexInSector() <= Sector::Ceiling)
        {
            return BM_NORMAL;
        }

        blendmode_t blendmode = plane.surface().blendMode();
        if(blendmode == BM_NORMAL && noSpriteTrans)
        {
            blendmode = BM_ZEROALPHA; // "no translucency" mode
        }
        return blendmode;
    }
#endif

    static blendmode_t chooseWallBlendmode(WallEdgeSection const &wallSection)
    {
        LineSide &side = wallSection.edge().lineSide();

        if(wallSection.id() != WallEdge::WallMiddle || side.considerOneSided())
        {
            return BM_NORMAL;
        }

        blendmode_t blendmode = side.surface(LineSide::Middle).blendMode();
        if(blendmode == BM_NORMAL && noSpriteTrans)
        {
            blendmode = BM_ZEROALPHA; // "no translucency" mode
        }
        return blendmode;
    }

    static void chooseWallTintColors(WallEdgeSection const &wallSection,
        Vector3f const **bottomColor, Vector3f const **topColor)
    {
        DENG2_ASSERT(bottomColor != 0 && topColor != 0);

        LineSide &side = wallSection.edge().lineSide();

        switch(wallSection.id())
        {
        case WallEdge::WallMiddle:
            if(side.isFlagged(SDF_BLENDMIDTOTOP))
            {
                *bottomColor = &side.middle().tintColor();
                *topColor    = &side.top().tintColor();
            }
            else if(side.isFlagged(SDF_BLENDMIDTOBOTTOM))
            {
                *bottomColor = &side.bottom().tintColor();
                *topColor    = &side.middle().tintColor();
            }
            else
            {
                *bottomColor = 0;
                *topColor    = &side.middle().tintColor();
            }
            break;

        case WallEdge::WallBottom:
            if(side.isFlagged(SDF_BLENDBOTTOMTOMID))
            {
                *bottomColor = &side.bottom().tintColor();
                *topColor    = &side.middle().tintColor();
            }
            else
            {
                *bottomColor = 0;
                *topColor    = &side.bottom().tintColor();
            }
            break;

        case WallEdge::WallTop:
            if(side.isFlagged(SDF_BLENDTOPTOMID))
            {
                *bottomColor = &side.middle().tintColor();
                *topColor    = &side.top().tintColor();
            }
            else
            {
                *bottomColor = 0;
                *topColor    = &side.top().tintColor();
            }
            break;

        default:
            *bottomColor = *topColor = 0;
            break;
        }
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

    static void quadTexFakeradioCoords(Vector2f *tc, Vector3f const *rverts, float wallLength,
        Vector3f const &texTopLeft, Vector3f const &texBottomRight,
        Vector2f const &texOrigin, Vector2f const &texDimensions, bool horizontal)
    {
        if(horizontal)
        {
            // Special horizontal coordinates for wall shadows.
            tc[0].x = tc[2].x = rverts[0].x - texTopLeft.x + texOrigin.y / texDimensions.y;
            tc[0].y = tc[1].y = rverts[0].y - texTopLeft.y + texOrigin.x / texDimensions.x;

            tc[1].x = tc[0].x + (rverts[1].z - texBottomRight.z) / texDimensions.y;
            tc[3].x = tc[0].x + (rverts[3].z - texBottomRight.z) / texDimensions.y;
            tc[3].y = tc[0].y + wallLength / texDimensions.x;
            tc[2].y = tc[0].y + wallLength / texDimensions.x;
            return;
        }

        tc[0].x = tc[1].x = rverts[0].x - texTopLeft.x + texOrigin.x / texDimensions.x;
        tc[3].y = tc[1].y = rverts[0].y - texTopLeft.y + texOrigin.y / texDimensions.y;

        tc[3].x = tc[2].x = tc[0].x + wallLength / texDimensions.x;
        tc[2].y = tc[3].y + (rverts[1].z - rverts[0].z) / texDimensions.y;
        tc[0].y = tc[3].y + (rverts[3].z - rverts[2].z) / texDimensions.y;
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

    static void wallLightLevelDeltas(WallEdgeSection const &leftSection,
        WallEdgeSection const &rightSection, float deltas[2])
    {
        float &leftDelta  = deltas[0];
        float &rightDelta = deltas[1];

        if(leftSection.flags().testFlag(WallEdgeSection::NoLightDeltas))
        {
            leftDelta = rightDelta = 0;
            return;
        }

        leftDelta = calcLightLevelDelta(leftSection.normal());

        if(leftSection.normal() == rightSection.normal())
        {
            rightDelta = leftDelta;
        }
        else
        {
            rightDelta = calcLightLevelDelta(rightSection.normal());

            // Linearly interpolate to find the light level delta values for the
            // vertical edges of this wall section.
            coord_t const lineLength    = leftSection.edge().lineSide().line().length();
            coord_t const sectionOffset = leftSection.edge().lineSideOffset();
            coord_t const sectionWidth  = de::abs(Vector2d(rightSection.edge().origin() - leftSection.edge().origin()).length());

            float deltaDiff = rightDelta - leftDelta;
            rightDelta = leftDelta + ((sectionOffset + sectionWidth) / lineLength) * deltaDiff;
            leftDelta += (sectionOffset / lineLength) * deltaDiff;
        }
    }

    static bool coveredOpenRange(HEdge &hedge, coord_t middleBottomZ, coord_t middleTopZ,
        bool wroteOpaqueMiddle)
    {
        LineSide const &front = hedge.mapElementAs<LineSideSegment>().lineSide();

        if(front.considerOneSided())
        {
            return wroteOpaqueMiddle;
        }

        /// @todo fixme: This additional test should not be necessary. For the obove
        /// test to fail and this to pass means that the geometry produced by the BSP
        /// builder is not correct (see: eternall.wad MAP10; note mapping errors).
        if(!hedge.twin().hasFace())
        {
            return wroteOpaqueMiddle;
        }

        SectorCluster const &cluster     = hedge.face().mapElementAs<ConvexSubspace>().cluster();
        SectorCluster const &backCluster = hedge.twin().face().mapElementAs<ConvexSubspace>().cluster();

        coord_t const ffloor = cluster.visFloor().heightSmoothed();
        coord_t const fceil  = cluster.visCeiling().heightSmoothed();
        coord_t const bfloor = backCluster.visFloor().heightSmoothed();
        coord_t const bceil  = backCluster.visCeiling().heightSmoothed();

        bool middleCoversOpening = false;
        if(wroteOpaqueMiddle)
        {
            coord_t xbottom = de::max(bfloor, ffloor);
            coord_t xtop    = de::min(bceil,  fceil);

            Surface const &middle = front.middle();
            xbottom += middle.materialOriginSmoothed().y;
            xtop    += middle.materialOriginSmoothed().y;

            middleCoversOpening = (middleTopZ >= xtop &&
                                   middleBottomZ <= xbottom);
        }

        if(wroteOpaqueMiddle && middleCoversOpening)
            return true;

        if(   (bceil <= ffloor &&
                   (front.top().hasMaterial()    || front.middle().hasMaterial()))
           || (bfloor >= fceil &&
                   (front.bottom().hasMaterial() || front.middle().hasMaterial())))
        {
            Surface const &ffloorSurface = cluster.visFloor().surface();
            Surface const &fceilSurface  = cluster.visCeiling().surface();
            Surface const &bfloorSurface = backCluster.visFloor().surface();
            Surface const &bceilSurface  = backCluster.visCeiling().surface();

            // A closed gap?
            if(de::fequal(fceil, bfloor))
            {
                return (bceil <= bfloor) ||
                       !(fceilSurface.hasSkyMaskedMaterial() &&
                         bceilSurface.hasSkyMaskedMaterial());
            }

            if(de::fequal(ffloor, bceil))
            {
                return (bfloor >= bceil) ||
                       !(ffloorSurface.hasSkyMaskedMaterial() &&
                         bfloorSurface.hasSkyMaskedMaterial());
            }

            return true;
        }

        /// @todo Is this still necessary?
        if(bceil <= bfloor ||
           (!(bceil - bfloor > 0) && bfloor > ffloor && bceil < fceil &&
            front.top().hasMaterial() && front.bottom().hasMaterial()))
        {
            // A zero height back segment
            return true;
        }

        return false;
    }

    /**
     * Configure @a prim according to the arguments given.
     */
    static Shard::Geom::Primitive &makeListPrimitive(Shard::Geom &shard,
        gl::Primitive type, WorldVBuf::Index vertCount,
        WorldVBuf::Index indicesOffset = 0)
    {
        shard.primitives.append(Shard::Geom::Primitive());
        Shard::Geom::Primitive &prim = shard.primitives.last();

        prim.type            = type;
        prim.vertCount       = vertCount;
        prim.indices         = shard.indices.data() + indicesOffset;
        prim.texScale        = Vector2f(1, 1);
        prim.texOffset       = Vector2f(0, 0);
        prim.detailTexScale  = Vector2f(1, 1);
        prim.detailTexOffset = Vector2f(0, 0);

        return prim;
    }

    /**
     * Configure @a prim according to the arguments given.
     */
    static Shard::Geom::Primitive &makeListPrimitive(Shard::Geom &shard,
        gl::Primitive type, WorldVBuf::Index vertCount,
        Vector2f const &texScale, Vector2f const &texOffset,
        Vector2f const &detailTexScale, Vector2f const &detailTexOffset,
        WorldVBuf::Index indicesOffset = 0)
    {
        shard.primitives.append(Shard::Geom::Primitive());
        Shard::Geom::Primitive &prim = shard.primitives.last();

        prim.type            = type;
        prim.vertCount       = vertCount;
        prim.indices         = shard.indices.data() + indicesOffset;
        prim.texScale        = texScale;
        prim.texOffset       = texOffset;
        prim.detailTexScale  = detailTexScale;
        prim.detailTexOffset = detailTexOffset;

        return prim;
    }

#endif // __CLIENT__
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

    typedef QMultiMap<de::HEdge *, de::WallEdge *> WallEdges;
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

    /**
     * Determines whether ambient light for the given @a wallSection should be
     * sourced from the Sector attributed to the Line::Side instead of "this"
     * cluster. (Support id Tech 1 map-hacks).
     */
    bool useAmbientLightFromSide(WallEdgeSection const &wallSection)
    {
        if(wallSection.id() != WallEdge::WallMiddle) return false;
        if(wallSection.edge().lineSide().considerOneSided()) return false;
        return wallSection.edge().lineSide().sectorPtr() != &self.sector();
    }

    /**
     * Determines whether ambient light for the given @a plane should be sourced
     * from the Sector attributed to the Plane instead of "this" cluster.
     * (Support id Tech 1 map-hacks).
     */
    bool useAmbientLightFromPlane(Plane const &plane)
    {
        return &plane.sector() != &self.sector();
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

    /**
     * Returns the vertex buffer we'll write all geometry to.
     */
    inline WorldVBuf &worldVBuf() const {
        return ClientApp::renderSystem().worldVBuf();
    }

    struct splinterdynlight_params_t
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

    struct splinterdynshadow_params_t
    {
        Instance *inst;
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
     * @param light Projected light geometry to splinter into a shard.
     */
    Shard::Geom *splinterDynlight(TexProjection const &light, splinterdynlight_params_t const &p)
    {
        WorldVBuf &vbuf = worldVBuf();

        DrawListSpec listSpec;
        listSpec.group = LightGeom;
        listSpec.texunits[TU_PRIMARY] = GLTextureUnit(light.texture, gl::ClampToEdge, gl::ClampToEdge);

        Shard::Geom *shard = new Shard::Geom(listSpec);

        if(p.leftSection) // A wall.
        {
            WallEdgeSection const &leftSection  = *p.leftSection;
            WallEdgeSection const &rightSection = *p.rightSection;

            Vector2f const quadTexCoords[4] = {
                Vector2f(light.topLeft.x,     light.bottomRight.y),
                Vector2f(light.topLeft.x,     light.topLeft.y    ),
                Vector2f(light.bottomRight.x, light.bottomRight.y),
                Vector2f(light.bottomRight.x, light.topLeft.y    )
            };

            bool const mustSubdivide = (leftSection.divisionCount() || rightSection.divisionCount());

            if(mustSubdivide) // Generate two triangle fans.
            {
                WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();
                WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();
                shard->indices.resize(leftFanSize + rightFanSize);

                vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                Rend_DivPosCoords(vbuf, shard->indices.data(), p.posCoords, leftSection, rightSection);
                Rend_DivTexCoords(vbuf, shard->indices.data(), quadTexCoords, leftSection, rightSection,
                                  WorldVBuf::PrimaryTex);

                for(WorldVBuf::Index i = 0; i < leftFanSize + rightFanSize; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                    //vertex.pos  = vbuf[p.indices[i]].pos;
                    vertex.rgba = light.color;
                }

                makeListPrimitive(*shard, gl::TriangleFan, leftFanSize);
                makeListPrimitive(*shard, gl::TriangleFan, rightFanSize,
                                  leftFanSize /*indices offset*/);
            }
            else // Generate one triangle strip.
            {
                WorldVBuf::Index stripSize = p.vertCount;
                shard->indices.resize(stripSize);

                vbuf.reserveElements(stripSize, shard->indices);
                for(WorldVBuf::Index i = 0; i < stripSize; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                    vertex.pos  = p.posCoords[i];//vbuf[p.indices[i]].pos;
                    vertex.rgba = light.color;
                }

                vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].x =
                vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].x = light.topLeft.x;

                vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].y =
                vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].y = light.topLeft.y;

                vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].x =
                vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].x = light.bottomRight.x;

                vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].y =
                vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].y = light.bottomRight.y;

                makeListPrimitive(*shard, gl::TriangleStrip, stripSize);
            }
        }
        else // A flat.
        {
            WorldVBuf::Index fanSize = p.vertCount;
            shard->indices.resize(fanSize);

            vbuf.reserveElements(fanSize, shard->indices);
            Vector2f const pDimensions = p.bottomRight->xy() - p.topLeft->xy();
            for(WorldVBuf::Index i = 0; i < fanSize; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                vertex.pos  = vbuf[p.indices[i]].pos;
                vertex.rgba = light.color;
                vertex.texCoord[WorldVBuf::PrimaryTex] =
                    Vector2f(((p.bottomRight->x - vertex.pos.x) / pDimensions.x * light.topLeft.x) +
                             ((vertex.pos.x     - p.topLeft->x) / pDimensions.x * light.bottomRight.x)
                             ,
                             ((p.bottomRight->y - vertex.pos.y) / pDimensions.y * light.topLeft.y) +
                             ((vertex.pos.y     - p.topLeft->y) / pDimensions.y * light.bottomRight.y));
            }

            makeListPrimitive(*shard, gl::TriangleFan, fanSize);
        }

        return shard;
    }

    /**
     * @param shadow  Projected shadow geometry to splinter into a shard.
     */
    Shard::Geom *splinterDynshadow(TexProjection const &shadow,
        splinterdynshadow_params_t const &p, DrawListSpec const &listSpec)
    {
        WorldVBuf &vbuf = worldVBuf();

        Shard::Geom *shard = new Shard::Geom(listSpec);

        if(p.leftSection) // A wall.
        {
            WallEdgeSection const &leftSection  = *p.leftSection;
            WallEdgeSection const &rightSection = *p.rightSection;

            Vector2f const quadTexCoords[4] = {
                Vector2f(shadow.topLeft.x,     shadow.bottomRight.y),
                Vector2f(shadow.topLeft.x,     shadow.topLeft.y    ),
                Vector2f(shadow.bottomRight.x, shadow.bottomRight.y),
                Vector2f(shadow.bottomRight.x, shadow.topLeft.y    )
            };

            bool const mustSubdivide = (leftSection.divisionCount() || rightSection.divisionCount());

            if(mustSubdivide) // Generate two triangle fans.
            {
                WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();
                WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();
                shard->indices.resize(leftFanSize + rightFanSize);

                vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                Rend_DivPosCoords(vbuf, shard->indices.data(), p.posCoords, leftSection, rightSection);
                Rend_DivTexCoords(vbuf, shard->indices.data(), quadTexCoords, leftSection, rightSection,
                                  WorldVBuf::PrimaryTex);

                for(WorldVBuf::Index i = 0; i < leftFanSize + rightFanSize; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                    //vertex.pos  = vbuf[p.indices[i]].pos;
                    vertex.rgba = shadow.color;
                }

                makeListPrimitive(*shard, gl::TriangleFan, leftFanSize);
                makeListPrimitive(*shard, gl::TriangleFan, rightFanSize,
                                  leftFanSize /*indices offset*/);
            }
            else // Generate one triangle strip.
            {
                WorldVBuf::Index stripSize = p.vertCount;
                shard->indices.resize(stripSize);

                vbuf.reserveElements(stripSize, shard->indices);
                for(WorldVBuf::Index i = 0; i < stripSize; ++i)
                {
                    WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                    vertex.pos  = p.posCoords[i];//vbuf[p.indices[i]].pos;
                    vertex.rgba = shadow.color;
                }

                vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].x =
                vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].x = shadow.topLeft.x;

                vbuf[shard->indices[1]].texCoord[WorldVBuf::PrimaryTex].y =
                vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].y = shadow.topLeft.y;

                vbuf[shard->indices[3]].texCoord[WorldVBuf::PrimaryTex].x =
                vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].x = shadow.bottomRight.x;

                vbuf[shard->indices[2]].texCoord[WorldVBuf::PrimaryTex].y =
                vbuf[shard->indices[0]].texCoord[WorldVBuf::PrimaryTex].y = shadow.bottomRight.y;

                makeListPrimitive(*shard, gl::TriangleStrip, stripSize);
            }
        }
        else // A flat.
        {
            WorldVBuf::Index fanSize = p.vertCount;
            shard->indices.resize(fanSize);

            vbuf.reserveElements(fanSize, shard->indices);
            Vector2f const pDimensions = p.bottomRight->xy() - p.topLeft->xy();
            for(WorldVBuf::Index i = 0; i < fanSize; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                vertex.pos  = vbuf[p.indices[i]].pos;
                vertex.rgba =    shadow.color;
                vertex.texCoord[WorldVBuf::PrimaryTex] =
                    Vector2f(((p.bottomRight->x - vertex.pos.x) / pDimensions.x * shadow.topLeft.x) +
                             ((vertex.pos.x     - p.topLeft->x) / pDimensions.x * shadow.bottomRight.x)
                             ,
                             ((p.bottomRight->y - vertex.pos.y) / pDimensions.y * shadow.topLeft.y) +
                             ((vertex.pos.y     - p.topLeft->y) / pDimensions.y * shadow.bottomRight.y));
            }

            makeListPrimitive(*shard, gl::TriangleFan, fanSize);
        }

        return shard;
    }

    void prepareDynlightShards(TexProjection const &light, splinterdynlight_params_t &p)
    {
        // If multitexturing is in use we skip the first.
        if(!(Rend_IsMTexLights() && p.lastIdx == 0))
        {
            p.subspace->shards() << splinterDynlight(light, p);
        }
        p.lastIdx++;
    }

    struct drawshadowworker_params_t
    {
        DrawListSpec drawListSpec;
        splinterdynshadow_params_t *drawShadowParms;
    };

    void prepareDynshadowShards(TexProjection const &shadow, splinterdynshadow_params_t &p,
        DrawListSpec const &listSpec)
    {
        p.subspace->shards() << splinterDynshadow(shadow, p, listSpec);
    }

    /// Generates a new primitive for each light projection.
    static int splinterDynlightWorker(TexProjection const *light, void *context)
    {
        splinterdynlight_params_t &parm = *static_cast<splinterdynlight_params_t *>(context);
        parm.inst->prepareDynlightShards(*light, parm);
        return 0; // Continue iteration.
    }

    /// Generates a new primitive for each shadow projection.
    static int splinterDynshadowWorker(TexProjection const *shadow, void *context)
    {
        drawshadowworker_params_t &itParm = *static_cast<drawshadowworker_params_t *>(context);
        splinterdynshadow_params_t &parm  = *itParm.drawShadowParms;
        parm.inst->prepareDynshadowShards(*shadow, parm, itParm.drawListSpec);
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
    uint splinterAllDynlights(uint listIdx, splinterdynlight_params_t &parm)
    {
        uint numRendered = parm.lastIdx;

        Rend_IterateProjectionList(listIdx, splinterDynlightWorker, &parm);

        numRendered = parm.lastIdx - numRendered;
        if(Rend_IsMTexLights())
        {
            numRendered -= 1;
        }
        return numRendered;
    }

    /**
     * Render all shadows in projection list @a listIdx according to @a parm
     * writing them to the renderering lists for the current frame.
     */
    void splinterAllDynshadows(uint listIdx, splinterdynshadow_params_t &p)
    {
        // Write shadows to the render lists.
        drawshadowworker_params_t parm;
        parm.drawListSpec.group                = ShadowGeom;
        parm.drawListSpec.texunits[TU_PRIMARY] = GLTextureUnit(GL_PrepareLSTexture(LST_DYNAMIC), gl::ClampToEdge, gl::ClampToEdge);
        parm.drawShadowParms = &p;

        Rend_IterateProjectionList(listIdx, splinterDynshadowWorker, &parm);
    }

    coord_t skyPlaneZ(bool upper) const
    {
        if(!P_IsInVoid(viewPlayer))
        {
            Map &map = self.sector().map();
            return map.skyPlane(upper? Map::SkyCeiling : Map::SkyFloor).height();
        }
        else
        {
            return self.visPlane(upper? Sector::Ceiling : Sector::Floor).heightSmoothed();
        }
    }

    /// @param skyCap  @ref skyCapFlags.
    void prepareSkyMaskCapShards(ConvexSubspace &subspace, bool upper)
    {
        WorldVBuf &vbuf = worldVBuf();

        // Caps are unnecessary in sky debug mode (will be drawn as regular planes).
        if(devRendSkyMode) return;

        Shard::Geom *shard = new Shard::Geom(DrawListSpec(SkyMaskGeom));
        subspace.shards() << shard;

        ClockDirection const direction = (upper? Anticlockwise : Clockwise);
        coord_t const height           = skyPlaneZ(upper);
        Face const &poly               = subspace.poly();

        HEdge *fanBase = subspace.fanBase();
        WorldVBuf::Index const fanSize = poly.hedgeCount() + (!fanBase? 2 : 0);
        shard->indices.resize(fanSize);

        vbuf.reserveElements(fanSize, shard->indices);
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

        makeListPrimitive(*shard, gl::TriangleFan, fanSize);
    }

    void prepareSkyMaskWallShardStrip(ConvexSubspace &subspace, WorldVBuf::Index vertCount,
        Vector3f const *posCoords, Vector2f const *texCoords, Material *material)
    {
        DENG2_ASSERT(posCoords != 0);

        WorldVBuf &vbuf = worldVBuf();

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
        subspace.shards() << shard;

        shard->indices.resize(vertCount);

        vbuf.reserveElements(vertCount, shard->indices);
        Vector4f const saturated(1, 1, 1, 1); // This geometry is always fully lit.
        for(WorldVBuf::Index i = 0; i < vertCount; ++i)
        {
            WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
            vertex.pos = posCoords[i];
            if(devRendSkyMode)
            {
                vertex.rgba = saturated;
                DENG2_ASSERT(texCoords != 0);
                vertex.texCoord[WorldVBuf::PrimaryTex] = texCoords[i];
            }
        }

        if(!devRendSkyMode)
        {
            makeListPrimitive(*shard, gl::TriangleStrip, vertCount);
        }
        else
        {
            makeListPrimitive(*shard, gl::TriangleStrip, vertCount,
                              shard->listSpec.unit(TU_PRIMARY       ).scale,
                              shard->listSpec.unit(TU_PRIMARY       ).offset,
                              shard->listSpec.unit(TU_PRIMARY_DETAIL).scale,
                              shard->listSpec.unit(TU_PRIMARY_DETAIL).offset);
        }
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

    void prepareAllSkyMaskShards(ConvexSubspace &subspace)
    {
        // Lower?
        if(self.visFloor().surface().hasSkyMaskedMaterial())
        {
            prepareSkyMaskWallShards(subspace, WallEdge::SkyBottom);
            prepareSkyMaskCapShards(subspace, false /*lower*/);
        }

        // Upper?
        if(self.visCeiling().surface().hasSkyMaskedMaterial())
        {
            prepareSkyMaskWallShards(subspace, WallEdge::SkyTop);
            prepareSkyMaskCapShards(subspace, true /*upper*/);
        }
    }

    void prepareWallFakeradioShards(ConvexSubspace &subspace,
        WallEdgeSection const &leftSection, WallEdgeSection const &rightSection,
        Vector3f const *origPosCoords,
        rendershadowseg_params_t const &wsParms)
    {
        DENG2_ASSERT(origPosCoords != 0);

        WorldVBuf &vbuf = worldVBuf();

        if(rendFakeRadio > 1) return;

        // Shadows are black.
        Vector4f const shadowColor(0, 0, 0, de::clamp(0.f, wsParms.shadowDark * wsParms.shadowMul, 1.f));

        Vector2f quadCoords[4];
        quadTexFakeradioCoords(quadCoords, origPosCoords, wsParms.sectionWidth,
                               leftSection.top().origin(), rightSection.bottom().origin(),
                               wsParms.texOrigin, wsParms.texDimensions, wsParms.horizontal);

        DrawListSpec shadowListSpec;
        shadowListSpec.group = ShadowGeom;
        shadowListSpec.texunits[TU_PRIMARY] = GLTextureUnit(GL_PrepareLSTexture(wsParms.texture), gl::ClampToEdge, gl::ClampToEdge);

        bool const mustSubdivide = (leftSection.divisionCount() || rightSection.divisionCount());

        if(mustSubdivide) // Draw as two triangle fans.
        {
            WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();
            WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();

            Shard::Geom *shard = new Shard::Geom(shadowListSpec);
            shard->indices.resize(leftFanSize + rightFanSize);
            subspace.shards() << shard;

            vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
            Rend_DivPosCoords(vbuf, shard->indices.data(), origPosCoords, leftSection, rightSection);
            Rend_DivTexCoords(vbuf, shard->indices.data(), quadCoords, leftSection, rightSection,
                              WorldVBuf::PrimaryTex);

            for(WorldVBuf::Index i = 0; i < leftFanSize + rightFanSize; ++i)
            {
                vbuf[shard->indices[i]].rgba = shadowColor;
            }

            makeListPrimitive(*shard, gl::TriangleFan, leftFanSize);
            makeListPrimitive(*shard, gl::TriangleFan, rightFanSize,
                              leftFanSize /*indices offset*/);
        }
        else
        {
            Shard::Geom *shard = new Shard::Geom(shadowListSpec);
            shard->indices.resize(4);
            subspace.shards() << shard;

            vbuf.reserveElements(4, shard->indices);
            for(WorldVBuf::Index i = 0; i < 4; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[shard->indices[i]];
                vertex.pos  = origPosCoords[i];
                vertex.rgba = shadowColor;
                vertex.texCoord[WorldVBuf::PrimaryTex] = quadCoords[i];
            }

            makeListPrimitive(*shard, gl::TriangleStrip, 4);
        }
    }

    /**
     * Prepare all FakeRadio Shards for the specified wall section.
     *
     * @param leftSection        Geometry for the left wall section edge.
     * @param rightSection       Geometry for the right wall section edge.
     * @param ambientLightColor  Ambient light values for the wall section. This is
     *                           @em not automatically taken from the sector on the
     *                           front side of the wall as various map-hacks dictate
     *                           otherwise.
     */
    void prepareAllWallFakeradioShards(ConvexSubspace &subspace,
        WallEdgeSection const &leftSection, WallEdgeSection const &rightSection,
        Vector4f const &ambientLightColor)
    {
        // Disabled?
        if(!rendFakeRadio) return;
        if(levelFullBright) return;

        // Don't bother with shadows on geometry that is near enough "black" already.
        if(ambientLightColor.w < 0.01f) return;

        /// Determine the shadow properties (@todo Make cvars out of constants).
        float const shadowSize = 2 * (8 + 16 - ambientLightColor.w * 16);
        float const shadowDark = Rend_RadioCalcShadowDarkness(ambientLightColor.w);

        if(shadowSize <= 0)
            return;

        LineSide &side = leftSection.edge().lineSide();
        Rend_RadioUpdateForLineSide(side);

        HEdge const *hedge = side.leftHEdge();
        SectorCluster const *cluster = &hedge->face().mapElementAs<ConvexSubspace>().cluster();
        SectorCluster const *backCluster = 0;

        if(leftSection.id() != WallEdge::WallMiddle && hedge->twin().hasFace())
        {
            backCluster = hedge->twin().face().mapElementAs<ConvexSubspace>().clusterPtr();
        }

        bool const haveBottomShadower = Rend_RadioPlaneCastsShadow(cluster->visFloor());
        bool const haveTopShadower    = Rend_RadioPlaneCastsShadow(cluster->visCeiling());

        // Walls unaffected by floor and ceiling shadow casters will receive no side
        // shadows either. We could do better here...
        if(!haveBottomShadower && !haveTopShadower)
            return;

        coord_t const lineLength    = side.line().length();
        coord_t const sectionOffset = leftSection.edge().lineSideOffset();
        coord_t const sectionWidth  = de::abs(Vector2d(rightSection.edge().origin() - leftSection.edge().origin()).length());

        coord_t const fFloor = cluster->visFloor().heightSmoothed();
        coord_t const fCeil  = cluster->visCeiling().heightSmoothed();
        coord_t const bFloor = (backCluster? backCluster->visFloor().heightSmoothed() : 0);
        coord_t const bCeil  = (backCluster? backCluster->visCeiling().heightSmoothed() : 0);

        Vector3f const posCoords[4] = {
            Vector3f( leftSection.bottom().origin()),
            Vector3f( leftSection.top   ().origin()),
            Vector3f(rightSection.bottom().origin()),
            Vector3f(rightSection.top   ().origin())
        };

        LineSideRadioData &frData = Rend_RadioDataForLineSide(side);

        // Top Shadow?
        if(haveTopShadower)
        {
            if(rightSection.top().z() > fCeil - shadowSize &&
               leftSection.bottom().z() < fCeil)
            {
                rendershadowseg_params_t parms;
                parms.setupForTop(shadowSize, shadowDark,
                                  leftSection.top().z(), sectionOffset, sectionWidth,
                                  fFloor, fCeil, frData);

                prepareWallFakeradioShards(subspace, leftSection, rightSection, posCoords, parms);
            }
        }

        // Bottom Shadow?
        if(haveBottomShadower)
        {
            if(leftSection.bottom().z() < fFloor + shadowSize &&
               rightSection.top().z() > fFloor)
            {
                rendershadowseg_params_t parms;
                parms.setupForBottom(shadowSize, shadowDark,
                                     leftSection.top().z(), sectionOffset, sectionWidth,
                                     fFloor, fCeil, frData);

                prepareWallFakeradioShards(subspace, leftSection, rightSection, posCoords, parms);
            }
        }

        // Left Shadow?
        if(frData.sideCorners[0].corner > 0 && sectionOffset < shadowSize)
        {
            rendershadowseg_params_t parms;
            parms.setupForSide(shadowSize, shadowDark,
                               leftSection.bottom().z(), leftSection.top().z(), false,
                               haveBottomShadower, haveTopShadower,
                               sectionOffset, sectionWidth,
                               fFloor, fCeil, backCluster != 0, bFloor, bCeil, lineLength,
                               frData);

            prepareWallFakeradioShards(subspace, leftSection, rightSection, posCoords, parms);
        }

        // Right Shadow?
        if(frData.sideCorners[1].corner > 0 &&
           sectionOffset + sectionWidth > lineLength - shadowSize)
        {
            rendershadowseg_params_t parms;
            parms.setupForSide(shadowSize, shadowDark,
                               leftSection.bottom().z(), leftSection.top().z(), true,
                               haveBottomShadower, haveTopShadower, sectionOffset, sectionWidth,
                               fFloor, fCeil, backCluster != 0, bFloor, bCeil, lineLength,
                               frData);

            prepareWallFakeradioShards(subspace, leftSection, rightSection, posCoords, parms);
        }
    }

    void prepareWallSectionShards(ConvexSubspace &subspace,
        WallEdge &leftEdge, WallEdge &rightEdge, WallEdge::SectionId sectionId,
        bool *retWroteOpaque = 0, coord_t *retBottomZ = 0, coord_t *retTopZ = 0)
    {
        DENG2_ASSERT(sectionId >= WallEdge::WallMiddle && sectionId <= WallEdge::WallTop);

        WallEdgeSection &leftSection  =  leftEdge.section(sectionId);
        WallEdgeSection &rightSection = rightEdge.section(sectionId);
        Surface &surface              = *leftSection.surfacePtr();

        if(retWroteOpaque) *retWroteOpaque = false;
        if(retBottomZ)     *retBottomZ     = 0;
        if(retTopZ)        *retTopZ        = 0;

        // Do the edge geometries describe a valid polygon?
        if(!leftSection.isValid() || !rightSection.isValid() ||
           de::fequal(leftSection.bottom().z(), rightSection.top().z()))
            return;

        // Skip nearly transparent surfaces.
        float opacity = surface.opacity();
        if(opacity < .001f) return;

        // Determine which Material to use (a drawable material is required).
        Material *material = Rend_ChooseMapSurfaceMaterial(surface);
        if(!material || !material->isDrawable())
            return;

        // Should we apply a fade out when the viewer is near to this geometry?
        bool const useNearFade = Rend_NearFadeOpacity(leftSection, rightSection, opacity);

        if(leftSection.flags().testFlag(WallEdgeSection::ForceOpaque))
        {
            opacity = 1;
        }

        MaterialSnapshot const &matSnapshot = material->prepare(Rend_MapSurfaceMaterialSpec());
        bool const skyMasked = (material->isSkyMasked() && !devRendSkyMode);

        float glowing = 0;
        if(!skyMasked)
        {
            if(glowFactor > .0001f)
            {
                if(material == surface.materialPtr())
                {
                    glowing = matSnapshot.glowStrength();
                }
                else
                {
                    Material *actualMaterial =
                        surface.hasMaterial()? surface.materialPtr()
                                             : &ClientApp::renderSystem().missingMaterial();

                    MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                    glowing = ms.glowStrength();
                }

                glowing *= glowFactor; // Global scale factor.
            }
        }

        Vector3d const topLeft        = leftSection.top().origin();
        Vector3d const bottomRight    = rightSection.bottom().origin();

        Vector3f const materialOrigin = leftSection.materialOrigin();
        Vector2f const materialScale  = surface.materialScale();

        blendmode_t const blendmode   = chooseWallBlendmode(leftSection);
        Vector4f const ambientLight   =
            useAmbientLightFromSide(leftSection)? Rend_AmbientLightColor(leftEdge.lineSide().sector())
                                                : self.lightSourceColorfIntensity();

        Vector3f const *topTintColor  = 0, *bottomTintColor = 0;
        if(!skyMasked)
        {
            chooseWallTintColors(leftSection, &bottomTintColor, &topTintColor);
        }

        // Calculate the light level deltas for this wall section.
        float lightLevelDeltas[2]; // [left, right]
        wallLightLevelDeltas(leftSection, rightSection, lightLevelDeltas);

        // Lists of projected lights/shadows that affect this geometry.
        uint lightListIdx = 0, shadowListIdx = 0;
        if(!skyMasked)
        {
            projectDynamics(subspace, surface, glowing, topLeft, bottomRight,
                            leftSection.flags().testFlag(WallEdgeSection::NoDynLights),
                            leftSection.flags().testFlag(WallEdgeSection::NoDynShadows),
                            leftSection.flags().testFlag(WallEdgeSection::SortDynLights),
                            lightListIdx, shadowListIdx);
        }

        bool wroteOpaque = true;
        if(!Rend_MustDrawAsVissprite(leftSection.flags().testFlag(WallEdgeSection::ForceOpaque),
                                     skyMasked, opacity, blendmode, matSnapshot))
        {
            WorldVBuf &vbuf = worldVBuf();

            // Map RTU configuration from prepared MaterialSnapshot(s).
            GLTextureUnit const *primaryRTU       = (!skyMasked)? &matSnapshot.unit(RTU_PRIMARY) : NULL;
            GLTextureUnit const *primaryDetailRTU = (r_detail && !skyMasked && matSnapshot.unit(RTU_PRIMARY_DETAIL).hasTexture())? &matSnapshot.unit(RTU_PRIMARY_DETAIL) : NULL;
            GLTextureUnit const *interRTU         = (!skyMasked && matSnapshot.unit(RTU_INTER).hasTexture())? &matSnapshot.unit(RTU_INTER) : NULL;
            GLTextureUnit const *interDetailRTU   = (r_detail && !skyMasked && matSnapshot.unit(RTU_INTER_DETAIL).hasTexture())? &matSnapshot.unit(RTU_INTER_DETAIL) : NULL;
            GLTextureUnit const *shineRTU         = (useShinySurfaces && !skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture())? &matSnapshot.unit(RTU_REFLECTION) : NULL;
            GLTextureUnit const *shineMaskRTU     = (useShinySurfaces && !skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture() && matSnapshot.unit(RTU_REFLECTION_MASK).hasTexture())? &matSnapshot.unit(RTU_REFLECTION_MASK) : NULL;

            bool useLights = false, useShadows = false;

            DGLuint modTex = 0;
            Vector2f modTexSt[2]; // [topLeft, bottomRight]
            Vector3f modColor;

            bool const skyMaskedMaterial = (skyMasked || (matSnapshot.material().isSkyMasked()));
            if(!skyMaskedMaterial && glowing < 1)
            {
                useLights  = (lightListIdx  > 0);
                useShadows = (shadowListIdx > 0);

                // If multitexturing is enabled and there is at least one dynlight
                // affecting this surface, grab the parameters needed to draw it.
                if(useLights && Rend_IsMTexLights())
                {
                    TexProjection *dyn = 0;
                    Rend_IterateProjectionList(lightListIdx, RIT_FirstDynlightIterator, &dyn);

                    modTex       = dyn->texture;
                    modColor     = dyn->color;
                    modTexSt[0]  = dyn->topLeft;
                    modTexSt[1]  = dyn->bottomRight;
                }
            }

            Vector3f const posCoords[4] = {
                Vector3f( leftSection.bottom().origin()),
                Vector3f( leftSection.top   ().origin()),
                Vector3f(rightSection.bottom().origin()),
                Vector3f(rightSection.top   ().origin())
            };
            coord_t const sectionWidth = de::abs((rightSection.edge().origin() - leftSection.edge().origin()).length());

            Vector2f primaryTexCoords[4];
            quadTexCoords(primaryTexCoords, posCoords, sectionWidth, topLeft);

            Vector2f interTexCoords[4];
            if(interRTU)
            {
                quadTexCoords(interTexCoords, posCoords, sectionWidth, topLeft);
            }

            Vector2f shineTexCoords[4];
            if(shineRTU)
            {
                quadShinyTexCoords(shineTexCoords, posCoords, sectionWidth);
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
                if(levelFullBright || !(glowing < 1))
                {
                    // Uniform color. Apply to all vertices.
                    float ll = de::clamp(0.f, ambientLight.w + (levelFullBright? 1 : glowing), 1.f);
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
                        MapElement &mapElement = leftEdge.hedge().mapElement();
                        int const geomGroup = sectionId == WallEdge::WallMiddle? LineSide::Middle :
                                              sectionId == WallEdge::WallBottom? LineSide::Bottom :
                                                                                 LineSide::Top;

                        Map &map     = self.sector().map();
                        Shard &shard = self.shard(mapElement, geomGroup);

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
                        shard.lightWithBiasSources(posCoords, colorCoords, surface.tangentMatrix(),
                                                   map.biasCurrentTime());

                        // Apply surface glow.
                        if(glowing > 0)
                        {
                            Vector4f const glow(glowing, glowing, glowing, 0);
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
                        float llL = de::clamp(0.f, ambientLight.w + lightLevelDeltas[0] + glowing, 1.f);
                        float llR = de::clamp(0.f, ambientLight.w + lightLevelDeltas[1] + glowing, 1.f);

                        // Calculate the color for each vertex, blended with plane color?
                        if(topTintColor->x < 1 || topTintColor->y < 1 || topTintColor->z < 1)
                        {
                            Vector3f const finalColor = ambientLight.xyz() * (*topTintColor);

                            if(llL != llR)
                            {
                                Rend_LightVertex(colorCoords[0], posCoords[0], llL, finalColor);
                                Rend_LightVertex(colorCoords[1], posCoords[1], llL, finalColor);
                                Rend_LightVertex(colorCoords[2], posCoords[2], llR, finalColor);
                                Rend_LightVertex(colorCoords[3], posCoords[3], llR, finalColor);
                            }
                            else
                            {
                                Rend_LightVertices(4, colorCoords, posCoords, llL, finalColor);
                            }
                        }
                        else
                        {
                            // Use sector light+color only.
                            if(llL != llR)
                            {
                                Rend_LightVertex(colorCoords[0], posCoords[0], llL, ambientLight.xyz());
                                Rend_LightVertex(colorCoords[1], posCoords[1], llL, ambientLight.xyz());
                                Rend_LightVertex(colorCoords[2], posCoords[2], llR, ambientLight.xyz());
                                Rend_LightVertex(colorCoords[3], posCoords[3], llR, ambientLight.xyz());
                            }
                            else
                            {
                                Rend_LightVertices(4, colorCoords, posCoords, llL, ambientLight.xyz());
                            }
                        }

                        if(bottomTintColor)
                        {
                            Vector3f const finalColor = ambientLight.xyz() * (*bottomTintColor);

                            Rend_LightVertex(colorCoords[0], posCoords[0], llL, finalColor);
                            Rend_LightVertex(colorCoords[2], posCoords[2], llR, finalColor);
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
                    colorIt->w = opacity;
                }
            }

            if(useLights || useShadows)
            {
                // Geometry which is lit by dynamic lights may need to be drawn
                // differently than non-lit surfaces depending on the renderer
                // configuration. If the averaged vertex light level is nearly
                // fully saturated we can skip adding extra lights.
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
                // Project dynamic lights to shards.
                splinterdynlight_params_t parm; de::zap(parm);

                parm.inst         = this;
                parm.subspace     = &subspace;
                parm.vertCount    = 4;
                parm.posCoords    = posCoords;
                parm.topLeft      = &topLeft;
                parm.bottomRight  = &bottomRight;
                parm.leftSection  = &leftSection;
                parm.rightSection = &rightSection;

                hasDynlights = (0 != splinterAllDynlights(lightListIdx, parm));
            }

            if(useShadows)
            {
                // Project dynamic shadows to shards.
                splinterdynshadow_params_t parm; de::zap(parm);

                parm.inst         = this;
                parm.subspace     = &subspace;
                parm.vertCount    = 4;
                parm.posCoords    = posCoords;
                parm.topLeft      = &topLeft;
                parm.bottomRight  = &bottomRight;
                parm.leftSection  = &leftSection;
                parm.rightSection = &rightSection;

                splinterAllDynshadows(shadowListIdx, parm);
            }

            if(!skyMasked)
            {
                DrawListSpec listSpec((modTex || hasDynlights)? LitGeom : UnlitGeom);
                if(primaryRTU)
                {
                    listSpec.texunits[TU_PRIMARY] = *primaryRTU;
                    listSpec.texunits[TU_PRIMARY].offset += materialOrigin;
                    listSpec.texunits[TU_PRIMARY].scale  *= materialScale;
                    listSpec.texunits[TU_PRIMARY].offset *= materialScale;
                }
                if(primaryDetailRTU)
                {
                    listSpec.texunits[TU_PRIMARY_DETAIL] = *primaryDetailRTU;
                    listSpec.texunits[TU_PRIMARY_DETAIL].offset += materialOrigin;
                }
                if(interRTU)
                {
                    listSpec.texunits[TU_INTER] = *interRTU;
                    listSpec.texunits[TU_INTER].offset += materialOrigin;
                    listSpec.texunits[TU_INTER].scale  *= materialScale;
                    listSpec.texunits[TU_INTER].offset *= materialScale;
                }
                if(interDetailRTU)
                {
                    listSpec.texunits[TU_INTER_DETAIL] = *interDetailRTU;
                    listSpec.texunits[TU_INTER_DETAIL].offset += materialOrigin;
                }

                bool const mustSubdivide = (leftSection.divisionCount() || rightSection.divisionCount());

                if(mustSubdivide) // Generate two triangle fans.
                {
                    WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();
                    WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();

                    Shard::Geom *shard = new Shard::Geom(listSpec, BM_NORMAL, modTex, modColor, hasDynlights);
                    subspace.shards() << shard;

                    shard->indices.resize(leftFanSize + rightFanSize);

                    vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                    Rend_DivPosCoords(vbuf, shard->indices.data(), posCoords, leftSection, rightSection);
                    Rend_DivColorCoords(vbuf, shard->indices.data(), colorCoords, leftSection, rightSection);
                    if(primaryRTU)
                    {
                        Rend_DivTexCoords(vbuf, shard->indices.data(), primaryTexCoords, leftSection, rightSection,
                                          WorldVBuf::PrimaryTex);
                    }
                    if(interRTU)
                    {
                        Rend_DivTexCoords(vbuf, shard->indices.data(), interTexCoords, leftSection, rightSection,
                                          WorldVBuf::InterTex);
                    }
                    if(modTex && Rend_IsMTexLights())
                    {
                        Rend_DivTexCoords(vbuf, shard->indices.data(), modTexCoords, leftSection, rightSection,
                                          WorldVBuf::ModTex);
                    }

                    makeListPrimitive(*shard, gl::TriangleFan, leftFanSize,
                                      listSpec.unit(TU_PRIMARY       ).scale,
                                      listSpec.unit(TU_PRIMARY       ).offset,
                                      listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                      listSpec.unit(TU_PRIMARY_DETAIL).offset);

                    makeListPrimitive(*shard, gl::TriangleFan, rightFanSize,
                                      listSpec.unit(TU_PRIMARY       ).scale,
                                      listSpec.unit(TU_PRIMARY       ).offset,
                                      listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                      listSpec.unit(TU_PRIMARY_DETAIL).offset,
                                      leftFanSize /*indices offset*/);

                    if(shineRTU)
                    {
                        DrawListSpec shineListSpec(ShineGeom);
                        shineListSpec.texunits[TU_PRIMARY] = *shineRTU;
                        if(shineMaskRTU)
                        {
                            shineListSpec.texunits[TU_INTER] = *shineMaskRTU;
                            shineListSpec.texunits[TU_INTER].offset += materialOrigin;
                            shineListSpec.texunits[TU_INTER].scale  *= materialScale;
                            shineListSpec.texunits[TU_INTER].offset *= materialScale;
                        }

                        Shard::Geom *shineShard = new Shard::Geom(shineListSpec, matSnapshot.shineBlendMode());
                        subspace.shards() << shineShard;

                        shineShard->indices.resize(leftFanSize + rightFanSize);

                        vbuf.reserveElements(leftFanSize + rightFanSize, shineShard->indices);
                        Rend_DivPosCoords(vbuf, shineShard->indices.data(), posCoords, leftSection, rightSection);
                        Rend_DivColorCoords(vbuf, shineShard->indices.data(), shineColorCoords, leftSection, rightSection);
                        Rend_DivTexCoords(vbuf, shineShard->indices.data(), shineTexCoords, leftSection, rightSection,
                                          WorldVBuf::PrimaryTex);
                        if(shineMaskRTU)
                        {
                            Rend_DivTexCoords(vbuf, shineShard->indices.data(), primaryTexCoords, leftSection, rightSection,
                                              WorldVBuf::InterTex);
                        }

                        makeListPrimitive(*shineShard, gl::TriangleFan, leftFanSize,
                                          shineListSpec.unit(TU_INTER).scale,
                                          shineListSpec.unit(TU_INTER).offset,
                                          Vector2f(1, 1),
                                          Vector2f(0, 0));

                        makeListPrimitive(*shineShard, gl::TriangleFan, rightFanSize,
                                          shineListSpec.unit(TU_INTER).scale,
                                          shineListSpec.unit(TU_INTER).offset,
                                          Vector2f(1, 1),
                                          Vector2f(0, 0),
                                          leftFanSize /*indices offset*/);
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
                        vertex.pos  = posCoords[i];
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

                    makeListPrimitive(*shard, gl::TriangleStrip, 4,
                                      listSpec.unit(TU_PRIMARY       ).scale,
                                      listSpec.unit(TU_PRIMARY       ).offset,
                                      listSpec.unit(TU_PRIMARY_DETAIL).scale,
                                      listSpec.unit(TU_PRIMARY_DETAIL).offset);

                    if(shineRTU)
                    {
                        DrawListSpec shineListSpec(ShineGeom);
                        shineListSpec.texunits[TU_PRIMARY] = *shineRTU;
                        if(shineMaskRTU)
                        {
                            shineListSpec.texunits[TU_INTER] = *shineMaskRTU;
                            shineListSpec.texunits[TU_INTER].offset += materialOrigin;
                            shineListSpec.texunits[TU_INTER].scale  *= materialScale;
                            shineListSpec.texunits[TU_INTER].offset *= materialScale;
                        }

                        Shard::Geom *shineShard = new Shard::Geom(shineListSpec, matSnapshot.shineBlendMode());
                        subspace.shards() << shineShard;

                        shineShard->indices.resize(4);

                        vbuf.reserveElements(4, shineShard->indices);
                        for(WorldVBuf::Index i = 0; i < 4; ++i)
                        {
                            WorldVBuf::Type &vertex = vbuf[shineShard->indices[i]];
                            vertex.pos  = posCoords[i];
                            vertex.rgba = shineColorCoords[i];
                            vertex.texCoord[WorldVBuf::PrimaryTex] = shineTexCoords[i];
                            if(shineMaskRTU)
                            {
                                vertex.texCoord[WorldVBuf::InterTex] = primaryTexCoords[i];
                            }
                        }

                        makeListPrimitive(*shineShard, gl::TriangleStrip, 4,
                                          shineListSpec.unit(TU_INTER         ).scale,
                                          shineListSpec.unit(TU_INTER         ).offset,
                                          shineListSpec.unit(TU_PRIMARY_DETAIL).scale,
                                          shineListSpec.unit(TU_PRIMARY_DETAIL).offset);
                    }
                }
            }
            else // Sky-masked
            {
                bool const mustSubdivide = (leftSection.divisionCount() || rightSection.divisionCount());

                Shard::Geom *shard = new Shard::Geom(DrawListSpec(SkyMaskGeom));
                subspace.shards() << shard;

                if(mustSubdivide) // Generate two triangle fans.
                {
                    WorldVBuf::Index const leftFanSize  = 3 + leftSection.divisionCount();
                    WorldVBuf::Index const rightFanSize = 3 + rightSection.divisionCount();

                    shard->indices.resize(leftFanSize + rightFanSize);

                    vbuf.reserveElements(leftFanSize + rightFanSize, shard->indices);
                    Rend_DivPosCoords(vbuf, shard->indices.data(), posCoords, leftSection, rightSection);

                    makeListPrimitive(*shard, gl::TriangleFan, leftFanSize);
                    makeListPrimitive(*shard, gl::TriangleFan, rightFanSize,
                                      leftFanSize /*indices offset*/);
                }
                else // Generate one triangle strip.
                {
                    shard->indices.resize(4);

                    vbuf.reserveElements(4, shard->indices);
                    for(WorldVBuf::Index i = 0; i < 4; ++i)
                    {
                        vbuf[shard->indices[i]].pos = posCoords[i];
                    }

                    makeListPrimitive(*shard, gl::TriangleStrip, 4);
                }
            }

            // Prepare FakeRadio shards for this section?
            if(!leftSection.flags().testFlag(WallEdgeSection::NoFakeRadio) &&
               !skyMasked && !(glowing > 0))
            {
                prepareAllWallFakeradioShards(subspace, leftSection, rightSection,
                                              ambientLight);
            }
        }
        else
        {
            Rend_PrepareWallSectionVissprite(subspace, ambientLight,
                                             *topTintColor,
                                             glowing,
                                             opacity, blendmode,
                                             materialOrigin, matSnapshot,
                                             lightListIdx,
                                             lightLevelDeltas[0], lightLevelDeltas[1],
                                             surface.tangentMatrix(),
                                             &leftSection, &rightSection, bottomTintColor);

            wroteOpaque = false; /// We @em had to use a vissprite; clearly not opaque.
        }

        if(retWroteOpaque) *retWroteOpaque = wroteOpaque && !useNearFade;
        if(retBottomZ)     *retBottomZ     = leftSection.bottom().z();
        if(retTopZ)        *retTopZ        = rightSection.top().z();
    }

    void prepareWallShards(ConvexSubspace &subspace, HEdge *hedge)
    {
        DENG2_ASSERT(hedge != 0);

        // Edges without a map line segment implicitly have no surfaces.
        if(!hedge->hasMapElement()) return;

        // We can angle-occlude the range defined by the XY origins of the halfedge
        // if the opening between floor and ceiling has been covered by opaque geometry.
        bool geomCoversOpening = false;

        LineSideSegment &seg = hedge->mapElementAs<LineSideSegment>();
        if(seg.lineSide().hasSections() && seg.isFrontFacing())
        {
            // Generate the wall section shard geometries.
            bool builtOpaqueMiddle = false;
            coord_t middleBottomZ  = 0;
            coord_t middleTopZ     = 0;

            WallEdge leftEdge(*hedge, Line::From);// = findWallEdge(*hedge, Line::From);
            WallEdge rightEdge(*hedge, Line::To); // = findWallEdge(*hedge, Line::To);

            prepareWallSectionShards(subspace, leftEdge, rightEdge, WallEdge::WallBottom);
            prepareWallSectionShards(subspace, leftEdge, rightEdge, WallEdge::WallTop);
            prepareWallSectionShards(subspace, leftEdge, rightEdge, WallEdge::WallMiddle,
                                     &builtOpaqueMiddle, &middleBottomZ, &middleTopZ);

            geomCoversOpening = coveredOpenRange(*hedge, middleBottomZ, middleTopZ, builtOpaqueMiddle);
        }

        seg.setOpenRangeCovered(geomCoversOpening);
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
        Face const &poly       = subspace.poly();
        Surface const &surface = plane.surface();

        // Skip planes with a sky-masked material? (Drawn with the mask geometry).
        if(!devRendSkyMode && surface.hasSkyMaskedMaterial() &&
           plane.indexInSector() <= Sector::Ceiling)
        {
            return;
        }

        // Skip nearly transparent surfaces.
        float opacity = surface.opacity();
        if(opacity < .001f) return;

        // Determine which Material to use (a drawable material is required).
        Material *material = Rend_ChooseMapSurfaceMaterial(surface);
        if(!material || !material->isDrawable())
            return;

        // Force opaque?
        if(plane.indexInSector() <= Sector::Ceiling || (material->isSkyMasked() && devRendSkyMode))
        {
            opacity = 1;
        }

        MaterialSnapshot const &matSnapshot = material->prepare(Rend_MapSurfaceMaterialSpec());
        bool const skyMasked = (material->isSkyMasked() && !devRendSkyMode);

        float glowing = 0;
        if(!skyMasked)
        {
            if(glowFactor > .0001f)
            {
                if(material == surface.materialPtr())
                {
                    glowing = matSnapshot.glowStrength();
                }
                else
                {
                    Material *actualMaterial =
                        surface.hasMaterial()? surface.materialPtr()
                                             : &ClientApp::renderSystem().missingMaterial();

                    MaterialSnapshot const &ms = actualMaterial->prepare(Rend_MapSurfaceMaterialSpec());
                    glowing = ms.glowStrength();
                }

                glowing *= glowFactor; // Global scale factor.
            }
        }

        AABoxd const &aaBox           = poly.aaBox();
        Vector3d const topLeft        = Vector3d(aaBox.minX, aaBox.arvec2[plane.isSectorFloor()? 1 : 0][VY], plane.heightSmoothed());
        Vector3d const bottomRight    = Vector3d(aaBox.maxX, aaBox.arvec2[plane.isSectorFloor()? 0 : 1][VY], plane.heightSmoothed());

        Vector2f const materialOrigin = calcFlatMaterialOrigin(subspace, plane);
        Vector2f const materialScale  = surface.materialScale();

        //blendmode_t const blendmode   = chooseFlatBlendmode(plane);
        Vector4f const ambientLight   =
                useAmbientLightFromPlane(plane)? Rend_AmbientLightColor(plane.sector())
                                               : self.lightSourceColorfIntensity();

        // Lists of projected lights/shadows that affect this geometry.
        uint lightListIdx = 0, shadowListIdx = 0;
        if(!skyMasked)
        {
            projectDynamics(subspace, surface, glowing, topLeft, bottomRight,
                            false /*do light*/, false /*do shadow*/, false /*don't sort*/,
                            lightListIdx, shadowListIdx);
        }

        WorldVBuf &vbuf = worldVBuf();

        // Map RTU configuration from prepared MaterialSnapshot(s).
        GLTextureUnit const *primaryRTU       = (!skyMasked)? &matSnapshot.unit(RTU_PRIMARY) : NULL;
        GLTextureUnit const *primaryDetailRTU = (r_detail && !skyMasked && matSnapshot.unit(RTU_PRIMARY_DETAIL).hasTexture())? &matSnapshot.unit(RTU_PRIMARY_DETAIL) : NULL;
        GLTextureUnit const *interRTU         = (!skyMasked && matSnapshot.unit(RTU_INTER).hasTexture())? &matSnapshot.unit(RTU_INTER) : NULL;
        GLTextureUnit const *interDetailRTU   = (r_detail && !skyMasked && matSnapshot.unit(RTU_INTER_DETAIL).hasTexture())? &matSnapshot.unit(RTU_INTER_DETAIL) : NULL;
        GLTextureUnit const *shineRTU         = (useShinySurfaces && !skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture())? &matSnapshot.unit(RTU_REFLECTION) : NULL;
        GLTextureUnit const *shineMaskRTU     = (useShinySurfaces && !skyMasked && matSnapshot.unit(RTU_REFLECTION).hasTexture() && matSnapshot.unit(RTU_REFLECTION_MASK).hasTexture())? &matSnapshot.unit(RTU_REFLECTION_MASK) : NULL;

        bool useLights = false, useShadows = false;

        DGLuint modTex = 0;
        Vector2f modTexSt[2]; // [topLeft, bottomRight]
        Vector3f modColor;

        bool const skyMaskedMaterial = (skyMasked || (matSnapshot.material().isSkyMasked()));
        if(!skyMaskedMaterial && glowing < 1)
        {
            useLights  = (lightListIdx  > 0);
            useShadows = (shadowListIdx > 0);

            // If multitexturing is enabled and there is at least one dynlight
            // affecting this surface, grab the parameters needed to draw it.
            if(useLights && Rend_IsMTexLights())
            {
                TexProjection *dyn = 0;
                Rend_IterateProjectionList(lightListIdx, RIT_FirstDynlightIterator, &dyn);

                modTex      = dyn->texture;
                modColor    = dyn->color;
                modTexSt[0] = dyn->topLeft;
                modTexSt[1] = dyn->bottomRight;
            }
        }

        ClockDirection const direction = (plane.isSectorCeiling())? Anticlockwise : Clockwise;
        HEdge *fanBase                 = subspace.fanBase();
        WorldVBuf::Index const fanSize = poly.hedgeCount() + (!fanBase? 2 : 0);

        WorldVBuf::Indices indices(fanSize);

        vbuf.reserveElements(fanSize, indices);

        WorldVBuf::Index n = 0;
        if(!fanBase)
        {
            vbuf[indices[n]].pos = Vector3f(poly.center(), plane.heightSmoothed());
            n++;
        }

        // Add the vertices for each hedge.
        HEdge *base  = fanBase? fanBase : poly.hedge();
        HEdge *hedge = base;
        do
        {
            vbuf[indices[n]].pos = Vector3f(hedge->origin(), plane.heightSmoothed());
            n++;
        } while((hedge = &hedge->neighbor(direction)) != base);

        // The last vertex is always equal to the first.
        if(!fanBase)
        {
            vbuf[indices[n]].pos = Vector3f(poly.hedge()->origin(), plane.heightSmoothed());
        }

        WorldVBuf::Indices shineIndices;

        // ShinySurface?
        if(shineRTU)
        {
            shineIndices.resize(fanSize);
            vbuf.reserveElements(fanSize, shineIndices);
            for(WorldVBuf::Index i = 0; i < fanSize; ++i)
            {
                vbuf[shineIndices[i]].pos = vbuf[indices[i]].pos;
            }
        }

        for(WorldVBuf::Index i = 0; i < fanSize; ++i)
        {
            WorldVBuf::Type &vertex = vbuf[indices[i]];
            Vector3f const delta(vertex.pos - topLeft);

            if(primaryRTU)
            {
                vertex.texCoord[WorldVBuf::PrimaryTex] = Vector2f(delta.x, -delta.y);
            }

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
                Vector2f const pdimensions = bottomRight.xy() - topLeft.xy();

                vertex.texCoord[WorldVBuf::ModTex] =
                    Vector2f(((bottomRight.x - vertex.pos.x) / pdimensions.x * modTexSt[0].x) + (delta.x / pdimensions.x  * modTexSt[1].x)
                             ,
                             ((bottomRight.y - vertex.pos.y) / pdimensions.y * modTexSt[0].y) + (delta.y / pdimensions.y * modTexSt[1].y));
            }
        }

        // Light this polygon.
        if(!skyMaskedMaterial)
        {
            if(levelFullBright || !(glowing < 1))
            {
                // Uniform color. Apply to all vertices.
                float ll = de::clamp(0.f, ambientLight.w + (levelFullBright? 1 : glowing), 1.f);
                for(WorldVBuf::Index i = 0; i < fanSize; ++i)
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
                    Shard &shard = self.shard(subspace, plane.indexInSector());

                    // Apply the ambient light term from the grid (if available).
                    if(map.hasLightGrid())
                    {
                        for(WorldVBuf::Index i = 0; i < fanSize; ++i)
                        {
                            WorldVBuf::Type &vertex = vbuf[indices[i]];
                            vertex.rgba = map.lightGrid().evaluate(vertex.pos);
                        }
                    }

                    // Apply bias light source contributions.
                    shard.lightWithBiasSources(indices.data(), surface.tangentMatrix(),
                                               map.biasCurrentTime());

                    // Apply surface glow.
                    if(glowing > 0)
                    {
                        Vector4f const glow(glowing, glowing, glowing, 0);
                        for(WorldVBuf::Index i = 0; i < fanSize; ++i)
                        {
                            vbuf[indices[i]].rgba += glow;
                        }
                    }

                    // Apply light range compression and clamparm.
                    for(WorldVBuf::Index i = 0; i < fanSize; ++i)
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
                    float lightlevel = de::clamp(0.f, ambientLight.w + glowing, 1.f);

                    // Calculate the color for each vertex, blended with plane color?
                    Vector3f const &surfaceColor = surface.tintColor();
                    if(surfaceColor.x < 1 || surfaceColor.y < 1 || surfaceColor.z < 1)
                    {
                        Vector3f finalColor = ambientLight.xyz() * surfaceColor;
                        Rend_LightVertices(vbuf, fanSize, indices.data(), lightlevel, finalColor);
                    }
                    else
                    {
                        Rend_LightVertices(vbuf, fanSize, indices.data(), lightlevel, ambientLight.xyz());
                    }
                }

                // Apply torch light?
                if(viewPlayer->shared.fixedColorMap)
                {
                    for(WorldVBuf::Index i = 0; i < fanSize; ++i)
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
                for(WorldVBuf::Index i = 0; i < fanSize; ++i)
                {
                    Vector4f &color = vbuf[shineIndices[i]].rgba;
                    color = Vector3f(vbuf[indices[i]].rgba).max(minColor);
                    color.w = shineRTU->opacity;
                }
            }

            // Apply uniform alpha (overwritting luminance factors).
            for(WorldVBuf::Index i = 0; i < fanSize; ++i)
            {
                vbuf[indices[i]].rgba.w = opacity;
            }
        }
        else
        {
            // Uniform color. Apply to all vertices.
            for(WorldVBuf::Index i = 0; i < fanSize; ++i)
            {
                vbuf[indices[i]].rgba = Vector4f(1, 1, 1, 1);
            }
        }

        if(useLights || useShadows)
        {
            // Geometry which is lit by dynamic lights may need to be drawn
            // differently than non-lit surfaces depending on the renderer
            // configuration. If the averaged vertex light level is nearly
            // fully saturated we can skip adding extra lights.
            float avgLightlevel = 0;
            for(WorldVBuf::Index i = 0; i < fanSize; ++i)
            {
                WorldVBuf::Type &vertex = vbuf[indices[i]];
                avgLightlevel += vertex.rgba.x;
                avgLightlevel += vertex.rgba.y;
                avgLightlevel += vertex.rgba.z;
            }
            avgLightlevel /= fanSize * 3;

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
            // Project dynamic lights to shards.
            splinterdynlight_params_t parm; de::zap(parm);

            parm.inst        = this;
            parm.subspace    = &subspace;
            parm.vertCount   = fanSize;
            parm.indices     = indices.data();
            parm.topLeft     = &topLeft;
            parm.bottomRight = &bottomRight;

            hasDynlights = (0 != splinterAllDynlights(lightListIdx, parm));
        }

        if(useShadows)
        {
            // Project dynamic shadows to shards.
            splinterdynshadow_params_t parm; de::zap(parm);

            parm.inst        = this;
            parm.subspace    = &subspace;
            parm.vertCount   = fanSize;
            parm.indices     = indices.data();
            parm.topLeft     = &topLeft;
            parm.bottomRight = &bottomRight;

            splinterAllDynshadows(shadowListIdx, parm);
        }

        if(!skyMasked)
        {
            DrawListSpec listSpec((modTex || hasDynlights)? LitGeom : UnlitGeom);
            if(primaryRTU)
            {
                listSpec.texunits[TU_PRIMARY] = *primaryRTU;
                listSpec.texunits[TU_PRIMARY].offset += materialOrigin;
                listSpec.texunits[TU_PRIMARY].scale  *= materialScale;
                listSpec.texunits[TU_PRIMARY].offset *= materialScale;
            }
            if(primaryDetailRTU)
            {
                listSpec.texunits[TU_PRIMARY_DETAIL] = *primaryDetailRTU;
                listSpec.texunits[TU_PRIMARY_DETAIL].offset += materialOrigin;
            }
            if(interRTU)
            {
                listSpec.texunits[TU_INTER] = *interRTU;
                listSpec.texunits[TU_INTER].offset += materialOrigin;
                listSpec.texunits[TU_INTER].scale  *= materialScale;
                listSpec.texunits[TU_INTER].offset *= materialScale;
            }
            if(interDetailRTU)
            {
                listSpec.texunits[TU_INTER_DETAIL] = *interDetailRTU;
                listSpec.texunits[TU_INTER_DETAIL].offset += materialOrigin;
            }

            Shard::Geom *shard = new Shard::Geom(listSpec, BM_NORMAL, modTex, modColor, hasDynlights);
            shard->indices = indices;
            subspace.shards() << shard;

            makeListPrimitive(*shard, gl::TriangleFan, fanSize,
                              listSpec.unit(TU_PRIMARY       ).scale,
                              listSpec.unit(TU_PRIMARY       ).offset,
                              listSpec.unit(TU_PRIMARY_DETAIL).scale,
                              listSpec.unit(TU_PRIMARY_DETAIL).offset);

            if(shineRTU)
            {
                DrawListSpec shineListSpec(ShineGeom);
                shineListSpec.texunits[TU_PRIMARY] = *shineRTU;
                if(shineMaskRTU)
                {
                    shineListSpec.texunits[TU_INTER] = *shineMaskRTU;
                    shineListSpec.texunits[TU_INTER].offset += materialOrigin;
                    shineListSpec.texunits[TU_INTER].scale  *= materialScale;
                    shineListSpec.texunits[TU_INTER].offset *= materialScale;
                }

                Shard::Geom *shineShard = new Shard::Geom(shineListSpec, matSnapshot.shineBlendMode());
                shineShard->indices = shineIndices;
                subspace.shards() << shineShard;

                makeListPrimitive(*shineShard, gl::TriangleFan, fanSize,
                                  shineListSpec.unit(TU_INTER         ).scale,
                                  shineListSpec.unit(TU_INTER         ).offset,
                                  shineListSpec.unit(TU_PRIMARY_DETAIL).scale,
                                  shineListSpec.unit(TU_PRIMARY_DETAIL).offset);
            }
        }
        else // Sky-masked.
        {
            Shard::Geom *shard = new Shard::Geom(DrawListSpec(SkyMaskGeom));
            shard->indices = indices;
            subspace.shards() << shard;

            makeListPrimitive(*shard, gl::TriangleFan, fanSize);
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
