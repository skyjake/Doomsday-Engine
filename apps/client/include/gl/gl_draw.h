/**
 * @file gl_draw.h
 * Basic GL-Drawing Routines.
 *
 * @ingroup gl
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_GRAPHICS_DRAW_H
#define DE_GRAPHICS_DRAW_H

#include <de/rectangle.h>
#include <de/vector.h>
#include <de/legacy/rect.h>

#ifdef __cplusplus
extern "C" {
#endif

void GL_DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b, float a);

void GL_DrawRect(const de::Rectanglei &rect);
void GL_DrawRect2(int x, int y, int w, int h);

/**
 * @param coords  [topLeft, topRight, bottomRight, bottomLeft]
 */
void GL_DrawRectWithCoords(const de::Rectanglei &rect, de::Vec2i const coords[4]);

void GL_DrawRectf(const RectRawf* rect);
void GL_DrawRectf2(double x, double y, double w, double h);

void GL_DrawRectfWithCoords(const RectRawf* rect, Point2Rawf coords[4]);

void GL_DrawRectf2Color(double x, double y, double w, double h, float r, float g, float b, float a);
void GL_DrawRectf2TextureColor(double x, double y, double w, double h,
    int texW, int texH, const float topColor[3], float topAlpha,
    const float bottomColor[3], float bottomAlpha);

void GL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th);

/**
 * The cut rectangle must be inside the other one.
 */
void GL_DrawCutRectfTiled(const RectRawf* rect, int tw, int th, int txoff, int tyoff, const RectRawf* cutRect);
void GL_DrawCutRectf2Tiled(double x, double y, double w, double h, int tw, int th,
    int txoff, int tyoff, double cx, double cy, double cw, double ch);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_GRAPHICS_DRAW_H
