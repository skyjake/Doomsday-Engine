/** @file atlastexture.h  Atlas stored on a GLTexture.
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

#ifndef LIBGUI_ATLASTEXTURE_H
#define LIBGUI_ATLASTEXTURE_H

#include "../GLTexture"
#include "../Atlas"

namespace de {

/**
 * Atlas stored on a GLTexture.
 */
class LIBGUI_PUBLIC AtlasTexture : public Atlas, public GLTexture
{
public:
    AtlasTexture(Atlas::Flags const &flags = DefaultFlags,
                 Atlas::Size const &totalSize = Atlas::Size());

    void clear();

protected:
    /**
     * The atlas content is automatically committed to the GL texture when the
     * texture is bound for use.
     */
    void aboutToUse() const;

    /**
     * Replaces the entire content of the GL texture.
     *
     * @param fullImage  Full atlas content image.
     */
    void commitFull(Image const &fullImage) const;

    void commit(Image const &image, Vector2i const &topLeft) const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_ATLASTEXTURE_H
