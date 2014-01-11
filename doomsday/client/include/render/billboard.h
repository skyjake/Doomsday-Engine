/** @file billboard.h  Rendering billboard "sprites".
 *
 * @ingroup render
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_RENDER_BILLBOARD_H
#define DENG_CLIENT_RENDER_BILLBOARD_H

#include "dd_types.h"
#include "Material"
#include "MaterialVariantSpec"

class BspLeaf;

/**
 * Billboard drawing arguments for a masked wall.
 *
 * A sort of a sprite, I guess... Masked walls must be rendered sorted with
 * sprites, so no artifacts appear when sprites are seen behind masked walls.
 */
struct drawmaskedwallparams_t
{
    void *material; /// MaterialVariant
    blendmode_t blendMode; ///< Blendmode to be used when drawing
                               /// (two sided mid textures only)
    struct wall_vertex_s {
        float pos[3]; ///< x y and z coordinates.
        float color[4];
    } vertices[4];

    double texOffset[2];
    float texCoord[2][2]; ///< u and v coordinates.

    DGLuint modTex; ///< Texture to modulate with.
    float modTexCoord[2][2]; ///< [top-left, bottom-right][x, y]
    float modColor[4];
};

void Rend_DrawMaskedWall(drawmaskedwallparams_t const &parms);

/**
 * Billboard drawing arguments for a "player" sprite (HUD sprite).
 */
struct rendpspriteparams_t
{
    float pos[2]; // {X, Y} Screen-space position.
    float width, height;

    Material *mat;
    float texOffset[2];
    dd_bool texFlip[2]; // {X, Y} Flip along the specified axis.

    float ambientColor[4];
    uint vLightListIdx;
};

void Rend_DrawPSprite(rendpspriteparams_t const &parms);

/**
 * Billboard drawing arguments for a map entity, sprite visualization.
 */
struct drawspriteparams_t
{
// Position/Orientation/Scale
    coord_t center[3]; // The real center point.
    coord_t srvo[3]; // Short-range visual offset.
    coord_t distance; // Distance from viewer.
    dd_bool viewAligned;

// Appearance
    dd_bool noZWrite;
    blendmode_t blendMode;

    // Material:
    void *material; /// MaterialVariant
    dd_bool matFlip[2]; // [S, T] Flip along the specified axis.

    // Lighting/color:
    float ambientColor[4];
    uint vLightListIdx;

// Misc
    BspLeaf *bspLeaf;
};

void Rend_DrawSprite(drawspriteparams_t const &parms);

de::MaterialVariantSpec const &Rend_SpriteMaterialSpec(int tclass = 0, int tmap = 0);

/**
 * @defgroup rendFlareFlags  Flare renderer flags
 * @{
 */
#define RFF_NO_PRIMARY      0x1 ///< Do not draw a primary flare (aka halo).
#define RFF_NO_TURN         0x2 ///< Flares do not turn in response to viewangle/viewdir
/**@}*/

/**
 * Billboard drawing arguments for a lens flare.
 *
 * @see H_RenderHalo()
 */
struct drawflareparams_t
{
    byte flags; // @ref rendFlareFlags.
    int size;
    float color[3];
    byte factor;
    float xOff;
    DGLuint tex; // Flaremap if flareCustom ELSE (flaretexName id. Zero = automatical)
    float mul; // Flare brightness factor.
    dd_bool isDecoration;
    int lumIdx;
};

DENG_EXTERN_C int alwaysAlign;
DENG_EXTERN_C int spriteLight, useSpriteAlpha, useSpriteBlend;
DENG_EXTERN_C int noSpriteZWrite;
DENG_EXTERN_C byte noSpriteTrans;
DENG_EXTERN_C byte devNoSprites;

DENG_EXTERN_C void Rend_SpriteRegister();

#endif // DENG_CLIENT_RENDER_BILLBOARD_H
