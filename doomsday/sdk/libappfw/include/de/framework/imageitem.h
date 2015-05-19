/** @file imageitem.h  Data item with an image.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_UI_IMAGEITEM_H
#define LIBAPPFW_UI_IMAGEITEM_H

#include "item.h"
#include <de/Image>

namespace de {
namespace ui {

/**
 * UI context item that represents a user action.
 */
class LIBAPPFW_PUBLIC ImageItem : public Item
{
public:
    ImageItem(Semantics semantics, String const &label = "")
        : Item(semantics, label) {}

    ImageItem(Semantics semantics, Image const &image, String const &label = "")
        : Item(semantics, label), _image(image) {}

    Image const &image() const { return _image; }

    void setImage(Image const &image)
    {
        _image = image;
        notifyChange();
    }

private:
    Image _image;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_IMAGEITEM_H
