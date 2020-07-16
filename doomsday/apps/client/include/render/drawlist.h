/** @file drawlist.h  Drawable primitive list.
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

#ifndef CLIENT_RENDER_DRAWLIST_H
#define CLIENT_RENDER_DRAWLIST_H

#include <array>
#include <de/glbuffer.h>
#include <de/vector.h>
#include "api_gl.h" // blendmode_e
#include "gl/gltextureunit.h"

struct Store;

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

struct AttributeSpec
{
    enum Semantic
    {
        TexCoord0,
        TexCoord1,
        ModTexCoord,

        NUM_SEMANTICS
    };
};
typedef std::array<int, MAX_TEX_UNITS> TexUnitMap;

/**
 * A list of drawable GL geometry primitives (buffered) and optional GL attribute/state commands.
 *
 * Each list is expected to contain a batch (set) of one or more geometry primitives which have been
 * pre-prepared for uploading to GL from their backing store (buffer). Primitives should be batched
 * together in order to minimize the number of GL state changes when drawing geometry.
 *
 * Presently @ref DrawLists (class) is responsible for managing the lists and assigning list(s) for
 * a given primitive (according to the current logic for geometry batching).
 */
class DrawList
{
public:
    struct Spec
    {
        GeomGroup group;
        de::GLTextureUnit texunits[NUM_TEXTURE_UNITS];

        Spec(GeomGroup group = UnlitGeom) : group(group)
        {}

        inline de::GLTextureUnit &unit(int index) {
            DE_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }

        inline const de::GLTextureUnit &unit(int index) const {
            DE_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }
    };

    typedef de::List<de::duint> Indices;

    struct PrimitiveParams
    {
        de::gfx::Primitive type;

        // GL state and flags.
        enum Flag {
            Unlit      = 0,
            OneLight   = 0x1000,
            ManyLights = 0x2000
        };

        de::duint32 flags_blendMode;
        de::Vec2f   texScale;
        de::Vec2f   texOffset;
        de::Vec2f   detailTexScale;
        de::Vec2f   detailTexOffset;
        DGLuint     modTexture; ///< GL-name of the modulation texture; otherwise @c 0.
        de::Vec3f   modColor;   ///< Modulation color.

        PrimitiveParams(de::gfx::Primitive type,
                        de::Vec2f          texScale        = de::Vec2f(1, 1),
                        de::Vec2f          texOffset       = de::Vec2f(0, 0),
                        de::Vec2f          detailTexScale  = de::Vec2f(1, 1),
                        de::Vec2f          detailTexOffset = de::Vec2f(0, 0),
                        de::Flags          flags           = Unlit,
                        blendmode_t        blendMode       = BM_NORMAL,
                        DGLuint            modTexture      = 0,
                        de::Vec3f          modColor        = de::Vec3f());
    };

public:
    /**
     * Construct a new draw list.
     *
     * @param spec  List specification. A copy is made.
     */
    DrawList(const Spec &spec);

    /**
     * Write indices for a (buffered) geometry primitive to the list.
     *
     * @param buffer           Geometry buffer containing the primitive to write.
     *                         It is the caller's responsibility to ensure this data
     *                         remains accessible and valid while this DrawList is used
     *                         (i.e., until a @ref clear(), rewind() or the
     *                         list itself is destroyed).
     * @param primParams       GL primitive parameters.
     * @param indices          Indices for the vertex elements in @a buffer. A copy is made.
     */
    DrawList &write(const Store &          buffer,
                    const de::duint *      indices,
                    int                    indexCount,
                    const PrimitiveParams &primParms);

    DrawList &write(const Store &      buffer,
                    const de::duint *  indices,
                    int                indexCount,
                    de::gfx::Primitive primitiveType); // using default parameters

    DrawList &write(const Store &buffer, const Indices &indices, const PrimitiveParams &primParms);

    DrawList &write(const Store &      buffer,
                    const Indices &    indices,
                    de::gfx::Primitive primitiveType); // using default parameters

    void draw(DrawMode mode, const TexUnitMap &texUnitMap) const;

    /**
     * Returns @c true iff there are no commands/geometries in the list.
     */
    bool isEmpty() const;

    /**
     * Clear the list of all buffered GL commands, returning it to the default, empty state.
     */
    void clear();

    /**
     * Return the read/write cursor to the beginning of the list, retaining all allocated
     * storage for buffered GL commands so that it can be reused.
     *
     * To be called at the beginning of a new render frame before any geometry is written
     * to the list.
     */
    void rewind();

    /**
     * Provides mutable access to the list's specification. Note that any changes to this
     * configuration will affect @em all geometry in the list.
     */
    Spec &spec();

    /**
     * Provides immutable access to the list's specification.
     */
    const Spec &spec() const;

public:
    static void reserveSpace(Indices &idx, uint count);

private:
    DE_PRIVATE(d)
};

typedef DrawList::Spec DrawListSpec;

#endif  // CLIENT_RENDER_DRAWLIST_H
