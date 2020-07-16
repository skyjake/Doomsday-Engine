/** @file billboard.h  Rendering billboard "sprites".
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_CLIENT_RENDER_BILLBOARD_H
#define DE_CLIENT_RENDER_BILLBOARD_H

#include "dd_types.h"
#include "resource/clientmaterial.h"
#include "resource/materialanimator.h"
#include "resource/materialvariantspec.h"

namespace world { class BspLeaf; }
struct vissprite_t;

/**
 * Billboard drawing arguments for a masked wall.
 *
 * A sort of a sprite, I guess... Masked walls must be rendered sorted with
 * sprites, so no artifacts appear when sprites are seen behind masked walls.
 *
 * @ingroup render
 */
struct drawmaskedwallparams_t
{
    MaterialAnimator *animator;
    blendmode_t blendMode;         ///< Blendmode to be used when drawing (two sided mid textures only)

    struct wall_vertex_s {
        float pos[3];         ///< x y and z coordinates.
        float color[4];
    } vertices[4];

    double texOffset[2];
    float texCoord[2][2];     ///< u and v coordinates.

    DGLuint modTex;                ///< Texture to modulate with.
    float modTexCoord[2][2];  ///< [top-left, bottom-right][x, y]
    float modColor[4];
};

void Rend_DrawMaskedWall(const drawmaskedwallparams_t &parms);

/**
 * Billboard drawing arguments for a "player" sprite (HUD sprite).
 * @ingroup render
 */
struct rendpspriteparams_t
{
    float pos[2];           ///< [X, Y] Screen-space position.
    float width;
    float height;

    ClientMaterial *mat;
    float texOffset[2];
    dd_bool texFlip[2];          ///< [X, Y] Flip along the specified axis.

    float ambientColor[4];
    de::duint vLightListIdx;
};

void Rend_DrawPSprite(const rendpspriteparams_t &parms);

/**
 * Billboard drawing arguments for a map entity, sprite visualization.
 * @ingroup render
 */
struct drawspriteparams_t
{
    dd_bool noZWrite;
    blendmode_t blendMode;
    MaterialAnimator *matAnimator;
    dd_bool matFlip[2];             ///< [S, T] Flip along the specified axis.
    world::BspLeaf *bspLeaf;
};

void Rend_DrawSprite(const vissprite_t &spr);

const de::MaterialVariantSpec &Rend_SpriteMaterialSpec(int tclass = 0, int tmap = 0);

/**
 * @defgroup rendFlareFlags  Flare renderer flags
 * @ingroup render
 * @{
 */
#define RFF_NO_PRIMARY      0x1  ///< Do not draw a primary flare (aka halo).
#define RFF_NO_TURN         0x2  ///< Flares do not turn in response to viewangle/viewdir
/**@}*/

/**
 * Billboard drawing arguments for a lens flare.
 *
 * @see H_RenderHalo()
 * @ingroup render
 */
struct drawflareparams_t {
    de::dbyte  flags; ///< @ref rendFlareFlags.
    int   size;
    float color[3];
    de::dbyte  factor;
    float xOff;
    DGLuint    tex; ///< Flaremap if flareCustom ELSE (flaretexName id. Zero = automatical)
    float mul; ///< Flare brightness factor.
    dd_bool    isDecoration;
    int   lumIdx;
};

DE_EXTERN_C int alwaysAlign;
DE_EXTERN_C int spriteLight;
DE_EXTERN_C int useSpriteAlpha;
DE_EXTERN_C int useSpriteBlend;
DE_EXTERN_C int noSpriteZWrite;
DE_EXTERN_C de::dbyte noSpriteTrans;
DE_EXTERN_C de::dbyte devNoSprites;

DE_EXTERN_C void Rend_SpriteRegister();

#endif  // CLIENT_RENDER_BILLBOARD_H
