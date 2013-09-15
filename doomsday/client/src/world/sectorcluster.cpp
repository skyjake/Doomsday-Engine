/** @file sectorcluster.cpp World map sector cluster.
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

#include <QRect>
#include <QtAlgorithms>

#include "Face"

#include "BspLeaf"
#include "Line"
#include "Plane"
#include "Surface"
#include "world/map.h"
#include "world/p_object.h"
#include "world/p_players.h"

#ifdef __CLIENT__
#  include "render/rend_main.h" // useBias
#endif

#include "world/sector.h"

using namespace de;

namespace internal
{
    /// Classification flags:
    enum ClusterFlag
    {
        NeverMapped        = 0x1,
        AllSelfRef         = 0x2
    };

    Q_DECLARE_FLAGS(ClusterFlags, ClusterFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(ClusterFlags)
}

using namespace internal;

static QRectF qrectFromAABox(AABoxd const &aaBox)
{
    return QRectF(QPointF(aaBox.minX, aaBox.maxY), QPointF(aaBox.maxX, aaBox.minY));
}

DENG2_PIMPL(Sector::Cluster),
DENG2_OBSERVES(Sector::Cluster, Deletion),
DENG2_OBSERVES(Plane, Deletion),
DENG2_OBSERVES(Plane, HeightChange)
#ifdef __CLIENT__
, DENG2_OBSERVES(Plane, HeightSmoothedChange)
#endif
{
    bool needClassify; ///< @c true= (Re)classification is necessary.
    ClusterFlags flags;
    BspLeafs bspLeafs;
    QScopedPointer<AABoxd> aaBox;

    Cluster *mappedVisFloor;
    Cluster *mappedVisCeiling;

#ifdef __CLIENT__
    /// BSP leafs in the neighborhood effecting environmental audio characteristics.
    typedef QSet<BspLeaf *> ReverbBspLeafs;
    ReverbBspLeafs reverbBspLeafs;

    /// Final environmental audio characteristics.
    AudioEnvironmentFactors reverb;
    bool needReverbUpdate; ///< @c true= marked for update.
#endif

    Instance(Public *i)
        : Base(i),
          needClassify(true),
          flags(0),
          mappedVisFloor(0),
          mappedVisCeiling(0)
#ifdef __CLIENT__
         ,needReverbUpdate(true)
#endif
    {
#ifdef __CLIENT__
        zap(reverb);
#endif
    }

    ~Instance()
    {
        clearMapping(Sector::Floor);
        clearMapping(Sector::Ceiling);

        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->sectorClusterBeingDeleted(self);
    }

    Cluster **mappedClusterAdr(int planeIdx)
    {
        if(planeIdx == Sector::Floor)   return &mappedVisFloor;
        if(planeIdx == Sector::Ceiling) return &mappedVisCeiling;
        return 0;
    }

    inline Plane *mappedPlane(int planeIdx)
    {
        Cluster **clusterAdr = mappedClusterAdr(planeIdx);
        if(clusterAdr && *clusterAdr)
        {
            return &(*clusterAdr)->plane(planeIdx);
        }
        return 0;
    }

    void observeCluster(Cluster *cluster, bool yes = true)
    {
        if(!cluster || cluster == thisPublic)
            return;

        if(yes)
        {
            cluster->audienceForDeletion += this;
        }
        else
        {
            cluster->audienceForDeletion -= this;
        }
    }

    void observePlane(Plane *plane, bool yes = true, bool observeHeight = true)
    {
        if(!plane) return;

        if(yes)
        {
            plane->audienceForDeletion += this;
            if(observeHeight)
            {
                plane->audienceForHeightChange += this;
#ifdef __CLIENT__
                plane->audienceForHeightSmoothedChange += this;
#endif
            }
        }
        else
        {
            plane->audienceForDeletion -= this;
            plane->audienceForHeightChange -= this;
#ifdef __CLIENT__
            plane->audienceForHeightSmoothedChange -= this;
#endif
        }
    }

    void map(int planeIdx, Cluster *newCluster, bool permanent = false)
    {
        Cluster **clusterAdr = mappedClusterAdr(planeIdx);
        if(!clusterAdr || *clusterAdr == newCluster)
            return;

        observeCluster(*clusterAdr, false);
        observePlane(mappedPlane(planeIdx), false);

        *clusterAdr = newCluster;

        observeCluster(*clusterAdr);
        observePlane(mappedPlane(planeIdx), true, !permanent);
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

        Cluster **clusterAdr = mappedClusterAdr(planeIdx);
        if(!clusterAdr || *clusterAdr == thisPublic)
            return;

        clearMapping(planeIdx);
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

            flags &= ~NeverMapped;
            flags |= AllSelfRef;
            foreach(BspLeaf *leaf, bspLeafs)
            {
                HEdge const *base  = leaf->poly().hedge();
                HEdge const *hedge = base;
                do
                {
                    if(hedge->mapElement())
                    {
                        // This edge defines a section of a map line.
                        if(hedge->twin().hasFace())
                        {
                            if(flags.testFlag(AllSelfRef))
                            {
                                BspLeaf const &backLeaf    = hedge->twin().face().mapElement()->as<BspLeaf>();
                                Cluster const *backCluster = backLeaf.hasCluster()? &backLeaf.cluster() : 0;
                                if(backCluster != thisPublic)
                                {
                                    if(!hedge->mapElement()->as<LineSideSegment>().line().isSelfReferencing())
                                    {
                                        flags &= ~AllSelfRef;
                                    }
                                }
                            }
                        }
                        else
                        {
                            // If a back geometry is missing then never map planes.
                            flags |= NeverMapped;
                            flags &= ~AllSelfRef;

                            // We're done.
                            return flags;
                        }
                    }
                } while((hedge = &hedge->next()) != base);
            }
        }

        return flags;
    }

    /**
     * @todo Redesign the implementation to avoid recursion via visPlane().
     */
    void remapVisPlanes()
    {
        // By default both planes are mapped to the parent sector.
        map(Sector::Floor, thisPublic);
        map(Sector::Ceiling, thisPublic);

        if(classification() & NeverMapped)
            return;

        // Should we permanently map planes to another cluster?
        forever
        {
            // Locate the next exterior cluster.
            Cluster *exteriorCluster = 0;
            foreach(BspLeaf *leaf, bspLeafs)
            {
                HEdge *base = leaf->poly().hedge();
                HEdge *hedge = base;
                do
                {
                    if(hedge->mapElement())
                    {
                        DENG_ASSERT(hedge->twin().hasFace());

                        if(hedge->mapElement()->as<LineSideSegment>().line().isSelfReferencing())
                        {
                            BspLeaf &backLeaf = hedge->twin().face().mapElement()->as<BspLeaf>();
                            if(backLeaf.hasCluster())
                            {
                                Cluster *otherCluster = &backLeaf.cluster();
                                if(otherCluster != thisPublic &&
                                   otherCluster->d->mappedVisFloor != thisPublic &&
                                   !(!(classification() & AllSelfRef) && (otherCluster->d->classification() & AllSelfRef)))
                                {
                                    // Remember the exterior cluster.
                                    exteriorCluster = otherCluster;
                                }
                            }
                        }
                    }
                } while((hedge = &hedge->next()) != base);
            }

            if(!exteriorCluster)
                break; // Nothing to map to.

            // Ensure we don't produce a cyclic dependency...
            Sector *finalSector = &exteriorCluster->visPlane(Floor).sector();
            if(finalSector == &self.sector())
            {
                // Must share a boundary edge.
                QRectF boundingRect = qrectFromAABox(self.aaBox());
                if(boundingRect.contains(qrectFromAABox(exteriorCluster->aaBox())))
                {
                    // The contained cluster will map to this. However, we may
                    // still need to map this one to another, so re-evaluate.
                    continue;
                }
                else
                {
                    // This cluster is contained. Remove the mapping from exterior
                    // to this thereby forcing it to be re-evaluated (however next
                    // time a different cluster will be selected from the boundary).
                    exteriorCluster->d->clearMapping(Sector::Floor);
                    exteriorCluster->d->clearMapping(Sector::Ceiling);
                }
            }

            // Setup the mapping and we're done.
            map(Sector::Floor, exteriorCluster, true /*permanently*/);
            map(Sector::Ceiling, exteriorCluster, true /*permanently*/);
            return;
        }

        // Dynamic mapping may be needed for one or more planes.
        if(!classification().testFlag(AllSelfRef))
        {
            Plane const &sectorFloor   = self.sector().floor();
            Plane const &sectorCeiling = self.sector().ceiling();

            // The sector must have open space.
            if(!(sectorCeiling.height() > sectorFloor.height()))
                return;

            // The plane must not use a sky-masked material.
            bool missingAllBottom = !sectorFloor.surface().hasSkyMaskedMaterial();
            bool missingAllTop    = !sectorCeiling.surface().hasSkyMaskedMaterial();
            if(!missingAllBottom && !missingAllTop)
                return;

            // Evaluate the boundary to determine if mapping is required.
            Cluster *exteriorCluster = 0;
            foreach(BspLeaf *leaf, bspLeafs)
            {
                HEdge *base = leaf->poly().hedge();
                HEdge *hedge = base;
                do
                {
                    if(hedge->mapElement())
                    {
                        DENG_ASSERT(hedge->twin().hasFace());

                        // Only consider non-selfref edges whose back face lies
                        // in another cluster.
                        LineSideSegment const &seg = hedge->mapElement()->as<LineSideSegment>();
                        if(!seg.line().isSelfReferencing())
                        {
                            BspLeaf const &backLeaf = hedge->twin().face().mapElement()->as<BspLeaf>();
                            if(backLeaf.hasCluster())
                            {
                                Cluster *otherCluster = &backLeaf.cluster();
                                if(otherCluster != thisPublic &&
                                   otherCluster->d->mappedVisFloor != thisPublic)
                                {
                                    LineSide const &lineSide = seg.lineSide();

                                    if(lineSide.bottom().hasMaterial() &&
                                       !lineSide.bottom().hasFixMaterial())
                                        missingAllBottom = false;

                                    if(lineSide.top().hasMaterial() &&
                                       !lineSide.top().hasFixMaterial())
                                        missingAllTop = false;

                                    if(!missingAllBottom && !missingAllTop)
                                        return;

                                    // Remember the exterior cluster.
                                    exteriorCluster = otherCluster;
                                }
                            }
                        }
                    }
                } while((hedge = &hedge->next()) != base);
            }

            if(!exteriorCluster) return;

            if(missingAllBottom &&
               exteriorCluster->visFloor().height() > sectorFloor.height())
            {
                map(Sector::Floor, exteriorCluster);
            }
            if(missingAllTop &&
               exteriorCluster->visCeiling().height() < sectorCeiling.height())
            {
                map(Sector::Ceiling, exteriorCluster);
            }
        }
    }

#ifdef __CLIENT__

    /**
     * To be called when the height changes to update the plotted decoration
     * origins for surfaces whose material offset is dependant upon this.
     */
    void markDependantSurfacesForDecorationUpdate()
    {
        if(ddMapSetup) return;

        foreach(LineSide *side, self.sector().sides())
        {
            if(side->hasSections())
            {
                side->middle().markAsNeedingDecorationUpdate();
                side->bottom().markAsNeedingDecorationUpdate();
                side->top().markAsNeedingDecorationUpdate();
            }

            if(side->back().hasSections())
            {
                LineSide &back = side->back();
                back.middle().markAsNeedingDecorationUpdate();
                back.bottom().markAsNeedingDecorationUpdate();
                back.top().markAsNeedingDecorationUpdate();
            }
        }
    }

#endif // __CLIENT__

    /// Observes Sector::Cluster Deletion.
    void sectorClusterBeingDeleted(Cluster const &cluster)
    {
        clearMapping(mappedVisFloor == &cluster? Sector::Floor : Sector::Ceiling);
    }

    /// Observes Plane Deletion.
    void planeBeingDeleted(Plane const &plane)
    {
        clearMapping(plane.indexInSector());
    }

    /// Observes Plane HeightChange.
    void planeHeightChanged(Plane &plane, coord_t oldHeight)
    {
        DENG2_UNUSED(oldHeight);

        // Check if there are any camera players in this sector. If their height
        // is now above the ceiling/below the floor they are now in the void.
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t *ddpl = &plr->shared;

            if(!ddpl->inGame || !ddpl->mo)
                continue;
            if(!ddpl->mo->bspLeaf || !ddpl->mo->bspLeaf->isDegenerate())
                continue;
            if(&ddpl->mo->bspLeaf->cluster() != thisPublic)
                continue;

            if((ddpl->flags & DDPF_CAMERA) &&
               (ddpl->mo->origin[VZ] > self.visCeiling().height() - 4 ||
                ddpl->mo->origin[VZ] < self.visFloor().height()))
            {
                ddpl->inVoid = true;
            }
        }

#ifdef __CLIENT__
        // A plane move means we must re-apply missing material fixes.
        /// @todo optimize: Defer until actually necessary.
        self.sector().fixMissingMaterialsForSides();

        // We'll need to recalculate environmental audio characteristics.
        needReverbUpdate = true;

        if(!ddMapSetup && useBias)
        {
            // Inform bias surfaces of changed geometry.
            foreach(BspLeaf *bspLeaf, bspLeafs)
            {
                bspLeaf->updateBiasAfterGeometryMove(plane.indexInSector());
            }
        }

        markDependantSurfacesForDecorationUpdate();
#endif // __CLIENT__

        // We may need to update one or both mapped planes.
        maybeInvalidateMapping(plane.indexInSector());
    }

#ifdef __CLIENT__

    /// Observes Plane HeightSmoothedChange.
    void planeHeightSmoothedChanged(Plane &plane, coord_t oldHeight)
    {
        DENG2_UNUSED(oldHeight);

        markDependantSurfacesForDecorationUpdate();

        // We may need to update one or both mapped planes.
        maybeInvalidateMapping(plane.indexInSector());
    }

    void addReverbBspLeaf(BspLeaf *bspLeaf)
    {
        // Degenerate leafs never contribute.
        if(!bspLeaf || !bspLeaf->isDegenerate())
            return;

        reverbBspLeafs.insert(bspLeaf);
    }

    static int addReverbBspLeafWorker(BspLeaf *bspLeaf, void *context)
    {
        static_cast<Instance *>(context)->addReverbBspLeaf(bspLeaf);
        return false; // Continue iteration.
    }

    /**
     * Perform environmental audio (reverb) initialization.
     *
     * Determines the BSP leafs which contribute to the environmental audio
     * characteristics. Given that BSP leafs do not change shape (on the XY plane,
     * that is), they do not move and are not created/destroyed once the map has
     * been loaded; this step can be pre-processed.
     *
     * @pre The Map's BSP leaf blockmap must be ready for use.
     */
    void findReverbBspLeafs()
    {
        AABoxd affectionBounds = self.aaBox();
        affectionBounds.minX -= 128;
        affectionBounds.minY -= 128;
        affectionBounds.maxX += 128;
        affectionBounds.maxY += 128;

        // Link all non-degenerate BspLeafs whose axis-aligned bounding box intersects
        // with the affection bounds to the reverb set.
        self.sector().map().bspLeafsBoxIterator(affectionBounds, 0, addReverbBspLeafWorker, this);
    }

    /**
     * Recalculate environmental audio (reverb) for the sector.
     */
    void updateReverb()
    {
        // Need to initialize?
        if(reverbBspLeafs.isEmpty())
        {
            findReverbBspLeafs();
        }

        needReverbUpdate = false;

        uint spaceVolume = int((self.visCeiling().height() - self.visFloor().height())
                               * self.roughArea());

        reverb[SRD_SPACE] = reverb[SRD_VOLUME] =
            reverb[SRD_DECAY] = reverb[SRD_DAMPING] = 0;

        foreach(BspLeaf *bspLeaf, reverbBspLeafs)
        {
            if(bspLeaf->updateReverb())
            {
                BspLeaf::AudioEnvironmentFactors const &leafReverb = bspLeaf->reverb();

                reverb[SRD_SPACE]   += leafReverb[SRD_SPACE];

                reverb[SRD_VOLUME]  += leafReverb[SRD_VOLUME]  / 255.0f * leafReverb[SRD_SPACE];
                reverb[SRD_DECAY]   += leafReverb[SRD_DECAY]   / 255.0f * leafReverb[SRD_SPACE];
                reverb[SRD_DAMPING] += leafReverb[SRD_DAMPING] / 255.0f * leafReverb[SRD_SPACE];
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

#endif // __CLIENT__
};

Sector::Cluster::Cluster(BspLeafs const &bspLeafs) : d(new Instance(this))
{
    d->bspLeafs.append(bspLeafs);
    foreach(BspLeaf *bspLeaf, bspLeafs)
    {
        // Attribute the BSP leaf to the cluster.
        bspLeaf->setCluster(this);
    }
}

Sector &Sector::Cluster::sector() const
{
    return d->bspLeafs.first()->parent().as<Sector>();
}

Plane &Sector::Cluster::plane(int planeIndex) const
{
    // Physical planes are never mapped.
    return sector().plane(planeIndex);
}

Plane &Sector::Cluster::visPlane(int planeIndex) const
{
    if(planeIndex >= Floor && planeIndex <= Ceiling)
    {
        // Time to remap the planes?
        if(!d->mappedVisFloor)
        {
            d->remapVisPlanes();
        }

        /// @todo Cache this result.
        Cluster *mappedCluster = (planeIndex == Ceiling? d->mappedVisCeiling : d->mappedVisFloor);
        if(mappedCluster != this)
        {
            return mappedCluster->visPlane(planeIndex);
        }
    }
    // Not mapped.
    return sector().plane(planeIndex);
}

AABoxd const &Sector::Cluster::aaBox() const
{
    // If the cluster is comprised of a single BSP leaf we can use the bounding
    // box of the leaf's geometry directly.
    if(d->bspLeafs.count() == 1)
    {
        return d->bspLeafs.first()->poly().aaBox();
    }

    // Time to determine bounds?
    if(d->aaBox.isNull())
    {
        // Unite the geometry bounding boxes of all BSP leafs in the cluster.
        foreach(BspLeaf *leaf, d->bspLeafs)
        {
            AABoxd const &leafAABox = leaf->poly().aaBox();
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

Sector::Cluster::BspLeafs const &Sector::Cluster::bspLeafs() const
{
    return d->bspLeafs;
}

#ifdef __CLIENT__

coord_t Sector::Cluster::roughArea() const
{
    AABoxd const &bounds = aaBox();
    return (bounds.maxX - bounds.minX) * (bounds.maxY - bounds.minY);
}

void Sector::Cluster::markReverbDirty(bool yes)
{
    d->needReverbUpdate = yes;
}

AudioEnvironmentFactors const &Sector::Cluster::reverb() const
{
    // Perform any scheduled update now.
    if(d->needReverbUpdate)
    {
        d->updateReverb();
    }
    return d->reverb;
}

#endif // __CLIENT__
