/** @file drawlist.h  Drawable primitive list.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DENG_CLIENT_RENDER_DRAWLIST_H
#define DENG_CLIENT_RENDER_DRAWLIST_H

#include "gl/gltextureunit.h"
#include "api_gl.h" // blendmode_e
#include "render/rendersystem.h"
#include <de/Vector>
#include <QList>

class Shard;

/// Semantic geometry group identifiers.
enum GeomGroup
{
    UnlitGeom = 0,  ///< Normal, unlit geometries.
    LitGeom,        ///< Normal, lit goemetries.
    SkyMaskGeom,    ///< Sky mask geometries.
    LightGeom,      ///< Dynamic light geometries.
    ShadowGeom,     ///< Map object and/or Fake Radio shadow geometries.
    ShineGeom       ///< Surface reflection geometries.
};

/// Logical drawing modes.
enum DrawMode
{
    DM_SKYMASK,
    DM_ALL,
    DM_LIGHT_MOD_TEXTURE,
    DM_FIRST_LIGHT,
    DM_TEXTURE_PLUS_LIGHT,
    DM_UNBLENDED_TEXTURE_AND_DETAIL,
    DM_BLENDED,
    DM_BLENDED_FIRST_LIGHT,
    DM_NO_LIGHTS,
    DM_WITHOUT_TEXTURE,
    DM_LIGHTS,
    DM_MOD_TEXTURE,
    DM_MOD_TEXTURE_MANY_LIGHTS,
    DM_UNBLENDED_MOD_TEXTURE_AND_DETAIL,
    DM_BLENDED_MOD_TEXTURE,
    DM_ALL_DETAILS,
    DM_BLENDED_DETAILS,
    DM_SHADOW,
    DM_SHINY,
    DM_MASKED_SHINY,
    DM_ALL_SHINY
};

/// Virtual/logical texture unit indices. These map to real GL texture units.
enum texunitid_t
{
    TU_PRIMARY = 0,
    TU_PRIMARY_DETAIL,
    TU_INTER,
    TU_INTER_DETAIL,
    NUM_TEXTURE_UNITS
};

typedef uint TexUnitMap[MAX_TEX_UNITS];

/**
 * A buffered list of drawable GL primitives and/or GL commands.
 */
class DrawList
{
public:
    struct Spec
    {
        GeomGroup group;
        typedef de::GLTextureUnit UnitSpecs[NUM_TEXTURE_UNITS];
        UnitSpecs texunits;

        Spec(GeomGroup group = UnlitGeom) : group(group)
        {}

        inline de::GLTextureUnit &unit(int index) {
            DENG2_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }

        inline de::GLTextureUnit const &unit(int index) const {
            DENG2_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }
    };

public:
    /**
     * Construct a new draw list with the given @a spec (a copy is made).
     */
    DrawList(Spec const &spec);

    /**
     * Provides mutable access to the list's specification. Note that any changes
     * to this configuration will affect @em all geometries in the list.
     */
    Spec &spec();

    /**
     * Provides immutable access to the list's specification.
     */
    Spec const &spec() const;

    /**
     * Draw all geometry Shards in the list, in the order in which they were added.
     */
    void draw(DrawMode mode, TexUnitMap const &texUnitMap) const;

    /**
     * Add the geometry @a shard onto the end of the DrawList. The order of the
     * list determines draw order.
     *
     * @note It is assumed that the shard's draw list specification is compatible
     * with "this" list (glitches will occur if its not).
     */
    void append(Shard const &shard);

    /**
     * Add each of the geometry @a shards onto the end of the DrawList. The order
     * of the list determines draw order.
     *
     * @note It is assumed that the shard's draw list specification is compatible
     * with "this" list (glitches will occur if its not).
     */
    void append(QList<Shard *> const &shards);

    /// @see append()
    inline DrawList &operator << (Shard const &shard) {
        append(shard);
        return *this;
    }

    /// @see append()
    inline DrawList &operator << (QList<Shard *> const &shardList) {
        append(shardList);
        return *this;
    }

    /**
     * Returns @c true iff there are no geometries in the list.
     */
    bool isEmpty() const;

    /**
     * Empty the list of all geometries and rewind the read/write cursor.
     */
    void clear();

    /**
     * Return the read/write cursor to the beginning of the list, retaining any
     * storage allocated so that it can be reused.
     *
     * To be called at the beginning of a new render frame before any geometries
     * are added to the list.
     */
    void rewind();

private:
    DENG2_PRIVATE(d)
};

typedef DrawList::Spec DrawListSpec;

#endif // DENG_CLIENT_RENDER_DRAWLIST_H
