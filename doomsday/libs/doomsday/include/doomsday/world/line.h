/** @file line.h  Map line.
 * @ingroup world
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2016 Daniel Swanson <danij@dengine.net>
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

#pragma once

#include <doomsday/world/mapelement.h>
#include <doomsday/world/polyobj.h>
#include <doomsday/world/vertex.h>
#include <doomsday/mesh/hedge.h>

#include <de/legacy/binangle.h>
#include <de/error.h>
#include <de/observers.h>
#include <de/string.h>
#include <de/vector.h>

namespace world {

namespace bsp { class Partitioner; }

class Line;
class LineSide;
class LineOwner;
class Plane;
class Sector;
class Surface;

/**
 * Side geometry segment on the XY plane.
 */
class LIBDOOMSDAY_PUBLIC LineSideSegment : public world::MapElement
{
    DE_NO_COPY  (LineSideSegment)
    DE_NO_ASSIGN(LineSideSegment)

public:
    /**
     * Construct a new line side segment.
     *
     * @param lineSide  Side parent which will own the segment.
     * @param hedge     Half-edge from the map geometry mesh which the
     *                  new segment visualizes.
     */
    LineSideSegment(LineSide &lineSide, mesh::HEdge &hedge);

    /**
     * Returns the half-edge for the segment.
     */
    mesh::HEdge &hedge() const;

    /**
     * Returns the Side owner of the segment.
     */
    LineSide       &lineSide();
    const LineSide &lineSide() const;

    /**
     * Returns the Line attributed to the Side owner of the segment.
     */
    inline Line       &line();
    inline const Line &line() const;

    /**
     * Returns the distance along the attributed map line at which the
     * from vertex vertex occurs.
     */
    double lineSideOffset() const { return _lineSideOffset; }

    void setLineSideOffset(double offset) { _lineSideOffset = offset; }

    /**
     * Returns the accurate length of the segment, from the 'from'
     * vertex to the 'to' vertex in map coordinate space units.
     */
    double length() const { return _length; };

    void setLength(double length) { _length = length; }

private:
    mesh::HEdge *_hedge  = nullptr; // Half-edge attributed to the line segment (not owned).
    double       _length = 0;       // Accurate length of the segment.
    double       _lineSideOffset = 0; // Distance along the attributed map line at which the half-edge vertex occurs.
};

/**
 * Logical side of which there are always two (Front and Back).
 *
 * @todo Invert the object model so that Side always contains a full set of logical
 * subsections. Each Section having ownership of an Emitter and an optional Surface.
 * This should allow for a significant reduction in implementation complexity. -ds
 */
class LIBDOOMSDAY_PUBLIC LineSide : public MapElement
{
    DE_NO_COPY  (LineSide)
    DE_NO_ASSIGN(LineSide)

    /// The given side section identifier is invalid. @ingroup errors
    DE_ERROR(InvalidSectionIdError);

public:
    // Section identifiers:
    enum { Middle = 0, Bottom = 1, Top = 2 };

    static de::String sectionIdAsText(int sectionId);

    /**
     * Flags used as Section identifiers:
     */
    enum SectionFlag {
        MiddleFlag = 0x1,
        BottomFlag = 0x2,
        TopFlag    = 0x4,

        AllSectionFlags = MiddleFlag | BottomFlag | TopFlag
    };
    using SectionFlags = de::Flags;

public:
    /**
     * Construct a new line side.
     *
     * @param line    Line parent which will own the side.
     * @param sector  Sector on "this" side of the line. Can be @c nullptr.
     *                Note that once attributed, the sector cannot normally
     *                be changed.
     */
    LineSide(Line &line, Sector *sector = nullptr);

    /**
     * Composes a human-friendly, styled, textual description of the side.
     */
    de::String description() const;

    /**
     * Returns the @ref sdefFlags for the side.
     */
    int flags() const;

    /**
     * Change the side's flags.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(int flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Returns @c true iff the side is flagged @a flagsToTest.
     */
    inline bool isFlagged(int flagsToTest) const { return (flags() & flagsToTest) != 0; }

    /**
     * Returns the Line owner of the side.
     */
    Line       &line();
    const Line &line() const;

    /**
     * Returns the logical side identifier for the side (Front or Back), according to
     * the "face" of the owning Line.
     *
     * @see line()
     */
    int sideId() const;

    /**
     * Returns @c true if the side is the @ref Front of the owning Line.
     *
     * @see sideId(), isBack()
     */
    bool isFront() const;

    /**
     * Returns @c true if the side is the @ref Back of the owning Line.
     *
     * @see sideId(), isFront()
     */
    inline bool isBack() const { return !isFront(); }

    /**
     * Determines whether "this" side of the owning Line should be handled as if there
     * were no back Sector, irrespective of whether a Sector is attributed to both.
     *
     * Primarily for use with id Tech 1 format maps (which, supports partial suppression
     * of the back sector, for use with special case drawing and playsim functionality).
     */
    bool considerOneSided() const;

    /**
     * Returns @c true if a Sector is attributed to the side.
     *
     * @see sectorPtr(), sector(), considerOneSided()
     */
    inline bool hasSector() const { return _sector != nullptr; }

    /**
     * Returns the Sector attributed to the side.
     *
     * @see hasSector(), sectorPtr()
     */
    inline Sector &sector() const {
        DE_ASSERT(_sector != nullptr);
        return *_sector;
    }

    /**
     * Returns a pointer to the Sector attributed to the side; otherwise @c nullptr.
     *
     * @see hasSector(), sector()
     */
    inline Sector *sectorPtr() const { return _sector; }

    /**
     * Returns @c true iff Sections are defined for the side.
     *
     * @see addSections()
     */
    bool hasSections() const;

    /**
     * Add default sections to the side if they aren't already defined.
     *
     * @see hasSections()
     */
    void addSections();

    /**
     * Returns the frame number of the last time shadows were drawn for the side.
     */
    int shadowVisCount() const;

    /**
     * Change the frame number of the last time shadows were drawn for the side.
     *
     * @param newCount  New shadow vis count.
     */
    void setShadowVisCount(int newCount);

//- Surfaces ------------------------------------------------------------------------

    /**
     * Returns the specified Surface of the side.
     *
     * @param sectionId  Identifier of the surface to return.
     *
     * @see middle(), bottom(), top()
     */
    Surface       &surface(int sectionId);
    const Surface &surface(int sectionId) const;

    /**
     * Returns the @em Middle Surface of the side.
     *
     * @see surface(), bottom(), top()
     */
    Surface       &middle();
    const Surface &middle() const;

    /**
     * Returns the @em Bottom surface of the side.
     *
     * @see surface(), middle(), top()
     */
    Surface       &bottom();
    const Surface &bottom() const;

    /**
     * Returns the @em Top Surface of the side.
     *
     * @see surface(), middle(), bottom()
     */
    Surface       &top();
    const Surface &top() const;

    /**
     * Iterate the Surfaces of the side. If no sections are present then nothing happens.
     *
     * @param func  Callback to make for each Surface.
     */
    de::LoopResult forAllSurfaces(const std::function<de::LoopResult(Surface &)>& func) const;

    /**
     * Update the tangent space normals of the side's surfaces according to the points
     * defined by the Line's vertices. If no Sections are defined this is a no-op.
     */
    void updateAllSurfaceNormals();

    void chooseSurfaceColors(int sectionId, const de::Vec3f **topColor,
                             const de::Vec3f **bottomColor) const;

    bool hasAtLeastOneMaterial() const;

//- SoundEmitters -------------------------------------------------------------------

    /**
     * Returns the SoundEmitter of the section specified with @a sectionId.
     *
     * @see middleSoundEmitter(), bottomSoundEmitter(), topSoundEmitter()
     */
    SoundEmitter       &soundEmitter(int sectionId);
    const SoundEmitter &soundEmitter(int sectionId) const;

    /**
     * Returns the SoundEmitter of the @em Middle section.
     *
     * @see soundEmitter(), bottomSoundEmitter(), topSoundEmitter()
     */
    SoundEmitter       &middleSoundEmitter();
    const SoundEmitter &middleSoundEmitter() const;

    /**
     * Returns the bottom sound emitter (tee-hee) for the side.
     *
     * @see soundEmitter(), middleSoundEmitter(), topSoundEmitter()
     */
    SoundEmitter       &bottomSoundEmitter();
    const SoundEmitter &bottomSoundEmitter() const;

    /**
     * Returns the top sound emitter for the side.
     *
     * @see soundEmitter(), middleSoundEmitter(), bottomSoundEmitter()
     */
    SoundEmitter       &topSoundEmitter();
    const SoundEmitter &topSoundEmitter() const;

    /**
     * Update the sound emitter origin of the specified surface section. This
     * point is determined according to the center point of the owning line and
     * the current @em sharp heights of the sector on "this" side of the line.
     */
    void updateSoundEmitterOrigin(int sectionId);

    inline void updateMiddleSoundEmitterOrigin() { updateSoundEmitterOrigin(Middle); }
    inline void updateBottomSoundEmitterOrigin() { updateSoundEmitterOrigin(Bottom); }
    inline void updateTopSoundEmitterOrigin()    { updateSoundEmitterOrigin(Top); }

    /**
     * Update ALL sound emitter origins for the side.
     * @see updateSoundEmitterOrigin()
     */
    void updateAllSoundEmitterOrigins();

//- Segments ------------------------------------------------------------------------

    /**
     * Clears (destroys) all segments for the side.
     */
    void clearSegments();

    /**
     * Create a Segment for the specified half-edge. If an existing Segment
     * is present for the half-edge it will be returned instead (nothing will
     * happen).
     *
     * It is assumed that the half-edge is collinear with and represents a
     * subsection of the line geometry. It is also assumed that the half-edge
     * faces the same direction as this side. It is the caller's responsibility
     * to ensure these two requirements are met otherwise the segment list
     * will be ordered illogically.
     *
     * @param hedge  Half-edge to create a new Segment for.
     *
     * @return  Pointer to the (possibly newly constructed) Segment.
     */
    LineSideSegment *addSegment(mesh::HEdge &hedge);

    /**
     * Convenient method of returning the half-edge of the left-most segment
     * on this side of the line; otherwise @c nullptr (no segments exist).
     */
    mesh::HEdge *leftHEdge() const;

    /**
     * Convenient method of returning the half-edge of the right-most segment
     * on this side of the line; otherwise @c nullptr (no segments exist).
     */
    mesh::HEdge *rightHEdge() const;

//- Line Accessors (side relative) --------------------------------------------------

    /**
     * Returns the relative back Side from the Line owner.
     *
     * @see sideId(), line()
     */
    inline LineSide       &back();
    inline const LineSide &back() const;

    /**
     * Returns the relative From Vertex for the side, from the Line owner.
     *
     * @see vertex(), to()
     */
    inline Vertex       &from()       { return vertex(0); }
    inline const Vertex &from() const { return vertex(0); }

    /**
     * Returns the relative To Vertex for the side, from the Line owner.
     *
     * @see vertex(), from()
     */
    inline Vertex       &to()       { return vertex(1); }
    inline const Vertex &to() const { return vertex(1); }

    /**
     * Returns the specified relative vertex from the Line owner.
     *
     * @see from(), to(),
     */
    inline Vertex       &vertex(int to);
    inline const Vertex &vertex(int to) const;

protected:
    int property(world::DmuArgs &args) const;
    int setProperty(const world::DmuArgs &args);

private:
    DE_PRIVATE(d)

    // Heavily used; visible for inline access:
    Sector *_sector;
};

Line       &LineSideSegment::line()       { return lineSide().line(); }
const Line &LineSideSegment::line() const { return lineSide().line(); }

/**
 * World map line.
 *
 * @attention This component has a notably different design and slightly different
 * purpose when compared to a Linedef in the id Tech 1 map format. The definitions
 * of which are not always interchangeable.
 *
 * DENG lines always have two logical sides, however they may not have a sector
 * attributed to either or both sides.
 *
 * @note Lines are @em not considered to define the geometry of a map. Instead
 * a line should be thought of as a finite line segment in the plane, according
 * to the standard definition of a line as used with an arrangement of lines in
 * computational geometry.
 *
 * @see http://en.wikipedia.org/wiki/Arrangement_of_lines
 */
class LIBDOOMSDAY_PUBLIC Line : public MapElement
{
    DE_NO_COPY  (Line)
    DE_NO_ASSIGN(Line)

public:
    /// Required polyobj attribution is missing. @ingroup errors
    DE_ERROR(MissingPolyobjError);

    /// Notified whenever the flags change.
    DE_DEFINE_AUDIENCE(FlagsChange, void lineFlagsChanged(Line &line, int oldFlags))

    // Logical edge identifiers:
    enum { From, To };

    // Logical side identifiers:
    enum { Front, Back };

    static de::String sideIdAsText(int sideId);

public:
    Line(Vertex &from,
         Vertex &to,
         int     flags       = 0,
         Sector *frontSector = nullptr,
         Sector *backSector  = nullptr);

    /**
     * Returns the public DDLF_* flags for the line.
     */
    int flags() const;

    /**
     * Change the line's flags. The FlagsChange audience is notified whenever the flags
     * are changed.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(int flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Returns @c true iff the line is flagged @a flagsToTest.
     */
    inline bool isFlagged(int flagsToTest) const {
        return (flags() & flagsToTest) != 0;
    }

    /**
     * Returns @c true if the line is considered to be "self referencing" (term originates
     * from the DOOM modding community), meaning that @em both Sides are attributed to/with
     * the same (valid) Sector.
     *
     * @see isBspWindow()
     */
    bool isSelfReferencing() const;

    /**
     * Returns @c true if the line resulted in the creation of a BSP window effect when
     * partitioning the map.
     *
     * @todo Refactor away. The prescence of a BSP window effect can now be trivially determined
     * through inspection of the tree elements.
     */
    bool isBspWindow() const;

    /**
     * Returns @c true if the line is marked as @em mapped for @a playerNum.
     */
    bool isMappedByPlayer(int playerNum) const;

    /**
     * Change the @em mapped by player state of the line.
     */
    void setMappedByPlayer(int playerNum, bool yes = true);

    /**
     * Returns @c true if the line defines a section of some Polyobj.
     */
    bool definesPolyobj() const;

    /**
     * Returns the Polyobj for which the line is a defining section.
     *
     * @see definesPolyobj()
     */
    Polyobj &polyobj() const;

    /**
     * Change the polyobj attributed to the line.
     *
     * @param newPolyobj  New polyobj to attribute the line to. Can be @c nullptr, to clear
     * the attribution. (Note that the polyobj may also represent this relationship, so
     * the relevant method(s) of Polyobj will also need to be called to complete the job
     * of clearing this relationship.)
     */
    void setPolyobj(Polyobj *newPolyobj);

//- Sides -------------------------------------------------------------------------------

    /**
     * Returns the @em Front side of the line.
     */
    LineSide       &front();
    const LineSide &front() const;

    /**
     * Returns the @em Back side of the line.
     */
    LineSide       &back();
    const LineSide &back() const;

    /**
     * Returns the logical side of the line by it's fixed index.
     *
     * @param back  If not @c 0 return the Back side; otherwise the Front side.
     */
    LineSide       &side(int back);
    const LineSide &side(int back) const;

    /**
     * Iterate through the Sides of the line.
     *
     * @param func  Callback to make for each Side (front then back).
     */
    de::LoopResult forAllSides(std::function<de::LoopResult (LineSide &)> func) const;

//- Geometry ----------------------------------------------------------------------------

    /**
     * Returns the axis-aligned bounding box which encompases both vertex origin points,
     * in map coordinate space units.
     */
    const AABoxd &bounds() const;

    /**
     * Returns the binary angle of the line (which, is derived from the direction vector).
     *
     * @see direction()
     */
    binangle_t angle() const;

    /**
     * Returns the map space point (on the line) which lies at the center of the line's
     * two Vertices.
     */
    de::Vec2d center() const;

    /**
     * Returns a direction vector for the line from Start to End vertex.
     *
     * @see angle()
     */
    const de::Vec2d &direction() const;

    /**
     * Returns the accurate length of the line from Start to End vertex.
     */
    double length() const;

    /**
     * Returns the logical @em slopetype for the line (which, is determined according to
     * the global direction of the line).
     *
     * @see direction()
     * @see M_SlopeType()
     */
    slopetype_t slopeType() const;

    /**
     * Returns the @em From Vertex for the line.
     */
    Vertex       &from();
    const Vertex &from() const;

    /**
     * Returns the @em To Vertex for the line.
     */
    Vertex       &to();
    const Vertex &to() const;

    /**
     * Returns the specified Vertex of the line.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex       &vertex(int to);
    const Vertex &vertex(int to) const;

    /**
     * Iterate through the edge Vertices for the line.
     *
     * @param func  Callback to make for each Vertex.
     */
    de::LoopResult forAllVertices(std::function<de::LoopResult(Vertex &)> func) const;

    /**
     * On which side of the line does the specified box lie?
     *
     * @param box  Bounding box to test.
     *
     * @return One of the following:
     * - Negative: @a box is entirely on the left side.
     * - Zero: @a box intersects the line.
     * - Positive: @a box is entirely on the right side.
     */
    int boxOnSide(const AABoxd &box) const;

    /**
     * On which side of the line does the specified box lie? The test is carried out using
     * fixed-point math for behavior compatible with vanilla DOOM. Note that this means
     * there is a maximum size for both the bounding box and the line: neither can exceed
     * the fixed-point 16.16 range (about 65k units).
     *
     * @param box  Bounding box to test.
     *
     * @return One of the following:
     * - Negative: @a box is entirely on the left side.
     * - Zero: @a box intersects the line.
     * - Positive: @a box is entirely on the right side.
     */
    int boxOnSide_FixedPrecision(const AABoxd &box) const;

    /**
     * @param offset  Returns the position of the nearest point along the line [0..1].
     */
    double pointDistance(const de::Vec2d &point, double *offset = nullptr) const;

    /**
     * Where does the given @a point lie relative to the line? Note that the line is considered
     * to extend to infinity for this test.
     *
     * @param point  The point to test.
     *
     * @return @c <0 Point is to the left of the line.
     *         @c =0 Point lies directly on/incident with the line.
     *         @c >0 Point is to the right of the line.
     */
    double pointOnSide(const de::Vec2d &point) const;

public:
    /**
     * Returns the @em validCount of the line. Used by some legacy iteration algorithms
     * for marking lines as processed/visited.
     *
     * @todo Refactor away.
     */
    int validCount() const;

    /// @todo Refactor away.
    void setValidCount(int newValidCount);

    /**
     * Replace the specified edge vertex of the line.
     *
     * @attention Should only be called in map edit mode.
     *
     * @param to         If not @c 0 replace the To vertex; otherwise the From vertex.
     * @param newVertex  The replacement vertex.
     */
    void replaceVertex(int to, Vertex &newVertex);

    /**
     * Returns a pointer to the line owner node for the specified edge vertex of the line.
     *
     * @param to  If not @c 0 return the owner for the To vertex; otherwise the From vertex.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    LineOwner *vertexOwner(int to) const;

    /**
     * Returns a pointer to the line owner for the specified edge @a vertex of the line.
     * If the vertex is not an edge vertex for the line then @c nullptr will be returned.
     */
    inline LineOwner *vertexOwner(const Vertex &vertex) const {
        if(&vertex == &from()) return v1Owner();
        if(&vertex == &to())   return v2Owner();
        return nullptr;
    }

    /**
     * Returns a pointer to the line owner node for the From/Start vertex of the line.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    inline LineOwner *v1Owner() const { return vertexOwner(From); }

    /**
     * Returns a pointer to the line owner node for the To/End vertex of the line.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    inline LineOwner *v2Owner() const { return vertexOwner(To); }

    /**
     * Register the console commands and/or variables of this module.
     */
    static void consoleRegister();

protected:
    int property(world::DmuArgs &args) const;
    int setProperty(const world::DmuArgs &args);

private:
    DE_PRIVATE(d)

    /// Links to vertex line owner nodes:
    LineOwner *_vo1 = nullptr;
    LineOwner *_vo2 = nullptr;

    /// Sector of the map for which this line acts as a "One-way window".
    /// @todo Now unnecessary, refactor away -ds
    Sector *_bspWindowSector = nullptr;
    
    friend class Map;
    friend class bsp::Partitioner;
};

LineSide       &LineSide::back()       { return line().side(sideId() ^ 1); }
const LineSide &LineSide::back() const { return line().side(sideId() ^ 1); }

Vertex       &LineSide::vertex(int to)       { return line().vertex(sideId() ^ to); }
const Vertex &LineSide::vertex(int to) const { return line().vertex(sideId() ^ to); }

} // namespace world
