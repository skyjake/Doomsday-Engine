/** @file line.h  Map line.
 * @ingroup world
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_WORLD_LINE_H
#define DENG_WORLD_LINE_H

#include <functional>
#include <QFlags>
#include <de/binangle.h>
#include <de/Error>
#include <de/Observers>
#include <de/String>
#include <de/Vector>
#include <doomsday/world/MapElement>
#include "HEdge"
#include "Polyobj"
#include "Vertex"

class LineOwner;
class Plane;
class Sector;
class Surface;

#ifdef __CLIENT__
struct edgespan_t;
struct shadowcorner_t;
#endif

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
class Line : public world::MapElement
{
    DENG2_NO_COPY  (Line)
    DENG2_NO_ASSIGN(Line)

public:
    /// The given side section identifier is invalid. @ingroup errors
    DENG2_ERROR(InvalidSectionIdError);

    /// Required polyobj attribution is missing. @ingroup errors
    DENG2_ERROR(MissingPolyobjError);

    /// Notified whenever the flags change.
    DENG2_DEFINE_AUDIENCE(FlagsChange, void lineFlagsChanged(Line &line, de::dint oldFlags))

    // Logical edge identifiers:
    enum { From, To };

    // Logical side identifiers:
    enum { Front, Back };

    static de::String sideIdAsText(de::dint sideId);

    /**
     * Logical side of which there are always two (Front and Back).
     *
     * @todo Invert the object model so that Side always contains a full set of logical
     * subsections. Each Section having ownership of an Emitter and an optional Surface.
     * This should allow for a significant reduction in implementation complexity. -ds
     */
    class Side : public world::MapElement
    {
        DENG2_NO_COPY  (Side)
        DENG2_NO_ASSIGN(Side)

    public:
        /// Required sector attribution is missing. @ingroup errors
        //DENG2_ERROR(MissingSectorError);

        // Section identifiers:
        enum {
            Middle = 0,
            Bottom = 1,
            Top    = 2
        };

        static de::String sectionIdAsText(de::dint sectionId);

        /**
         * Flags used as Section identifiers:
         */
        enum SectionFlag {
            MiddleFlag  = 0x1,
            BottomFlag  = 0x2,
            TopFlag     = 0x4,

            AllSectionFlags = MiddleFlag | BottomFlag | TopFlag
        };
        Q_DECLARE_FLAGS(SectionFlags, SectionFlag)

        /**
         * Side geometry segment on the XY plane.
         */
        class Segment : public world::MapElement
        {
            DENG2_NO_COPY  (Segment)
            DENG2_NO_ASSIGN(Segment)

        public:
            /**
             * Construct a new line side segment.
             *
             * @param lineSide  Side parent which will own the segment.
             * @param hedge     Half-edge from the map geometry mesh which the
             *                  new segment visualizes.
             */
            Segment(Side &lineSide, de::HEdge &hedge);

            /**
             * Returns the half-edge for the segment.
             */
            de::HEdge &hedge() const;

            /**
             * Returns the Side owner of the segment.
             *
             * @see line()
             */
            Side       &lineSide();
            Side const &lineSide() const;

            /**
             * Accessor. Returns the Line attributed to the Side owner of the segment.
             *
             * @see lineSide()
             */
            inline Line       &line()       { return lineSide().line(); }
            inline Line const &line() const { return lineSide().line(); }

#ifdef __CLIENT__

            /**
             * Returns the distance along the attributed map line at which the
             * from vertex vertex occurs.
             *
             * @see lineSide()
             */
            de::ddouble lineSideOffset() const;

            /// @todo Refactor away.
            void setLineSideOffset(de::ddouble newOffset);

            /**
             * Returns the accurate length of the segment, from the 'from'
             * vertex to the 'to' vertex in map coordinate space units.
             */
            de::ddouble length() const;

            /// @todo Refactor away.
            void setLength(de::ddouble newLength);

            /**
             * Returns @c true iff the segment is marked as "front facing".
             */
            bool isFrontFacing() const;

            /**
             * Mark the current segment as "front facing".
             */
            void setFrontFacing(bool yes = true);

#endif  // __CLIENT__

        private:
            DENG2_PRIVATE(d)
        };

    public:
        /**
         * Construct a new line side.
         *
         * @param line    Line parent which will own the side.
         * @param sector  Sector on "this" side of the line. Can be @c nullptr.
         *                Note that once attributed, the sector cannot normally
         *                be changed.
         */
        Side(Line &line, Sector *sector = nullptr);

        /**
         * Composes a human-friendly, styled, textual description of the side.
         */
        de::String description() const;

        /**
         * Returns the @ref sdefFlags for the side.
         */
        de::dint flags() const;

        /**
         * Change the side's flags.
         *
         * @param flagsToChange  Flags to change the value of.
         * @param operation      Logical operation to perform on the flags.
         */
        void setFlags(de::dint flagsToChange, de::FlagOp operation = de::SetFlags);

        /**
         * Returns @c true iff the side is flagged @a flagsToTest.
         */
        inline bool isFlagged(de::dint flagsToTest) const { return (flags() & flagsToTest) != 0; }

        /**
         * Returns the Line owner of the side.
         */
        Line       &line();
        Line const &line() const;

        /**
         * Returns the logical side identifier for the side (Front or Back), according to
         * the "face" of the owning Line.
         *
         * @see line()
         */
        de::dint sideId() const;

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
            DENG2_ASSERT(_sector != nullptr);
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
        de::dint shadowVisCount() const;

        /**
         * Change the frame number of the last time shadows were drawn for the side.
         *
         * @param newCount  New shadow vis count.
         */
        void setShadowVisCount(de::dint newCount);

    //- Surfaces ------------------------------------------------------------------------

        /**
         * Returns the specified Surface of the side.
         *
         * @param sectionId  Identifier of the surface to return.
         *
         * @see middle(), bottom(), top()
         */
        Surface       &surface(de::dint sectionId);
        Surface const &surface(de::dint sectionId) const;

        /**
         * Returns the @em Middle Surface of the side.
         *
         * @see surface(), bottom(), top()
         */
        Surface       &middle();
        Surface const &middle() const;

        /**
         * Returns the @em Bottom surface of the side.
         *
         * @see surface(), middle(), top()
         */
        Surface       &bottom();
        Surface const &bottom() const;

        /**
         * Returns the @em Top Surface of the side.
         *
         * @see surface(), middle(), bottom()
         */
        Surface       &top();
        Surface const &top() const;

        /**
         * Iterate the Surfaces of the side. If no sections are present then nothing happens.
         *
         * @param func  Callback to make for each Surface.
         */
        de::LoopResult forAllSurfaces(std::function<de::LoopResult(Surface &)> func) const;

        /**
         * Update the tangent space normals of the side's surfaces according to the points
         * defined by the Line's vertices. If no Sections are defined this is a no-op.
         */
        void updateAllSurfaceNormals();

        void chooseSurfaceColors(de::dint sectionId, de::Vector3f const **topColor,
                                 de::Vector3f const **bottomColor) const;

        bool hasAtLeastOneMaterial() const;

    //- SoundEmitters -------------------------------------------------------------------

        /**
         * Returns the SoundEmitter of the section specified with @a sectionId.
         *
         * @see middleSoundEmitter(), bottomSoundEmitter(), topSoundEmitter()
         */
        SoundEmitter       &soundEmitter(de::dint sectionId);
        SoundEmitter const &soundEmitter(de::dint sectionId) const;

        /**
         * Returns the SoundEmitter of the @em Middle section.
         *
         * @see soundEmitter(), bottomSoundEmitter(), topSoundEmitter()
         */
        SoundEmitter       &middleSoundEmitter();
        SoundEmitter const &middleSoundEmitter() const;

        /**
         * Returns the bottom sound emitter (tee-hee) for the side.
         *
         * @see soundEmitter(), middleSoundEmitter(), topSoundEmitter()
         */
        SoundEmitter       &bottomSoundEmitter();
        SoundEmitter const &bottomSoundEmitter() const;

        /**
         * Returns the top sound emitter for the side.
         *
         * @see soundEmitter(), middleSoundEmitter(), bottomSoundEmitter()
         */
        SoundEmitter       &topSoundEmitter();
        SoundEmitter const &topSoundEmitter() const;

        /**
         * Update the sound emitter origin of the specified surface section. This
         * point is determined according to the center point of the owning line and
         * the current @em sharp heights of the sector on "this" side of the line.
         */
        void updateSoundEmitterOrigin(de::dint sectionId);

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
        Segment *addSegment(de::HEdge &hedge);

        /**
         * Convenient method of returning the half-edge of the left-most segment
         * on this side of the line; otherwise @c nullptr (no segments exist).
         */
        de::HEdge *leftHEdge() const;

        /**
         * Convenient method of returning the half-edge of the right-most segment
         * on this side of the line; otherwise @c nullptr (no segments exist).
         */
        de::HEdge *rightHEdge() const;

#ifdef __CLIENT__
    //- FakeRadio -----------------------------------------------------------------------

        /**
         * To be called to update the shadow properties for the line side.
         *
         * @todo Handle this internally -ds.
         */
        void updateRadioForFrame(de::dint frameNumber);

        /**
         * Provides access to the FakeRadio shadowcorner_t data.
         */
        shadowcorner_t const &radioCornerTop   (bool right) const;
        shadowcorner_t const &radioCornerBottom(bool right) const;
        shadowcorner_t const &radioCornerSide  (bool right) const;

        /**
         * Provides access to the FakeRadio edgespan_t data.
         */
        edgespan_t const &radioEdgeSpan(bool top) const;

#endif  // __CLIENT__

    //- Line Accessors (side relative) --------------------------------------------------

        /**
         * Returns the relative back Side from the Line owner.
         *
         * @see sideId(), line()
         */
        inline Side       &back()       { return line().side(sideId() ^ 1); }
        inline Side const &back() const { return line().side(sideId() ^ 1); }

        /**
         * Returns the relative From Vertex for the side, from the Line owner.
         *
         * @see vertex(), to()
         */
        inline Vertex       &from()       { return vertex(From); }
        inline Vertex const &from() const { return vertex(From); }

        /**
         * Returns the relative To Vertex for the side, from the Line owner.
         *
         * @see vertex(), from()
         */
        inline Vertex       &to()       { return vertex(To); }
        inline Vertex const &to() const { return vertex(To); }

        /**
         * Returns the specified relative vertex from the Line owner.
         *
         * @see from(), to(),
         */
        inline Vertex       &vertex(de::dint to)       { return line().vertex(sideId() ^ to); }
        inline Vertex const &vertex(de::dint to) const { return line().vertex(sideId() ^ to); }

    protected:
        de::dint property(world::DmuArgs &args) const;
        de::dint setProperty(world::DmuArgs const &args);

    private:
        DENG2_PRIVATE(d)

        // Heavily used; visible for inline access:
        Sector *_sector;
    };

public: /// @todo make private:
    /// Links to vertex line owner nodes:
    LineOwner *_vo1 = nullptr;
    LineOwner *_vo2 = nullptr;

    /// Sector of the map for which this line acts as a "One-way window".
    /// @todo Now unnecessary, refactor away -ds
    Sector *_bspWindowSector = nullptr;

public:
    Line(Vertex &from, Vertex &to,
         de::dint flags      = 0,
         Sector *frontSector = nullptr,
         Sector *backSector  = nullptr);

    /**
     * Returns the public DDLF_* flags for the line.
     */
    de::dint flags() const;

    /**
     * Change the line's flags. The FlagsChange audience is notified whenever the flags
     * are changed.
     *
     * @param flagsToChange  Flags to change the value of.
     * @param operation      Logical operation to perform on the flags.
     */
    void setFlags(de::dint flagsToChange, de::FlagOp operation = de::SetFlags);

    /**
     * Returns @c true iff the line is flagged @a flagsToTest.
     */
    inline bool isFlagged(de::dint flagsToTest) const {
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
    bool isMappedByPlayer(de::dint playerNum) const;

    /**
     * Change the @em mapped by player state of the line.
     */
    void setMappedByPlayer(de::dint playerNum, bool yes = true);

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
    Side       &front();
    Side const &front() const;

    /**
     * Returns the @em Back side of the line.
     */
    Side       &back();
    Side const &back() const;

    /**
     * Returns the logical side of the line by it's fixed index.
     *
     * @param back  If not @c 0 return the Back side; otherwise the Front side.
     */
    Side       &side(de::dint back);
    Side const &side(de::dint back) const;

    /**
     * Iterate through the Sides of the line.
     *
     * @param func  Callback to make for each Side (front then back).
     */
    de::LoopResult forAllSides(std::function<de::LoopResult (Side &)> func) const;

//- Geometry ----------------------------------------------------------------------------

    /**
     * Returns the axis-aligned bounding box which encompases both vertex origin points,
     * in map coordinate space units.
     */
    AABoxd const &bounds() const;

    /**
     * Returns the binary angle of the line (which, is derived from the direction vector).
     *
     * @see direction()
     */
    binangle_t angle() const;

    /**
     * Returns the map space point (on the line) which lies at the center of the line's
     * two Vertexs.
     */
    de::Vector2d center() const;

    /**
     * Returns a direction vector for the line from Start to End vertex.
     *
     * @see angle()
     */
    de::Vector2d const &direction() const;

    /**
     * Returns the accurate length of the line from Start to End vertex.
     */
    de::ddouble length() const;

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
    Vertex const &from() const;

    /**
     * Returns the @em To Vertex for the line.
     */
    Vertex       &to();
    Vertex const &to() const;

    /**
     * Returns the specified Vertex of the line.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex       &vertex(de::dint to);
    Vertex const &vertex(de::dint to) const;

    /**
     * Iterate through the edge Vertexs for the line.
     *
     * @param func  Callback to make for each Vertex.
     */
    de::LoopResult forAllVertexs(std::function<de::LoopResult(Vertex &)> func) const;

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
    de::dint boxOnSide(AABoxd const &box) const;

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
    de::dint boxOnSide_FixedPrecision(AABoxd const &box) const;

    /**
     * @param offset  Returns the position of the nearest point along the line [0..1].
     */
    de::ddouble pointDistance(de::Vector2d const &point, de::ddouble *offset = nullptr) const;

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
    de::ddouble pointOnSide(de::Vector2d const &point) const;

protected:
    de::dint property(world::DmuArgs &args) const;
    de::dint setProperty(world::DmuArgs const &args);

public:
#ifdef __CLIENT__
    /**
    * Returns @c true if the line qualifies for FakeRadio shadow casting (on planes).
    */
    bool isShadowCaster() const;
#endif

    /**
     * Returns the @em validCount of the line. Used by some legacy iteration algorithms
     * for marking lines as processed/visited.
     *
     * @todo Refactor away.
     */
    de::dint validCount() const;

    /// @todo Refactor away.
    void setValidCount(de::dint newValidCount);

    /**
     * Replace the specified edge vertex of the line.
     *
     * @attention Should only be called in map edit mode.
     *
     * @param to         If not @c 0 replace the To vertex; otherwise the From vertex.
     * @param newVertex  The replacement vertex.
     */
    void replaceVertex(de::dint to, Vertex &newVertex);

    /**
     * Returns a pointer to the line owner node for the specified edge vertex of the line.
     *
     * @param to  If not @c 0 return the owner for the To vertex; otherwise the From vertex.
     *
     * @deprecated Will be replaced with half-edge ring iterator/rover. -ds
     */
    LineOwner *vertexOwner(de::dint to) const;

    /**
     * Returns a pointer to the line owner for the specified edge @a vertex of the line.
     * If the vertex is not an edge vertex for the line then @c nullptr will be returned.
     */
    inline LineOwner *vertexOwner(Vertex const &vertex) const {
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

public:
    /**
     * Register the console commands and/or variables of this module.
     */
    static void consoleRegister();

private:
    DENG2_PRIVATE(d)
};

typedef Line::Side LineSide;
typedef Line::Side::Segment LineSideSegment;

Q_DECLARE_OPERATORS_FOR_FLAGS(Line::Side::SectionFlags)

#endif  // DENG_WORLD_LINE_H
