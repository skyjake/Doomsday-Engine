/**\file gl_hq2x.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * High-Quality 2x Graphics Resizing.
 */

#ifndef LIBDENG_GRAPHICS_HQ2X_H
#define LIBDENG_GRAPHICS_HQ2X_H

/**
 * Initialize the lookup tables used in the hq2x algorithm.
 */
void GL_InitSmartFilterHQ2x(void);

/**
 * @param src  R8G8B8A8 source image to be scaled.
 * @param width  Width of the source image in pixels.
 * @param height  Height of the source image in pixels.
 * @param flags  @see imageConversionFlags
 */
uint8_t* GL_SmartFilterHQ2x(const uint8_t* src, int width, int height, int flags);

#endif /* LIBDENG_GRAPHICS_HQ2X_H */
