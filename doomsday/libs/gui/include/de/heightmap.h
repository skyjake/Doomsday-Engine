/** @file heightmap.h  Height map.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_HEIGHTMAP_H
#define LIBGUI_HEIGHTMAP_H

#include <de/libcore.h>
#include <de/image.h>
#include <de/vector.h>

namespace de {

/**
 * Height map.
 *
 * Operations related to height maps, such as conversion from grayscale height
 * map to normal map, and determining the height at a specific point.
 *
 * @ingroup gl
 */
class HeightMap
{
public:
    HeightMap();

    void setMapSize(const Vec2f &worldSize, float heightRange);

    void loadGrayscale(const Image &heightImage);

    Image toImage() const;

    Image makeNormalMap() const;

    float heightAtPosition(const Vec2f &worldPos) const;

    Vec3f normalAtPosition(const Vec2f &worldPos) const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_HEIGHTMAP_H
