/** @file proceduralimage.cpp  Procedural image.
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

#include "de/proceduralimage.h"

namespace de {

ProceduralImage::ProceduralImage(const Size &pointSize)
    : _pointSize(pointSize), _color(1, 1, 1, 1)
{}

ProceduralImage::~ProceduralImage()
{}

ProceduralImage::Size ProceduralImage::pointSize() const
{
    return _pointSize;
}

ProceduralImage::Color ProceduralImage::color() const
{
    return _color;
}

void ProceduralImage::setPointSize(const Size &pointSize)
{
    _pointSize = pointSize;
}

void ProceduralImage::setColor(const Color &color)
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
