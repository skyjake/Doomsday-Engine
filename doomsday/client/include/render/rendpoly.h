/** @file rendpoly.h RendPoly data buffers
 * @ingroup render
 *
 * @author Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_RENDPOLY_H
#define DENG_RENDER_RENDPOLY_H

#include "api_gl.h"
#include "color.h"

#include <de/vector1.h> /// @todo remove me.
#include <de/Vector>

#include "Texture"

typedef struct rvertex_s {
    float pos[3];
#ifdef __cplusplus
    rvertex_s(float x = 0, float y = 0, float z = 0) {
        pos[0] = x; pos[1] = y; pos[2] = z;
    }
    rvertex_s(de::Vector3f const &_pos) {
        pos[0] = _pos.x; pos[1] = _pos.y; pos[2] = _pos.z;
    }
    rvertex_s(rvertex_s const &other) {
        std::memcpy(pos, other.pos, sizeof(pos));
    }
#endif
} rvertex_t;

typedef struct rtexcoord_s {
    float st[2];
#ifdef __cplusplus
    rtexcoord_s(float s = 0, float t = 0) {
        st[0] = s; st[1] = t;
    }
    rtexcoord_s(de::Vector2f const &_st) {
        st[0] = _st.x; st[1] = _st.y;
    }
    rtexcoord_s(rtexcoord_s const &other) {
        std::memcpy(st, other.st, sizeof(st));
    }
#endif
} rtexcoord_t;

/**
 * Symbolic identifiers for (virtual) texture units.
 */
typedef enum {
    RTU_PRIMARY = 0,
    RTU_PRIMARY_DETAIL,
    RTU_INTER,
    RTU_INTER_DETAIL,
    RTU_REFLECTION,
    RTU_REFLECTION_MASK,
    NUM_TEXMAP_UNITS
} rtexmapunitid_t;

/**
 * @defgroup textureUnitFlags  Texture Unit Flags
 * @ingroup flags
 */
///@{
#define TUF_TEXTURE_IS_MANAGED    0x1 ///< A managed texture is bound to this unit.
///@}

typedef struct rtexmapunit_texture_s {
    union {
        struct {
            DGLuint name; ///< Texture used on this layer (if any).
            int magMode; ///< GL texture magnification filter.
            int wrapS; ///< GL texture S axis wrap mode.
            int wrapT; ///< GL texture T axis wrap mode.
        } gl;
        de::TextureVariant *variant;
    };
    /// @ref textureUnitFlags
    int flags;

#ifdef __cplusplus
    inline bool hasTexture() const
    {
        if(flags & TUF_TEXTURE_IS_MANAGED)
            return variant && variant->glName() != 0;
        return gl.name != 0;
    }
#endif
} rtexmapunit_texture_t;

/**
 * Texture unit state (POD).
 *
 * A simple Record data structure for storing properties used for
 * configuring a GL texture unit during render.
 */
typedef struct rtexmapuint_s {
    /// Info about the bound texture for this unit.
    rtexmapunit_texture_t texture;

    /// Currently used only with reflection.
    blendmode_t blendMode;

    /// Opacity of this layer [0..1].
    float opacity;

    /// Texture-space scale multiplier.
    vec2f_t scale;

    /// Texture-space origin translation (unscaled).
    vec2f_t offset;

#ifdef __cplusplus
    bool hasTexture() const { return texture.hasTexture(); }
#endif
} rtexmapunit_t;

extern byte rendInfoRPolys;

void R_PrintRendPoolInfo(void);

/**
 * @note Should be called at the start of each map.
 */
void R_InitRendPolyPools(void);

/**
 * Retrieves a batch of rvertex_t.
 * Possibly allocates new if necessary.
 *
 * @param num  The number of verts required.
 *
 * @return  Ptr to array of rvertex_t
 */
rvertex_t *R_AllocRendVertices(uint num);

/**
 * Retrieves a batch of ColorRawf.
 * Possibly allocates new if necessary.
 *
 * @param num  The number of verts required.
 *
 * @return  Ptr to array of ColorRawf
 */
ColorRawf *R_AllocRendColors(uint num);

/**
 * Retrieves a batch of rtexcoord_t.
 * Possibly allocates new if necessary.
 *
 * @param num  The number required.
 *
 * @return  Ptr to array of rtexcoord_t
 */
rtexcoord_t *R_AllocRendTexCoords(uint num);

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param rvertices  Ptr to array of rvertex_t to mark unused.
 */
void R_FreeRendVertices(rvertex_t *rvertices);

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param rcolors  Ptr to array of ColorRawf to mark unused.
 */
void R_FreeRendColors(ColorRawf *rcolors);

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param rtexcoords  Ptr to array of rtexcoord_t to mark unused.
 */
void R_FreeRendTexCoords(rtexcoord_t *rtexcoords);

/// Manipulators, for convenience.
void Rtu_Init(rtexmapunit_t *rtu);

boolean Rtu_HasTexture(rtexmapunit_t const *rtu);

/// Change the scale property.
void Rtu_SetScale(rtexmapunit_t *rtu, de::Vector2f const &st);

inline void Rtu_SetScale(rtexmapunit_t *rtu, float s, float t) {
    Rtu_SetScale(rtu, de::Vector2f(s, t));
}

/**
 * Multiply the offset and scale properties by @a scalar.
 * @note @a scalar is applied to both scale and offset properties
 * however the offset remains independent from scale (i.e., it is
 * still considered "unscaled").
 */
void Rtu_Scale(rtexmapunit_t *rtu, float scalar);

void Rtu_ScaleST(rtexmapunit_t *rtu, de::Vector2f const &scaleST);

/// Change the offset property.
void Rtu_SetOffset(rtexmapunit_t *rtu, de::Vector2f const &st);

inline void Rtu_SetOffset(rtexmapunit_t *rtu, float s, float t) {
    Rtu_SetOffset(rtu, de::Vector2f(s, t));
}

/// Translate the offset property.
void Rtu_TranslateOffset(rtexmapunit_t *rtu, de::Vector2f const &st);

inline void Rtu_TranslateOffset(rtexmapunit_t *rtu, float s, float t)
{
    Rtu_TranslateOffset(rtu, de::Vector2f(s, t));
}

#endif // DENG_RENDER_RENDPOLY_H
