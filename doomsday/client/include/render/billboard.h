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
#include "render/vissprite.h"
#include "MaterialVariantSpec"

/// @ingroup render
struct rendpspriteparams_t
{
    float pos[2]; // {X, Y} Screen-space position.
    float width, height;

    Material *mat;
    float texOffset[2];
    boolean texFlip[2]; // {X, Y} Flip along the specified axis.

    float ambientColor[4];
    uint vLightListIdx;
};

/// @addtogroup render
/// @{
DENG_EXTERN_C int alwaysAlign;
DENG_EXTERN_C int spriteLight, useSpriteAlpha, useSpriteBlend;
DENG_EXTERN_C int noSpriteZWrite;
DENG_EXTERN_C byte noSpriteTrans;
DENG_EXTERN_C byte devNoSprites;

DENG_EXTERN_C void Rend_SpriteRegister();

/**
 * A sort of a sprite, I guess... Masked walls must be rendered sorted
 * with sprites, so no artifacts appear when sprites are seen behind
 * masked walls.
 */
void Rend_DrawMaskedWall(rendmaskedwallparams_t const &parms);

void Rend_DrawPSprite(rendpspriteparams_t const &parms);

void Rend_DrawSprite(rendspriteparams_t const &parms);

de::MaterialVariantSpec const &Rend_SpriteMaterialSpec(int tclass = 0, int tmap = 0);

/// @}

#endif // DENG_CLIENT_RENDER_BILLBOARD_H
