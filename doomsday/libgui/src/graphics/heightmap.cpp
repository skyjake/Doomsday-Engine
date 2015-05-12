/** @file heightmap.h  Height map.
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

#include "de/HeightMap"

namespace de {

static float const NORMAL_Z = .2f;

DENG2_PIMPL_NOREF(HeightMap)
{
    QImage heightImage;
    QImage normalImage;
    Vector2f mapSize;
    float heightRange = 1.f;

    Vector2f pixelCoordf(Vector2f const &worldPos) const
    {
        Vector2f normPos = worldPos / mapSize + Vector2f(.5f, .5f);
        return normPos * Vector2f(heightImage.width(), heightImage.height())
                - Vector2f(.5f, .5f);
    }

    Vector3f normalAtCoord(Vector2i const &pos) const
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

        return (Vector3f(base - right, base - down, NORMAL_Z) +
                Vector3f(left - base,  up - base,   NORMAL_Z)).normalize();
    }
};

HeightMap::HeightMap() : d(new Instance)
{}

void HeightMap::setMapSize(Vector2f const &worldSize, float heightRange)
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

    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            Vector3f norm = d->normalAtCoord(Vector2i(x, y));

            img.setPixel(x, y, qRgba(clamp(0.f, (norm.x + 1) * 128.f, 255.f),
                                     clamp(0.f, (norm.y + 1) * 128.f, 255.f),
                                     clamp(0.f, (norm.z + 1) * 128.f, 255.f),
                                     255));
        }
    }

    d->normalImage = img;

    return img;
}

float HeightMap::heightAtPosition(Vector2f const &worldPos) const
{
    QImage const &img = d->heightImage;

    Vector2f coord = d->pixelCoordf(worldPos);
    Vector2i pixelCoord = coord.toVector2i();

    if(pixelCoord.x < 0 || pixelCoord.y < 0 ||
       pixelCoord.x >= img.width() - 1 || pixelCoord.y >= img.height() - 1) return 0;

    float A = qRed(img.pixel(pixelCoord.x,     pixelCoord.y))     / 255.f - 0.5f;
    float B = qRed(img.pixel(pixelCoord.x + 1, pixelCoord.y))     / 255.f - 0.5f;
    float C = qRed(img.pixel(pixelCoord.x + 1, pixelCoord.y + 1)) / 255.f - 0.5f;
    float D = qRed(img.pixel(pixelCoord.x,     pixelCoord.y + 1)) / 255.f - 0.5f;

    // Bilinear interpolation.
    Vector2f i(fract(coord.x), fract(coord.y));
    float value = A + i.x * (B - A) + i.y * (D - A) + i.x * i.y * (A - B - D + C);

    return value * -d->heightRange;
}

Vector3f HeightMap::normalAtPosition(Vector2f const &worldPos) const
{
    Vector2i const pos = d->pixelCoordf(worldPos).toVector2i();
    return d->normalAtCoord(pos); // * Vector3f(1, 1, NORMAL_Z)).normalize();
}

} // namespace de
