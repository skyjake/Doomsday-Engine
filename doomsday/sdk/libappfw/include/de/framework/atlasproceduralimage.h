/** @file atlasproceduralimage.h  Procedural image for a static 2D texture.
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

#ifndef LIBAPPFW_ATLASPROCEDURALIMAGE_H
#define LIBAPPFW_ATLASPROCEDURALIMAGE_H

#include "../libappfw.h"
#include "../ProceduralImage"
#include "../GuiWidget"
#include "../GuiRootWidget"

#include <de/Atlas>
#include <de/Image>
#include <de/Rectangle>

namespace de {

/**
 * Procedural image that draws a simple 2D texture stored on an atlas.
 *
 * @ingroup appfw
 */
class LIBAPPFW_PUBLIC AtlasProceduralImage : public ProceduralImage
{
public:
    AtlasProceduralImage(GuiWidget &owner)
        : _owner(owner), _atlas(0), _id(Id::None), _needUpdate(false), _imageOwned(true)
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
        if (_atlas)
        {
            if (_imageOwned)
            {
                _atlas->release(_id);
            }
            _atlas = 0;
            _id = Id::None;
        }
    }

    void setImage(Image const &image)
    {
        _image = image;
        _needUpdate = true;
        _imageOwned = true;
        setSize(image.size());
    }

    void setPreallocatedImage(Id const &id)
    {
        _image = Image();
        _needUpdate = false;
        _imageOwned = false;
        _id = id;
        _atlas = &ownerAtlas();
        setSize(_atlas->imageRect(id).size());
    }

    bool update()
    {
        if (_needUpdate)
        {
            alloc();
            _needUpdate = false;
            return true;
        }
        return false;
    }

    void glInit()
    {
        if (_id.isNone())
        {
            alloc();
        }
    }

    void glDeinit()
    {
        release();
    }

    void glMakeGeometry(GuiVertexBuilder &verts, Rectanglef const &rect)
    {
        if (_atlas)
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
    bool _imageOwned;
};

} // namespace de

#endif // ATLASPROCEDURALIMAGE_H
