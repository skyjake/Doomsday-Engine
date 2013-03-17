/** @file sidedef.h Map SideDef.
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

#ifndef LIBDENG_MAP_SIDEDEF
#define LIBDENG_MAP_SIDEDEF

#include <de/Error>
#include "resource/r_data.h"
#include "map/p_dmu.h"
#include "map/surface.h"
#include "MapElement"

class LineDef;

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
 * @attention SideDef is in the process of being replaced by lineside_t. All
 * data/values which concern the geometry of surfaces should be relocated to
 * lineside_t. There is no need to model the side of map's line as an object
 * in Doomsday when a flag would suffice. -ds
 *
 * @ingroup map
 */
class SideDef : public de::MapElement
{
public:
    /// The given surface section identifier is invalid. @ingroup errors
    DENG2_ERROR(InvalidSectionError);

    /// The referenced property does not exist. @ingroup errors
    DENG2_ERROR(UnknownPropertyError);

    /// The referenced property is not writeable. @ingroup errors
    DENG2_ERROR(WritePropertyError);

public:
    /// Component section surfaces:
    Surface _middleSurface;
    Surface _bottomSurface;
    Surface _topSurface;

    /// Owning line of the sidedef.
    LineDef *_line;

    /// @ref sdefFlags
    short _flags;

    /// @todo This temporary load-time data does not belong here. -ds
    struct {
        // Sidedef index. Always valid after loading & pruning.
        int index;
        int refCount;
    } _buildData;

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
    SideDef();
    ~SideDef();

    /// @todo Refactor away.
    SideDef &operator = (SideDef const &other);

    /**
     * Returns the line which owns the sidedef.
     */
    LineDef &line() const;

    /**
     * Returns the specified surface of the sidedef.
     *
     * @param surface  Identifier of the surface to return.
     */
    Surface &surface(int surface);

    /// @copydoc section()
    Surface const &surface(int surface) const;

    /**
     * Returns the middle surface of the sidedef.
     */
    inline Surface &middle() { return surface(SS_MIDDLE); }

    /// @copydoc middle()
    inline Surface const &middle() const { return surface(SS_MIDDLE); }

    /**
     * Returns the bottom surface of the sidedef.
     */
    inline Surface &bottom() { return surface(SS_BOTTOM); }

    /// @copydoc middle()
    inline Surface const &bottom() const { return surface(SS_BOTTOM); }

    /**
     * Returns the middle surface of the sidedef.
     */
    inline Surface &top() { return surface(SS_TOP); }

    /// @copydoc middle()
    inline Surface const &top() const { return surface(SS_TOP); }

    /**
     * Returns the @ref sdefFlags of the sidedef.
     */
    short flags() const;

    /**
     * Update the side's map space surface base origins according to the points
     * defined by the associated LineDef's vertices and the plane heights of the
     * Sector on this side. If no LineDef is presently associated this is a no-op.
     */
    void updateSoundEmitterOrigins();

    /**
     * Update the side's map space surface tangents according to the points
     * defined by the associated LineDef's vertices. If no LineDef is presently
     * associated this is a no-op.
     */
    void updateSurfaceTangents();

#ifdef __CLIENT__

    /**
     * Returns the FakeRadio data for the sidedef.
     */
    FakeRadioData &fakeRadioData();

    /// @copydoc fakeRadioData()
    FakeRadioData const &fakeRadioData() const;

#endif // __CLIENT__

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

#endif // LIBDENG_MAP_SIDEDEF
