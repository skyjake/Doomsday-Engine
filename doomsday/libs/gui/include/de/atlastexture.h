/** @file atlastexture.h  Atlas stored on a GLTexture.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_ATLASTEXTURE_H
#define LIBGUI_ATLASTEXTURE_H

#include "de/gltexture.h"
#include "de/atlas.h"

namespace de {

/**
 * Atlas stored on a (2D) GLTexture.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC AtlasTexture : public Atlas, public GLTexture
{
public:
    AtlasTexture(const Flags &flags = DefaultFlags,
                 const Atlas::Size &totalSize = Atlas::Size());

    /**
     * Constructs an AtlasTexture with a RowAtlasAllocator.
     *
     * @param flags      Atlas flags.
     * @param totalSize  Total size for atlas.
     *
     * @return AtlasTexture instance.
     */
    static AtlasTexture *newWithRowAllocator(const Flags &flags = DefaultFlags,
                                             const Atlas::Size &totalSize = Atlas::Size());

    static AtlasTexture *newWithKdTreeAllocator(const Flags &flags = DefaultFlags,
                                                const Atlas::Size &totalSize = Atlas::Size());

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
    void commitFull(const Image &fullImage) const;

    void commit(const Image &image, const Vec2i &topLeft) const;
    void commit(const Image &fullImage, const Rectanglei &subregion) const;
};

} // namespace de

#endif // LIBGUI_ATLASTEXTURE_H
