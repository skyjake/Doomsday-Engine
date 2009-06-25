/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * gl_draw.h: Basic Drawing Routines
 */

#ifndef __DOOMSDAY_GRAPHICS_DRAW_H__
#define __DOOMSDAY_GRAPHICS_DRAW_H__

void            GL_UsePatchOffset(boolean enable);

// 2D drawing routines:
void            GL_DrawLine(float x1, float y1, float x2, float y2, float r,
                            float g, float b, float a);
void            GL_DrawRect(float x, float y, float w, float h, float r,
                            float g, float b, float a);
void            GL_DrawRectTiled(float x, float y, float w, float h, int tw, int th);
void            GL_DrawCutRectTiled(float x, float y, float w, float h, int tw, int th,
                                    int txoff, int tyoff, float cx, float cy, float cw, float ch);

// Filters:
int             GL_DrawFilter(void);

#endif
