/** @file line.h World Map Line.
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

#ifndef DENG_WORLD_MAP_LINE
#define DENG_WORLD_MAP_LINE

#include <de/binangle.h>
#include <de/vector1.h>

#include <de/Vector>
#include <de/Error>

#include "MapElement"
#include "map/surface.h"
#include "map/vertex.h"
#include "p_dmu.h"

class HEdge;
class LineOwner;
class Sector;

// Internal flags:
#define LF_POLYOBJ              0x1 ///< Line is part of a polyobject.
#define LF_BSPWINDOW            0x2 ///< Line produced a BSP window. @todo Refactor away.

// Logical face identifiers:
/// @addtogroup map
///@{
#define FRONT                   0
#define BACK                    1
///@}

// Logical edge identifiers:
/// @addtogroup map
///@{
#define FROM                    0
#define TO                      1

/// Aliases:
#define START                   FROM
#define END                     TO
///@}

/**
 * @defgroup sideSectionFlags  Side Section Flags
 * @ingroup map
 */
///@{
#define SSF_MIDDLE              0x1
#define SSF_BOTTOM              0x2
#define SSF_TOP                 0x4
///@}

#ifdef __CLIENT__

/**
 * FakeRadio shadow data.
 * @ingroup map
 */
struct shadowcorner_t
{
    float corner;
    Sector *proximity;
    float pOffset;
    float pHeight;
};

/**
 * FakeRadio connected edge data.
 * @ingroup map
 */
struct edgespan_t
{
    float length;
    float shift;
};

#endif // __CLIENT__

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
 * @note Lines are @em not considered to define the geometry of a map. Instead a
 * line should be thought of as a finite line segment in the plane, according to
 * the standard definition of a line as used with an arrangement of lines in
 * computational geometry.
 *
 * @see http://en.wikipedia.org/wiki/Arrangement_of_lines
 *
 * @ingroup map
 */
class Line : public de::MapElement
{
public:
    /// Required sector attribution is missing. @ingroup errors
    DENG2_ERROR(MissingSectorError);

    /// The given side section identifier is invalid. @ingroup errors
    DENG2_ERROR(InvalidSectionIdError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

    /**
     * Logical side of which there are always two (a front and a back).
     */
    class Side : public de::MapElement
    {
    public:
        /// The referenced property does not exist. @ingroup errors
        DENG2_ERROR(UnknownPropertyError);

        /// The referenced property is not writeable. @ingroup errors
        DENG2_ERROR(WritePropertyError);

        class Section
        {
        public: /// @todo make private:
            Surface _surface;
            ddmobj_base_t _soundEmitter;

        public:
            Section(Side &side)
                : _surface(dynamic_cast<MapElement &>(side))
            {
                std::memset(&_soundEmitter, 0, sizeof(_soundEmitter));
            }

            Surface &surface() {
                return _surface;
            }

            Surface const &surface() const {
                return const_cast<Surface const &>(const_cast<Section &>(*this).surface());
            }

            ddmobj_base_t &soundEmitter() {
                return _soundEmitter;
            }

            ddmobj_base_t const &soundEmitter() const {
                return const_cast<ddmobj_base_t const &>(const_cast<Section &>(*this).soundEmitter());
            }
        };

        struct Sections /// @todo choose a better name
        {
            Section middle;
            Section bottom;
            Section top;

            Sections(Side &side)
                : middle(side),
                  bottom(side),
                  top(side)
            {}
        };

    public: /// @todo make private:
        /// Line owner of the side.
        Line &_line;

        /// Attributed sector.
        Sector *_sector;

        /// 1-based index of the associated sidedef in the archived map; otherwise @c 0.
        uint _sideDefArchiveIndex;

        /// Sections.
        Sections *_sections;

        /// Left-most half-edge on this side of the owning line.
        HEdge *_leftHEdge;

        /// Right-most half-edge on this side of the owning line.
        HEdge *_rightHEdge;

        /// Framecount of last time shadows were drawn on this side.
        int _shadowVisCount;

        /// @ref sdefFlags
        short _flags;

#ifdef __CLIENT__
        /// @todo Does not belong here - move to the map renderer. -ds
        struct FakeRadioData
        {
            /// Frame number of last update
            int updateCount;

            shadowcorner_t topCorners[2];
            shadowcorner_t bottomCorners[2];
            shadowcorner_t sideCorners[2];

            /// [left, right]
            edgespan_t spans[2];
        } _fakeRadioData;

#endif // __CLIENT__

    public:
        Side(Line &line, Sector *sector = 0);
        ~Side();

        /**
         * Returns the Line owner of the side.
         */
        Line &line();

        /// @copydoc line()
        Line const &line() const;

        /**
         * Returns @c true if this is the front side of the owning line.
         */
        bool isFront() const;

        /**
         * Returns @c true if this is the back side of the owning line.
         */
        inline bool isBack() const { return !isFront(); }

        /**
         * Returns @c true iff a Sector is attributed to the side.
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
         * Returns @c true iff Sections are defined for the side.
         */
        bool hasSections() const;

        /**
         * Returns the specified section of the side.
         *
         * @param sectionId  Identifier of the section to return.
         */
        Section &section(SideSection sectionId);

        /// @copydoc section()
        Section const &section(SideSection sectionId) const;

        /**
         * Returns the specified surface of the side.
         *
         * @param sectionId  Identifier of the surface to return.
         */
        inline Surface &surface(SideSection sectionId) {
            return section(sectionId).surface();
        }

        /// @copydoc surface()
        inline Surface const &surface(SideSection sectionId) const {
            return const_cast<Surface const &>(const_cast<Side *>(this)->surface(sectionId));
        }

        /**
         * Returns the middle surface of the side.
         */
        inline Surface &middle() { return surface(SS_MIDDLE); }

        /// @copydoc middle()
        inline Surface const &middle() const { return surface(SS_MIDDLE); }

        /**
         * Returns the bottom surface of the side.
         */
        inline Surface &bottom() { return surface(SS_BOTTOM); }

        /// @copydoc bottom()
        inline Surface const &bottom() const { return surface(SS_BOTTOM); }

        /**
         * Returns the top surface of the side.
         */
        inline Surface &top() { return surface(SS_TOP); }

        /// @copydoc top()
        inline Surface const &top() const { return surface(SS_TOP); }

        /**
         * Returns the specified sound emitter of the side.
         *
         * @param sectionId  Identifier of the sound emitter to return.
         */
        inline ddmobj_base_t &soundEmitter(SideSection sectionId) {
            return section(sectionId).soundEmitter();
        }

        /// @copydoc surface()
        inline ddmobj_base_t const &soundEmitter(SideSection sectionId) const {
            return const_cast<ddmobj_base_t const &>(const_cast<Side *>(this)->soundEmitter(sectionId));
        }

        /**
         * Returns the middle sound emitter of the side.
         */
        inline ddmobj_base_t &middleSoundEmitter() {
            return section(SS_MIDDLE).soundEmitter();
        }

        /// @copydoc middleSoundEmitter()
        inline ddmobj_base_t const &middleSoundEmitter() const {
            return section(SS_MIDDLE).soundEmitter();
        }

        /**
         * Update the middle sound emitter origin according to the point defined by
         * the owning line's vertices and the current @em sharp heights of the sector
         * on this side of the line.
         */
        void updateMiddleSoundEmitterOrigin();

        /**
         * Returns the bottom sound emitter (tee-hee) for the side.
         */
        inline ddmobj_base_t &bottomSoundEmitter() {
            return section(SS_BOTTOM).soundEmitter();
        }

        /// @copydoc bottomSoundEmitter()
        inline ddmobj_base_t const &bottomSoundEmitter() const {
            return section(SS_BOTTOM).soundEmitter();
        }

        /**
         * Update the bottom sound emitter origin according to the point defined by
         * the owning line's vertices and the current @em sharp heights of the sector
         * on this side of the line.
         */
        void updateBottomSoundEmitterOrigin();

        /**
         * Returns the top sound emitter for the side.
         */
        inline ddmobj_base_t &topSoundEmitter() {
            return section(SS_TOP).soundEmitter();
        }

        /// @copydoc topSoundEmitter()
        inline ddmobj_base_t const &topSoundEmitter() const {
            return section(SS_TOP).soundEmitter();
        }

        /**
         * Update the top sound emitter origin according to the point defined by the
         * owning line's vertices and the current @em sharp heights of the sector on
         * this side of the line.
         */
        void updateTopSoundEmitterOrigin();

        /**
         * Update the side's sound emitter origins according to the points defined by
         * the Line's vertices and the plane heights of the Sector on this side.
         * If no Sections are defined this is a no-op.
         */
        void updateAllSoundEmitterOrigins();

#ifdef __CLIENT__

        /**
         * Returns the FakeRadio data for the side.
         */
        FakeRadioData &fakeRadioData();

        /// @copydoc fakeRadioData()
        FakeRadioData const &fakeRadioData() const;

#endif // __CLIENT__

        /**
         * Returns the left-most HEdge for the side.
         */
        HEdge &leftHEdge() const;

        /**
         * Returns the right-most HEdge for the side.
         */
        HEdge &rightHEdge() const;

        /**
         * Update the tangent space normals of the side's surfaces according to the
         * points defined by the Line's vertices. If no Sections are defined this is
         * a no-op.
         */
        void updateSurfaceNormals();

        /**
         * Returns the @ref sdefFlags fro the side.
         */
        short flags() const;

        /**
         * Returns the frame number of the last time shadows were drawn for the side.
         */
        int shadowVisCount() const;

        /**
         * Change the "archive index" of the associated sidedef. The archive
         * index is the position of the sidedef in the archived map data. Note
         * that this index is unrelated to the "in map index" used by GameMap.
         *
         * @param newIndex  New 1-based index. Can be @c 0 signifying "no-index".
         */
        void setSideDefArchiveIndex(uint newIndex);

        /**
         * Get a property value, selected by DMU_* name.
         *
         * @param args  Property arguments.
         * @return  Always @c 0 (can be used as an iterator).
         */
        int property(setargs_t &args) const;

        /**
         * Update a property value, selected by DMU_* name.
         *
         * @param args  Property arguments.
         * @return  Always @c 0 (can be used as an iterator).
         */
        int setProperty(setargs_t const &args);
    };

public: /// @todo make private:
    /// Links to vertex line owner nodes:
    LineOwner *_vo1;
    LineOwner *_vo2;

    /// Public DDLF_* flags.
    int _flags;

    /// Internal LF_* flags.
    byte _inFlags;

public:
    Line(Vertex &from, Vertex &to,
            Sector *frontSector = 0,
            Sector *backSector  = 0);

    /**
     * Returns @c true iff the line is part of some Polyobj.
     */
    bool isFromPolyobj() const;

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
     * Returns @c true if the line is flagged @a flagsToTest.
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
     * Returns the original index of the line.
     */
    uint origIndex() const;

    /**
     * Change the original index of the line.
     *
     * @param newIndex  New original index.
     */
    void setOrigIndex(uint newIndex);

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
    inline Side &front() { return side(FRONT); }

    /// @copydoc front()
    inline Side const &front() const { return side(FRONT); }

    /**
     * Returns the logical Back side of the line.
     */
    inline Side &back() { return side(BACK); }

    /// @copydoc back()
    inline Side const &back() const { return side(BACK); }

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
    inline bool hasFrontSections() const { return hasSections(FRONT); }

    /**
     * Returns @c true iff Side::Sections are defined for the Back side of the line.
     */
    inline bool hasBackSections() const { return hasSections(BACK); }

    /**
     * Returns @c true iff a sector is attributed to the specified side of the line.
     *
     * @param back  If not @c 0 test the Back side; otherwise the Front side.
     */
    inline bool hasSector(int back) const { return side(back).hasSector(); }

    /**
     * Returns @c true iff a sector is attributed to the Front side of the line.
     */
    inline bool hasFrontSector() const { return hasSector(FRONT); }

    /**
     * Returns @c true iff a sector is attributed to the Back side of the line.
     */
    inline bool hasBackSector() const { return hasSector(BACK); }

    /**
     * Convenient accessor method for returning the sector attributed to the
     * specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector &sector(int back) { return side(back).sector(); }

    /// @copydoc sector()
    inline Sector const &sector(int back) const { return side(back).sector(); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the specified side of the line.
     *
     * @param back  If not @c 0 return the sector for the Back side; otherwise
     *              the sector of the Front side.
     */
    inline Sector *sectorPtr(int back) { return side(back).sectorPtr(); }

    /// @copydoc sector()
    inline Sector const *sectorPtr(int back) const { return side(back).sectorPtr(); }

    /**
     * Returns the sector attributed to the Front side of the line.
     */
    inline Sector &frontSector() { return sector(FRONT); }

    /// @copydoc backSector()
    inline Sector const &frontSector() const { return sector(FRONT); }

    /**
     * Returns the sector attributed to the Back side of the line.
     */
    inline Sector &backSector() { return sector(BACK); }

    /// @copydoc backSector()
    inline Sector const &backSector() const { return sector(BACK); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the front side of the line.
     */
    inline Sector *frontSectorPtr() { return sectorPtr(FRONT); }

    /// @copydoc frontSectorPtr()
    inline Sector const *frontSectorPtr() const { return sectorPtr(FRONT); }

    /**
     * Convenient accessor method for returning a pointer to the sector attributed
     * to the back side of the line.
     */
    inline Sector *backSectorPtr() { return sectorPtr(BACK); }

    /// @copydoc frontSectorPtr()
    inline Sector const *backSectorPtr() const { return sectorPtr(BACK); }

    /**
     * Returns @c true iff the line is considered @em self-referencing.
     * In this context, self-referencing (a term whose origins stem from the
     * DOOM modding community) means a two-sided line (which is to say that
     * a Sector is attributed to both logical sides of the line) where the
     * attributed sectors for each logical side are the same.
     */
    inline bool isSelfReferencing() const
    {
        return hasFrontSections() && hasBackSections() && frontSectorPtr() == backSectorPtr();
    }

    /**
     * Returns the specified edge vertex for the line.
     *
     * @param to  If not @c 0 return the To vertex; otherwise the From vertex.
     */
    Vertex &vertex(int to);

    /// @copydoc vertex()
    Vertex const &vertex(int to) const;

    /**
     * Convenient accessor method for returning the origin of the specified
     * edge vertex for the line.
     *
     * @see vertex()
     */
    inline const_pvec2d_t &vertexOrigin(int to) const
    {
        return vertex(to).origin();
    }

    /**
     * Returns a pointer to the line owner node for the specified edge vertex
     * of the line.
     *
     * @param to  If not @c 0 return the owner for the To vertex; otherwise the
     *            From vertex.
     */
    LineOwner *vertexOwner(int to) const;

    /**
     * Returns the From/Start vertex for the line.
     */
    inline Vertex &v1() { return vertex(FROM); }

    /// @copydoc v1()
    inline Vertex const &v1() const { return vertex(FROM); }

    /// @copydoc v1()
    /// An alias of v1().
    inline Vertex &from() { return v1(); }

    /// @copydoc from()
    /// An alias of v1().
    inline Vertex const &from() const { return v1(); }

    /**
     * Convenient accessor method for returning the origin of the From/Start
     * vertex for the line.
     *
     * @see v1()
     */
    inline const_pvec2d_t &v1Origin() const { return v1().origin(); }

    /// @copydoc v1Origin()
    /// An alias of v1Origin()
    inline const_pvec2d_t &fromOrigin() const { return v1Origin(); }

    /**
     * Returns a pointer to the line owner node for the From/Start vertex of the line.
     */
    inline LineOwner *v1Owner() const { return vertexOwner(FROM); }

    /**
     * Returns the To/End vertex for the line.
     */
    inline Vertex &v2() { return vertex(TO); }

    /// @copydoc v2()
    inline Vertex const &v2() const { return vertex(TO); }

    /// @copydoc v2()
    /// An alias of v2().
    inline Vertex &to() { return v2(); }

    /// @copydoc to()
    /// An alias of v2().
    inline Vertex const &to() const { return v2(); }

    /**
     * Convenient accessor method for returning the origin of the To/End
     * vertex for the line.
     *
     * @see v2()
     */
    inline const_pvec2d_t &v2Origin() const { return v2().origin(); }

    /// @copydoc v2Origin()
    /// An alias of v2Origin()
    inline const_pvec2d_t &toOrigin() const { return v2Origin(); }

    /**
     * Returns a pointer to the line owner node for the To/End vertex of the line.
     */
    inline LineOwner *v2Owner() const { return vertexOwner(TO); }

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
        coord_t v1Direction[2] = { direction().x, direction().y };
        return V2d_PointLineDistance(point, v1().origin(), v1Direction, offset);
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
        coord_t v1Direction[2] = { direction().x, direction().y };
        return V2d_PointOnLineSide(point, v1().origin(), v1Direction);
    }

    /// @copydoc pointOnSide()
    inline coord_t pointOnSide(coord_t x, coord_t y) const
    {
        coord_t point[2] = { x, y };
        return pointOnSide(point);
    }

    /**
     * Get a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int property(setargs_t &args) const;

    /**
     * Update a property value, selected by DMU_* name.
     *
     * @param args  Property arguments.
     * @return  Always @c 0 (can be used as an iterator).
     */
    int setProperty(setargs_t const &args);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_WORLD_MAP_LINE
