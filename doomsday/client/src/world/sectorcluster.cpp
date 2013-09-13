/** @file sectorcluster.cpp World map sector cluster.
 *
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "world/sector.h"

using namespace de;

static QRectF qrectFromAABox(AABoxd const &aaBox)
{
    return QRectF(QPointF(aaBox.minX, aaBox.maxY), QPointF(aaBox.maxX, aaBox.minY));
}

DENG2_PIMPL(Sector::Cluster)
{
    bool needClassify; ///< @c true= (Re)classification is necessary.
    Flags flags;
    BspLeafs bspLeafs;
    QScopedPointer<AABoxd> aaBox;

    Cluster *mappedVisFloor;
    Cluster *mappedVisCeiling;

    Instance(Public *i)
        : Base(i),
          needClassify(true),
          flags(0),
          mappedVisFloor(0),
          mappedVisCeiling(0)
    {}

    /**
     * (Re)classify the cluster by examining the boundary edges.
     */
    void classify()
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
                        return;
                    }
                }
            } while((hedge = &hedge->next()) != base);
        }
    }

    /**
     * @todo Redesign the implementation to avoid recursion via visPlane().
     */
    void remapVisPlanes()
    {
        if(needClassify)
        {
            classify();
        }

        // By default both planes are mapped to the parent sector.
        mappedVisFloor   = thisPublic;
        mappedVisCeiling = thisPublic;

        // Should we permanently map planes to another cluster?
        if(flags.testFlag(NeverMapped))
            return;

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
                                   !(!(flags.testFlag(AllSelfRef)) && (otherCluster->flags().testFlag(AllSelfRef))))
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
                    // The contained cluster will map to this. However, we may still
                    // need to map this one to another, so re-evaluate.
                    continue;
                }
                else
                {
                    // This cluster is contained. Remove the mapping from exterior
                    // to this thereby forcing it to be re-evaluated (however next
                    // time a different cluster will be selected from the boundary).
                    exteriorCluster->d->mappedVisFloor =
                        exteriorCluster->d->mappedVisCeiling = 0;
                }
            }

            // Setup the mapping and we're done.
            mappedVisFloor   = exteriorCluster;
            mappedVisCeiling = exteriorCluster;
            break;
        }

        // No permanent mapping?
        if(mappedVisFloor == thisPublic && !(flags & AllSelfRef))
        {
            // Dynamic mapping may be needed for one or more planes.
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

                        // Only consider non-selfref edges whose back face lies in
                        // another cluster.
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

            if(exteriorCluster)
            {
                if(missingAllBottom && exteriorCluster->visPlane(Floor).height() > sectorFloor.height())
                {
                    mappedVisFloor = exteriorCluster;
                }
                if(missingAllTop && exteriorCluster->visPlane(Ceiling).height() < sectorCeiling.height())
                {
                    mappedVisCeiling = exteriorCluster;
                }
            }
        }
    }
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

Sector::Cluster::Flags Sector::Cluster::flags() const
{
    if(d->needClassify)
    {
        d->classify();
    }
    return d->flags;
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
