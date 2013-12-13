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

namespace internal {

/**
 * Example: "R8G8B8"
 */
static void parseColorFormat(QString const &fmt, Vector3ui &compOrder, Vector3ui &compBits)
{
    compBits = Vector3ui();

    int const end = fmt.length();

    int readComponents = 0;
    int pos = 0;
    while(pos < end)
    {
        QChar ch = fmt[pos]; pos++;

        int comp = -1;
        if     (ch == 'R' || ch == 'r') comp = 0;
        else if(ch == 'G' || ch == 'g') comp = 1;
        else if(ch == 'B' || ch == 'b') comp = 2;

        if(comp != -1 && compBits[comp] == 0)
        {
            compOrder[comp] = readComponents++;

            // Read the number of bits.
            int start = pos;
            ch = fmt[pos];
            while(ch.isDigit() && ++pos < end)
            {
                ch = fmt[pos];
            }

            int numDigits = pos - start;
            if(numDigits)
            {
                compBits[comp] = fmt.mid(start, numDigits).toInt();

                // Are we done?
                if(readComponents == 3)
                    break;

                continue;
            }
        }

        /// @throw ColorTableReader::FormatError
        throw ColorTableReader::FormatError("parseColorFormat", QString("Unexpected character '%1' at position %2").arg(ch).arg(pos));
    }

    if(readComponents != 3)
    {
        /// @throw ColorTableReader::FormatError
        throw ColorTableReader::FormatError("parseColorFormat", "Incomplete format specification");
    }
}

} // namespace internal

using namespace internal;

typedef QVector<de::Vector3ub> ColorTable;

ColorTable ColorTableReader::read(String format, int colorCount,
    dbyte const *colorData) // static
{
    Vector3ui order;
    Vector3ui bits;
    parseColorFormat(format, order, bits);

    ColorTable colors(colorCount);

    // Already in the format we want?
    if(8 == bits.x && 8 == bits.y && 8 == bits.z)
    {
        // Great! Just copy it as-is.
        dbyte const *src = colorData;
        for(int i = 0; i < colorCount; ++i, src += 3)
        {
            colors[i] = Vector3ub(src[order.x], src[order.y], src[order.z]);
        }
    }
    else
    {
        // Conversion is necessary.
        dbyte const *src = colorData;
        dbyte cb = 0;
        for(int i = 0; i < colorCount; ++i)
        {
            Vector3ub &dst = colors[i];

            Vector3i tmp;
            M_ReadBits(bits[order.x], &src, &cb, (dbyte *) &(tmp[order.x]));
            M_ReadBits(bits[order.y], &src, &cb, (dbyte *) &(tmp[order.y]));
            M_ReadBits(bits[order.z], &src, &cb, (dbyte *) &(tmp[order.z]));

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
            dst = Vector3ub(de::clamp<dbyte>(0, tmp.x, 255),
                            de::clamp<dbyte>(0, tmp.y, 255),
                            de::clamp<dbyte>(0, tmp.z, 255));
        }
    }

    return colors;
}

#define RGB18(r, g, b)      ((r)+((g)<<6)+((b)<<12))

DENG2_PIMPL(ColorPalette)
{
    typedef Vector3ub Color;
    typedef QVector<Color> ColorTable;
    ColorTable colors;

    /// 18-bit to 8-bit, nearest color translation table.
    typedef QVector<int> XLat18To8;
    QScopedPointer<XLat18To8> xlat18To8;
    bool need18To8Update;

    Id id;

    Instance(Public *i)
        : Base(i)
        , need18To8Update(false) // No color table yet.
    {
        LOG_VERBOSE("New color palette %s") << id;
    }

    void notifyColorTableChanged()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(ColorTableChange, i)
        {
            i->colorPaletteColorTableChanged(self);
        }
    }

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

ColorPalette::ColorPalette() : d(new Instance(this))
{}

ColorPalette::ColorPalette(ColorTable const &colors)
    : d(new Instance(this))
{
    replaceColorTable(colors);
}

Id ColorPalette::id() const
{
    return d->id;
}

int ColorPalette::colorCount() const
{
    return d->colors.count();
}

ColorPalette &ColorPalette::replaceColorTable(ColorTable const &colorTable)
{
    // We may need a new 18 => 8 bit xlat table.
    d->need18To8Update = true;

    // Replace the whole color table.
    d->colors = colorTable;

    // Notify interested parties.
    d->notifyColorTableChanged();

    return *this;
}

Vector3ub ColorPalette::color(int colorIndex) const
{
    LOG_AS("ColorPalette::color");

    if(colorIndex < 0 || colorIndex >= d->colors.count())
    {
        LOG_DEBUG("Index %i out of range %s in palette %s, will clamp.")
            << colorIndex << Rangeui(0, d->colors.count()).asText() << d->id;
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
