/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * rend_sprite.h: Rendering Map Objects as 2D Sprites
 */

#ifndef __DOOMSDAY_RENDER_SPRITE_H__
#define __DOOMSDAY_RENDER_SPRITE_H__

typedef struct rendpspriteparams_s {
// Position/Orientation/Scale
    float           pos[2]; // {X, Y} Screen-space position.
    float           width, height;

// Appearance
    // Texture:
    material_t*     mat;
    float           texOffset[2];
    boolean         texFlip[2]; // {X, Y} Flip along the specified axis.

    // Lighting/color:
    float           ambientColor[4];
    uint            vLightListIdx;
} rendpspriteparams_t;

extern int spriteLight, useSpriteAlpha;
extern byte noSpriteTrans;
extern byte devNoSprites;

void            Rend_SpriteRegister(void);
void            Rend_DrawMasked(void);
void            Rend_Draw2DPlayerSprites(void);
void            Rend_Draw3DPlayerSprites(void);
void            Rend_SpriteTexCoord(int pnum, int x, int y);
void            Rend_RenderSprite(const rendspriteparams_t* params);

#endif
