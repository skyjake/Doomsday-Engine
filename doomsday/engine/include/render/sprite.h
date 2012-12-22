/**
 * @file render/sprite.h Rendering Map Objects as 2D Sprites.
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_RENDER_SPRITE_H
#define LIBDENG_RENDER_SPRITE_H

#include "resource/materialvariant.h"

/// @addtogroup render
///@{

typedef struct rendpspriteparams_s
{
    float pos[2]; // {X, Y} Screen-space position.
    float width, height;

    struct material_s* mat;
    float texOffset[2];
    boolean texFlip[2]; // {X, Y} Flip along the specified axis.

    float ambientColor[4];
    uint vLightListIdx;
} rendpspriteparams_t;

DENG_EXTERN_C int spriteLight, useSpriteAlpha, useSpriteBlend;
DENG_EXTERN_C byte noSpriteTrans;
DENG_EXTERN_C byte devNoSprites;

#ifdef __cplusplus
extern "C" {
#endif

void Rend_SpriteRegister(void);

#ifdef __CLIENT__

/**
 * Render sprites, 3D models, masked wall segments and halos, ordered
 * back to front. Halos are rendered with Z-buffer tests and writes
 * disabled, so they don't go into walls or interfere with real objects.
 * It means that halos can be partly occluded by objects that are closer
 * to the viewpoint, but that's the price to pay for not having access to
 * the actual Z-buffer per-pixel depth information. The other option would
 * be for halos to shine through masked walls, sprites and models, which
 * looks even worse. (Plus, they are *halos*, not real lens flares...)
 */
void Rend_DrawMasked(void);

/**
 * Draws 2D HUD sprites. If they were already drawn 3D, this won't do anything.
 */
void Rend_Draw2DPlayerSprites(void);

void Rend_Draw3DPlayerSprites(void);

materialvariantspecification_t const* Sprite_MaterialSpec(int tclass, int tmap);

void Rend_RenderSprite(rendspriteparams_t const* params);

#endif // __CLIENT__

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_RENDER_SPRITE_H */
