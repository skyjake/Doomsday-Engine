/** @file gloom/render/defs.h
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef GLOOM_RENDER_DEFS_H
#define GLOOM_RENDER_DEFS_H

#include <cstdint>

namespace gloom {

static const uint32_t InvalidIndex = 0xffffffff;

enum Texture {
    Diffuse            = 0, // RGB: Diffuse  | A: Opacity
    SpecularGloss      = 1, // RGB: Specular | A: Gloss
    Emissive           = 2, // RGB: Emissive
    NormalDisplacement = 3, // RGB: Normal   | A: Displacement

    TextureMapCount = 4,
};

enum BloomMode {
    BloomHorizontal = 0,
    BloomVertical   = 1,
};

} // namespace gloom

#endif // GLOOM_RENDER_DEFS_H
