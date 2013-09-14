/** @file sector.h World map sector.
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

#include <QList>
#include <QtAlgorithms>

#include <de/Log>

#include "dd_main.h" // App_Materials(), verbose

#include "Face"
#include "BspLeaf"
#include "Line"
#include "Plane"

#include "world/map.h"
#include "world/maputil.h"
#include "world/p_object.h"
#include "world/p_players.h"

#ifdef __CLIENT__
#  include "render/rend_main.h" // useBias
#endif

#include "world/sector.h"

using namespace de;

#ifdef __CLIENT__
typedef QSet<BspLeaf *> ReverbBspLeafs;
#endif

DENG2_PIMPL(Sector),
DENG2_OBSERVES(Plane, HeightChange)
#ifdef __CLIENT__
, DENG2_OBSERVES(Plane, HeightSmoothedChange)
#endif
{
    /// Bounding box for the whole sector (all clusters).
    AABoxd aaBox;
    bool needAABoxUpdate; ///< @c true= marked for update.

    SoundEmitter emitter; ///< Head of the sound emitter chain.

    Planes planes;        ///< All owned planes.
    Sides sides;          ///< All referencing line sides (not owned).
    Clusters clusters;    ///< All owned BSP leaf clusters.

    float lightLevel;     ///< Ambient light level.
    Vector3f lightColor;  ///< Ambient light color.

    int validCount;

#ifdef __CLIENT__
    /// Rough approximation of sector area (map units squared).
    coord_t roughArea;
    bool needRoughAreaUpdate; /// @c true= marked for update.

    /// Ambient lighting data for the bias lighting model.
    LightGridData lightGridData;

    /// All BSP leafs which effect environmental audio characteristics.
    ReverbBspLeafs reverbBspLeafs;

    /// Final environmental audio characteristics.
    AudioEnvironmentFactors reverb;
    bool needReverbUpdate; ///< @c true= marked for update.
#endif

    Instance(Public *i, float lightLevel, Vector3f const &lightColor)
        : Base(i),
          needAABoxUpdate(false), // No BSP leafs thus no geometry.
          lightLevel(lightLevel),
          lightColor(lightColor),
          validCount(0)
#ifdef __CLIENT__
         ,roughArea(0),
          needRoughAreaUpdate(false), // No BSP leafs thus zero area.
          needReverbUpdate(true)
#endif
    {
        zap(emitter);
#ifdef __CLIENT__
        zap(lightGridData);
        zap(reverb);
#endif
    }

    ~Instance()
    {
        qDeleteAll(clusters);
        qDeleteAll(planes);
    }

    void notifyLightLevelChanged(float oldLightLevel)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(LightLevelChange, i)
        {
            i->sectorLightLevelChanged(self, oldLightLevel);
        }
    }

    void notifyLightColorChanged(Vector3f const &oldLightColor, int changedComponents)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(LightColorChange, i)
        {
            i->sectorLightColorChanged(self, oldLightColor, changedComponents);
        }
    }

    void notifyLightColorChanged(Vector3f const &oldLightColor)
    {
        // Predetermine which components have changed.
        int changedComponents = 0;
        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(lightColor[i], oldLightColor[i]))
                changedComponents |= (1 << i);
        }
        notifyLightColorChanged(oldLightColor, changedComponents);
    }

    /**
     * Update the axis-aligned bounding box in the map coordinate space to
     * encompass the geometry of all BSP leaf clusters of the sector.
     */
    void updateAABox()
    {
        needAABoxUpdate = false;

        aaBox.clear();
        bool haveGeometry = false;
        foreach(Cluster *cluster, clusters)
        {
            if(haveGeometry)
            {
                V2d_UniteBox(aaBox.arvec2, cluster->aaBox().arvec2);
            }
            else
            {
                aaBox = cluster->aaBox();
                haveGeometry = true;
            }
        }

        // The XY origin of our sound emitter can now be updated as the center
        // point of the sector geometry is now known.
        if(haveGeometry)
        {
            emitter.origin[VX] = (aaBox.minX + aaBox.maxX) / 2;
            emitter.origin[VY] = (aaBox.minY + aaBox.maxY) / 2;
        }
        else
        {
            emitter.origin[VX] = emitter.origin[VY] = 0;
        }
    }

#ifdef __CLIENT__
    void updateRoughArea()
    {
        needRoughAreaUpdate = false;

        roughArea = 0;
        foreach(Cluster *cluster, clusters)
        {
            AABoxd const &clusterAABox = cluster->aaBox();
            roughArea += (clusterAABox.maxX - clusterAABox.minX) *
                         (clusterAABox.maxY - clusterAABox.minY);
        }
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
     * Recalculate environmental audio (reverb) for the sector.
     */
    void updateReverb()
    {
        needReverbUpdate = false;

        // Sectors with no referencing lines need no reverb.
        if(sides.isEmpty())
            return;

        uint spaceVolume = int((self.ceiling().height() - self.floor().height()) * self.roughArea());

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

        if(self.ceilingSurface().hasSkyMaskedMaterial() ||
           self.floorSurface().hasSkyMaskedMaterial())
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

    /**
     * To be called when the height changes to update the plotted decoration
     * origins for surfaces whose material offset is dependant upon this.
     */
    void markDependantSurfacesForDecorationUpdate()
    {
        if(ddMapSetup) return;

        foreach(LineSide *side, self.sides())
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

    // Observes Plane HeightChange.
    void planeHeightChanged(Plane &plane, coord_t oldHeight)
    {
        DENG2_UNUSED(oldHeight);

        // Update the z-height origin of our sound emitter right away.
        emitter.origin[VZ] = (self.floor().height() + self.ceiling().height()) / 2;

        // Update the sound emitter origins for all dependent surfaces.
        foreach(LineSide *side, sides)
        {
            side->updateAllSoundEmitterOrigins();
            side->back().updateAllSoundEmitterOrigins();
        }

        // Check if there are any camera players in this sector. If their height
        // is now above the ceiling/below the floor they are now in the void.
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t *plr = &ddPlayers[i];
            ddplayer_t *ddpl = &plr->shared;

            if(!ddpl->inGame || !ddpl->mo || !ddpl->mo->bspLeaf)
                continue;

            if((ddpl->flags & DDPF_CAMERA) && ddpl->mo->bspLeaf->sectorPtr() == &self &&
               (ddpl->mo->origin[VZ] > self.ceiling().height() - 4 ||
                ddpl->mo->origin[VZ] < self.floor().height()))
            {
                ddpl->inVoid = true;
            }
        }

#ifdef __CLIENT__
        // A plane move means we must re-apply missing material fixes.
        /// @todo optimize: Defer until actually necessary.
        self.fixMissingMaterialsForSides();

        // We'll need to recalculate environmental audio characteristics.
        needReverbUpdate = true;

        if(!ddMapSetup && useBias)
        {
            // Inform bias surfaces of changed geometry.
            foreach(Cluster *cluster, clusters)
            foreach(BspLeaf *bspLeaf, cluster->bspLeafs())
            {
                bspLeaf->updateBiasAfterGeometryMove(plane.indexInSector());
            }
        }

        markDependantSurfacesForDecorationUpdate();
#endif // __CLIENT__
    }

#ifdef __CLIENT__
    /// Observes Plane HeightSmoothedChange
    void planeHeightSmoothedChanged(Plane &plane, coord_t oldHeight)
    {
        DENG2_UNUSED(oldHeight);
        markDependantSurfacesForDecorationUpdate();
    }
#endif
};

Sector::Sector(float lightLevel, Vector3f const &lightColor)
    : MapElement(DMU_SECTOR), d(new Instance(this, lightLevel, lightColor))
{
    _mobjList = 0;
}

float Sector::lightLevel() const
{
    return d->lightLevel;
}

void Sector::setLightLevel(float newLightLevel)
{
    newLightLevel = de::clamp(0.f, newLightLevel, 1.f);

    if(!de::fequal(d->lightLevel, newLightLevel))
    {
        float oldLightLevel = d->lightLevel;
        d->lightLevel = newLightLevel;

        // Notify interested parties of the change.
        d->notifyLightLevelChanged(oldLightLevel);
    }
}

Vector3f const &Sector::lightColor() const
{
    return d->lightColor;
}

void Sector::setLightColor(Vector3f const &newLightColor)
{
    Vector3f newColorClamped(de::clamp(0.f, newLightColor.x, 1.f),
                             de::clamp(0.f, newLightColor.y, 1.f),
                             de::clamp(0.f, newLightColor.z, 1.f));

    if(d->lightColor != newColorClamped)
    {
        Vector3f oldLightColor = d->lightColor;
        d->lightColor = newColorClamped;

        // Notify interested parties of the change.
        d->notifyLightColorChanged(oldLightColor);
    }
}

void Sector::setLightColorComponent(int component, float newStrength)
{
    DENG_ASSERT(component >= 0 && component < 3);
    newStrength = de::clamp(0.f, newStrength, 1.f);
    if(!de::fequal(d->lightColor[component], newStrength))
    {
        Vector3f oldLightColor = d->lightColor;
        d->lightColor[component] = newStrength;

        // Notify interested parties of the change.
        d->notifyLightColorChanged(oldLightColor, (1 << component));
    }
}

struct mobj_s *Sector::firstMobj() const
{
    return _mobjList;
}

SoundEmitter &Sector::soundEmitter()
{
    // The origin is determined by the axis-aligned bounding box, so perform
    // any scheduled update now.
    if(d->needAABoxUpdate)
    {
        d->updateAABox();
    }
    return d->emitter;
}

SoundEmitter const &Sector::soundEmitter() const
{
    return const_cast<SoundEmitter const &>(const_cast<Sector &>(*this).soundEmitter());
}

int Sector::validCount() const
{
    return d->validCount;
}

void Sector::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

Plane &Sector::plane(int planeIndex)
{
    if(planeIndex >= 0 && planeIndex < d->planes.count())
    {
        return *d->planes[planeIndex];
    }
    /// @throw MissingPlaneError The referenced plane does not exist.
    throw MissingPlaneError("Sector::plane", QString("Missing plane %1").arg(planeIndex));
}

Plane const &Sector::plane(int planeIndex) const
{
    return const_cast<Plane const &>(const_cast<Sector &>(*this).plane(planeIndex));
}

Sector::Sides const &Sector::sides() const
{
    return d->sides;
}

void Sector::buildSides()
{
    d->sides.clear();

#ifdef DENG2_QT_4_7_OR_NEWER
    int count = 0;
    foreach(Line *line, map().lines())
    {
        if(line->frontSectorPtr() == this || line->backSectorPtr()  == this)
            ++count;
    }

    if(!count) return;

    d->sides.reserve(count);
#endif

    foreach(Line *line, map().lines())
    {
        if(line->frontSectorPtr() == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line->front());
        }
        else if(line->backSectorPtr()  == this)
        {
            // Ownership of the side is not given to the sector.
            d->sides.append(&line->back());
        }
    }
}

Plane *Sector::addPlane(Vector3f const &normal, coord_t height)
{
    Plane *plane = new Plane(*this, normal, height);

    plane->setIndexInSector(d->planes.count());
    d->planes.append(plane);

    if(plane->isSectorFloor() || plane->isSectorCeiling())
    {
        // We want notification of height changes so that we can relay and/or
        // update other components (e.g., BSP leafs) accordingly.
        plane->audienceForHeightChange += d;
#ifdef __CLIENT__
        plane->audienceForHeightSmoothedChange += d;
#endif
    }

    // Once both floor and ceiling are known we can determine the z-height origin
    // of our sound emitter.
    /// @todo fixme: Assume planes are defined in order.
    if(planeCount() == 2)
    {
        d->emitter.origin[VZ] = (floor().height() + ceiling().height()) / 2;
    }

    return plane;
}

Sector::Planes const &Sector::planes() const
{
    return d->planes;
}

bool Sector::hasSkyMaskedPlane() const
{
    foreach(Plane *plane, d->planes)
    {
        if(plane->surface().hasSkyMaskedMaterial())
            return true;
    }
    return false;
}

Sector::Clusters const &Sector::clusters() const
{
    return d->clusters;
}

void Sector::buildClusters()
{
    d->clusters.clear();

    // We'll need to recalculate the axis-aligned bounding box.
    d->needAABoxUpdate = true;
#ifdef __CLIENT__
    // ...and the rough area approximation.
    d->needRoughAreaUpdate = true;
#endif

    typedef QList<BspLeaf *> BspLeafs;
    typedef QList<BspLeafs> BspLeafSets;
    BspLeafSets bspLeafSets;

    /*
     * Separate the BSP leafs into edge-adjacency clusters. We'll do this by
     * starting with a set per BSP leaf and then keep merging these sets until
     * no more shared edges are found.
     */
    foreach(BspLeaf *bspLeaf, map().bspLeafs())
    {
        if(&bspLeaf->parent().as<Sector>() != this)
            continue;

        // Degenerate BSP leafs are excluded (no geometry).
        if(bspLeaf->isDegenerate())
            continue;

        bspLeafSets.append(BspLeafs());
        bspLeafSets.last().append(bspLeaf);
    }

    if(bspLeafSets.isEmpty()) return;

    // Merge sets whose BSP leafs share a common edge.
    while(bspLeafSets.count() > 1)
    {
        bool didMerge = false;
        for(int i = 0; i < bspLeafSets.count(); ++i)
        for(int k = 0; k < bspLeafSets.count(); ++k)
        {
            if(i == k) continue;

            foreach(BspLeaf *leaf, bspLeafSets[i])
            {
                HEdge *baseHEdge = leaf->poly().hedge();
                HEdge *hedge = baseHEdge;
                do
                {
                    if(hedge->twin().hasFace())
                    {
                        BspLeaf &otherLeaf = hedge->twin().face().mapElement()->as<BspLeaf>();
                        if(&otherLeaf.parent() == this &&
                           bspLeafSets[k].contains(&otherLeaf))
                        {
                            // Merge k into i.
                            bspLeafSets[i].append(bspLeafSets[k]);
                            bspLeafSets.removeAt(k);

                            // Compare the next pair.
                            if(i >= k) i -= 1;
                            k -= 1;

                            // We'll need to repeat in any case.
                            didMerge = true;
                            break;
                        }
                    }
                } while((hedge = &hedge->next()) != baseHEdge);

                if(didMerge) break;
            }
        }

        if(!didMerge) break;
    }
    // Clustering complete.

    // Build clusters.
    foreach(BspLeafs const &bspLeafSet, bspLeafSets)
    {
        // BSP leaf ownership is not given to the cluster.
        d->clusters.append(new Cluster(bspLeafSet));
    }
}

AABoxd const &Sector::aaBox() const
{
    // Perform any scheduled update now.
    if(d->needAABoxUpdate)
    {
        d->updateAABox();
    }
    return d->aaBox;
}

static void linkSoundEmitter(SoundEmitter &root, SoundEmitter &newEmitter)
{
    // The sector's base is always root of the chain, so link the other after it.
    newEmitter.thinker.prev = &root.thinker;
    newEmitter.thinker.next = root.thinker.next;
    if(newEmitter.thinker.next)
        newEmitter.thinker.next->prev = &newEmitter.thinker;
    root.thinker.next = &newEmitter.thinker;
}

void Sector::chainSoundEmitters()
{
    SoundEmitter &root = d->emitter;

    // Clear the root of the emitter chain.
    root.thinker.next = root.thinker.prev = 0;

    // Link plane surface emitters:
    foreach(Plane *plane, d->planes)
    {
        linkSoundEmitter(root, plane->soundEmitter());
    }

    // Link wall surface emitters:
    foreach(LineSide *side, d->sides)
    {
        if(side->hasSections())
        {
            linkSoundEmitter(root, side->middleSoundEmitter());
            linkSoundEmitter(root, side->bottomSoundEmitter());
            linkSoundEmitter(root, side->topSoundEmitter());
        }
        if(side->line().isSelfReferencing() && side->back().hasSections())
        {
            LineSide &back = side->back();
            linkSoundEmitter(root, back.middleSoundEmitter());
            linkSoundEmitter(root, back.bottomSoundEmitter());
            linkSoundEmitter(root, back.topSoundEmitter());
        }
    }
}

#ifdef __CLIENT__

Sector::LightGridData &Sector::lightGridData()
{
    return d->lightGridData;
}

/**
 * Determine the BSP leafs which contribute to the sector's environmental audio
 * characteristics. Given that BSP leafs do not change shape (on the XY plane,
 * that is), they do not move and are not created/destroyed once the map has been
 * loaded; this step can be pre-processed.
 */
void Sector::initReverb()
{
    d->reverbBspLeafs.clear();

    // A sector with no referencing line needs no reverb.
    if(!sideCount()) return;

    AABoxd affectionBounds = aaBox();
    affectionBounds.minX -= 128;
    affectionBounds.minY -= 128;
    affectionBounds.maxX += 128;
    affectionBounds.maxY += 128;

    // LOG_DEBUG("Finding reverb BSP leafs for sector %u (min:%s  max:%s)")
    //    << map().sectorIndex(this)
    //    << Vector2d(affectionBounds.min).asText()
    //    << Vector2d(affectionBounds.max).asText();

    // Link all non-degenerate BspLeafs whose axis-aligned bounding box intersects
    // with the affection bounds to the reverb set.
    map().bspLeafsBoxIterator(affectionBounds, 0, Instance::addReverbBspLeafWorker, d);

    // We still need to update the final characteristics.
    d->needReverbUpdate = true;
}

void Sector::markReverbDirty(bool yes)
{
    d->needReverbUpdate = yes;
}

AudioEnvironmentFactors const &Sector::reverb() const
{
    // Perform any scheduled update now.
    if(d->needReverbUpdate)
    {
        d->updateReverb();
    }
    return d->reverb;
}

coord_t Sector::roughArea() const
{
    // Perform any scheduled update now.
    if(d->needRoughAreaUpdate)
    {
        d->updateRoughArea();
    }
    return d->roughArea;
}

/**
 * Given a side section, look at the neighbouring surfaces and pick the
 * best choice of material used on those surfaces to be applied to "this"
 * surface.
 *
 * Material on back neighbour plane has priority.
 * Non-animated materials are preferred.
 * Sky materials are ignored.
 */
static Material *chooseFixMaterial(LineSide &side, int section)
{
    Material *choice1 = 0, *choice2 = 0;

    Sector *frontSec = side.sectorPtr();
    Sector *backSec  = side.back().sectorPtr();

    if(backSec)
    {
        // Our first choice is a material in the other sector.
        if(section == LineSide::Bottom)
        {
            if(frontSec->floor().height() < backSec->floor().height())
            {
                choice1 = backSec->floorSurface().materialPtr();
            }
        }
        else if(section == LineSide::Top)
        {
            if(frontSec->ceiling().height()  > backSec->ceiling().height())
            {
                choice1 = backSec->ceilingSurface().materialPtr();
            }
        }

        // In the special case of sky mask on the back plane, our best
        // choice is always this material.
        if(choice1 && choice1->isSkyMasked())
        {
            return choice1;
        }
    }
    else
    {
        // Our first choice is a material on an adjacent wall section.
        // Try the left neighbor first.
        Line *other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.sideId()),
                                         false /*next clockwise*/);
        if(!other)
            // Try the right neighbor.
            other = R_FindLineNeighbor(frontSec, &side.line(), side.line().vertexOwner(side.sideId()^1),
                                       true /*next anti-clockwise*/);

        if(other)
        {
            if(!other->hasBackSector())
            {
                // Our choice is clear - the middle material.
                choice1 = other->front().middle().materialPtr();
            }
            else
            {
                // Compare the relative heights to decide.
                LineSide &otherSide = other->side(&other->frontSector() == frontSec? Line::Front : Line::Back);
                Sector &otherSec = other->side(&other->frontSector() == frontSec? Line::Back : Line::Front).sector();

                if(otherSec.ceiling().height() <= frontSec->floor().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() >= frontSec->ceiling().height())
                    choice1 = otherSide.bottom().materialPtr();
                else if(otherSec.ceiling().height() < frontSec->ceiling().height())
                    choice1 = otherSide.top().materialPtr();
                else if(otherSec.floor().height() > frontSec->floor().height())
                    choice1 = otherSide.bottom().materialPtr();
                // else we'll settle for a plane material.
            }
        }
    }

    // Our second choice is a material from this sector.
    choice2 = frontSec->planeSurface(section == LineSide::Bottom? Sector::Floor : Sector::Ceiling).materialPtr();

    // Prefer a non-animated, non-masked material.
    if(choice1 && !choice1->isAnimated() && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isAnimated() && !choice2->isSkyMasked())
        return choice2;

    // Prefer a non-masked material.
    if(choice1 && !choice1->isSkyMasked())
        return choice1;
    if(choice2 && !choice2->isSkyMasked())
        return choice2;

    // At this point we'll accept anything if it means avoiding HOM.
    if(choice1) return choice1;
    if(choice2) return choice2;

    // We'll assign the special "missing" material...
    return &App_Materials().find(de::Uri("System", Path("missing"))).material();
}

static void addMissingMaterial(LineSide &side, int section)
{
    // Sides without sections need no fixing.
    if(!side.hasSections()) return;
    // ...nor those of self-referencing lines.
    if(side.line().isSelfReferencing()) return;
    // ...nor those of "one-way window" lines.
    if(!side.back().hasSections() && side.back().hasSector()) return;

    // A material must actually be missing to qualify for fixing.
    Surface &surface = side.surface(section);
    if(surface.hasMaterial()) return;

    // Look for and apply a suitable replacement if found.
    surface.setMaterial(chooseFixMaterial(side, section), true/* is missing fix */);

    // During map setup we log missing materials.
    if(ddMapSetup && verbose)
    {
        String path = surface.hasMaterial()? surface.material().manifest().composeUri().asText() : "<null>";

        LOG_WARNING("%s of Line #%d is missing a material for the %s section.\n"
                    "  %s was chosen to complete the definition.")
            << (side.isBack()? "Back" : "Front") << side.line().indexInMap()
            << (section == LineSide::Middle? "middle" : section == LineSide::Top? "top" : "bottom")
            << path;
    }
}

void Sector::fixMissingMaterialsForSides()
{
    foreach(LineSide *side, d->sides)
    {
        /*
         * Do as in the original Doom if the texture has not been defined -
         * extend the floor/ceiling to fill the space (unless it is skymasked),
         * or if there is a midtexture use that instead.
         */
        if(side->hasSector() && side->back().hasSector())
        {
            Sector const &frontSec = side->sector();
            Sector const &backSec  = side->back().sector();

            // A potential bottom section fix?
            if(!(frontSec.floorSurface().hasSkyMaskedMaterial() &&
                  backSec.floorSurface().hasSkyMaskedMaterial()))
            {
                if(frontSec.floor().height() < backSec.floor().height())
                {
                    addMissingMaterial(*side, LineSide::Bottom);
                }
                /*else if(frontSec.floor().height() > backSec.floor().height())
                {
                    addMissingMaterial(side->back(), LineSide::Bottom);
                }*/
            }

            // A potential top section fix?
            if(!(frontSec.ceilingSurface().hasSkyMaskedMaterial() &&
                  backSec.ceilingSurface().hasSkyMaskedMaterial()))
            {
                if(backSec.ceiling().height() < frontSec.ceiling().height())
                {
                    addMissingMaterial(*side, LineSide::Top);
                }
                /*else if(backSec.ceiling().height() > frontSec.ceiling().height())
                {
                    addMissingMaterial(side->back(), LineSide::Top);
                }*/
            }
        }
        else
        {
            // A potential middle section fix.
            addMissingMaterial(*side, LineSide::Middle);
        }
    }
}

#endif // __CLIENT__

int Sector::property(DmuArgs &args) const
{
    switch(args.prop)
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
        SoundEmitter const *emitterAdr = &d->emitter;
        args.setValue(DMT_SECTOR_EMITTER, &emitterAdr, 0);
        break; }
    case DMT_MOBJS:
        args.setValue(DMT_SECTOR_MOBJLIST, &_mobjList, 0);
        break;
    case DMU_VALID_COUNT:
        args.setValue(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    case DMU_FLOOR_PLANE: {
        Plane *pln = d->planes[Floor];
        args.setValue(DMT_SECTOR_FLOORPLANE, &pln, 0);
        break; }
    case DMU_CEILING_PLANE: {
        Plane *pln = d->planes[Ceiling];
        args.setValue(DMT_SECTOR_CEILINGPLANE, &pln, 0);
        break; }
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Sector::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_COLOR: {
        Vector3f newLightColor;
        args.value(DMT_SECTOR_RGB, &newLightColor.x, 0);
        args.value(DMT_SECTOR_RGB, &newLightColor.y, 1);
        args.value(DMT_SECTOR_RGB, &newLightColor.z, 2);
        setLightColor(newLightColor);
        break; }
    case DMU_COLOR_RED: {
        float newStrength;
        args.value(DMT_SECTOR_RGB, &newStrength, 0);
        setLightRed(newStrength);
        break; }
    case DMU_COLOR_GREEN: {
        float newStrength;
        args.value(DMT_SECTOR_RGB, &newStrength, 0);
        setLightGreen(newStrength);
        break; }
    case DMU_COLOR_BLUE: {
        float newStrength;
        args.value(DMT_SECTOR_RGB, &newStrength, 0);
        setLightBlue(newStrength);
        break; }
    case DMU_LIGHT_LEVEL: {
        float newLightLevel;
        args.value(DMT_SECTOR_LIGHTLEVEL, &newLightLevel, 0);
        setLightLevel(newLightLevel);
        break; }
    case DMU_VALID_COUNT:
        args.value(DMT_SECTOR_VALIDCOUNT, &d->validCount, 0);
        break;
    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}
