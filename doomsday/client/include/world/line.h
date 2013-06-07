/** @file world/line.h World Line.
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

#ifndef DENG_WORLD_LINE_H
#define DENG_WORLD_LINE_H

#include <de/binangle.h>
#include <de/vector1.h> /// @todo remove me

#include <QFlags>

#include <de/Error>
#include <de/Observers>
#include <de/Vector>

#include "MapElement"
#include "Polyobj"
#include "Surface"
#include "Vertex"

class Segment;
class LineOwner;
class Sector;

/**
 * World line.
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
 *
 * @ingroup world
 */
class Line : public de::MapElement
{
public:
    /// Required sector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSectorError);

    /// Required polyobj attribution is missing. @ingroup errors
    DENG2_ERROR(MissingPolyobjError);

    /// The given side section identifier is invalid. @ingroup errors
    DENG2_ERROR(InvalidSectionIdError);

    /**
     * Observers to be notified when the flags change.
     */
    DENG2_DEFINE_AUDIENCE(FlagsChange, void lineFlagsChanged(Line &line, int oldFlags))

    /// Logical edge identifiers:
    enum { From, To };

    /// Logical side identifiers:
    enum { Front, Back };

    /**
     * Logical side of which there are always two (a front and a back).
     */
    class Side : public de::MapElement
    {
    public:
        /// Section identifiers:
        enum
        {
            Middle,
            Bottom,
            Top
        };

        /**
         * Flags used as Section identifiers:
         */
        enum SectionFlag
        {
            MiddleFlag  = 0x1,
            BottomFlag  = 0x2,
            TopFlag     = 0x4,

            AllSectionFlags = MiddleFlag | BottomFlag | TopFlag
        };
        Q_DECLARE_FLAGS(SectionFlags, SectionFlag)

    public:
        Side(Line &line, Sector *sector = 0);

        /**
         * Returns the Line owner of the side.
         */
        Line &line() const;

        /**
         * Returns the logical identifier for the side (Front or Back).
         */
        int sideId() const;

        /**
         * Returns @c true iff this is the front side of the owning line.
         *
         * @see lineSideId()
         */
        inline bool isFront() const { return sideId() == Front; }

        /**
         * Returns @c true iff this is the back side of the owning line.
         *
         * @see lineSideId(), isFront()
         */
        inline bool isBack() const { return !isFront(); }

        /**
         * Returns the relative back Side from the Line owner.
         *
         * @see lineSideId(), line(), Line::side(),
         */
        inline Side &back() const { return line().side(sideId() ^ 1); }

        /**
         * Determines whether "this" side of the respective line should be
         * considered as though there were no back sector. Primarily for use
         * with id Tech 1 format maps (which, supports partial suppression of
         * the back sector, for use with special case drawing and playsim
         * functionality).
         */
        bool considerOneSided() const;

        /**
         * Returns the specified relative vertex from the Line owner.
         *
         * @see lineSideId(), line(), Line::vertex(),
         */
        inline Vertex &vertex(int to) const { return line().vertex(sideId() ^ to); }

        /**
         * Returns the relative From Vertex for the side, from the Line owner.
         *
         * @see vertex(), to()
         */
        inline Vertex &from() const { return vertex(From); }

        /**
         * Returns the relative To Vertex for the side, from the Line owner.
         *
         * @see vertex(), from()
         */
        inline Vertex &to() const { return vertex(To); }

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
         * Returns the specified surface of the side.
         *
         * @param sectionId  Identifier of the surface to return.
         */
        Surface &surface(int sectionId);

        /// @copydoc surface()
        Surface const &surface(int sectionId) const;

        /**
         * Returns the middle surface of the side.
         *
         * @see surface()
         */
        inline Surface &middle() { return surface(Middle); }

        /// @copydoc middle()
        inline Surface const &middle() const { return surface(Middle); }

        /**
         * Returns the bottom surface of the side.
         *
         * @see surface()
         */
        inline Surface &bottom() { return surface(Bottom); }

        /// @copydoc bottom()
        inline Surface const &bottom() const { return surface(Bottom); }

        /**
         * Returns the top surface of the side.
         *
         * @see surface()
         */
        inline Surface &top() { return surface(Top); }

        /// @copydoc top()
        inline Surface const &top() const { return surface(Top); }

        /**
         * Returns the specified sound emitter of the side.
         *
         * @param sectionId  Identifier of the sound emitter to return.
         *
         * @see Section::soundEmitter()
         */
        ddmobj_base_t &soundEmitter(int sectionId);

        /// @copydoc soundEmitter()
        ddmobj_base_t const &soundEmitter(int sectionId) const;

        /**
         * Returns the middle sound emitter of the side.
         *
         * @see Section::soundEmitter()
         */
        inline ddmobj_base_t &middleSoundEmitter() { return soundEmitter(Middle); }

        /// @copydoc middleSoundEmitter()
        inline ddmobj_base_t const &middleSoundEmitter() const { return soundEmitter(Middle); }

        /**
         * Returns the bottom sound emitter (tee-hee) for the side.
         *
         * @see Section::soundEmitter()
         */
        inline ddmobj_base_t &bottomSoundEmitter() { return soundEmitter(Bottom); }

        /// @copydoc bottomSoundEmitter()
        inline ddmobj_base_t const &bottomSoundEmitter() const { return soundEmitter(Bottom); }

        /**
         * Returns the top sound emitter for the side.
         *
         * @see Section::soundEmitter()
         */
        inline ddmobj_base_t &topSoundEmitter() { return soundEmitter(Top); }

        /// @copydoc topSoundEmitter()
        inline ddmobj_base_t const &topSoundEmitter() const { return soundEmitter(Top); }

        /**
         * Update the sound emitter origin of the specified surface section. This
         * point is determined according to the center point of the owning line and
         * the current @em sharp heights of the sector on "this" side of the line.
         */
        void updateSoundEmitterOrigin(int sectionId);

        /**
         * Update the @em middle sound emitter origin for the side.
         * @see updateSoundEmitterOrigin()
         */
        inline void updateMiddleSoundEmitterOrigin() { updateSoundEmitterOrigin(Middle); }

        /**
         * Update the @em bottom sound emitter origin for the side.
         * @see updateSoundEmitterOrigin()
         */
        inline void updateBottomSoundEmitterOrigin() { updateSoundEmitterOrigin(Bottom); }

        /**
         * Update the @em top sound emitter origin for the side.
         * @see updateSoundEmitterOrigin()
         */
        inline void updateTopSoundEmitterOrigin() { updateSoundEmitterOrigin(Top); }

        /**
         * Update ALL sound emitter origins for the side.
         * @see updateSoundEmitterOrigin()
         */
        void updateAllSoundEmitterOrigins();

        /**
         * Returns @c true iff a Sector is attributed to the side.
         *
         * @see considerOneSided()
         */
        bool hasSector() const;

        /**
         * Returns the Sector attributed to the side.
         *
         * @see hasSector()
         */
        Sector &sector() const;

        /**
         * Returns a pointer to the Sector attributed to the side; otherwise @c 0.
         *
         * @see hasSector()
         */
        inline Sector *sectorPtr() const { return hasSector()? &sector() : 0; }

        /**
         * Returns a pointer the left-most segment for the side; otherwise @c 0.
         */
        Segment *leftSegment() const;

        /**
         * Change the left-most segment for the side.
         *
         * @param newSegment   New segment to set as the left-most. Can be @c 0.
         *
         * @todo Refactor away. Only needed presently because of Polyobj.
         */
        void setLeftSegment(Segment *newSegment);

        /**
         * Returns a pointer to the right-most segment for the side; otherwise @c 0.
         */
        Segment *rightSegment() const;

        /**
         * Change the right-most segment for the side.
         *
         * @param newSegment  New segment to set as the right-most. Can be @c 0.
         *
         * @todo Refactor away. Only needed presently because of Polyobj.
         */
        void setRightSegment(Segment *newSegment);

        /**
         * Update the tangent space normals of the side's surfaces according to the
         * points defined by the Line's vertices. If no Sections are defined this is
         * a no-op.
         */
        void updateSurfaceNormals();

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

        void chooseSurfaceTintColors(int sectionId, de::Vector3f const **topColor,
                                     de::Vector3f const **bottomColor) const;

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

        int property(DmuArgs &args) const;
        int setProperty(DmuArgs const &args);

    private:
        DENG2_PRIVATE(d)
    };

public: /// @todo make private:
    /// Links to vertex line owner nodes:
    LineOwner *_vo1;
    LineOwner *_vo2;

    /// Sector of the map for which this line acts as a "One-way window".
    /// @todo Now unnecessary, refactor away -ds
    Sector *_bspWindowSector;

public:
    Line(Vertex &from, Vertex &to,
         int flags                   = 0,
         Sector *frontSector         = 0,
         Sector *backSector          = 0);

    /**
     * Returns the specified logical side of the line.
     *
     * @param back  If not @c 0 return the Back side; otherwise the Front side.
     */
    Side &side(int back);

    /// @copydoc side()
    Side const &side(int back) const;

    /**
     * Returns the logical Front side of the line.
     */
    inline Side &front() { return side(Front); }

    /// @copydoc front()
    inline Side const &front() const { return side(Front); }

    /**
     * Returns the logical Back side of the line.
     */
    inline Side &back() { return side(Back); }

    /// @copydoc back()
    inline Side const &back() const { return side(Back); }

    /**
     * Returns @c true iff Side::Sections are defined for the specified side
     * of the line.
     *
     * @param back  If not @c 0 test the Back side; otherwise the Front side.
     */
    inline bool hasSections(int back) const { return side(back).hasSections(); }

    /**
     * Returns @c true iff Side::Sections are defined for the Front side of the line.
     */
    inline bool hasFrontSections() const { return hasSections(Front); }

    /**
     * Returns @c true iff Side::Sections are defined for the Back side of the line.
     */
    inline bool hasBackSections() const { return hasSections(Back); }

    /**
     * Returns @c true iff a sector is attributed to the specified side of the line.
     *
     * @param back  If not @c 0 test the Back side; otherwise the Front side.
     */
    inline bool hasSector(int back) const { return side(back).hasSector(); }

    /**
     * Returns @c true iff a sector is attributed to the Front side of the line.
     */
    inline bool hasFrontSector() const { return hasSector(Front); }

    /**
     * Returns @c true iff a sector is attributed to the Back side of the line.
     */
    inline bool hasBackSector() const { return hasSector(Back); }

    /**
     * Convenient accessor method for returning the sector attributed to the
     * specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector &sector(int back) const { return side(back).sector(); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector *sectorPtr(int back) const { return side(back).sectorPtr(); }

    /**
     * Returns the sector attributed to the Front side of the line.
     */
    inline Sector &frontSector() const { return sector(Front); }

    /**
     * Returns the sector attributed to the Back side of the line.
     */
    inline Sector &backSector() const { return sector(Back); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the front side of the line.
     */
    inline Sector *frontSectorPtr() const { return sectorPtr(Front); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the back side of the line.
     */
    inline Sector *backSectorPtr() const { return sectorPtr(Back); }

    /**
     * Returns @c true iff the line is considered @em self-referencing.
     * In this context, self-referencing (a term whose origins stem from the
     * DOOM modding community) means a two-sided line (which is to say that
     * a Sector is attributed to both logical sides of the line) where the
     * attributed sectors for each logical side are the same.
     */
    inline bool isSelfReferencing() const {
        return hasFrontSector() && frontSectorPtr() == backSectorPtr();
    }

    /**
     * Returns the specified edge vertex of the line.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(int to) const;

    /**
     * Convenient accessor method for returning the origin of the specified
     * edge vertex for the line.
     *
     * @see vertex()
     */
    inline de::Vector2d const &vertexOrigin(int to) const {
        return vertex(to).origin();
    }

    /**
     * Returns the From/Start vertex for the line.
     */
    inline Vertex &from() const { return vertex(From); }

    /**
     * Convenient accessor method for returning the origin of the From/Start
     * vertex for the line.
     *
     * @see from()
     */
    inline de::Vector2d const &fromOrigin() const { return from().origin(); }

    /**
     * Returns the To/End vertex for the line.
     */
    inline Vertex &to() const { return vertex(To); }

    /**
     * Convenient accessor method for returning the origin of the To/End
     * vertex for the line.
     *
     * @see to()
     */
    inline de::Vector2d const &toOrigin() const { return to().origin(); }

    /**
     * Returns the point on the line which lies at the exact center of the
     * two vertexes.
     */
    inline de::Vector2d center() const { return fromOrigin() + direction() / 2; }

    /**
     * Returns the binary angle of the line (which, is derived from the
     * direction vector).
     *
     * @see direction()
     */
    binangle_t angle() const;

    /**
     * Returns a direction vector for the line from Start to End vertex.
     */
    de::Vector2d const &direction() const;

    /**
     * Returns the logical @em slopetype for the line (which, is determined
     * according to the global direction of the line).
     *
     * @see direction()
     * @see M_SlopeType()
     */
    slopetype_t slopeType() const;

    /**
     * Update the line's logical slopetype and direction according to the
     * points defined by the origins of it's vertexes.
     */
    void updateSlopeType();

    /**
     * Returns the accurate length of the line from Start to End vertex.
     */
    coord_t length() const;

    /**
     * Returns the axis-aligned bounding box which encompases both vertex
     * origin points, in map coordinate space units.
     */
    AABoxd const &aaBox() const;

    /**
     * Update the line's map space axis-aligned bounding box to encompass
     * the points defined by it's vertexes.
     */
    void updateAABox();

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
    int boxOnSide(AABoxd const &box) const;

    /**
     * On which side of the line does the specified box lie? The test is
     * carried out using fixed-point math for behavior compatible with
     * vanilla DOOM. Note that this means there is a maximum size for both
     * the bounding box and the line: neither can exceed the fixed-point
     * 16.16 range (about 65k units).
     *
     * @param box  Bounding box to test.
     *
     * @return One of the following:
     * - Negative: @a box is entirely on the left side.
     * - Zero: @a box intersects the line.
     * - Positive: @a box is entirely on the right side.
     */
    int boxOnSide_FixedPrecision(AABoxd const &box) const;

    /**
     * @param offset  Returns the position of the nearest point along the line [0..1].
     */
    inline coord_t pointDistance(const_pvec2d_t point, coord_t *offset) const
    {
        coord_t fromV1[2]      = { fromOrigin().x, fromOrigin().y };
        coord_t directionV1[2] = { direction().x, direction().y };
        return V2d_PointLineDistance(point, fromV1, directionV1, offset);
    }

    /// @copydoc pointDistance()
    inline coord_t pointDistance(coord_t x, coord_t y, coord_t *offset) const
    {
        coord_t point[2] = { x, y };
        return pointDistance(point, offset);
    }

    /**
     * On which side of the line does the specified point lie?
     *
     * @param point  Point in the map coordinate space to test.
     *
     * @return One of the following:
     * - Negative: @a point is to the left/back side.
     * - Zero: @a point lies directly on the line.
     * - Positive: @a point is to the right/front side.
     */
    inline coord_t pointOnSide(const_pvec2d_t point) const
    {
        coord_t fromV1[2]      = { fromOrigin().x, fromOrigin().y };
        coord_t directionV1[2] = { direction().x, direction().y };
        return V2d_PointOnLineSide(point, fromV1, directionV1);
    }

    /// @copydoc pointOnSide()
    inline coord_t pointOnSide(coord_t x, coord_t y) const
    {
        coord_t point[2] = { x, y };
        return pointOnSide(point);
    }

    /**
     * Returns @c true iff the line defines a section of some Polyobj.
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
     * @param newPolyobj New polyobj to attribute the line to. Can be @c 0,
     *                   to clear the attribution. (Note that the polyobj may
     *                   also represent this relationship, so the relevant
     *                   method(s) of Polyobj will also need to be called to
     *                   complete the job of clearing this relationship.)
     */
    void setPolyobj(Polyobj *newOwner);

    /**
     * Returns @c true iff the line resulted in the creation of a BSP window
     * effect when partitioning the map.
     *
     * @todo Refactor away. The prescence of a BSP window effect can now be
     *       trivially determined through inspection of the tree elements.
     */
    bool isBspWindow() const;

    /**
     * Returns the public DDLF_* flags for the line.
     */
    int flags() const;

    /**
     * Change the line's flags. The FlagsChange audience is notified whenever
     * the flags are changed.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(int flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Returns @c true iff the line is flagged @a flagsToTest.
     */
    inline bool isFlagged(int flagsToTest) const { return (flags() & flagsToTest) != 0; }

    /**
     * Returns @c true if the line is marked as @em mapped for @a playerNum.
     */
    bool isMappedByPlayer(int playerNum) const;

    /**
     * Change the @em mapped by player state of the line.
     */
    void markMappedByPlayer(int playerNum, bool yes = true);

    /**
     * Returns the @em validCount of the line. Used by some legacy iteration
     * algorithms for marking lines as processed/visited.
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
     * @param to  If not @c 0 replace the To vertex; otherwise the From vertex.
     * @param newVertex  The replacement vertex.
     */
    void replaceVertex(int to, Vertex &newVertex);

    inline void replaceFrom(Vertex &newVertex) { replaceVertex(From, newVertex); }
    inline void replaceTo(Vertex &newVertex)   { replaceVertex(To, newVertex); }

protected:
    int property(DmuArgs &args) const;
    int setProperty(DmuArgs const &args);

public:
    /**
     * Returns a pointer to the line owner node for the specified edge vertex
     * of the line.
     *
     * @param to  If not @c 0 return the owner for the To vertex; otherwise the
     *            From vertex.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    LineOwner *vertexOwner(int to) const;

    /**
     * Returns a pointer to the line owner for the specified edge @a vertex
     * of the line. If the vertex is not an edge vertex for the line then
     * @c 0 will be returned.
     */
    inline LineOwner *vertexOwner(Vertex const &vertex) const {
        if(&vertex == &from()) return v1Owner();
        if(&vertex == &to())   return v2Owner();
        return 0;
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

private:
    DENG2_PRIVATE(d)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Line::Side::SectionFlags)

#endif // DENG_WORLD_LINE_H
