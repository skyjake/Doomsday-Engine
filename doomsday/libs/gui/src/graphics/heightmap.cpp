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

#include "de/HeightMap"

namespace de {

static float const NORMAL_Z = .2f;

DE_PIMPL_NOREF(HeightMap)
{
    QImage heightImage;
    QImage normalImage;
    Vec2f  mapSize;
    float  heightRange = 1.f;

    Vec2f pixelCoordf(Vec2f const &worldPos) const
    {
        Vec2f normPos = worldPos / mapSize + Vec2f(.5f, .5f);
        return normPos * Vec2f(heightImage.width(), heightImage.height()) - Vec2f(.5f, .5f);
    }

    Vec3f normalAtCoord(Vec2i const &pos) const
    {
        int const w = heightImage.width();
        int const h = heightImage.height();

        int x0 = max(pos.x - 1, 0);
        int y0 = max(pos.y - 1, 0);
        int x2 = min(pos.x + 1, w - 1);
        int y2 = min(pos.y + 1, h - 1);

        float base  = qRed(heightImage.pixel(pos.x, pos.y)) / 255.f;
        float left  = qRed(heightImage.pixel(x0,    pos.y)) / 255.f;
        float right = qRed(heightImage.pixel(x2,    pos.y)) / 255.f;
        float up    = qRed(heightImage.pixel(pos.x, y0))    / 255.f;
        float down  = qRed(heightImage.pixel(pos.x, y2))    / 255.f;

        return (Vec3f(base - right, base - down, NORMAL_Z) +
                Vec3f(left - base,  up - base,   NORMAL_Z)).normalize();
    }
};

HeightMap::HeightMap() : d(new Impl)
{}

void HeightMap::setMapSize(Vec2f const &worldSize, float heightRange)
{
    d->mapSize     = worldSize;
    d->heightRange = heightRange;
}

void HeightMap::loadGrayscale(Image const &heightImage)
{
    d->heightImage = heightImage.toQImage();
}

Image HeightMap::toImage() const
{
    return d->heightImage;
}

Image HeightMap::makeNormalMap() const
{
    QImage const &heightMap = d->heightImage;

    QImage img(heightMap.size(), QImage::Format_ARGB32);

    int const w = heightMap.width();
    int const h = heightMap.height();

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            Vec3f norm = d->normalAtCoord(Vec2i(x, y)) * 0.5f + Vec3f(.5f, .5f, .5f);

            img.setPixel(x, y, qRgba(int(clamp(0.f, norm.x * 256.f, 255.f)),
                                     int(clamp(0.f, norm.y * 256.f, 255.f)),
                                     int(clamp(0.f, norm.z * 256.f, 255.f)),
                                     max(1, qRed(heightMap.pixel(x, y)))));
        }
    }

    d->normalImage = img;

    return img;
}

float HeightMap::heightAtPosition(Vec2f const &worldPos) const
{
    QImage const &img = d->heightImage;

    Vec2f coord = d->pixelCoordf(worldPos);
    Vec2i pixelCoord = coord.toVec2i();

    if (pixelCoord.x < 0 || pixelCoord.y < 0 ||
       pixelCoord.x >= img.width() - 1 || pixelCoord.y >= img.height() - 1) return 0;

    float A = qRed(img.pixel(pixelCoord.x,     pixelCoord.y))     / 255.f - 0.5f;
    float B = qRed(img.pixel(pixelCoord.x + 1, pixelCoord.y))     / 255.f - 0.5f;
    float C = qRed(img.pixel(pixelCoord.x + 1, pixelCoord.y + 1)) / 255.f - 0.5f;
    float D = qRed(img.pixel(pixelCoord.x,     pixelCoord.y + 1)) / 255.f - 0.5f;

    // Bilinear interpolation.
    Vec2f i(fract(coord.x), fract(coord.y));
    float value = A + i.x * (B - A) + i.y * (D - A) + i.x * i.y * (A - B - D + C);

    return value * -d->heightRange;
}

Vec3f HeightMap::normalAtPosition(Vec2f const &worldPos) const
{
    Vec2i const pos = d->pixelCoordf(worldPos).toVec2i();
    return d->normalAtCoord(pos); // * Vec3f(1, 1, NORMAL_Z)).normalize();
}

} // namespace de
