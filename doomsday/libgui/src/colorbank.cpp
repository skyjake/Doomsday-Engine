/** @file colorbank.cpp  Bank of colors.
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

#include "de/ColorBank"
#include <de/ArrayValue>

namespace de {

DENG2_PIMPL(ColorBank)
{
    struct ColorSource : public ISource
    {
        ColorBank &bank;
        String id;

        ColorSource(ColorBank &b, String const &colorId) : bank(b), id(colorId) {}
        Time modifiedAt() const { return bank.sourceModifiedAt(); }

        Vector4d load() const
        {
            ArrayValue const &colorDef = bank[id].value<ArrayValue>();

            // Alpha component is optional.
            ddouble alpha = 1.0;
            if(colorDef.size() >= 4)
            {
                alpha = colorDef.at(3).asNumber();
            }

            return Vector4d(colorDef.at(0).asNumber(),
                            colorDef.at(1).asNumber(),
                            colorDef.at(2).asNumber(),
                            alpha);
        }
    };

    struct ColorData : public IData
    {
        Vector4d color;

        ColorData(Vector4d const &c = Vector4d()) : color(c) {}
        duint sizeInMemory() const { return 0; /* we don't count */ }
    };

    Instance(Public *i) : Base(i)
    {}
};

ColorBank::ColorBank() : InfoBank(DisableHotStorage), d(new Instance(this))
{}

void ColorBank::addFromInfo(File const &file)
{
    LOG_AS("ColorBank");
    parse(file);
    addFromInfoBlocks("color");
}

ColorBank::Color ColorBank::color(Path const &path) const
{
    Colorf col = colorf(path);
    return Color(dbyte(col.x * 255 + .5f), dbyte(col.y * 255 + .5f),
                 dbyte(col.z * 255 + .5f), dbyte(col.w * 255 + .5f));
}

ColorBank::Colorf ColorBank::colorf(Path const &path) const
{
    Vector4d clamped = static_cast<Instance::ColorData &>(data(path)).color;
    clamped = clamped.max(Vector4d(0, 0, 0, 0)).min(Vector4d(1, 1, 1, 1));
    return Colorf(float(clamped.x), float(clamped.y), float(clamped.z), float(clamped.w));
}

Bank::ISource *ColorBank::newSourceFromInfo(String const &id)
{
    return new Instance::ColorSource(*this, id);
}

Bank::IData *ColorBank::loadFromSource(ISource &source)
{
    return new Instance::ColorData(static_cast<Instance::ColorSource &>(source).load());
}

} // namespace de
