/** @file line.cpp  World map line.
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

#include "de_base.h"
#include "world/line.h"

#include "dd_main.h" // App_Materials(), verbose
#include "m_misc.h"

#include "Face"
#include "HEdge"

#include "BspLeaf"
#include "Sector"
#include "SectorCluster"
#include "Surface"
#include "Vertex"
#include "world/maputil.h"

#ifdef __CLIENT__
#  include "world/map.h"

#  include "BiasDigest"
#  include "BiasIllum"
#  include "BiasSource"
#  include "BiasTracker"
#endif
#include <QMap>
#include <QtAlgorithms>

#ifdef WIN32
#  undef max
#  undef min
#endif

using namespace de;

#ifdef __CLIENT__
struct GeometryGroup
{
    typedef QList<BiasIllum> BiasIllums;

    uint biasLastUpdateFrame;
    BiasIllums biasIllums;
    BiasTracker biasTracker;
};

/// Geometry group identifier => group data.
typedef QMap<int, GeometryGroup> GeometryGroups;
#endif

DENG2_PIMPL_NOREF(Line::Side::Segment)
{
    /// Half-edge attributed to the line segment (not owned).
    HEdge *hedge;

#ifdef __CLIENT__
    /// Distance along the attributed map line at which the half-edge vertex occurs.
    coord_t lineSideOffset;

    /// Accurate length of the segment.
    coord_t length;

    /// Bias lighting data for each geometry group (i.e., each Line::Side section).
    GeometryGroups geomGroups;
    bool frontFacing;
#endif

    Instance(HEdge *hedge)
        : hedge(hedge)
#ifdef __CLIENT__
        , lineSideOffset(0)
        , length(0)
        , frontFacing(false)
#endif
    {}

#ifdef __CLIENT__
    /**
     * Retrieve geometry data by it's associated unique @a group identifier.
     *
     * @param group     Geometry group identifier.
     * @param canAlloc  @c true= to allocate if no data exists.
     */
    GeometryGroup *geometryGroup(int group, bool canAlloc = true)
    {
        DENG2_ASSERT(group >= 0 && group < 3); // sanity check

        GeometryGroups::iterator foundAt = geomGroups.find(group);
        if(foundAt != geomGroups.end())
        {
            return &*foundAt;
        }

        if(!canAlloc) return 0;

        // Number of bias illumination points for this geometry. Presently we
        // define a 1:1 mapping to strip geometry vertices.
        int const numBiasIllums = 4;

        GeometryGroup &newGeomGroup = *geomGroups.insert(group, GeometryGroup());
        newGeomGroup.biasIllums.reserve(numBiasIllums);
        for(int i = 0; i < numBiasIllums; ++i)
        {
            newGeomGroup.biasIllums.append(BiasIllum(&newGeomGroup.biasTracker));
        }

        return &newGeomGroup;
    }

    /**
     * @todo This could be enhanced so that only the lights on the right
     * side of the surface are taken into consideration.
     */
    void updateBiasContributors(Line::Side const &lineSide,
        GeometryGroup &geomGroup, int /*sectionIndex*/)
    {
        // If the data is already up to date, nothing needs to be done.
        uint lastChangeFrame = lineSide.map().biasLastChangeOnFrame();
        if(geomGroup.biasLastUpdateFrame == lastChangeFrame)
            return;

        geomGroup.biasLastUpdateFrame = lastChangeFrame;

        geomGroup.biasTracker.clearContributors();

        Surface const &surface = lineSide.middle();
        Vector2d const &from   = hedge->origin();
        Vector2d const &to     = hedge->twin().origin();
        Vector2d const center  = (from + to) / 2;

        foreach(BiasSource *source, lineSide.map().biasSources())
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

            geomGroup.biasTracker.addContributor(source, source->evaluateIntensity() / de::max(distance, 1.0));
        }
    }
#endif
};

Line::Side::Segment::Segment(Line::Side &lineSide, HEdge &hedge)
    : MapElement(DMU_SEGMENT, &lineSide)
    , d(new Instance(&hedge))
{}

HEdge &Line::Side::Segment::hedge() const
{
    DENG2_ASSERT(d->hedge != 0);
    return *d->hedge;
}

#ifdef __CLIENT__

coord_t Line::Side::Segment::lineSideOffset() const
{
    return d->lineSideOffset;
}

void Line::Side::Segment::setLineSideOffset(coord_t newOffset)
{
    d->lineSideOffset = newOffset;
}

coord_t Line::Side::Segment::length() const
{
    return d->length;
}

void Line::Side::Segment::setLength(coord_t newLength)
{
    d->length = newLength;
}

bool Line::Side::Segment::isFrontFacing() const
{
    return d->frontFacing;
}

void Line::Side::Segment::setFrontFacing(bool yes)
{
    d->frontFacing = yes;
}

void Line::Side::Segment::updateBiasAfterGeometryMove(int group)
{
    if(GeometryGroup *geomGroup = d->geometryGroup(group, false /*don't allocate*/))
    {
        geomGroup->biasTracker.updateAllContributors();
    }
}

void Line::Side::Segment::applyBiasDigest(BiasDigest &changes)
{
    for(GeometryGroups::iterator it = d->geomGroups.begin();
        it != d->geomGroups.end(); ++it)
    {
        it.value().biasTracker.applyChanges(changes);
    }
}

void Line::Side::Segment::lightBiasPoly(int group, Vector3f const *posCoords,
    Vector4f *colorCoords)
{
    DENG_ASSERT(posCoords != 0 && colorCoords != 0);

    Line::Side const &side   = lineSide();
    int const sectionIndex   = group;
    GeometryGroup *geomGroup = d->geometryGroup(sectionIndex);

    // Should we update?
    if(devUpdateBiasContributors)
    {
        d->updateBiasContributors(side, *geomGroup, sectionIndex);
    }

    Surface const &surface = side.surface(sectionIndex);
    uint const biasTime = map().biasCurrentTime();

    Vector3f const *posIt = posCoords;
    Vector4f *colorIt     = colorCoords;
    for(int i = 0; i < geomGroup->biasIllums.count(); ++i, posIt++, colorIt++)
    {
        *colorIt += geomGroup->biasIllums[i].evaluate(*posIt, surface.normal(), biasTime);
    }

    // Any changes from contributors will have now been applied.
    geomGroup->biasTracker.markIllumUpdateCompleted();
}

#endif // __CLIENT__

/**
 * Line side section of which there are three (middle, bottom and top).
 */
struct Section
{
    Surface surface;
    SoundEmitter soundEmitter;

    Section(Line::Side &side) : surface(dynamic_cast<MapElement &>(side))
    {
        zap(soundEmitter);
    }
};

DENG2_PIMPL_NOREF(Line::Side)
#ifdef __CLIENT__
, DENG2_OBSERVES(Line, FlagsChange)
#endif
{
    int flags;             ///< @ref sdefFlags
    Sector *sector;        ///< Attributed sector (not owned).
    Segments segments;     ///< On "this" side, sorted.
    bool needSortSegments; ///< set to @c true when the list needs sorting.
    int shadowVisCount;    ///< Framecount of last time shadows were drawn.

    struct Sections
    {
        Section middle;
        Section bottom;
        Section top;

        Sections(Side &side) : middle(side), bottom(side), top(side)
        {}
    };
    QScopedPointer<Sections> sections;

    Instance(Sector *sector)
        : flags(0)
        , sector(sector)
        , needSortSegments(false)
        , shadowVisCount(0)
    {}

    ~Instance()
    {
        qDeleteAll(segments);
    }

    /**
     * Retrieve the Section associated with @a sectionId.
     */
    Section &sectionById(int sectionId)
    {
        if(!sections.isNull())
        {
            switch(sectionId)
            {
            case Middle: return sections->middle;
            case Bottom: return sections->bottom;
            case Top:    return sections->top;
            }
        }
        /// @throw Line::InvalidSectionIdError The given section identifier is not valid.
        throw Line::InvalidSectionIdError("Line::Side::section", QString("Invalid section id %1").arg(sectionId));
    }

    void sortSegments(Vector2d lineSideOrigin)
    {
        needSortSegments = false;

        if(segments.count() < 2)
            return;

        // We'll use a QMap for sorting the segments.
        QMap<coord_t, Segment *> sortedSegs;
        foreach(Segment *seg, segments)
        {
            sortedSegs.insert((seg->hedge().origin() - lineSideOrigin).length(), seg);
        }
        segments = sortedSegs.values();
    }

#ifdef __CLIENT__
    /// Observes Line FlagsChange
    void lineFlagsChanged(Line &line, int oldFlags)
    {
        if(!sections.isNull())
        {
            if((line.flags() & DDLF_DONTPEGTOP) != (oldFlags & DDLF_DONTPEGTOP))
            {
                sections->top.surface.markAsNeedingDecorationUpdate();
            }
            if((line.flags() & DDLF_DONTPEGBOTTOM) != (oldFlags & DDLF_DONTPEGBOTTOM))
            {
                sections->bottom.surface.markAsNeedingDecorationUpdate();
            }
        }
    }
#endif
};

Line::Side::Side(Line &line, Sector *sector)
    : MapElement(DMU_SIDE, &line)
    , d(new Instance(sector))
{
#ifdef __CLIENT__
    line.audienceForFlagsChange += d;
#endif
}

int Line::Side::sideId() const
{
    return &line().front() == this? Line::Front : Line::Back;
}

bool Line::Side::considerOneSided() const
{
    // Are we suppressing the back sector?
    if(d->flags & SDF_SUPPRESS_BACK_SECTOR) return true;

    if(!back().hasSector()) return true;
    // Front side of a "one-way window"?
    if(!back().hasSections()) return true;

    if(!line().definesPolyobj())
    {
        // If no segment is linked then the convex subspace on "this" side must
        // have been degenerate (thus no geometry).
        HEdge *hedge = leftHEdge();

        if(!hedge || !hedge->twin().hasFace())
            return true;

        if(!hedge->twin().face().mapElementAs<BspLeaf>().hasCluster())
            return true;
    }

    return false;
}

bool Line::Side::hasSector() const
{
    return d->sector != 0;
}

Sector &Line::Side::sector() const
{
    if(d->sector)
    {
        return *d->sector;
    }
    /// @throw Line::MissingSectorError Attempted with no sector attributed.
    throw Line::MissingSectorError("Line::Side::sector", "No sector is attributed");
}

bool Line::Side::hasSections() const
{
    return !d->sections.isNull();
}

void Line::Side::addSections()
{
    // Already defined?
    if(hasSections()) return;

    d->sections.reset(new Instance::Sections(*this));
}

Surface &Line::Side::surface(int sectionId)
{
    return d->sectionById(sectionId).surface;
}

Surface const &Line::Side::surface(int sectionId) const
{
    return const_cast<Surface const &>(const_cast<Side *>(this)->surface(sectionId));
}

SoundEmitter &Line::Side::soundEmitter(int sectionId)
{
    return d->sectionById(sectionId).soundEmitter;
}

SoundEmitter const &Line::Side::soundEmitter(int sectionId) const
{
    return const_cast<SoundEmitter const &>(const_cast<Side *>(this)->soundEmitter(sectionId));
}

void Line::Side::clearSegments()
{
    d->segments.clear();
    d->needSortSegments = false; // An empty list is sorted.
}

Line::Side::Segment *Line::Side::addSegment(de::HEdge &hedge)
{
    // Have we an exiting segment for this half-edge?
    foreach(Segment *seg, d->segments)
    {
        if(&seg->hedge() == &hedge)
            return seg;
    }

    // No, insert a new one.
    Segment *newSeg = new Segment(*this, hedge);
    d->segments.append(newSeg);
    d->needSortSegments = true; // We'll need to (re)sort.

    // Attribute the segment to half-edge.
    hedge.setMapElement(newSeg);

    return newSeg;
}

Line::Side::Segments const &Line::Side::segments() const
{
    if(d->needSortSegments)
    {
        d->sortSegments(from().origin());
    }
    return d->segments;
}

HEdge *Line::Side::leftHEdge() const
{
    if(d->segments.isEmpty()) return 0;
    if(d->needSortSegments)
    {
        d->sortSegments(from().origin());
    }
    return &d->segments.first()->hedge();
}

HEdge *Line::Side::rightHEdge() const
{
    if(d->segments.isEmpty()) return 0;
    if(d->needSortSegments)
    {
        d->sortSegments(from().origin());
    }
    return &d->segments.last()->hedge();
}

void Line::Side::updateSoundEmitterOrigin(int sectionId)
{
    LOG_AS("Line::Side::updateSoundEmitterOrigin");

    if(!hasSections()) return;

    SoundEmitter &emitter = d->sectionById(sectionId).soundEmitter;

    Vector2d lineCenter = line().center();
    emitter.origin[VX] = lineCenter.x;
    emitter.origin[VY] = lineCenter.y;

    DENG2_ASSERT(d->sector != 0);
    coord_t const ffloor = d->sector->floor().height();
    coord_t const fceil  = d->sector->ceiling().height();

    /// @todo fixme what if considered one-sided?
    switch(sectionId)
    {
    case Middle:
        if(!back().hasSections() || line().isSelfReferencing())
        {
            emitter.origin[VZ] = (ffloor + fceil) / 2;
        }
        else
        {
            emitter.origin[VZ] = (de::max(ffloor, back().sector().floor().height()) +
                                  de::min(fceil,  back().sector().ceiling().height())) / 2;
        }
        break;

    case Bottom:
        if(!back().hasSections() || line().isSelfReferencing() ||
           back().sector().floor().height() <= ffloor)
        {
            emitter.origin[VZ] = ffloor;
        }
        else
        {
            emitter.origin[VZ] = (de::min(back().sector().floor().height(), fceil) + ffloor) / 2;
        }
        break;

    case Top:
        if(!back().hasSections() || line().isSelfReferencing() ||
           back().sector().ceiling().height() >= fceil)
        {
            emitter.origin[VZ] = fceil;
        }
        else
        {
            emitter.origin[VZ] = (de::max(back().sector().ceiling().height(), ffloor) + fceil) / 2;
        }
        break;
    }
}

void Line::Side::updateAllSoundEmitterOrigins()
{
    if(!hasSections()) return;

    updateMiddleSoundEmitterOrigin();
    updateBottomSoundEmitterOrigin();
    updateTopSoundEmitterOrigin();
}

void Line::Side::updateSurfaceNormals()
{
    if(!hasSections()) return;

    Vector3f normal((  to().origin().y - from().origin().y) / line().length(),
                    (from().origin().x -   to().origin().x) / line().length(),
                    0);

    // All line side surfaces have the same normals.
    middle().setNormal(normal); // will normalize
    bottom().setNormal(normal);
    top().setNormal(normal);
}

int Line::Side::flags() const
{
    return d->flags;
}

void Line::Side::setFlags(int flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

void Line::Side::chooseSurfaceTintColors(int sectionId, Vector3f const **topColor,
                                         Vector3f const **bottomColor) const
{
    if(hasSections())
    {
        switch(sectionId)
        {
        case Middle:
            if(isFlagged(SDF_BLENDMIDTOTOP))
            {
                *topColor    = &top().tintColor();
                *bottomColor = &middle().tintColor();
            }
            else if(isFlagged(SDF_BLENDMIDTOBOTTOM))
            {
                *topColor    = &middle().tintColor();
                *bottomColor = &bottom().tintColor();
            }
            else
            {
                *topColor    = &middle().tintColor();
                *bottomColor = 0;
            }
            return;

        case Top:
            if(isFlagged(SDF_BLENDTOPTOMID))
            {
                *topColor    = &top().tintColor();
                *bottomColor = &middle().tintColor();
            }
            else
            {
                *topColor    = &top().tintColor();
                *bottomColor = 0;
            }
            return;

        case Bottom:
            if(isFlagged(SDF_BLENDBOTTOMTOMID))
            {
                *topColor    = &middle().tintColor();
                *bottomColor = &bottom().tintColor();
            }
            else
            {
                *topColor    = &bottom().tintColor();
                *bottomColor = 0;
            }
            return;
        }
    }
    /// @throw Line::InvalidSectionIdError The given section identifier is not valid.
    throw Line::InvalidSectionIdError("Line::Side::chooseSurfaceTintColors", QString("Invalid section id %1").arg(sectionId));
}

int Line::Side::shadowVisCount() const
{
    return d->shadowVisCount;
}

void Line::Side::setShadowVisCount(int newCount)
{
    d->shadowVisCount = newCount;
}

#ifdef __CLIENT__

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
            if(frontSec->ceiling().height() > backSec->ceiling().height())
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
    return &App_ResourceSystem().material(de::Uri("System", Path("missing")));
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
    if(surface.hasMaterial() && !surface.hasFixMaterial())
        return;

    Material *oldMaterial = surface.materialPtr();

    // Look for and apply a suitable replacement (if found).
    surface.setMaterial(chooseFixMaterial(side, section), true/* is missing fix */);

    if(oldMaterial == surface.materialPtr())
        return;

    // We'll need to recalculate reverb.
    if(HEdge *hedge = side.leftHEdge())
    {
        if(hedge->hasFace())
        {
            BspLeaf &bspLeaf = hedge->face().mapElementAs<BspLeaf>();
            if(bspLeaf.hasCluster())
            {
                bspLeaf.cluster().markReverbDirty();
                bspLeaf.cluster().markVisPlanesDirty();
            }
        }
    }

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

void Line::Side::fixMissingMaterials()
{
    if(hasSector() && back().hasSector())
    {
        Sector const &frontSec = sector();
        Sector const &backSec  = back().sector();

        // A potential bottom section fix?
        if(!(frontSec.floorSurface().hasSkyMaskedMaterial() &&
              backSec.floorSurface().hasSkyMaskedMaterial()))
        {
            if(frontSec.floor().height() < backSec.floor().height())
            {
                addMissingMaterial(*this, LineSide::Bottom);
            }
            else if(bottom().hasFixMaterial())
            {
                bottom().setMaterial(0);
            }
        }

        // A potential top section fix?
        if(!(frontSec.ceilingSurface().hasSkyMaskedMaterial() &&
              backSec.ceilingSurface().hasSkyMaskedMaterial()))
        {
            if(backSec.ceiling().height() < frontSec.ceiling().height())
            {
                addMissingMaterial(*this, LineSide::Top);
            }
            else if(top().hasFixMaterial())
            {
                top().setMaterial(0);
            }
        }
    }
    else
    {
        // A potential middle section fix.
        addMissingMaterial(*this, LineSide::Middle);
    }
}

#endif // __CLIENT__

int Line::Side::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        args.setValue(DMT_SIDE_SECTOR, &d->sector, 0);
        break;
    case DMU_LINE: {
        Line const *lineAdr = &line();
        args.setValue(DMT_SIDE_LINE, &lineAdr, 0);
        break; }
    case DMU_FLAGS:
        args.setValue(DMT_SIDE_FLAGS, &d->flags, 0);
        break;
    default:
        return MapElement::property(args);
    }
    return false; // Continue iteration.
}

int Line::Side::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        if(P_IsDummy(&line()))
        {
            args.value(DMT_SIDE_SECTOR, &d->sector, 0);
        }
        else
        {
            /// @throw WritePropertyError Sector is not writable for non-dummy sides.
            throw WritePropertyError("Line::Side::setProperty", QString("Property %1 is only writable for dummy Line::Sides").arg(DMU_Str(args.prop)));
        }
        break; }

    case DMU_FLAGS: {
        int newFlags;
        args.value(DMT_SIDE_FLAGS, &newFlags, 0);
        setFlags(newFlags, de::ReplaceFlags);
        break; }

    default:
        return MapElement::setProperty(args);
    }
    return false; // Continue iteration.
}

DENG2_PIMPL(Line)
{
    int flags;             ///< Public DDLF_* flags.
    Vertex *from;          ///< Start vertex (not owned).
    Vertex *to;            ///< End vertex (not owned).
    Vector2d direction;    ///< From start to end vertex.
    binangle_t angle;      ///< Calculated from the direction vector.
    slopetype_t slopeType; ///< Logical line slope (i.e., world angle) classification.
    coord_t length;        ///< Accurate length.

    ///< Map space bounding box encompassing both vertexes.
    AABoxd aaBox;

    /// Logical sides:
    Side front;
    Side back;

    Polyobj *polyobj; ///< Polyobj "this" line defines a section of, if any.
    int validCount;   ///< Used by legacy algorithms to prevent repeated processing.

    /// Whether the line has been mapped by each player yet.
    bool mapped[DDMAXPLAYERS];

    Instance(Public *i, Vertex &from, Vertex &to, int flags, Sector *frontSector,
             Sector *backSector)
        : Base(i)
        , flags(flags)
        , from(&from)
        , to(&to)
        , direction(to.origin() - from.origin())
        , angle(bamsAtan2(int( direction.y ), int( direction.x )))
        , slopeType(M_SlopeTypeXY(direction.x, direction.y))
        , length(direction.length())
        , front(*i, frontSector)
        , back(*i, backSector)
        , polyobj(0)
        , validCount(0)
    {
        zap(mapped);
    }

    void notifyFlagsChanged(int oldFlags)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(FlagsChange, i) i->lineFlagsChanged(self, oldFlags);
    }
};

Line::Line(Vertex &from, Vertex &to, int flags, Sector *frontSector, Sector *backSector)
    : MapElement(DMU_LINE)
    , _vo1(0)
    , _vo2(0)
    , _bspWindowSector(0)
    , d(new Instance(this, from, to, flags, frontSector, backSector))
{
    updateAABox();
}

int Line::flags() const
{
    return d->flags;
}

void Line::setFlags(int flagsToChange, FlagOp operation)
{
    int newFlags = d->flags;

    applyFlagOperation(newFlags, flagsToChange, operation);

    if(d->flags != newFlags)
    {
        int oldFlags = d->flags;
        d->flags = newFlags;

        // Notify interested parties of the change.
        d->notifyFlagsChanged(oldFlags);
    }
}

bool Line::isBspWindow() const
{
    return _bspWindowSector != 0;
}

bool Line::definesPolyobj() const
{
    return d->polyobj != 0;
}

Polyobj &Line::polyobj() const
{
    if(d->polyobj)
    {
        return *d->polyobj;
    }
    /// @throw Line::MissingPolyobjError Attempted with no polyobj attributed.
    throw Line::MissingPolyobjError("Line::polyobj", "No polyobj is attributed");
}

void Line::setPolyobj(Polyobj *newPolyobj)
{
    d->polyobj = newPolyobj;
}

Line::Side &Line::side(int back)
{
    return back? d->back : d->front;
}

Line::Side const &Line::side(int back) const
{
    return back? d->back : d->front;
}

Vertex &Line::vertex(int to) const
{
    DENG2_ASSERT((to? d->to : d->from) != 0);
    return to? *d->to : *d->from;
}

void Line::replaceVertex(int to, Vertex &newVertex)
{
    if(to) d->to   = &newVertex;
    else   d->from = &newVertex;
}

AABoxd const &Line::aaBox() const
{
    return d->aaBox;
}

void Line::updateAABox()
{
    V2d_InitBoxXY(d->aaBox.arvec2, fromOrigin().x, fromOrigin().y);
    V2d_AddToBoxXY(d->aaBox.arvec2, toOrigin().x, toOrigin().y);
}

coord_t Line::length() const
{
    return d->length;
}

Vector2d const &Line::direction() const
{
    return d->direction;
}

slopetype_t Line::slopeType() const
{
    return d->slopeType;
}

void Line::updateSlopeType()
{
    d->direction = d->to->origin() - d->from->origin();
    d->angle     = bamsAtan2(int( d->direction.y ), int( d->direction.x ));
    d->slopeType = M_SlopeTypeXY(d->direction.x, d->direction.y);
}

binangle_t Line::angle() const
{
    return d->angle;
}

int Line::boxOnSide(AABoxd const &box) const
{
    coord_t fromOriginV1[2] = { fromOrigin().x, fromOrigin().y };
    coord_t directionV1[2]  = { direction().x, direction().y };
    return M_BoxOnLineSide(&box, fromOriginV1, directionV1);
}

int Line::boxOnSide_FixedPrecision(AABoxd const &box) const
{
    /*
     * Apply an offset to both the box and the line to bring everything into
     * the 16.16 fixed-point range. We'll use the midpoint of the line as the
     * origin, as typically this test is called when a bounding box is
     * somewhere in the vicinity of the line. The offset is floored to integers
     * so we won't change the discretization of the fractional part into 16-bit
     * precision.
     */
    coord_t offset[2] = { std::floor(d->from->origin().x + d->direction.x/2.0),
                          std::floor(d->from->origin().y + d->direction.y/2.0) };

    fixed_t boxx[4];
    boxx[BOXLEFT]   = DBL2FIX(box.minX - offset[VX]);
    boxx[BOXRIGHT]  = DBL2FIX(box.maxX - offset[VX]);
    boxx[BOXBOTTOM] = DBL2FIX(box.minY - offset[VY]);
    boxx[BOXTOP]    = DBL2FIX(box.maxY - offset[VY]);

    fixed_t pos[2] = { DBL2FIX(d->from->origin().x - offset[VX]),
                       DBL2FIX(d->from->origin().y - offset[VY]) };

    fixed_t delta[2] = { DBL2FIX(d->direction.x),
                         DBL2FIX(d->direction.y) };

    return M_BoxOnLineSide_FixedPrecision(boxx, pos, delta);
}

coord_t Line::pointDistance(Vector2d const &point, coord_t *offset) const
{
    Vector2d lineVec = d->direction - fromOrigin();
    coord_t len = lineVec.length();
    if(len == 0)
    {
        if(offset) *offset = 0;
        return 0;
    }

    Vector2d delta = fromOrigin() - point;
    if(offset)
    {
        *offset = (  delta.y * (fromOrigin().y - d->direction.y)
                   - delta.x * (d->direction.x - fromOrigin().x) ) / len;
    }

    return (delta.y * lineVec.x - delta.x * lineVec.y) / len;
}

coord_t Line::pointOnSide(Vector2d const &point) const
{
    Vector2d delta = fromOrigin() - point;
    return delta.y * d->direction.x - delta.x * d->direction.y;
}

bool Line::isMappedByPlayer(int playerNum) const
{
    DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    return d->mapped[playerNum];
}

void Line::markMappedByPlayer(int playerNum, bool yes)
{
    DENG2_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    d->mapped[playerNum] = yes;
}

int Line::validCount() const
{
    return d->validCount;
}

void Line::setValidCount(int newValidCount)
{
    d->validCount = newValidCount;
}

int Line::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_VERTEX0:
        args.setValue(DMT_LINE_V, &d->from, 0);
        break;
    case DMU_VERTEX1:
        args.setValue(DMT_LINE_V, &d->to, 0);
        break;
    case DMU_DX:
        args.setValue(DMT_LINE_DX, &d->direction.x, 0);
        break;
    case DMU_DY:
        args.setValue(DMT_LINE_DY, &d->direction.y, 0);
        break;
    case DMU_DXY:
        args.setValue(DMT_LINE_DX, &d->direction.x, 0);
        args.setValue(DMT_LINE_DY, &d->direction.y, 1);
        break;
    case DMU_LENGTH:
        args.setValue(DMT_LINE_LENGTH, &d->length, 0);
        break;
    case DMU_ANGLE: {
        angle_t lineAngle = BANG_TO_ANGLE(d->angle);
        args.setValue(DDVT_ANGLE, &lineAngle, 0);
        break; }
    case DMU_SLOPETYPE:
        args.setValue(DMT_LINE_SLOPETYPE, &d->slopeType, 0);
        break;
    case DMU_FLAGS:
        args.setValue(DMT_LINE_FLAGS, &d->flags, 0);
        break;
    case DMU_FRONT: {
        /// @todo Update the games so that sides without sections can be returned.
        Line::Side const *frontAdr = hasFrontSections()? &d->front : 0;
        args.setValue(DDVT_PTR, &frontAdr, 0);
        break; }
    case DMU_BACK: {
        /// @todo Update the games so that sides without sections can be returned.
        Line::Side const *backAdr = hasBackSections()? &d->back : 0;
        args.setValue(DDVT_PTR, &backAdr, 0);
        break; }
    case DMU_BOUNDING_BOX:
        if(args.valueType == DDVT_PTR)
        {
            AABoxd const *aaBoxAdr = &d->aaBox;
            args.setValue(DDVT_PTR, &aaBoxAdr, 0);
        }
        else
        {
            args.setValue(DMT_LINE_AABOX, &d->aaBox.minX, 0);
            args.setValue(DMT_LINE_AABOX, &d->aaBox.maxX, 1);
            args.setValue(DMT_LINE_AABOX, &d->aaBox.minY, 2);
            args.setValue(DMT_LINE_AABOX, &d->aaBox.maxY, 3);
        }
        break;
    case DMU_VALID_COUNT:
        args.setValue(DMT_LINE_VALIDCOUNT, &d->validCount, 0);
        break;
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

int Line::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_VALID_COUNT:
        args.value(DMT_LINE_VALIDCOUNT, &d->validCount, 0);
        break;
    case DMU_FLAGS: {
        int newFlags;
        args.value(DMT_LINE_FLAGS, &newFlags, 0);
        setFlags(newFlags, de::ReplaceFlags);
        break; }

    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}

LineOwner *Line::vertexOwner(int to) const
{
    DENG2_ASSERT((to? _vo2 : _vo1) != 0);
    return to? _vo2 : _vo1;
}
