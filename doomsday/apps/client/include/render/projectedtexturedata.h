/** @file projectedtexturedata.h  Projected texture data.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_RENDER_PROJECTEDTEXTUREDATA_H
#define CLIENT_RENDER_PROJECTEDTEXTUREDATA_H

#include <de/vector.h>
#include "api_gl.h"  // DGLuint

/**
 * POD for a texture => surface projection.
 * @ingroup render
 */
struct ProjectedTextureData
{
    DGLuint texture = 0;
    de::Vec2f topLeft;
    de::Vec2f bottomRight;
    de::Vec4f color;
};

#endif  // CLIENT_RENDER_PROJECTEDTEXTUREDATA_H
