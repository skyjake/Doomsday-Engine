/** @file drawlist.h  Drawable primitive list.
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

#ifndef DENG_CLIENT_RENDER_DRAWLIST_H
#define DENG_CLIENT_RENDER_DRAWLIST_H

#include "de_platform.h"
#include "gl/gl_main.h"
#include <de/GLBuffer>
#include <de/Vector>
#include <QFlags>

struct GLTextureUnit;

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

/**
 * Drawing condition flags.
 *
 * @todo Most of these are actually list specification parameters. Rather than
 * set them each time an identified list is drawn it would be better to record
 * in the list itself. -ds
 */
enum DrawCondition
{
    NoBlend            = 0x00000001,
    Blend              = 0x00000002,
    SetLightEnv0       = 0x00000004,
    SetLightEnv1       = 0x00000008,
    JustOneLight       = 0x00000010,
    ManyLights         = 0x00000020,
    SetBlendMode       = 0x00000040, // Primitive-specific blending.
    SetMatrixDTexture0 = 0x00000080,
    SetMatrixDTexture1 = 0x00000100,
    SetMatrixTexture0  = 0x00000200,
    SetMatrixTexture1  = 0x00000400,
    NoColor            = 0x00000800,

    Skip               = 0x80000000,

    SetLightEnv        = SetLightEnv0 | SetLightEnv1,
    SetMatrixDTexture  = SetMatrixDTexture0 | SetMatrixDTexture1,
    SetMatrixTexture   = SetMatrixTexture0 | SetMatrixTexture1
};

Q_DECLARE_FLAGS(DrawConditions, DrawCondition)
Q_DECLARE_OPERATORS_FOR_FLAGS(DrawConditions)

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
        GLTextureUnit texunits[NUM_TEXTURE_UNITS];

        inline GLTextureUnit &unit(int index) {
            DENG2_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }

        inline GLTextureUnit const &unit(int index) const {
            DENG2_ASSERT(index >= 0 && index < NUM_TEXTURE_UNITS);
            return texunits[index];
        }
    };

public:
    /**
     * Construct a new draw list.
     *
     * @param spec  List specification. A copy is made.
     */
    DrawList(Spec const &spec);

    /**
     * Write the given geometry primitive to the list.
     *
     * @param primitive       Type identifier for the GL primitive being written.
     * @param isLit           @todo Retrieve from list specification?
     * @param vertCount       Number of vertices being written.
     * @param posCoods        Map space position coordinates for each vertex.
     * @param colorCoords     Color coordinates for each vertex (if any).
     * @param texCoords       @em Primary texture coordinates for each vertex (if any).
     * @param interTexCoords  @em Inter texture coordinates for each vertex (if any).
     * @param modTexture      GL name of the modulation texture (if any).
     * @param modColor        Modulation color (if any).
     * @param modTexCoords    Modulation texture coordinates for each vertex (if any).
     */
    DrawList &write(de::gl::Primitive primitive, blendmode_t blendMode,
        de::Vector2f const &texScale, de::Vector2f const &texOffset,
        de::Vector2f const &detailTexScale, de::Vector2f const &detailTexOffset,
        bool isLit, uint vertCount,
        de::Vector3f const *posCoords, de::Vector4f const *colorCoords = 0,
        de::Vector2f const *texCoords = 0, de::Vector2f const *interTexCoords = 0,
        DGLuint modTexture = 0, de::Vector3f const *modColor = 0,
        de::Vector2f const *modTexCoords = 0);

    /**
     * Draws all geometries in the list, in write order, which pass the supplied
     * draw @a conditions. If no conditions are specified then all geometry is
     * considered eligible and will be drawn.
     */
    void draw(DrawConditions conditions, TexUnitMap const &texUnitMap) const;

    /**
     * Returns @c true iff there are no commands/geometries in the list.
     */
    bool isEmpty() const;

    /**
     * Clear the list of all buffered GL commands, returning it to the default,
     * empty state.
     */
    void clear();

    /**
     * Return the read/write cursor to the beginning of the list, retaining all
     * allocated storage for buffered GL commands so that it can be reused.
     *
     * To be called at the beginning of a new render frame before any geometry
     * is written to the list.
     */
    void rewind();

    /**
     * Provides mutable access to the list's specification. Note that any changes
     * to this configuration will affect @em all geometry in the list.
     */
    Spec &spec();

    /**
     * Provides immutable access to the list's specification.
     */
    Spec const &spec() const;

private:
    DENG2_PRIVATE(d)
};

typedef DrawList::Spec DrawListSpec;

#endif // DENG_CLIENT_RENDER_DRAWLIST_H
