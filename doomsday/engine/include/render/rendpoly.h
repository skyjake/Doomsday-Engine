/**
 * @file rendpoly.h RendPoly data buffers
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_RENDPOLY_H
#define LIBDENG_RENDER_RENDPOLY_H

#include "color.h"

#ifdef __cplusplus
extern "C" {
#endif

struct walldivnode_s;

typedef struct rvertex_s {
    float pos[3];
} rvertex_t;

typedef struct rtexcoord_s {
    float st[2];
} rtexcoord_t;

/**
 * Logical texture unit indices.
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
        } gl;
        struct texturevariant_s *variant;
    };
    /// @ref textureUnitFlags
    int flags;
} rtexmapunit_texture_t;

/**
 * Texture unit state. POD.
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
 * @param vertices  Ptr to array of rvertex_t to mark unused.
 */
void R_FreeRendVertices(rvertex_t *rvertices);

/**
 * Doesn't actually free anything. Instead, mark them as unused ready for
 * the next time a batch of rendvertex_t is needed.
 *
 * @param vertices  Ptr to array of ColorRawf to mark unused.
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
void Rtu_SetScale(rtexmapunit_t *rtu, float s, float t);
void Rtu_SetScalev(rtexmapunit_t *rtu, float const st[2]);

/**
 * Multiply the offset and scale properties by @a scalar.
 * @note @a scalar is applied to both scale and offset properties
 * however the offset remains independent from scale (i.e., it is
 * still considered "unscaled").
 */
void Rtu_Scale(rtexmapunit_t *rtu, float scalar);
void Rtu_ScaleST(rtexmapunit_t *rtu, float const scalarST[2]);

/// Change the offset property.
void Rtu_SetOffset(rtexmapunit_t *rtu, float s, float t);
void Rtu_SetOffsetv(rtexmapunit_t *rtu, float const st[2]);

/// Translate the offset property.
void Rtu_TranslateOffset(rtexmapunit_t *rtu, float s, float t);
void Rtu_TranslateOffsetv(rtexmapunit_t *rtu, float const st[2]);

void R_DivVerts(rvertex_t *dst, rvertex_t const *src,
    struct walldivnode_s *leftDivFirst, uint leftDivCount,
    struct walldivnode_s *rightDivFirst, uint rightDivCount);

void R_DivTexCoords(rtexcoord_t *dst, rtexcoord_t const *src,
    struct walldivnode_s *leftDivFirst, uint leftDivCount,
    struct walldivnode_s *rightDivFirst, uint rightDivCount,
    float bL, float tL, float bR, float tR);

void R_DivVertColors(ColorRawf *dst, ColorRawf const *src,
    struct walldivnode_s *leftDivFirst, uint leftDivCount,
    struct walldivnode_s *rightDivFirst, uint rightDivCount,
    float bL, float tL, float bR, float tR);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_RENDPOLY_H */
