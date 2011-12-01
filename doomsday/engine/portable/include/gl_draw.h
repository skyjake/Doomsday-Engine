/**\file gl_draw.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Basic Drawing Routines
 */

#ifndef LIBDENG_GRAPHICS_DRAW_H
#define LIBDENG_GRAPHICS_DRAW_H

#include "rect.h"

// 2D drawing routines:
void GL_DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a);

void GL_DrawRectd(const RectRawf* rect);
void GL_DrawRecti(const RectRawi* rect);
void GL_DrawRect(float x, float y, float w, float h);
void GL_DrawRectColor(float x, float y, float w, float h, float r, float g, float b, float a);
void GL_DrawRectTextureColor(float x, float y, float w, float h, DGLuint tex, int texW, int texH, const float topColor[3], float topAlpha, const float bottomColor[3], float bottomAlpha);

void GL_DrawRectTiled(float x, float y, float w, float h, int tw, int th);
void GL_DrawCutRectTiled(float x, float y, float w, float h, int tw, int th, int txoff, int tyoff, float cx, float cy, float cw, float ch);

// Filters:
boolean GL_FilterIsVisible(void);
void GL_SetFilter(boolean enable);
void GL_SetFilterColor(float r, float g, float b, float a);
void GL_DrawFilter(void);

void GL_ConfigureBorderedProjection2(borderedprojectionstate_t* bp, int flags,
    int width, int height, int availWidth, int availHeight, scalemode_t overrideMode,
    float stretchEpsilon);
void GL_ConfigureBorderedProjection(borderedprojectionstate_t* bp, int flags,
    int width, int height, int availWidth, int availHeight, scalemode_t overrideMode);

void GL_BeginBorderedProjection(borderedprojectionstate_t* bp);
void GL_EndBorderedProjection(borderedprojectionstate_t* bp);

#endif
