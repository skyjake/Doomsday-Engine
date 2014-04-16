/** @file proceduralimage.cpp  Procedural image.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ProceduralImage"

namespace de {

ProceduralImage::ProceduralImage(Size const &size) : _size(size), _color(1, 1, 1, 1)
{}

ProceduralImage::~ProceduralImage()
{}

ProceduralImage::Size ProceduralImage::size() const
{
    return _size;
}

ProceduralImage::Color ProceduralImage::color() const
{
    return _color;
}

void ProceduralImage::setSize(Size const &size)
{
    _size = size;
}

void ProceduralImage::setColor(Color const &color)
{
    _color = color;
}

bool ProceduralImage::update()
{
    // optional for derived classes
    return false;
}

void ProceduralImage::glInit()
{
    // optional for derived classes
}

void ProceduralImage::glDeinit()
{
    // optional for derived classes
}

} // namespace de
