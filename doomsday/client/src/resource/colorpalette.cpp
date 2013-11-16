/** @file colorpalette.cpp  Color palette resource.
 *
 * @authors Copyright © 1999-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "resource/colorpalette.h"

#include "dd_share.h" // reciprocal255
#include "m_misc.h" // M_ReadBits
#include <de/Log>
#include <de/Range>
#include <QVector>

using namespace de;

#define RGB18(r, g, b)      ((r)+((g)<<6)+((b)<<12))

DENG2_PIMPL_NOREF(ColorPalette)
{
    typedef Vector3ub Color;
    typedef QVector<Color> ColorTable;
    ColorTable colors;

    /// 18-bit to 8-bit, nearest color translation table.
    typedef QVector<int> XLat18To8;
    QScopedPointer<XLat18To8> xlat18To8;
    bool need18To8Update;

    Instance() : need18To8Update(false) // No color table yet.
    {}

    /// @note A time-consuming operation.
    void prepareNearestLUT()
    {
#define COLORS18BIT 262144

        need18To8Update = false;

        if(xlat18To8.isNull())
        {
            xlat18To8.reset(new XLat18To8(COLORS18BIT));
        }

        for(int r = 0; r < 64; ++r)
        for(int g = 0; g < 64; ++g)
        for(int b = 0; b < 64; ++b)
        {
            int nearest = 0;
            int smallestDiff = DDMAXINT;
            for(int i = 0; i < colors.count(); ++i)
            {
                Color const &color = colors[i];
                int diff = (color.x - (r << 2)) * (color.x - (r << 2)) +
                           (color.y - (g << 2)) * (color.y - (g << 2)) +
                           (color.z - (b << 2)) * (color.z - (b << 2));

                if(diff < smallestDiff)
                {
                    smallestDiff = diff;
                    nearest = i;
                }
            }

            (*xlat18To8)[RGB18(r, g, b)] = nearest;
        }

#undef COLORS18BIT
    }
};

ColorPalette::ColorPalette() : d(new Instance())
{}

ColorPalette::ColorPalette(int const compOrder[3], uint8_t const compBits[3],
    uint8_t const *colorData, int colorCount)
    : d(new Instance())
{
    DENG2_ASSERT(compOrder != 0 && compBits != 0);
    if(colorCount > 0 && colorData)
    {
        replaceColorTable(compOrder, compBits, colorData, colorCount);
    }
}

void ColorPalette::replaceColorTable(int const compOrder[3],
    uint8_t const compBits[3], uint8_t const *colorData, int colorCount)
{
    // We may need a new 18 => 8 bit xlat table.
    d->need18To8Update = true;

    // Ensure input is in range.
    Vector3i order(de::clamp(0, compOrder[0], 2),
                   de::clamp(0, compOrder[1], 2),
                   de::clamp(0, compOrder[2], 2));

    Vector3ub bits = Vector3ub(compBits).min(Vector3ub(max_component_bits,
                                                       max_component_bits,
                                                       max_component_bits));

    d->colors.resize(colorCount);

    // Already in the format we want?
    if(8 == bits.x && 8 == bits.y && 8 == bits.z)
    {
        // Great! Just copy it as-is.
        uint8_t const *src = colorData;
        for(int i = 0; i < colorCount; ++i, src += 3)
        {
            d->colors[i] = Vector3ub(src[order.x], src[order.y], src[order.z]);
        }
        return;
    }

    // Conversion is necessary.
    uint8_t const *src = colorData;
    uint8_t cb = 0;
    for(int i = 0; i < colorCount; ++i)
    {
        Vector3ub &dst = d->colors[i];

        Vector3i tmp;
        M_ReadBits(bits[order.x], &src, &cb, (uint8_t *) &(tmp[order.x]));
        M_ReadBits(bits[order.y], &src, &cb, (uint8_t *) &(tmp[order.y]));
        M_ReadBits(bits[order.z], &src, &cb, (uint8_t *) &(tmp[order.z]));

        // Need to do any scaling?
        if(8 != bits.x)
        {
            if(bits.x < 8)
                tmp.x <<= 8 - bits.x;
            else
                tmp.x >>= bits.x - 8;
        }

        if(8 != bits.y)
        {
            if(bits.y < 8)
                tmp.y <<= 8 - bits.y;
            else
                tmp.y >>= bits.y - 8;
        }

        if(8 != bits.z)
        {
            if(bits.z < 8)
                tmp.z <<= 8 - bits.z;
            else
                tmp.z >>= bits.z - 8;
        }

        // Store the final color.
        dst = Vector3ub(de::clamp<uint8_t>(0, tmp.x, 255),
                        de::clamp<uint8_t>(0, tmp.y, 255),
                        de::clamp<uint8_t>(0, tmp.z, 255));
    }
}

Vector3ub ColorPalette::color(int colorIndex) const
{
    LOG_AS("ColorPalette::color");

    if(colorIndex < 0 || colorIndex >= d->colors.count())
    {
        LOG_DEBUG("Index %i out of range %s, will clamp.")
            << colorIndex << Rangeui(0, d->colors.count()).asText();
    }

    if(!d->colors.isEmpty())
    {
        return d->colors[de::clamp(0, colorIndex, d->colors.count() - 1)];
    }

    return Vector3ub();
}

Vector3f ColorPalette::colorf(int colorIdx) const
{
    return color(colorIdx).toVector3f() * reciprocal255;
}

int ColorPalette::nearestIndex(Vector3ub const &rgb) const
{
    if(d->colors.isEmpty()) return -1;

    // Ensure we've prepared the 18 to 8 table.
    if(d->need18To8Update || d->xlat18To8.isNull())
    {
        d->prepareNearestLUT();
    }

    return (*d->xlat18To8)[RGB18(rgb.x >> 2, rgb.y >> 2, rgb.z >> 2)];
}
