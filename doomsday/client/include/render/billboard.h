/** @file billboard.h  Rendering billboard "sprites".
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

/// @addtogroup render
///@{

typedef struct rendpspriteparams_s
{
    float pos[2]; // {X, Y} Screen-space position.
    float width, height;

    Material *mat;
    float texOffset[2];
    boolean texFlip[2]; // {X, Y} Flip along the specified axis.

    float ambientColor[4];
    uint vLightListIdx;
} rendpspriteparams_t;

DENG_EXTERN_C int alwaysAlign;
DENG_EXTERN_C int spriteLight, useSpriteAlpha, useSpriteBlend;
DENG_EXTERN_C int noSpriteZWrite;
DENG_EXTERN_C byte noSpriteTrans;
DENG_EXTERN_C byte devNoSprites;

DENG_EXTERN_C void Rend_SpriteRegister(void);

#ifdef __CLIENT__
#ifdef __cplusplus

#include "MaterialVariantSpec"

de::MaterialVariantSpec const &Rend_SpriteMaterialSpec(int tclass = 0, int tmap = 0);

extern "C" {
#endif // __cplusplus

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

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __CLIENT__

///@}

#endif // DENG_CLIENT_RENDER_BILLBOARD_H
