/** @file imageitem.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ui/imageitem.h"
#include "de/ui/style.h"

namespace de {
namespace ui {

const Image &ImageItem::image() const
{
    if (_styleId.isEmpty())
    {
        return _image;
    }
    return Style::get().images().image(_styleId);
}

const DotPath &ImageItem::styleImageId() const
{
    return _styleId;
}

void ImageItem::setImage(const Image &image)
{
    _image   = image;
    _styleId = DotPath();
    notifyChange();
}

void ImageItem::setImage(const DotPath &styleImageId)
{
    _image   = Image();
    _styleId = styleImageId;
    notifyChange();
}

} // namespace ui
} // namespace de
