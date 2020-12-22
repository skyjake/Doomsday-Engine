/** @file colorpalette.cpp  Color palette resource.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/res/colorpalette.h"

#include <de/log.h>
#include <de/range.h>
#include <de/keymap.h>
#include <de/legacy/reader.h>
#include <de/legacy/mathutil.h>

using namespace de;

#define RGB18(r, g, b)      ((r)+((g)<<6)+((b)<<12))

namespace res {

/*
 * Example: "R8G8B8"
 */
static void parseColorFormat(const String &fmt, Vec3ui &compOrder, Vec3ui &compBits)
{
    compBits = Vec3ui();

    mb_iterator end = fmt.end();

    int readComponents = 0;
    mb_iterator pos = fmt.begin();
    while (pos != end)
    {
        Char ch = *pos++;

        int comp = -1;
        if      (ch == 'R' || ch == 'r') comp = 0;
        else if (ch == 'G' || ch == 'g') comp = 1;
        else if (ch == 'B' || ch == 'b') comp = 2;

        if (comp != -1 && compBits[comp] == 0)
        {
            compOrder[comp] = readComponents++;

            // Read the number of bits.
            mb_iterator start = pos;
            ch = *pos;
            while (ch.isNumeric() && ++pos != end)
            {
                ch = *pos;
            }

            auto numDigits = pos - start;
            if (numDigits)
            {
                compBits[comp] = fmt.substr(start.pos(), numDigits).toInt();

                // Are we done?
                if (readComponents == 3)
                    break;

                continue;
            }
        }

        /// @throw ColorTableReader::FormatError
        throw ColorTableReader::FormatError(
            "parseColorFormat",
            stringf("Unexpected character '%lc' at position %zu", ch, pos.pos().index));
    }

    if (readComponents != 3)
    {
        /// @throw ColorTableReader::FormatError
        throw ColorTableReader::FormatError("parseColorFormat", "Incomplete format specification");
    }
}

typedef List<de::Vec3ub> ColorTable;

ColorTable ColorTableReader::read(String format, int colorCount,
                                  const dbyte *colorData) // static
{
    Vec3ui order;
    Vec3ui bits;
    parseColorFormat(format, order, bits);

    ColorTable colors(colorCount);

    // Already in the format we want?
    if (8 == bits.x && 8 == bits.y && 8 == bits.z)
    {
        // Great! Just copy it as-is.
        const dbyte *src = colorData;
        for (int i = 0; i < colorCount; ++i, src += 3)
        {
            colors[i] = Vec3ub(src[order.x], src[order.y], src[order.z]);
        }
    }
    else
    {
        // Conversion is necessary.
        const dbyte *src = colorData;
        dbyte cb = 0;
        for (int i = 0; i < colorCount; ++i)
        {
            Vec3ub &dst = colors[i];

            Vec3i tmp;
            M_ReadBits(bits[order.x], &src, &cb, (dbyte *) &(tmp[order.x]));
            M_ReadBits(bits[order.y], &src, &cb, (dbyte *) &(tmp[order.y]));
            M_ReadBits(bits[order.z], &src, &cb, (dbyte *) &(tmp[order.z]));

            // Need to do any scaling?
            if (8 != bits.x)
            {
                if (bits.x < 8)
                    tmp.x <<= 8 - bits.x;
                else
                    tmp.x >>= bits.x - 8;
            }

            if (8 != bits.y)
            {
                if (bits.y < 8)
                    tmp.y <<= 8 - bits.y;
                else
                    tmp.y >>= bits.y - 8;
            }

            if (8 != bits.z)
            {
                if (bits.z < 8)
                    tmp.z <<= 8 - bits.z;
                else
                    tmp.z >>= bits.z - 8;
            }

            // Store the final color.
            dst = Vec3ub(de::clamp<dbyte>(0, tmp.x, 255),
                            de::clamp<dbyte>(0, tmp.y, 255),
                            de::clamp<dbyte>(0, tmp.z, 255));
        }
    }

    return colors;
}

DE_PIMPL(ColorPalette)
{
    typedef Vec3ub Color;
    typedef List<Color> ColorTable;
    ColorTable colors;

    typedef KeyMap<String, Translation> Translations;
    Translations translations;

    /// 18-bit to 8-bit, nearest color translation table.
    typedef List<int> XLat18To8;
    std::unique_ptr<XLat18To8> xlat18To8;
    bool need18To8Update = false;  // Table built only when needed.

    Id id;

    Impl(Public *i) : Base(i)
    {
        LOG_RES_VERBOSE("New color palette %s") << id;
    }

    Translation *translation(const String& id)
    {
        Translations::iterator found = translations.find(id);
        if (found != translations.end())
        {
            return &found->second;
        }
        return 0;
    }

    void notifyColorTableChanged()
    {
        DE_NOTIFY_PUBLIC_VAR(ColorTableChange, i)
        {
            i->colorPaletteColorTableChanged(self());
        }
    }

    /// @note A time-consuming operation.
    void prepareNearestLUT()
    {
#define COLORS18BIT 262144

        need18To8Update = false;

        if (!xlat18To8)
        {
            xlat18To8.reset(new XLat18To8(COLORS18BIT));
        }

        for (int r = 0; r < 64; ++r)
        for (int g = 0; g < 64; ++g)
        for (int b = 0; b < 64; ++b)
        {
            int nearest = 0;
            int smallestDiff = DDMAXINT;
            for (int i = 0; i < colors.count(); ++i)
            {
                const Color &color = colors[i];
                int diff = (color.x - (r << 2)) * (color.x - (r << 2)) +
                           (color.y - (g << 2)) * (color.y - (g << 2)) +
                           (color.z - (b << 2)) * (color.z - (b << 2));

                if (diff < smallestDiff)
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

ColorPalette::ColorPalette() : d(new Impl(this))
{}

ColorPalette::ColorPalette(const ColorTable &colors)
    : d(new Impl(this))
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

ColorPalette &ColorPalette::replaceColorTable(const ColorTable &colorTable)
{
    LOG_AS("ColorPalette");

    const int colorCountBefore = colorCount();

    // We may need a new 18 => 8 bit xlat table.
    d->need18To8Update = true;

    // Replace the whole color table.
    d->colors = colorTable;

    // Notify interested parties.
    d->notifyColorTableChanged();

    // When the color count changes all existing translations are destroyed as
    // they will no longer be valid.
    if (colorCountBefore != colorCount())
    {
        clearTranslations();
    }

    return *this;
}

Vec3ub ColorPalette::color(int colorIndex) const
{
    LOG_AS("ColorPalette");

    if (colorIndex < 0 || colorIndex >= d->colors.count())
    {
        LOG_DEBUG("Index %i out of range %s in palette %s, will clamp.")
            << colorIndex << Rangeui(0, d->colors.count()).asText() << d->id;
    }

    if (!d->colors.isEmpty())
    {
        return d->colors[de::clamp(0, colorIndex, d->colors.count() - 1)];
    }

    return Vec3ub();
}

Vec3f ColorPalette::colorf(int colorIdx) const
{
    return color(colorIdx).toVec3f() * reciprocal255;
}

int ColorPalette::nearestIndex(const Vec3ub &rgb) const
{
    LOG_AS("ColorPalette");

    if (d->colors.isEmpty()) return -1;

    // Ensure we've prepared the 18 to 8 table.
    if (d->need18To8Update || !d->xlat18To8)
    {
        d->prepareNearestLUT();
    }

    return (*d->xlat18To8)[RGB18(rgb.x >> 2, rgb.y >> 2, rgb.z >> 2)];
}

void ColorPalette::clearTranslations()
{
    LOG_AS("ColorPalette");
    d->translations.clear();
}

const ColorPalette::Translation *ColorPalette::translation(String id) const
{
    LOG_AS("ColorPalette");
    return d->translation(id);
}

void ColorPalette::newTranslation(String xlatId, const Translation &mappings)
{
    LOG_AS("ColorPalette");

    if (!colorCount())
    {
        //qDebug() << "Cannot define a translation for an empty palette!";
        return;
    }

    DE_ASSERT(mappings.count() == colorCount()); // sanity check

    if (!xlatId.isEmpty())
    {
        Translation *xlat = d->translation(xlatId);
        if (!xlat)
        {
            // An entirely new translation.
            d->translations.insert(xlatId, Translation());
            xlat = &d->translations.find(xlatId)->second;
        }

        // Replace the whole mapping table.
        *xlat = mappings;

        // Ensure the mappings are within valid range.
        for (int i = 0; i < colorCount(); ++i)
        {
            int palIdx = (*xlat)[i];
            if (palIdx < 0 || palIdx >= colorCount())
            {
                (*xlat)[i] = i;
            }
        }
        return;
    }
    /// @throw InvalidTranslationIdError .
    throw InvalidTranslationIdError("ColorPalette::newTranslation", "A zero-length id was specified");
}

} // namespace res
