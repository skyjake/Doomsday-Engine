/** @file gltexture.h  GL texture.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLTEXTURE_H
#define LIBGUI_GLTEXTURE_H

#include <de/libdeng2.h>
#include <de/Asset>
#include <de/Vector>

#include "libgui.h"
#include "opengl.h"

namespace de {

/**
 * GL texture.
 *
 * @todo Support for cube textures (6 faces instead of one).
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLTexture : public Asset
{
public:
    typedef Vector2ui Size;

public:
    GLTexture();

    /**
     * Returns the size of the texture (mipmap level 0).
     */
    Size size() const;

    GLuint glName() const;

    void glBindToUnit(int unit);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLTEXTURE_H
