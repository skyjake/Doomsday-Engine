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
 * gl_draw.h: Basic Drawing Routines
 */

#ifndef __DOOMSDAY_GRAPHICS_DRAW_H__
#define __DOOMSDAY_GRAPHICS_DRAW_H__

void			GL_UsePatchOffset(boolean enable);

// 2D drawing routines:
void			GL_DrawPatch_CS(int x, int y, int lumpnum);
void			GL_DrawPatch(int x, int y, int lumpnum);
void			GL_DrawPatchLitAlpha(int x, int y, float light, float alpha, 
					int lumpnum);
void			GL_DrawFuzzPatch(int x, int y, int lumpnum);
void			GL_DrawAltFuzzPatch(int x, int y, int lumpnum);
void			GL_DrawShadowedPatch(int x, int y, int lumpnum);
void			GL_DrawRawScreen(int lump, float offx, float offy);
void			GL_DrawRawScreen_CS(int lump, float offx, float offy, float scalex, float scaley);
void			GL_DrawLine(float x1, float y1, float x2, float y2, 
					float r, float g, float b, float a);
void			GL_DrawRect(float x, float y, float w, float h, float r, 
					float g, float b, float a);
void			GL_DrawRectTiled(int x, int y, int w, int h, int tw, int th);
void			GL_DrawCutRectTiled(int x, int y, int w, int h, int tw, int th, 
					int cx, int cy, int cw, int ch);
void			GL_SetColor(int palidx);
void			GL_SetColor2(int palidx, float alpha);
void			GL_SetColorAndAlpha(float r, float g, float b, float a);
void			GL_DrawPSprite(float x, float y, float scale, int flip, int lump);

// Filters:
void			GL_SetFilter(int filterRGBA);
int				GL_DrawFilter();

#endif 
