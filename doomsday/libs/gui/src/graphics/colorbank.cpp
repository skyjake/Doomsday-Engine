/** @file colorbank.cpp  Bank of colors.
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

#include "de/colorbank.h"
#include <de/math.h>
#include <de/arrayvalue.h>

namespace de {

DE_PIMPL(ColorBank)
{
    struct ColorSource : public ISource
    {
        ColorBank &bank;
        String id;

        ColorSource(ColorBank &b, const String &colorId) : bank(b), id(colorId) {}
        Time modifiedAt() const { return bank.sourceModifiedAt(); }

        Vec4d load() const
        {
            const Record &def = bank[id];
            const ArrayValue *colorDef = nullptr;
            if (def.has("rgb"))
            {
                colorDef = &def.geta("rgb");
            }
            else
            {
                colorDef = &def.geta("rgba");
            }

            ddouble alpha = 1.0;
            if (colorDef->size() >= 4)
            {
                alpha = colorDef->at(3).asNumber();
            }

            return Vec4d(colorDef->at(0).asNumber(),
                            colorDef->at(1).asNumber(),
                            colorDef->at(2).asNumber(),
                            alpha);
        }
    };

    struct ColorData : public IData
    {
        Vec4d color;

        ColorData(const Vec4d &c = Vec4d()) : color(c) {}
    };

    Impl(Public *i) : Base(i)
    {}
};

ColorBank::ColorBank() : InfoBank("ColorBank", DisableHotStorage), d(new Impl(this))
{}

void ColorBank::addFromInfo(const File &file)
{
    LOG_AS("ColorBank");
    parse(file);
    addFromInfoBlocks("color");
}

ColorBank::Color ColorBank::color(const DotPath &path) const
{
    if (path.isEmpty()) return Color();
    Colorf col = colorf(path);
    return Color(round<dbyte>(col.x * 255),
                 round<dbyte>(col.y * 255),
                 round<dbyte>(col.z * 255),
                 round<dbyte>(col.w * 255));
}

ColorBank::Colorf ColorBank::colorf(const DotPath &path) const
{
    if (path.isEmpty()) return Colorf();
    Vec4d clamped = data(path).as<Impl::ColorData>().color;
    clamped = clamped.max(Vec4d(0, 0, 0, 0)).min(Vec4d(1, 1, 1, 1));
    return Colorf(float(clamped.x), float(clamped.y), float(clamped.z), float(clamped.w));
}

Bank::ISource *ColorBank::newSourceFromInfo(const String &id)
{
    return new Impl::ColorSource(*this, id);
}

Bank::IData *ColorBank::loadFromSource(ISource &source)
{
    return new Impl::ColorData(source.as<Impl::ColorSource>().load());
}

} // namespace de
