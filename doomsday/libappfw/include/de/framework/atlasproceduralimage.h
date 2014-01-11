/** @file atlasproceduralimage.h  Procedural image for a static 2D texture.
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

#ifndef LIBAPPFW_ATLASPROCEDURALIMAGE_H
#define LIBAPPFW_ATLASPROCEDURALIMAGE_H

#include "libappfw.h"
#include "../ProceduralImage"
#include "../GuiWidget"
#include "../GuiRootWidget"

#include <de/Atlas>
#include <de/Image>
#include <de/Rectangle>

namespace de {

/**
 * Procedural image that draws a simple 2D texture stored on an atlas.
 */
class LIBAPPFW_PUBLIC AtlasProceduralImage : public ProceduralImage
{
public:
    AtlasProceduralImage(GuiWidget &owner)
        : _owner(owner), _atlas(0), _needUpdate(false)
    {}

    ~AtlasProceduralImage()
    {
        release();
    }

    Atlas &ownerAtlas()
    {
        return _owner.root().atlas();
    }

    bool hasImage() const
    {
        return !_image.isNull();
    }

    void alloc()
    {
        release();

        _atlas = &ownerAtlas();
        _id = _atlas->alloc(_image);
    }

    void release()
    {
        if(_atlas)
        {
            _atlas->release(_id);
            _atlas = 0;
        }
    }

    void setImage(Image const &image)
    {
        _image = image;
        _needUpdate = true;
        setSize(image.size());
    }

    void update()
    {
        if(_needUpdate)
        {
            alloc();
            _needUpdate = false;
        }
    }

    void glDeinit()
    {
        release();
    }

    void glMakeGeometry(DefaultVertexBuf::Builder &verts, Rectanglef const &rect)
    {
        if(_atlas)
        {
            verts.makeQuad(rect, color(), _atlas->imageRectf(_id));
        }
    }

private:
    GuiWidget &_owner;
    Atlas *_atlas;
    Image _image;
    Id _id;
    bool _needUpdate;
};

} // namespace de

#endif // ATLASPROCEDURALIMAGE_H
