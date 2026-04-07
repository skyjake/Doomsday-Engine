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

#include "de/heightmap.h"

namespace de {

static const float NORMAL_Z = .2f;

DE_PIMPL_NOREF(HeightMap)
{
    Image heightImage;
    Image normalImage;
    Vec2f  mapSize;
    float  heightRange = 1.f;

    Vec2f pixelCoordf(const Vec2f &worldPos) const
    {
        Vec2f normPos = worldPos / mapSize + Vec2f(.5f, .5f);
        return normPos * Vec2f(heightImage.width(), heightImage.height()) - Vec2f(.5f, .5f);
    }

    Vec3f normalAtCoord(const Vec2i &pos) const
    {
        const int w = heightImage.width();
        const int h = heightImage.height();

        int x0 = max(pos.x - 1, 0);
        int y0 = max(pos.y - 1, 0);
        int x2 = min(pos.x + 1, w - 1);
        int y2 = min(pos.y + 1, h - 1);

        float base  = heightImage.pixel(pos.x, pos.y).x / 255.f;
        float left  = heightImage.pixel(x0,    pos.y).x / 255.f;
        float right = heightImage.pixel(x2,    pos.y).x / 255.f;
        float up    = heightImage.pixel(pos.x, y0).x    / 255.f;
        float down  = heightImage.pixel(pos.x, y2).x    / 255.f;

        return (Vec3f(base - right, base - down, NORMAL_Z) +
                Vec3f(left - base,  up - base,   NORMAL_Z)).normalize();
    }
};

HeightMap::HeightMap() : d(new Impl)
{}

void HeightMap::setMapSize(const Vec2f &worldSize, float heightRange)
{
    d->mapSize     = worldSize;
    d->heightRange = heightRange;
}

void HeightMap::loadGrayscale(const Image &heightImage)
{
    d->heightImage = heightImage;
}

Image HeightMap::toImage() const
{
    return d->heightImage;
}

Image HeightMap::makeNormalMap() const
{
    const Image &heightMap = d->heightImage;

    Image img(heightMap.size(), Image::RGBA_8888);

    const int w = heightMap.width();
    const int h = heightMap.height();

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            Vec3f norm = d->normalAtCoord(Vec2i(x, y)) * 0.5f + Vec3f(.5f, .5f, .5f);

            img.setPixel(x,
                         y,
                         Image::Color(dbyte(clamp(0.f, norm.x * 256.f, 255.f)),
                                      dbyte(clamp(0.f, norm.y * 256.f, 255.f)),
                                      dbyte(clamp(0.f, norm.z * 256.f, 255.f)),
                                      max<dbyte>(1, heightMap.pixel(x, y).x)));
        }
    }

    d->normalImage = img;

    return img;
}

float HeightMap::heightAtPosition(const Vec2f &worldPos) const
{
    const Image &img = d->heightImage;

    Vec2f coord = d->pixelCoordf(worldPos);
    Vec2i pixelCoord = coord.toVec2i();

    if (pixelCoord.x < 0 ||
        pixelCoord.y < 0 ||
        pixelCoord.x >= int(img.width()) - 1 ||
        pixelCoord.y >= int(img.height()) - 1)
    {
        return 0;
    }

    float A = img.pixel(pixelCoord.x,     pixelCoord.y).x     / 255.f - 0.5f;
    float B = img.pixel(pixelCoord.x + 1, pixelCoord.y).x     / 255.f - 0.5f;
    float C = img.pixel(pixelCoord.x + 1, pixelCoord.y + 1).x / 255.f - 0.5f;
    float D = img.pixel(pixelCoord.x,     pixelCoord.y + 1).x / 255.f - 0.5f;

    // Bilinear interpolation.
    Vec2f i(fract(coord.x), fract(coord.y));
    float value = A + i.x * (B - A) + i.y * (D - A) + i.x * i.y * (A - B - D + C);

    return value * -d->heightRange;
}

Vec3f HeightMap::normalAtPosition(const Vec2f &worldPos) const
{
    const Vec2i pos = d->pixelCoordf(worldPos).toVec2i();
    return d->normalAtCoord(pos); // * Vec3f(1, 1, NORMAL_Z)).normalize();
}

} // namespace de
