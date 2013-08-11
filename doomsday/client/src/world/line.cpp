/** @file line.cpp World map line.
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

#include <QMap>

#include <de/libdeng2.h>
#include <de/Vector>

#include "m_misc.h"

#include "Face"
#include "HEdge"

#include "BspLeaf"
#include "Sector"
#include "Vertex"

#ifdef __CLIENT__
#  include "world/map.h"
#  include "BiasIllum"
#  include "BiasSource"
#  include "BiasTracker"
#endif

#include "world/line.h"

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
         ,lineSideOffset(0),
          length(0),
          frontFacing(false)
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
        DENG_ASSERT(group >= 0 && group < 3); // sanity check

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
        GeometryGroup &geomGroup, int sectionIndex)
    {
        DENG_UNUSED(sectionIndex);

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
    : MapElement(DMU_SEGMENT, &lineSide),
      d(new Instance(&hedge))
{}

Line::Side &Line::Side::Segment::lineSide() const
{
    return *this->parent().as<Line::Side>();
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

BiasTracker *Line::Side::Segment::biasTracker(int group)
{
    if(GeometryGroup *geomGroup = d->geometryGroup(group, false /*don't allocate*/))
    {
        return &geomGroup->biasTracker;
    }
    return 0;
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
    ddmobj_base_t soundEmitter;

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
    struct Sections
    {
        Section middle;
        Section bottom;
        Section top;

        Sections(Side &side) : middle(side), bottom(side), top(side)
        {}
    };

    /// @ref sdefFlags
    int flags;

    /// Sections.
    QScopedPointer<Sections> sections;

    /// Attributed sector (not owned).
    Sector *sector;

    /// Left-most half-egde on this side of the owning line (not owned).
    HEdge *leftHEdge;

    /// Right-most half-edge on this side of the owning line (not owned).
    HEdge *rightHEdge;

    /// Framecount of last time shadows were drawn on this side.
    int shadowVisCount;

    Instance(Sector *sector)
        : flags(0),
          sector(sector),
          leftHEdge(0),
          rightHEdge(0),
          shadowVisCount(0)
    {}

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
    : MapElement(DMU_SIDE, &line), d(new Instance(sector))
{
#ifdef __CLIENT__
    line.audienceForFlagsChange += d;
#endif
}

Line &Line::Side::line() const
{
    return *this->parent().as<Line>();
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
        if(!d->leftHEdge)
            return true;

        if(!d->leftHEdge->twin().hasFace())
            return true;

        if(!d->leftHEdge->twin().face().mapElement()->as<BspLeaf>()->hasSector())
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

ddmobj_base_t &Line::Side::soundEmitter(int sectionId)
{
    return d->sectionById(sectionId).soundEmitter;
}

ddmobj_base_t const &Line::Side::soundEmitter(int sectionId) const
{
    return const_cast<ddmobj_base_t const &>(const_cast<Side *>(this)->soundEmitter(sectionId));
}

HEdge *Line::Side::leftHEdge() const
{
    return d->leftHEdge;
}

void Line::Side::setLeftHEdge(HEdge *newHEdge)
{
    d->leftHEdge = newHEdge;
}

HEdge *Line::Side::rightHEdge() const
{
    return d->rightHEdge;
}

void Line::Side::setRightHEdge(HEdge *newHEdge)
{
    d->rightHEdge = newHEdge;
}

void Line::Side::updateSoundEmitterOrigin(int sectionId)
{
    LOG_AS("Line::Side::updateSoundEmitterOrigin");

    if(!hasSections()) return;

    ddmobj_base_t &emitter = d->sectionById(sectionId).soundEmitter;

    Vector2d lineCenter = line().center();
    emitter.origin[VX] = lineCenter.x;
    emitter.origin[VY] = lineCenter.y;

    DENG_ASSERT(d->sector != 0);
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

int Line::Side::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        args.setValue(DMT_SIDE_SECTOR, &d->sector, 0);
        break;
    case DMU_LINE: {
        Line *lineAdr = &line();
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
    /**
     * @todo fixme: Changing the sector references via the DMU API has been
     * disabled. In order to effect such changes at run time we need to separate
     * the two different purposes of sector referencing at this level.
     *
     * A) The playsim uses this for stuff like line specials, to determine
     * which map elements are involved when building stairs or opening doors.
     *
     * B) The engine uses this for geometry construction and more.
     *
     * At best this will result in strange glitches but more than likely a
     * fatal error (or worse) would happen should anyone try to use this...
     */

    switch(args.prop)
    {
    /*case DMU_SECTOR: {
        Sector *newSector = sectorPtr();
        args.value(DMT_LINESIDE_SECTOR, &d->sector, 0);
        break; } */

    /*case DMU_LINE: { // Never changeable - throw exception?
        Line *line = &_line;
        args.value(DMT_LINESIDE_LINE, &line, 0);
        break; }*/

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
    /// Public DDLF_* flags.
    int flags;

    /// Vertexes (not owned):
    Vertex *from;
    Vertex *to;

    /// Direction vector from -> to.
    Vector2d direction;

    /// Calculated from the direction vector.
    binangle_t angle;

    /// Logical line slope (i.e., world angle) classification.
    slopetype_t slopeType;

    /// Accurate length.
    coord_t length;

    /// Bounding box encompassing the map space coordinates of both vertexes.
    AABoxd aaBox;

    /// Logical sides:
    Side front;
    Side back;

    /// The polyobj which this line defines a section of, if any.
    Polyobj *polyobj;

    /// Used by legacy algorithms to prevent repeated processing.
    int validCount;

    /// Whether the line has been mapped by each player yet.
    bool mapped[DDMAXPLAYERS];

    Instance(Public *i, Vertex &from, Vertex &to, int flags,
             Sector *frontSector, Sector *backSector)
        : Base(i),
          flags(flags),
          from(&from),
          to(&to),
          direction(to.origin() - from.origin()),
          angle(bamsAtan2(int( direction.y ), int( direction.x ))),
          slopeType(M_SlopeTypeXY(direction.x, direction.y)),
          length(direction.length()),
          front(*i, frontSector),
          back(*i, backSector),
          polyobj(0),
          validCount(0)
    {
        std::memset(mapped, 0, sizeof(mapped));
    }

    void notifyFlagsChanged(int oldFlags)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(FlagsChange, i) i->lineFlagsChanged(self, oldFlags);
    }
};

Line::Line(Vertex &from, Vertex &to, int flags, Sector *frontSector, Sector *backSector)
    : MapElement(DMU_LINE),
      _vo1(0),
      _vo2(0),
      _bspWindowSector(0),
      d(new Instance(this, from, to, flags, frontSector, backSector))
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
    DENG_ASSERT((to? d->to : d->from) != 0);
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
    /**
     * @todo fixme: Changing the side references via the DMU API has been
     * disabled. At best this will result in strange glitches but more than
     * likely a fatal error (or worse) would happen should anyone try to use
     * this...
     */

    switch(args.prop)
    {
    /*case DMU_FRONT: {
        SideDef *newFrontSideDef = frontSideDefPtr();
        args.value(DMT_LINE_SIDE, &newFrontSideDef, 0);
        d->front._sideDef = newFrontSideDef;
        break; }
    case DMU_BACK: {
        SideDef *newBackSideDef = backSideDefPtr();
        args.value(DMT_LINE_SIDE, &newBackSideDef, 0);
        d->back._sideDef = newBackSideDef;
        break; }*/

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
    DENG_ASSERT((to? _vo2 : _vo1) != 0);
    return to? _vo2 : _vo1;
}
