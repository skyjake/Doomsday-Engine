/** @file line.cpp  World map line.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/world/line.h"

#include "doomsday/world/map.h"
#include "doomsday/world/convexsubspace.h"
#include "doomsday/world/sector.h"
#include "doomsday/world/surface.h"
#include "doomsday/world/vertex.h"
#include "doomsday/world/world.h"
#include "doomsday/mesh/face.h"
#include "doomsday/mesh/hedge.h"

#include <doomsday/console/cmd.h>
#include <de/legacy/vector1.h>
#include <de/list.h>
#include <de/keymap.h>
#include <array>
#include <cwctype>

#ifdef WIN32
#  undef max
#  undef min
#endif

namespace world {

using namespace de;

LineSideSegment::LineSideSegment(LineSide &lineSide, mesh::HEdge &hedge)
    : MapElement(DMU_SEGMENT, &lineSide)
{
    _hedge = &hedge;
}

LineSide &LineSideSegment::lineSide()
{
    return parent().as<LineSide>();
}

const LineSide &LineSideSegment::lineSide() const
{
    return parent().as<LineSide>();
}

mesh::HEdge &LineSideSegment::hedge() const
{
    DE_ASSERT(_hedge);
    return *_hedge;
}

DE_PIMPL(LineSide)
{
    /**
     * Line side section of which there are three (middle, bottom and top).
     */
    struct Section
    {
        std::unique_ptr<Surface> surface;
        ThinkerT<SoundEmitter>   soundEmitter;

        Section(LineSide &side)
            : surface(world::Factory::newSurface(side))
        {}
    };

    struct Sections
    {
        Section *sections[3];

        inline Section &middle() { return *sections[Middle]; }
        inline Section &bottom() { return *sections[Bottom]; }
        inline Section &top()    { return *sections[Top];    }

        Sections(LineSide &side)
            : sections { new Section(side), new Section(side), new Section(side) }
        {}

        ~Sections()
        {
            for (auto *s : sections) delete s;
        }
    };

    dint flags = 0;                 ///< @ref sdefFlags

    List<LineSideSegment *> segments; ///< On "this" side, sorted. Owned.
    bool needSortSegments = false;  ///< set to @c true when the list needs sorting.

    dint shadowVisCount = 0;        ///< Framecount of last time shadows were drawn.

    std::unique_ptr<Sections> sections;

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        deleteAll(segments);
    }

    void clearSegments()
    {
        deleteAll(segments);
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
        throw Line::InvalidSectionIdError("LineSide::section", "Invalid section id " + String::asText(sectionId));*/

        DE_ASSERT(sectionId >= Middle && sectionId <= Top);

        return *sections->sections[sectionId];
    }

    void sortSegments(Vec2d lineSideOrigin)
    {
        needSortSegments = false;

        if (segments.count() < 2)
            return;

        // We'll use a Map for sorting the segments.
        de::KeyMap<ddouble, LineSideSegment *> sortedSegs;
        for (auto *seg : segments)
        {
            sortedSegs.insert((seg->hedge().origin() - lineSideOrigin).length(), seg);
        }
        segments = de::map<decltype(segments)>(
            sortedSegs, [](const std::pair<ddouble, LineSideSegment *> &v) { return v.second; });
    }
};

LineSide::LineSide(Line &line, Sector *sector)
    : MapElement(DMU_SIDE, &line)
    , d(new Impl(this))
    , _sector(sector)
{}

Line &LineSide::line()
{
    return parent().as<Line>();
}

const Line &LineSide::line() const
{
    return parent().as<Line>();
}

bool LineSide::isFront() const
{
    return sideId() == Line::Front;
}

String LineSide::description() const
{
    StringList flagNames;
    if (flags() & SDF_BLENDTOPTOMID)    flagNames << "blendtoptomiddle";
    if (flags() & SDF_BLENDMIDTOTOP)    flagNames << "blendmiddletotop";
    if (flags() & SDF_BLENDMIDTOBOTTOM) flagNames << "blendmiddletobottom";
    if (flags() & SDF_BLENDBOTTOMTOMID) flagNames << "blendbottomtomiddle";
    if (flags() & SDF_MIDDLE_STRETCH)   flagNames << "middlestretch";

    String flagsString;
    if (!flagNames.isEmpty())
    {
        const String flagsAsText = String::join(flagNames, "|");
        flagsString = Stringf(_E(l) " Flags: " _E(.)_E(i) "%s" _E(.), flagsAsText.c_str());
    }

    auto text = Stringf(_E(D)_E(b) "%s:\n"  _E(.)_E(.)
                       _E(l)  "Sector: "    _E(.)_E(i) "%s" _E(.)
                       _E(l) " One Sided: " _E(.)_E(i) "%s" _E(.)
                       "%s",
                    Line::sideIdAsText(sideId()).upperFirstChar().c_str(),
                    hasSector() ? String::asText(sector().indexInMap()).c_str() : "None",
                    DE_BOOL_YESNO(considerOneSided()),
                    flagsString.c_str());

    forAllSurfaces([this, &text] (Surface &surf)
    {
        text += Stringf("\n" _E(D) "%s:\n%s" _E(.),
                  sectionIdAsText(  &surf == &top   () ? LineSide::Top
                                  : &surf == &middle() ? LineSide::Middle
                                  :                      LineSide::Bottom).c_str(),
                  surf.description().c_str());
        return LoopContinue;
    });

    return text;
}

dint LineSide::sideId() const
{
    return &line().front() == this? Line::Front : Line::Back;
}

bool LineSide::considerOneSided() const
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
        auto *hedge = leftHEdge();

        if (!hedge || !hedge->twin().hasFace())
            return true;

        if (!hedge->twin().face().mapElementAs<ConvexSubspace>().hasSubsector())
            return true;
    }

    return false;
}

/*
bool LineSide::hasSector() const
{
    return d->sector != nullptr;
}

Sector &LineSide::sector() const
{
    if (d->sector) return *d->sector;
    /// @throw MissingSectorError Attempted with no sector attributed.
    throw MissingSectorError("LineSide::sector", "No sector is attributed");
}

Sector *LineSide::sectorPtr() const
{
    return d->sector;
}
*/

bool LineSide::hasSections() const
{
    return bool(d->sections);
}

void LineSide::addSections()
{
    // Already defined?
    if (hasSections()) return;

    d->sections.reset(new Impl::Sections(*this));
}

Surface &LineSide::surface(dint sectionId)
{
    return *d->sectionById(sectionId).surface;
}

const Surface &LineSide::surface(dint sectionId) const
{
    return *d->sectionById(sectionId).surface;
}

Surface &LineSide::middle()
{
    return surface(Middle);
}

const Surface &LineSide::middle() const
{
    return surface(Middle);
}

Surface &LineSide::bottom()
{
    return surface(Bottom);
}

const Surface &LineSide::bottom() const
{
    return surface(Bottom);
}

Surface &LineSide::top()
{
    return surface(Top);
}

const Surface &LineSide::top() const
{
    return surface(Top);
}

LoopResult LineSide::forAllSurfaces(const std::function<LoopResult(Surface &)>& func) const
{
    if (hasSections())
    {
        for (dint i = Middle; i <= Top; ++i)
        {
            if (auto result = func(const_cast<LineSide *>(this)->surface(i)))
                return result;
        }
    }
    return LoopContinue;
}

SoundEmitter &LineSide::soundEmitter(dint sectionId)
{
    return d->sectionById(sectionId).soundEmitter;
}

const SoundEmitter &LineSide::soundEmitter(dint sectionId) const
{
    return const_cast<LineSide *>(this)->soundEmitter(sectionId);
}

SoundEmitter &LineSide::middleSoundEmitter()
{
    return soundEmitter(Middle);
}

const SoundEmitter &LineSide::middleSoundEmitter() const
{
    return soundEmitter(Middle);
}

SoundEmitter &LineSide::bottomSoundEmitter()
{
    return soundEmitter(Bottom);
}

const SoundEmitter &LineSide::bottomSoundEmitter() const
{
    return soundEmitter(Bottom);
}

SoundEmitter &LineSide::topSoundEmitter()
{
    return soundEmitter(Top);
}

const SoundEmitter &LineSide::topSoundEmitter() const
{
    return soundEmitter(Top);
}

void LineSide::clearSegments()
{
    d->clearSegments();
}

LineSideSegment *LineSide::addSegment(mesh::HEdge &hedge)
{
    // Have we an existing segment for this half-edge?
    for (auto *seg : d->segments)
    {
        if (&seg->hedge() == &hedge)
            return seg;
    }

    // No, insert a new one.
    auto *newSeg = Factory::newLineSideSegment(*this, hedge);
    d->segments.append(newSeg);
    d->needSortSegments = true;  // We'll need to (re)sort.

    // Attribute the segment to half-edge.
    hedge.setMapElement(newSeg);

    return newSeg;
}

mesh::HEdge *LineSide::leftHEdge() const
{
    if(d->segments.isEmpty()) return nullptr;

    if (d->needSortSegments)
    {
        d->sortSegments(from().origin());
    }
    return &d->segments.first()->hedge();
}

mesh::HEdge *LineSide::rightHEdge() const
{
    if (d->segments.isEmpty()) return nullptr;

    if (d->needSortSegments)
    {
        d->sortSegments(from().origin());
    }
    return &d->segments.last()->hedge();
}

void LineSide::updateSoundEmitterOrigin(dint sectionId)
{
    LOG_AS("LineSide::updateSoundEmitterOrigin");

    if (!hasSections()) return;

    SoundEmitter &emitter = d->sectionById(sectionId).soundEmitter;

    Vec2d lineCenter = line().center();
    emitter.origin[0] = lineCenter.x;
    emitter.origin[1] = lineCenter.y;

    DE_ASSERT(_sector);
    const ddouble ffloor = _sector->floor().height();
    const ddouble fceil  = _sector->ceiling().height();

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

void LineSide::updateAllSoundEmitterOrigins()
{
    if (!hasSections()) return;

    updateMiddleSoundEmitterOrigin();
    updateBottomSoundEmitterOrigin();
    updateTopSoundEmitterOrigin();
}

void LineSide::updateAllSurfaceNormals()
{
    if (!hasSections()) return;

    Vec3f normal((to().origin().y - from().origin().y) / line().length(),
                 (from().origin().x - to().origin().x) / line().length(),
                 0);

    // All line side surfaces have the same normals.
    middle().setNormal(normal); // will normalize
    bottom().setNormal(normal);
    top   ().setNormal(normal);
}

dint LineSide::flags() const
{
    return d->flags;
}

void LineSide::setFlags(dint flagsToChange, FlagOp operation)
{
    applyFlagOperation(d->flags, flagsToChange, operation);
}

void LineSide::chooseSurfaceColors(dint sectionId, const Vec3f **topColor,
    const Vec3f **bottomColor) const
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
    throw InvalidSectionIdError("LineSide::chooseSurfaceColors", "Invalid section id " + String::asText(sectionId));
}

bool LineSide::hasAtLeastOneMaterial() const
{
    return middle().hasMaterial() || top().hasMaterial() || bottom().hasMaterial();
}

dint LineSide::shadowVisCount() const
{
    return d->shadowVisCount;
}

void LineSide::setShadowVisCount(dint newCount)
{
    d->shadowVisCount = newCount;
}

dint LineSide::property(DmuArgs &args) const
{
    switch(args.prop)
    {
    case DMU_SECTOR:
        args.setValue(DMT_SIDE_SECTOR, &_sector, 0);
        break;
    case DMU_LINE: {
        const Line *lineAdr = &line();
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

dint LineSide::setProperty(const DmuArgs &args)
{
    switch(args.prop)
    {
    case DMU_SECTOR: {
        if (Map::dummyElementType(&line()) != DMU_NONE)
        {
            args.value(DMT_SIDE_SECTOR, &_sector, 0);
        }
        else
        {
            /// @throw WritePropertyError Sector is not writable for non-dummy sides.
            throw WritePropertyError("LineSide::setProperty",
                                     "Property " + std::string(DMU_Str(args.prop)) +
                                         " is only writable for dummy LineSides");
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

String LineSide::sectionIdAsText(dint sectionId) // static
{
    switch (sectionId)
    {
    case Middle: return "middle";
    case Bottom: return "bottom";
    case Top:    return "top";

    default:     return "(invalid)";
    };
}

DE_PIMPL(Line)
, DE_OBSERVES(Vertex, OriginChange)
{
    dint flags;                      ///< Public DDLF_* flags.
    std::unique_ptr<LineSide> front; ///< Front side of the line.
    std::unique_ptr<LineSide> back;  ///< Back side of the line.
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
        Vec2d direction;     ///< From start to end vertex.
        ddouble length;         ///< Accurate length.
        binangle_t angle;       ///< Calculated from the direction vector.
        slopetype_t slopeType;  ///< Logical line slope (i.e., world angle) classification.
        AABoxd bounds;          ///< Axis-aligned bounding box.

        GeomData(const Vertex &from, const Vertex &to)
            : direction(to.origin() - from.origin())
            , length   (direction.length())
            , angle    (bamsAtan2(dint(direction.y), dint(direction.x)))
            , slopeType(M_SlopeType(direction.data().baseAs<ddouble>()))
        {
            V2d_InitBoxXY (bounds.arvec2, from.x(), from.y());
            V2d_AddToBoxXY(bounds.arvec2, to  .x(), to  .y());
        }

        static ddouble calcLength(const Vertex &from, const Vertex &to)
        {
            return (to.origin() - from.origin()).length();
        }
    };
    std::unique_ptr<GeomData> gdata;

    Impl(Public *i, Sector *frontSector, Sector *backSector)
        : Base (i)
        , front(Factory::newLineSide(*i, frontSector))
        , back (Factory::newLineSide(*i, backSector))
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
        DE_ASSERT(&vtx == from || &vtx == to); // Should only observe changes to "our" vertices.
        DE_ASSERT(polyobj != nullptr);         // Should only observe changes to moveable (not editable) vertices.
        DE_UNUSED(vtx);

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
        DE_NOTIFY_VAR(FlagsChange, i) i->lineFlagsChanged(*this, oldFlags);
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

LineSide &Line::side(dint back)
{
    return (back ? *d->back : *d->front);
}

const LineSide &Line::side(dint back) const
{
    return (back ? *d->back : *d->front);
}

LoopResult Line::forAllSides(std::function<LoopResult(LineSide &)> func) const
{
    for (dint i = 0; i < 2; ++i)
    {
        if (auto result = func(const_cast<LineSide &>(side(i))))
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
    DE_ASSERT((to ? d->to : d->from) != nullptr);
    return (to ? *d->to : *d->from);
}

const Vertex &Line::vertex(dint to) const
{
    DE_ASSERT((to ? d->to : d->from) != nullptr);
    return (to ? *d->to : *d->from);
}

Vertex &Line::from()
{
    return vertex(From);
}

const Vertex &Line::from() const
{
    return vertex(From);
}

Vertex &Line::to()
{
    return vertex(To);
}

const Vertex &Line::to() const
{
    return vertex(To);
}

LoopResult Line::forAllVertices(std::function<LoopResult(Vertex &)> func) const
{
    for (dint i = 0; i < 2; ++i)
    {
        if (auto result = func(const_cast<Line *>(this)->vertex(i)))
            return result;
    }
    return LoopContinue;
}

const AABoxd &Line::bounds() const
{
    return d->geom().bounds;
}

binangle_t Line::angle() const
{
    return d->geom().angle;
}

Vec2d Line::center() const
{
    /// @todo Worth caching in Impl::GeomData? -dj
    return from().origin() + direction() / 2;
}

const Vec2d &Line::direction() const
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

dint Line::boxOnSide(const AABoxd &box) const
{
    return M_BoxOnLineSide(&box, from().origin().data().baseAs<ddouble>(),
                           direction().data().baseAs<ddouble>());
}

int Line::boxOnSide_FixedPrecision(const AABoxd &box) const
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

ddouble Line::pointDistance(const Vec2d &point, ddouble *offset) const
{
    Vec2d lineVec = direction() - from().origin();
    ddouble len = lineVec.length();
    if (fequal(len, 0.0))
    {
        if (offset) *offset = 0;
        return 0;
    }

    Vec2d delta = from().origin() - point;
    if (offset)
    {
        *offset = (  delta.y * (from().y() - direction().y)
                   - delta.x * (direction().x - from().x()) )
                / len;
    }

    return (delta.y * lineVec.x - delta.x * lineVec.y) / len;
}

ddouble Line::pointOnSide(const Vec2d &point) const
{
    Vec2d delta = from().origin() - point;
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

LineSide &Line::front()
{
    return side(Front);
}

const LineSide &Line::front() const
{
    return side(Front);
}

LineSide &Line::back()
{
    return side(Back);
}

const LineSide &Line::back() const
{
    return side(Back);
}

LineOwner *Line::vertexOwner(dint to) const
{
    DE_ASSERT((to ? _vo2 : _vo1) != nullptr);
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
        const LineSide *frontAdr = front().hasSections() ? d->front.get() : nullptr;
        args.setValue(DDVT_PTR, &frontAdr, 0);
        break; }
    case DMU_BACK: {
        /// @todo Update the games so that sides without sections can be returned.
        const LineSide *backAdr  = back().hasSections() ? d->back.get()   : nullptr;
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
        const AABoxd *boxAdr = &bounds();
        args.setValue(DDVT_PTR, &boxAdr, 0);
        break; }
    default:
        return MapElement::property(args);
    }

    return false; // Continue iteration.
}

dint Line::setProperty(const DmuArgs &args)
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
    DE_UNUSED(src);

    LOG_AS("inspectline (Cmd)");

    if (argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (line-id)") << argv[0];
        return true;
    }

    if (!World::get().hasMap())
    {
        LOG_SCR_ERROR("No map is currently loaded");
        return false;
    }

    // Find the line.
    const dint index = String(argv[1]).toInt();
    const Line *line = World::get().map().linePtr(index);
    if (!line)
    {
        LOG_SCR_ERROR("Line #%i not found") << index;
        return false;
    }

    StringList flagNames;
    if (line->flags() & DDLF_BLOCKING)      flagNames << "blocking";
    if (line->flags() & DDLF_DONTPEGTOP)    flagNames << "nopegtop";
    if (line->flags() & DDLF_DONTPEGBOTTOM) flagNames << "nopegbottom";

    String flagsString;
    if (!flagNames.isEmpty())
    {
        flagsString = Stringf(_E(l) " Flags: " _E(.) _E(i) "%s" _E(.),
                                     String::join(flagNames, "|").c_str());
    }

    LOG_SCR_MSG(_E(b) "Line %i" _E(.) " [%p]")
            << line->indexInMap() << line;
    LOG_SCR_MSG(_E(l)  "From: " _E(.)_E(i) "%s" _E(.)
                _E(l) " To: "   _E(.)_E(i) "%s" _E(.)
                "%s")
            << line->from().origin().asText()
            << line->to  ().origin().asText()
            << flagsString;
    line->forAllSides([](LineSide &side)
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

} // namespace world
