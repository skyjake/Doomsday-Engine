/** @file line.cpp  World map line.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "world/line.h"

#include "world/map.h"
#include "world/maputil.h"  // R_FindLineNeighbor()...
#include "world/convexsubspace.h"
#include "world/sector.h"
#include "world/surface.h"
#include "world/vertex.h"
#ifdef __CLIENT__
#  include "world/lineowner.h"

#  include "render/r_main.h"  // levelFullBright
#  include "render/rend_fakeradio.h"
#endif

#include "Face"
#include "HEdge"
#include "dd_main.h"  // App_World()

#include <doomsday/console/cmd.h>
#include <de/vector1.h>

#include <QtAlgorithms>
#include <QList>
#include <QMap>
#include <array>

#ifdef WIN32
#  undef max
#  undef min
#endif

using namespace de;
using namespace world;

DENG2_PIMPL_NOREF(Line::Side::Segment)
{
    HEdge *hedge = nullptr;      ///< Half-edge attributed to the line segment (not owned).

#ifdef __CLIENT__
    ddouble length = 0;          ///< Accurate length of the segment.
    ddouble lineSideOffset = 0;  ///< Distance along the attributed map line at which the half-edge vertex occurs.
    bool frontFacing = false;
#endif
};

Line::Side::Segment::Segment(Line::Side &lineSide, HEdge &hedge)
    : MapElement(DMU_SEGMENT, &lineSide)
    , d(new Impl)
{
    d->hedge = &hedge;
}

Line::Side &Line::Side::Segment::lineSide()
{
    return parent().as<Side>();
}

Line::Side const &Line::Side::Segment::lineSide() const
{
    return parent().as<Side>();
}

HEdge &Line::Side::Segment::hedge() const
{
    DENG2_ASSERT(d->hedge);
    return *d->hedge;
}

#ifdef __CLIENT__

ddouble Line::Side::Segment::lineSideOffset() const
{
    return d->lineSideOffset;
}

void Line::Side::Segment::setLineSideOffset(ddouble newOffset)
{
    d->lineSideOffset = newOffset;
}

ddouble Line::Side::Segment::length() const
{
    return d->length;
}

void Line::Side::Segment::setLength(ddouble newLength)
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

#endif  // __CLIENT__

DENG2_PIMPL(Line::Side)
{
    /**
     * Line side section of which there are three (middle, bottom and top).
     */
    struct Section
    {
        Surface surface;
        ThinkerT<SoundEmitter> soundEmitter;

        Section(Line::Side &side) : surface(side) {}
    };

    struct Sections
    {
        Section *sections[3];

        inline Section &middle() { return *sections[Middle]; }
        inline Section &bottom() { return *sections[Bottom]; }
        inline Section &top()    { return *sections[Top];    }

        Sections(Side &side)
            : sections { new Section(side), new Section(side), new Section(side) }
        {}

        ~Sections()
        {
            for (auto *s : sections) delete s;
        }
    };

#ifdef __CLIENT__
    /**
     * POD: FakeRadio geometry and shadow state.
     */
    struct RadioData
    {
        std::array<edgespan_t, 2> spans;              ///< { bottom, top }
        std::array<shadowcorner_t, 2> topCorners;     ///< { left, right }
        std::array<shadowcorner_t, 2> bottomCorners;  ///< { left, right }
        std::array<shadowcorner_t, 2> sideCorners;    ///< { left, right }
        de::dint updateFrame = 0;
    };
#endif

    dint flags = 0;                 ///< @ref sdefFlags

    QList<Segment *> segments;      ///< On "this" side, sorted. Owned.
    bool needSortSegments = false;  ///< set to @c true when the list needs sorting.

    dint shadowVisCount = 0;        ///< Framecount of last time shadows were drawn.

    std::unique_ptr<Sections> sections;

#ifdef __CLIENT__
    RadioData radioData;
#endif

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        qDeleteAll(segments);
    }

    void clearSegments()
    {
        qDeleteAll(segments);
        segments.clear();
        needSortSegments = false;  // An empty list is sorted.
    }

    /**
     * Retrieve the Section associated with @a sectionId.
     */
    inline Section &sectionById(dint sectionId)
    {
        /*
        if (sections)
        {
            switch (sectionId)
            {
            case Middle: return sections->middle;
            case Bottom: return sections->bottom;
            case Top:    return sections->top;
            }
        }
        /// @throw Line::InvalidSectionIdError The given section identifier is not valid.
        throw Line::InvalidSectionIdError("Line::Side::section", "Invalid section id " + String::number(sectionId));*/

        DENG2_ASSERT(sectionId >= Middle && sectionId <= Top);

        return *sections->sections[sectionId];
    }

    void sortSegments(Vector2d lineSideOrigin)
    {
        needSortSegments = false;

        if (segments.count() < 2)
            return;

        // We'll use a QMap for sorting the segments.
        QMap<ddouble, Segment *> sortedSegs;
        for (Segment *seg : segments)
        {
            sortedSegs.insert((seg->hedge().origin() - lineSideOrigin).length(), seg);
        }
        segments = sortedSegs.values();
    }

#ifdef __CLIENT__
    void updateRadioCorner(shadowcorner_t &sc, dfloat openness, Plane *proximityPlane = nullptr, bool top = false)
    {
        DENG2_ASSERT(self()._sector);
        sc.corner    = openness;
        sc.proximity = proximityPlane;
        if (sc.proximity)
        {
            // Determine relative height offsets (affects shadow map selection).
            sc.pHeight = sc.proximity->heightSmoothed();
            sc.pOffset = sc.pHeight - self()._sector->plane(top? Sector::Ceiling : Sector::Floor).heightSmoothed();
        }
        else
        {
            sc.pOffset = 0;
            sc.pHeight = 0;
        }
    }

    /**
     * Change the FakeRadio side corner properties.
     */
    inline void setRadioCornerTop(bool right, dfloat openness, Plane *proximityPlane = nullptr)
    {
        updateRadioCorner(radioData.topCorners[dint(right)], openness, proximityPlane, true/*top*/);
    }
    inline void setRadioCornerBottom(bool right, dfloat openness, Plane *proximityPlane = nullptr)
    {
        updateRadioCorner(radioData.bottomCorners[dint(right)], openness, proximityPlane, false/*bottom*/);
    }
    inline void setRadioCornerSide(bool right, dfloat openness)
    {
        updateRadioCorner(radioData.sideCorners[dint(right)], openness);
    }

    /**
     * Change the FakeRadio "edge span" metrics.
     * @todo Replace shadow edge enumeration with a shadow corner enumeration. -ds
     */
    void setRadioEdgeSpan(bool top, bool right, ddouble length)
    {
        edgespan_t &span = radioData.spans[dint(top)];
        span.length = length;
        if (!right)
        {
            span.shift = span.length;
        }
    }
#endif // __CLIENT__
};

Line::Side::Side(Line &line, Sector *sector)
    : MapElement(DMU_SIDE, &line)
    , d(new Impl(this))
    , _sector(sector)
{}

Line &Line::Side::line()
{
    return parent().as<Line>();
}

Line const &Line::Side::line() const
{
    return parent().as<Line>();
}

bool Line::Side::isFront() const
{
    return sideId() == Line::Front;
}

String Line::Side::description() const
{
    QStringList flagNames;
    if (flags() & SDF_BLENDTOPTOMID)    flagNames << "blendtoptomiddle";
    if (flags() & SDF_BLENDMIDTOTOP)    flagNames << "blendmiddletotop";
    if (flags() & SDF_BLENDMIDTOBOTTOM) flagNames << "blendmiddletobottom";
    if (flags() & SDF_BLENDBOTTOMTOMID) flagNames << "blendbottomtomiddle";
    if (flags() & SDF_MIDDLE_STRETCH)   flagNames << "middlestretch";

    String flagsString;
    if (!flagNames.isEmpty())
    {
        String const flagsAsText = flagNames.join("|");
        flagsString = String(_E(l) " Flags: " _E(.)_E(i) "%1" _E(.)).arg(flagsAsText);
    }

    auto text = String(_E(D)_E(b) "%1:\n"  _E(.)_E(.)
                       _E(l)  "Sector: "    _E(.)_E(i) "%2" _E(.)
                       _E(l) " One Sided: " _E(.)_E(i) "%3" _E(.)
                       "%4")
                    .arg(Line::sideIdAsText(sideId()).upperFirstChar())
                    .arg(hasSector() ? String::number(sector().indexInMap()) : "None")
                    .arg(DENG2_BOOL_YESNO(considerOneSided()))
                    .arg(flagsString);

    forAllSurfaces([this, &text] (Surface &suf)
    {
        text += String("\n" _E(D) "%1:\n" _E(.))
                  .arg(sectionIdAsText(  &suf == &top   () ? Side::Top
                                       : &suf == &middle() ? Side::Middle
                                       :                     Side::Bottom))
              + suf.description();
        return LoopContinue;
    });

    return text;
}

dint Line::Side::sideId() const
{
    return &line().front() == this? Line::Front : Line::Back;
}

bool Line::Side::considerOneSided() const
{
    // Are we suppressing the back sector?
    if (d->flags & SDF_SUPPRESS_BACK_SECTOR) return true;

    if (!back().hasSector()) return true;
    // Front side of a "one-way window"?
    if (!back().hasSections()) return true;

    if (!line().definesPolyobj())
    {
        // If no segment is linked then the convex subspace on "this" side must
        // have been degenerate (thus no geometry).
        HEdge *hedge = leftHEdge();

        if (!hedge || !hedge->twin().hasFace())
            return true;

        if (!hedge->twin().face().mapElementAs<ConvexSubspace>().hasSubsector())
            return true;
    }

    return false;
}

/*
bool Line::Side::hasSector() const
{
    return d->sector != nullptr;
}

Sector &Line::Side::sector() const
{
    if (d->sector) return *d->sector;
    /// @throw MissingSectorError Attempted with no sector attributed.
    throw MissingSectorError("Line::Side::sector", "No sector is attributed");
}

Sector *Line::Side::sectorPtr() const
{
    return d->sector;
}
*/

bool Line::Side::hasSections() const
{
    return bool(d->sections);
}

void Line::Side::addSections()
{
    // Already defined?
    if (hasSections()) return;

    d->sections.reset(new Impl::Sections(*this));
}

Surface &Line::Side::surface(dint sectionId)
{
    return d->sectionById(sectionId).surface;
}

Surface const &Line::Side::surface(dint sectionId) const
{
    return d->sectionById(sectionId).surface;
}

Surface &Line::Side::middle()
{
    return surface(Middle);
}

Surface const &Line::Side::middle() const
{
    return surface(Middle);
}

Surface &Line::Side::bottom()
{
    return surface(Bottom);
}

Surface const &Line::Side::bottom() const
{
    return surface(Bottom);
}

Surface &Line::Side::top()
{
    return surface(Top);
}

Surface const &Line::Side::top() const
{
    return surface(Top);
}

LoopResult Line::Side::forAllSurfaces(std::function<LoopResult(Surface &)> func) const
{
    if (hasSections())
    {
        for (dint i = Middle; i <= Top; ++i)
        {
            if (auto result = func(const_cast<Side *>(this)->surface(i)))
                return result;
        }
    }
    return LoopContinue;
}

SoundEmitter &Line::Side::soundEmitter(dint sectionId)
{
    return d->sectionById(sectionId).soundEmitter;
}

SoundEmitter const &Line::Side::soundEmitter(dint sectionId) const
{
    return const_cast<Side *>(this)->soundEmitter(sectionId);
}

SoundEmitter &Line::Side::middleSoundEmitter()
{
    return soundEmitter(Middle);
}

SoundEmitter const &Line::Side::middleSoundEmitter() const
{
    return soundEmitter(Middle);
}

SoundEmitter &Line::Side::bottomSoundEmitter()
{
    return soundEmitter(Bottom);
}

SoundEmitter const &Line::Side::bottomSoundEmitter() const
{
    return soundEmitter(Bottom);
}

SoundEmitter &Line::Side::topSoundEmitter()
{
    return soundEmitter(Top);
}

SoundEmitter const &Line::Side::topSoundEmitter() const
{
    return soundEmitter(Top);
}

void Line::Side::clearSegments()
{
    d->clearSegments();
}

Line::Side::Segment *Line::Side::addSegment(HEdge &hedge)
{
    // Have we an exiting segment for this half-edge?
    for (Segment *seg : d->segments)
    {
        if (&seg->hedge() == &hedge)
            return seg;
    }

    // No, insert a new one.
    auto *newSeg = new Segment(*this, hedge);
    d->segments.append(newSeg);
    d->needSortSegments = true;  // We'll need to (re)sort.

    // Attribute the segment to half-edge.
    hedge.setMapElement(newSeg);

    return newSeg;
}

HEdge *Line::Side::leftHEdge() const
{
    if(d->segments.isEmpty()) return nullptr;

    if (d->needSortSegments)
    {
        d->sortSegments(from().origin());
    }
    return &d->segments.first()->hedge();
}

HEdge *Line::Side::rightHEdge() const
{
    if (d->segments.isEmpty()) return nullptr;

    if (d->needSortSegments)
    {
        d->sortSegments(from().origin());
    }
    return &d->segments.last()->hedge();
}

void Line::Side::updateSoundEmitterOrigin(dint sectionId)
{
    LOG_AS("Line::Side::updateSoundEmitterOrigin");

    if (!hasSections()) return;

    SoundEmitter &emitter = d->sectionById(sectionId).soundEmitter;

    Vector2d lineCenter = line().center();
    emitter.origin[0] = lineCenter.x;
    emitter.origin[1] = lineCenter.y;

    DENG2_ASSERT(_sector);
    ddouble const ffloor = _sector->floor().height();
    ddouble const fceil  = _sector->ceiling().height();

    /// @todo fixme what if considered one-sided?
    switch (sectionId)
    {
    case Middle:
        if (!back().hasSections() || line().isSelfReferencing())
        {
            emitter.origin[2] = (ffloor + fceil) / 2;
        }
        else
        {
            emitter.origin[2] = (de::max(ffloor, back().sector().floor().height()) +
                                 de::min(fceil,  back().sector().ceiling().height())) / 2;
        }
        break;

    case Bottom:
        if (!back().hasSections() || line().isSelfReferencing()
           || back().sector().floor().height() <= ffloor)
        {
            emitter.origin[2] = ffloor;
        }
        else
        {
            emitter.origin[2] = (de::min(back().sector().floor().height(), fceil) + ffloor) / 2;
        }
        break;

    case Top:
        if (!back().hasSections() || line().isSelfReferencing()
            || back().sector().ceiling().height() >= fceil)
        {
            emitter.origin[2] = fceil;
        }
        else
        {
            emitter.origin[2] = (de::max(back().sector().ceiling().height(), ffloor) + fceil) / 2;
        }
        break;
    }
}

void Line::Side::updateAllSoundEmitterOrigins()
{
    if (!hasSections()) return;

    updateMiddleSoundEmitterOrigin();
    updateBottomSoundEmitterOrigin();
    updateTopSoundEmitterOrigin();
}

void Line::Side::updateAllSurfaceNormals()
{
    if (!hasSections()) return;

    Vector3f normal((  to().origin().y - from().origin().y) / line().length(),
                    (from().origin().x -   to().origin().x) / line().length(),
                    0);

    // All line side surfaces have the same normals.
    middle().setNormal(normal); // will normalize
    bottom().setNormal(normal);
    top   ().setNormal(normal);
}

dint Line::Side::flags() const
{
    return d->flags;
}

void Line::Side::setFlags(dint flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

void Line::Side::chooseSurfaceColors(dint sectionId, Vector3f const **topColor,
    Vector3f const **bottomColor) const
{
    if (hasSections())
    {
        switch (sectionId)
        {
        case Middle:
            if (isFlagged(SDF_BLENDMIDTOTOP))
            {
                *topColor    = &top   ().color();
                *bottomColor = &middle().color();
            }
            else if (isFlagged(SDF_BLENDMIDTOBOTTOM))
            {
                *topColor    = &middle().color();
                *bottomColor = &bottom().color();
            }
            else
            {
                *topColor    = &middle().color();
                *bottomColor = nullptr;
            }
            return;

        case Top:
            if (isFlagged(SDF_BLENDTOPTOMID))
            {
                *topColor    = &top   ().color();
                *bottomColor = &middle().color();
            }
            else
            {
                *topColor    = &top   ().color();
                *bottomColor = nullptr;
            }
            return;

        case Bottom:
            if (isFlagged(SDF_BLENDBOTTOMTOMID))
            {
                *topColor    = &middle().color();
                *bottomColor = &bottom().color();
            }
            else
            {
                *topColor    = &bottom().color();
                *bottomColor = nullptr;
            }
            return;
        }
    }
    /// @throw InvalidSectionIdError The given section identifier is not valid.
    throw InvalidSectionIdError("Line::Side::chooseSurfaceColors", "Invalid section id " + String::number(sectionId));
}

bool Line::Side::hasAtLeastOneMaterial() const
{
    return middle().hasMaterial() || top().hasMaterial() || bottom().hasMaterial();
}

dint Line::Side::shadowVisCount() const
{
    return d->shadowVisCount;
}

void Line::Side::setShadowVisCount(dint newCount)
{
    d->shadowVisCount = newCount;
}

#ifdef __CLIENT__

shadowcorner_t const &Line::Side::radioCornerTop(bool right) const
{
    return d->radioData.topCorners[dint(right)];
}

shadowcorner_t const &Line::Side::radioCornerBottom(bool right) const
{
    return d->radioData.bottomCorners[dint(right)];
}

shadowcorner_t const &Line::Side::radioCornerSide(bool right) const
{
    return d->radioData.sideCorners[dint(right)];
}

edgespan_t const &Line::Side::radioEdgeSpan(bool top) const
{
    return d->radioData.spans[dint(top)];
}

/**
 * Convert a corner @a angle to a "FakeRadio corner openness" factor.
 */
static dfloat radioCornerOpenness(binangle_t angle)
{
    // Facing outwards?
    if (angle > BANG_180) return -1;

    // Precisely collinear?
    if (angle == BANG_180) return 0;

    // If the difference is too small consider it collinear (there won't be a shadow).
    if (angle < BANG_45 / 5) return 0;

    // 90 degrees is the largest effective difference.
    return (angle > BANG_90)? dfloat( BANG_90 ) / angle : dfloat( angle ) / BANG_90;
}

static inline binangle_t lineNeighborAngle(LineSide const &side, Line const *other, binangle_t diff)
{
    return (other && other != &side.line()) ? diff : 0 /*Consider it coaligned*/;
}

static binangle_t findSolidLineNeighborAngle(LineSide const &side, bool right)
{
    binangle_t diff = 0;
    Line const *other = R_FindSolidLineNeighbor(side.line(),
                                                *side.line().vertexOwner(dint(right) ^ side.sideId()),
                                                right ? Anticlockwise : Clockwise, side.sectorPtr(), &diff);
    return lineNeighborAngle(side, other, diff);
}

/**
 * Returns @c true if there is open space in the sector.
 */
static inline bool sectorIsOpen(Sector const *sector)
{
    return (sector && sector->ceiling().height() > sector->floor().height());
}

struct edge_t
{
    Line *line;
    Sector *sector;
    dfloat length;
    binangle_t diff;
};

/// @todo fixme: Should be rewritten to work at half-edge level.
/// @todo fixme: Should use the visual plane heights of subsectors.
static void scanNeighbor(LineSide const &side, bool top, bool right, edge_t &edge)
{
    static dint const SEP = 10;

    de::zap(edge);

    ClockDirection const direction = (right ? Anticlockwise : Clockwise);
    Sector const *startSector = side.sectorPtr();
    ddouble const fFloor      = side.sector().floor  ().heightSmoothed();
    ddouble const fCeil       = side.sector().ceiling().heightSmoothed();

    ddouble gap    = 0;
    LineOwner *own = side.line().vertexOwner(side.vertex(dint(right)));
    forever
    {
        // Select the next line.
        binangle_t diff  = (direction == Clockwise ? own->angle() : own->prev()->angle());
        Line const *iter = &own->navigate(direction)->line();
        dint scanSecSide = (iter->front().hasSector() && iter->front().sectorPtr() == startSector ? Line::Back : Line::Front);
        // Step selfreferencing lines.
        while ((!iter->front().hasSector() && !iter->back().hasSector()) || iter->isSelfReferencing())
        {
            own         = own->navigate(direction);
            diff       += (direction == Clockwise ? own->angle() : own->prev()->angle());
            iter        = &own->navigate(direction)->line();
            scanSecSide = (iter->front().sectorPtr() == startSector);
        }

        // Determine the relative backsector.
        LineSide const &scanSide = iter->side(scanSecSide);
        Sector const *scanSector = scanSide.sectorPtr();

        // Select plane heights for relative offset comparison.
        ddouble const iFFloor = iter->front().sector().floor  ().heightSmoothed();
        ddouble const iFCeil  = iter->front().sector().ceiling().heightSmoothed();
        Sector const *bsec    = iter->back().sectorPtr();
        ddouble const iBFloor = (bsec ? bsec->floor  ().heightSmoothed() : 0);
        ddouble const iBCeil  = (bsec ? bsec->ceiling().heightSmoothed() : 0);

        // Determine whether the relative back sector is closed.
        bool closed = false;
        if (side.isFront() && iter->back().hasSector())
        {
            closed = top? (iBFloor >= fCeil) : (iBCeil <= fFloor);  // Compared to "this" sector anyway.
        }

        // This line will attribute to this segment's shadow edge - remember it.
        edge.line   = const_cast<Line *>(iter);
        edge.diff   = diff;
        edge.sector = scanSide.sectorPtr();

        // Does this line's length contribute to the alignment of the texture on the
        // segment shadow edge being rendered?
        ddouble lengthDelta = 0;
        if (top)
        {
            if (iter->back().hasSector()
                && (   (side.isFront() && iter->back().sectorPtr() == side.line().front().sectorPtr() && iFCeil >= fCeil)
                    || (side.isBack () && iter->back().sectorPtr() == side.line().back().sectorPtr () && iFCeil >= fCeil)
                    || (side.isFront() && closed == false && iter->back().sectorPtr() != side.line().front().sectorPtr()
                        && iBCeil >= fCeil && sectorIsOpen(iter->back().sectorPtr()))))
            {
                gap += iter->length();  // Should we just mark it done instead?
            }
            else
            {
                edge.length += iter->length() + gap;
                gap = 0;
            }
        }
        else
        {
            if (iter->back().hasSector()
                && (   (side.isFront() && iter->back().sectorPtr() == side.line().front().sectorPtr() && iFFloor <= fFloor)
                    || (side.isBack () && iter->back().sectorPtr() == side.line().back().sectorPtr () && iFFloor <= fFloor)
                    || (side.isFront() && closed == false && iter->back().sectorPtr() != side.line().front().sectorPtr()
                        && iBFloor <= fFloor && sectorIsOpen(iter->back().sectorPtr()))))
            {
                gap += iter->length();  // Should we just mark it done instead?
            }
            else
            {
                lengthDelta = iter->length() + gap;
                gap = 0;
            }
        }

        // Time to stop?
        if (iter == &side.line()) break;

        // Not coalignable?
        if (!(diff >= BANG_180 - SEP && diff <= BANG_180 + SEP)) break;

        // Perhaps a closed edge?
        if (scanSector)
        {
            if (!sectorIsOpen(scanSector)) break;

            // A height difference from the start sector?
            if (top)
            {
                if (scanSector->ceiling().heightSmoothed() != fCeil
                    && scanSector->floor().heightSmoothed() < startSector->ceiling().heightSmoothed())
                {
                    break;
                }
            }
            else
            {
                if (scanSector->floor().heightSmoothed() != fFloor
                    && scanSector->ceiling().heightSmoothed() > startSector->floor().heightSmoothed())
                {
                    break;
                }
            }
        }

        // Swap to the iter line's owner node (i.e., around the corner)?
        if (own->navigate(direction) == iter->v2Owner())
        {
            own = iter->v1Owner();
        }
        else if (own->navigate(direction) == iter->v1Owner())
        {
            own = iter->v2Owner();
        }

        // Skip into the back neighbor sector of the iter line if heights are within
        // the accepted range.
        if (scanSector && side.back().hasSector() && scanSector != side.back().sectorPtr()
            && (   ( top && scanSector->ceiling().heightSmoothed() == startSector->ceiling().heightSmoothed())
                || (!top && scanSector->floor  ().heightSmoothed() == startSector->floor  ().heightSmoothed())))
        {
            // If the map is formed correctly, we should find a back neighbor attached
            // to this line. However, if this is not the case and a line which *should*
            // be two sided isn't, we need to check whether there is a valid neighbor.
            Line *backNeighbor = R_FindLineNeighbor(*iter, *own, direction, startSector);

            if (backNeighbor && backNeighbor != iter)
            {
                // Into the back neighbor sector.
                own = own->navigate(direction);
                startSector = scanSector;
            }
        }

        // The last line was co-alignable so apply any length delta.
        edge.length += lengthDelta;
    }

    // Now we've found the furthest coalignable neighbor, select the back neighbor if
    // present for "edge open-ness" comparison.
    if (edge.sector)  // The back sector of the coalignable neighbor.
    {
        // Since we have the details of the backsector already, simply get the next
        // neighbor (it *is* the back neighbor).
        DENG2_ASSERT(edge.line);
        edge.line = R_FindLineNeighbor(*edge.line,
                                       *edge.line->vertexOwner(dint(edge.line->back().hasSector() && edge.line->back().sectorPtr() == edge.sector) ^ dint(right)),
                                       direction, edge.sector, &edge.diff);
    }
}

/**
 * To determine the dimensions of a shadow, we'll need to scan edges. Edges are composed
 * of aligned lines. It's important to note that the scanning is done separately for the
 * top/bottom edges (both in the left and right direction) and the left/right edges.
 *
 * The length of the top/bottom edges are returned in the array 'spans'.
 *
 * This may look like a complicated operation (performed for all line sides) but in most
 * cases this won't take long. Aligned neighbours are relatively rare.
 *
 * @todo fixme: Should use the visual plane heights of subsectors.
 */
void Line::Side::updateRadioForFrame(dint frameNumber)
{
    // Disabled completely?
    if (!::rendFakeRadio || ::levelFullBright) return;

    // Updates are disabled?
    if (!::devFakeRadioUpdate) return;

    // Sides without sectors don't need updating.
    if (!hasSector()) return;

    // Sides of self-referencing lines do not receive shadows. (Not worth it?).
    if (line().isSelfReferencing()) return;

    // Have already determined the shadow properties?
    if (d->radioData.updateFrame == frameNumber) return;
    d->radioData.updateFrame = frameNumber;  // Mark as done.

    // Process the side corners first.
    d->setRadioCornerSide(false/*left*/, radioCornerOpenness(findSolidLineNeighborAngle(*this, false/*left*/)));
    d->setRadioCornerSide(true/*right*/, radioCornerOpenness(findSolidLineNeighborAngle(*this, true/*right*/)));

    // Top and bottom corners are somewhat more complex as we must traverse neighbors
    // to find the extent of the coalignable surfaces for texture mapping/selection.
    for (dint i = 0; i < 2; ++i)
    {
        bool const rightEdge = i != 0;

        edge_t bottom; scanNeighbor(*this, false/*bottom*/, rightEdge, bottom);
        edge_t top;    scanNeighbor(*this, true/*top*/    , rightEdge, top   );

        d->setRadioEdgeSpan(false/*left*/, rightEdge, line().length() + bottom.length);
        d->setRadioEdgeSpan(true/*right*/, rightEdge, line().length() + top   .length);

        d->setRadioCornerBottom(rightEdge, radioCornerOpenness(lineNeighborAngle(*this, bottom.line, bottom.diff)),
                                bottom.sector? &bottom.sector->floor  () : nullptr);

        d->setRadioCornerTop   (rightEdge, radioCornerOpenness(lineNeighborAngle(*this, top   .line, top   .diff)),
                                top.sector   ? &top   .sector->ceiling() : nullptr);
    }
}
#endif  // __CLIENT__

dint Line::Side::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        args.setValue(DMT_SIDE_SECTOR, &_sector, 0);
        break;
    case DMU_LINE: {
        Line const *lineAdr = &line();
        args.setValue(DMT_SIDE_LINE, &lineAdr, 0);
        break; }
    case DMU_FLAGS:
        args.setValue(DMT_SIDE_FLAGS, &d->flags, 0);
        break;
    case DMU_EMITTER:
        args.setValue(DMT_SIDE_EMITTER,
                      args.modifiers & DMU_TOP_OF_SIDE?    &soundEmitter(Top)    :
                      args.modifiers & DMU_MIDDLE_OF_SIDE? &soundEmitter(Middle) :
                                                           &soundEmitter(Bottom), 0);
        break;
    default:
        return MapElement::property(args);
    }
    return false; // Continue iteration.
}

dint Line::Side::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        if (P_IsDummy(&line()))
        {
            args.value(DMT_SIDE_SECTOR, &_sector, 0);
        }
        else
        {
            /// @throw WritePropertyError Sector is not writable for non-dummy sides.
            throw WritePropertyError("Line::Side::setProperty", "Property " + String(DMU_Str(args.prop)) + " is only writable for dummy Line::Sides");
        }
        break; }

    case DMU_FLAGS: {
        dint newFlags;
        args.value(DMT_SIDE_FLAGS, &newFlags, 0);
        setFlags(newFlags, de::ReplaceFlags);
        break; }

    default:
        return MapElement::setProperty(args);
    }
    return false; // Continue iteration.
}

String Line::Side::sectionIdAsText(dint sectionId) // static
{
    switch (sectionId)
    {
    case Middle: return "middle";
    case Bottom: return "bottom";
    case Top:    return "top";

    default:     return "(invalid)";
    };
}

DENG2_PIMPL(Line)
, DENG2_OBSERVES(Vertex, OriginChange)
{
    dint flags;                 ///< Public DDLF_* flags.
    Side front;                 ///< Front side of the line.
    Side back;                  ///< Back side of the line.
    std::array<bool, DDMAXPLAYERS> mapped {}; ///< Whether the line has been seen by each player yet.

    Vertex *from     = nullptr; ///< Start vertex (not owned).
    Vertex *to       = nullptr; ///< End vertex (not owned).
    Polyobj *polyobj = nullptr; ///< Polyobj the line defines a section of, if any (not owned).

    dint validCount  = 0;       ///< Used by legacy algorithms to prevent repeated processing.

    enum SelfReferencing { Unknown, IsSelfRef, IsNotSelfRef };
    SelfReferencing selfRef = Unknown;

    /**
     * POD: Additional metrics describing the geometry of the line (the vertices).
     */
    struct GeomData
    {
        Vector2d direction;     ///< From start to end vertex.
        ddouble length;         ///< Accurate length.
        binangle_t angle;       ///< Calculated from the direction vector.
        slopetype_t slopeType;  ///< Logical line slope (i.e., world angle) classification.
        AABoxd bounds;          ///< Axis-aligned bounding box.

        GeomData(Vertex const &from, Vertex const &to)
            : direction(to.origin() - from.origin())
            , length   (direction.length())
            , angle    (bamsAtan2(dint(direction.y), dint(direction.x)))
            , slopeType(M_SlopeType(direction.data().baseAs<ddouble>()))
        {
            V2d_InitBoxXY (bounds.arvec2, from.x(), from.y());
            V2d_AddToBoxXY(bounds.arvec2, to  .x(), to  .y());
        }

        static ddouble calcLength(Vertex const &from, Vertex const &to)
        {
            return (to.origin() - from.origin()).length();
        }
    };
    std::unique_ptr<GeomData> gdata;

    Impl(Public *i, Sector *frontSector, Sector *backSector)
        : Base (i)
        , front(*i, frontSector)
        , back (*i, backSector)
    {}

    /**
     * Returns the additional geometry metrics (cached).
     */
    GeomData &geom()
    {
        if (!gdata)
        {
            // Time to calculate this info.
            gdata.reset(new GeomData(self().from(), self().to()));
        }
        return *gdata;
    }

    void vertexOriginChanged(Vertex &vtx)
    {
        DENG2_ASSERT(&vtx == from || &vtx == to); // Should only observe changes to "our" vertices.
        DENG2_ASSERT(polyobj != nullptr);         // Should only observe changes to moveable (not editable) vertices.
        DENG2_UNUSED(vtx);

        // Clear the now invalid geometry metrics (will update later).
        gdata.release();
    }
};

Line::Line(Vertex &from, Vertex &to, dint flags, Sector *frontSector, Sector *backSector)
    : MapElement(DMU_LINE)
    , d(new Impl(this, frontSector, backSector))
{
    d->flags = flags;
    replaceVertex(From, from);
    replaceVertex(To  , to);
}

dint Line::flags() const
{
    return d->flags;
}

void Line::setFlags(dint flagsToChange, FlagOp operation)
{
    dint newFlags = d->flags;

    applyFlagOperation(newFlags, flagsToChange, operation);

    if (d->flags != newFlags)
    {
        dint oldFlags = d->flags;
        d->flags = newFlags;

        // Notify interested parties of the change.
        DENG2_FOR_AUDIENCE(FlagsChange, i) i->lineFlagsChanged(*this, oldFlags);
    }
}

bool Line::isBspWindow() const
{
    return _bspWindowSector != nullptr;
}

bool Line::definesPolyobj() const
{
    return d->polyobj != nullptr;
}

Polyobj &Line::polyobj() const
{
    if (d->polyobj) return *d->polyobj;
    /// @throw MissingPolyobjError Attempted with no polyobj attributed.
    throw MissingPolyobjError("Line::polyobj", "No polyobj is attributed");
}

void Line::setPolyobj(Polyobj *newPolyobj)
{
    if (d->polyobj == newPolyobj) return;

    if (d->polyobj)
    {
        to  ().audienceForOriginChange -= d;
        from().audienceForOriginChange -= d;
    }

    d->polyobj = newPolyobj;

    if (d->polyobj)
    {
        from().audienceForOriginChange += d;
        to  ().audienceForOriginChange += d;
    }
}

bool Line::isSelfReferencing() const
{
    if (d->selfRef == Impl::Unknown)
    {
        d->selfRef = (front().hasSector() && front().sectorPtr() == back().sectorPtr()) ?
            Impl::IsSelfRef : Impl::IsNotSelfRef;
    }
    return d->selfRef == Impl::IsSelfRef;
}

Line::Side &Line::side(dint back)
{
    return (back ? d->back : d->front);
}

Line::Side const &Line::side(dint back) const
{
    return (back ? d->back : d->front);
}

LoopResult Line::forAllSides(std::function<LoopResult(Side &)> func) const
{
    for (dint i = 0; i < 2; ++i)
    {
        if (auto result = func(const_cast<Side &>(side(i))))
            return result;
    }
    return LoopContinue;
}

void Line::replaceVertex(dint to, Vertex &newVertex)
{
    Vertex **adr = (to ? &d->to : &d->from);

    // No change?
    if (*adr && *adr == &newVertex) return;

    *adr = &newVertex;

    // Clear the now invalid geometry metrics (will update later).
    d->gdata.release();
}

Vertex &Line::vertex(dint to)
{
    DENG2_ASSERT((to ? d->to : d->from) != nullptr);
    return (to ? *d->to : *d->from);
}

Vertex const &Line::vertex(dint to) const
{
    DENG2_ASSERT((to ? d->to : d->from) != nullptr);
    return (to ? *d->to : *d->from);
}

Vertex &Line::from()
{
    return vertex(From);
}

Vertex const &Line::from() const
{
    return vertex(From);
}

Vertex &Line::to()
{
    return vertex(To);
}

Vertex const &Line::to() const
{
    return vertex(To);
}

LoopResult Line::forAllVertexs(std::function<LoopResult(Vertex &)> func) const
{
    for (dint i = 0; i < 2; ++i)
    {
        if (auto result = func(const_cast<Line *>(this)->vertex(i)))
            return result;
    }
    return LoopContinue;
}

AABoxd const &Line::bounds() const
{
    return d->geom().bounds;
}

binangle_t Line::angle() const
{
    return d->geom().angle;
}

Vector2d Line::center() const
{
    /// @todo Worth caching in Impl::GeomData? -dj
    return from().origin() + direction() / 2;
}

Vector2d const &Line::direction() const
{
    return d->geom().direction;
}

ddouble Line::length() const
{
    if (d->gdata) return d->gdata->length;
    return Impl::GeomData::calcLength(*d->from, *d->to);
}

slopetype_t Line::slopeType() const
{
    return d->geom().slopeType;
}

dint Line::boxOnSide(AABoxd const &box) const
{
    return M_BoxOnLineSide(&box, from().origin().data().baseAs<ddouble>(),
                           direction().data().baseAs<ddouble>());
}

int Line::boxOnSide_FixedPrecision(AABoxd const &box) const
{
    /// Apply an offset to both the box and the line to bring everything into
    /// the 16.16 fixed-point range. We'll use the midpoint of the line as the
    /// origin, as typically this test is called when a bounding box is
    /// somewhere in the vicinity of the line. The offset is floored to integers
    /// so we won't change the discretization of the fractional part into 16-bit
    /// precision.
    ddouble offset[2] = { std::floor(from().x() + direction().x / 2.0),
                          std::floor(from().y() + direction().y / 2.0) };

    fixed_t boxx[4];
    boxx[BOXLEFT]   = DBL2FIX(box.minX - offset[0]);
    boxx[BOXRIGHT]  = DBL2FIX(box.maxX - offset[0]);
    boxx[BOXBOTTOM] = DBL2FIX(box.minY - offset[1]);
    boxx[BOXTOP]    = DBL2FIX(box.maxY - offset[1]);

    fixed_t pos[2] = { DBL2FIX(from().x() - offset[0]),
                       DBL2FIX(from().y() - offset[1]) };

    fixed_t delta[2] = { DBL2FIX(direction().x),
                         DBL2FIX(direction().y) };

    return M_BoxOnLineSide_FixedPrecision(boxx, pos, delta);
}

ddouble Line::pointDistance(Vector2d const &point, ddouble *offset) const
{
    Vector2d lineVec = direction() - from().origin();
    ddouble len = lineVec.length();
    if (len == 0)
    {
        if (offset) *offset = 0;
        return 0;
    }

    Vector2d delta = from().origin() - point;
    if (offset)
    {
        *offset = (  delta.y * (from().y() - direction().y)
                   - delta.x * (direction().x - from().x()) )
                / len;
    }

    return (delta.y * lineVec.x - delta.x * lineVec.y) / len;
}

ddouble Line::pointOnSide(Vector2d const &point) const
{
    Vector2d delta = from().origin() - point;
    return delta.y * direction().x - delta.x * direction().y;
}

bool Line::isMappedByPlayer(dint playerNum) const
{
    return d->mapped[playerNum];
}

void Line::setMappedByPlayer(dint playerNum, bool yes)
{
    d->mapped[playerNum] = yes;
}

dint Line::validCount() const
{
    return d->validCount;
}

void Line::setValidCount(dint newValidCount)
{
    d->validCount = newValidCount;
}

Line::Side &Line::front()
{
    return side(Front);
}

Line::Side const &Line::front() const
{
    return side(Front);
}

Line::Side &Line::back()
{
    return side(Back);
}

Line::Side const &Line::back() const
{
    return side(Back);
}

#ifdef __CLIENT__
bool Line::isShadowCaster() const
{
    if (definesPolyobj()) return false;
    if (isSelfReferencing()) return false;

    // Lines with no other neighbor do not qualify as shadow casters.
    if (&v1Owner()->next()->line() == this || &v2Owner()->next()->line() == this)
       return false;

    return true;
}
#endif

LineOwner *Line::vertexOwner(dint to) const
{
    DENG2_ASSERT((to ? _vo2 : _vo1) != nullptr);
    return (to ? _vo2 : _vo1);
}

dint Line::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_FLAGS:
        args.setValue(DMT_LINE_FLAGS, &d->flags, 0);
        break;
    case DMU_FRONT: {
        /// @todo Update the games so that sides without sections can be returned.
        Line::Side const *frontAdr = front().hasSections() ? &d->front : nullptr;
        args.setValue(DDVT_PTR, &frontAdr, 0);
        break; }
    case DMU_BACK: {
        /// @todo Update the games so that sides without sections can be returned.
        Line::Side const *backAdr  = back().hasSections() ? &d->back   : nullptr;
        args.setValue(DDVT_PTR, &backAdr, 0);
        break; }
    case DMU_VERTEX0:
        args.setValue(DMT_LINE_V, &d->from, 0);
        break;
    case DMU_VERTEX1:
        args.setValue(DMT_LINE_V, &d->to, 0);
        break;
    case DMU_VALID_COUNT:
        args.setValue(DMT_LINE_VALIDCOUNT, &d->validCount, 0);
        break;

    case DMU_DX:
        args.setValue(DMT_LINE_DX, &direction().x, 0);
        break;
    case DMU_DY:
        args.setValue(DMT_LINE_DY, &direction().y, 0);
        break;
    case DMU_DXY:
        args.setValue(DMT_LINE_DX, &direction().x, 0);
        args.setValue(DMT_LINE_DY, &direction().y, 1);
        break;
    case DMU_LENGTH: {
        ddouble len = length();
        args.setValue(DMT_LINE_LENGTH, &len, 0);
        break; }
    case DMU_ANGLE: {
        angle_t ang = BANG_TO_ANGLE(angle());
        args.setValue(DDVT_ANGLE, &ang, 0);
        break; }
    case DMU_SLOPETYPE: {
        slopetype_t st = slopeType();
        args.setValue(DMT_LINE_SLOPETYPE, &st, 0);
        break; }
    case DMU_BOUNDING_BOX: {
        AABoxd const *boxAdr = &bounds();
        args.setValue(DDVT_PTR, &boxAdr, 0);
        break; }
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

dint Line::setProperty(DmuArgs const &args)
{
    switch(args.prop)
    {
    case DMU_VALID_COUNT:
        args.value(DMT_LINE_VALIDCOUNT, &d->validCount, 0);
        break;
    case DMU_FLAGS: {
        dint newFlags;
        args.value(DMT_LINE_FLAGS, &newFlags, 0);
        setFlags(newFlags, de::ReplaceFlags);
        break; }

    default:
        return MapElement::setProperty(args);
    }

    return false; // Continue iteration.
}

D_CMD(InspectLine)
{
    DENG2_UNUSED(src);

    LOG_AS("inspectline (Cmd)");

    if (argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (line-id)") << argv[0];
        return true;
    }

    if (!App_World().hasMap())
    {
        LOG_SCR_ERROR("No map is currently loaded");
        return false;
    }

    // Find the line.
    dint const index = String(argv[1]).toInt();
    Line const *line = App_World().map().linePtr(index);
    if (!line)
    {
        LOG_SCR_ERROR("Line #%i not found") << index;
        return false;
    }

    QStringList flagNames;
    if (line->flags() & DDLF_BLOCKING)      flagNames << "blocking";
    if (line->flags() & DDLF_DONTPEGTOP)    flagNames << "nopegtop";
    if (line->flags() & DDLF_DONTPEGBOTTOM) flagNames << "nopegbottom";

    String flagsString;
    if (!flagNames.isEmpty())
    {
        String const flagsAsText = flagNames.join("|");
        flagsString = String(_E(l) " Flags: " _E(.)_E(i) "%1" _E(.)).arg(flagsAsText);
    }

    LOG_SCR_MSG(_E(b) "Line %i" _E(.) " [%p]")
            << line->indexInMap() << line;
    LOG_SCR_MSG(_E(l)  "From: " _E(.)_E(i) "%s" _E(.)
                _E(l) " To: "   _E(.)_E(i) "%s" _E(.)
                "%s")
            << line->from().origin().asText()
            << line->to  ().origin().asText()
            << flagsString;
    line->forAllSides([](Line::Side &side)
    {
        LOG_SCR_MSG("") << side.description();
        return LoopContinue;
    });

    return true;
}

void Line::consoleRegister() // static
{
    C_CMD("inspectline", "i", InspectLine);
}

String Line::sideIdAsText(dint sideId) // static
{
    switch (sideId)
    {
    case Front: return "front";
    case Back:  return "back";

    default:    return "(invalid)";
    };
}
