/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * rend_sprite.h: Rendering Map Objects as 2D Sprites
 */

#ifndef __DOOMSDAY_RENDER_SPRITE_H__
#define __DOOMSDAY_RENDER_SPRITE_H__

extern float maxSpriteAngle;

void Rend_DrawMasked (void);
void Rend_DrawPlayerSprites(void);
void Rend_Draw3DPlayerSprites(void);
void Rend_SpriteTexCoord(int pnum, int x, int y);
void Rend_RenderSprite(vissprite_t *spr);

#endif 
