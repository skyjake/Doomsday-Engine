/**
 * @file hq2x.h
 * High-Quality 2x Graphics Resizing. @ingroup resource
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG_GRAPHICS_HQ2X_H
#define LIBDENG_GRAPHICS_HQ2X_H

/**
 * Initialize the lookup tables used in the hq2x algorithm.
 */
void GL_InitSmartFilterHQ2x(void);

/**
 * Upscales an image to 2x the original size using an intelligent scaling
 * algorithm that avoids blurriness.
 *
 * Based on the routine by Maxim Stepin <maxst@hiend3d.com>
 * For more information, see: http://hiend3d.com/hq2x.html
 *
 * Uses 32-bit data and our native ABGR8888 pixel format.
 * Alpha is taken into account in the processing to preserve edges.
 * (Not quite as efficient as the original version.)
 *
 * @param src  R8G8B8A8 source image to be scaled.
 * @param width  Width of the source image in pixels.
 * @param height  Height of the source image in pixels.
 * @param flags  @ref imageConversionFlags
 */
uint8_t* GL_SmartFilterHQ2x(const uint8_t* src, int width, int height, int flags);

#endif /* LIBDENG_GRAPHICS_HQ2X_H */
