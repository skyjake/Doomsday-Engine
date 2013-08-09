/** @file proceduralimage.cpp  Procedural image.
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

#include "ui/widgets/proceduralimage.h"

using namespace de;

ProceduralImage::ProceduralImage(Size const &size) : _size(size), _color(1, 1, 1, 1)
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

void ProceduralImage::update()
{
    // optional for derived classes
}

void ProceduralImage::glDeinit()
{
    // optional for derived classes
}
