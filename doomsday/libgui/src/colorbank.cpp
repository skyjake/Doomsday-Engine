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
#include <de/ScriptedInfo>

namespace de {

DENG2_PIMPL(ColorBank)
{
    struct ColorSource : public ISource
    {
        Instance *d;
        String id;

        ColorSource(Instance *inst, String const &colorId) : d(inst), id(colorId) {}
        Time modifiedAt() const { return d->modTime; }

        Vector4d load() const
        {
            ArrayValue const &colorDef = d->info[id].value<ArrayValue>();

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

    Time modTime;
    ScriptedInfo info;

    Instance(Public *i) : Base(i)
    {}
};

ColorBank::ColorBank()
    : Bank(DisableHotStorage), d(new Instance(this))
{}

void ColorBank::addFromInfo(String const &source)
{
    LOG_AS("ColorBank");
    try
    {
        d->modTime = Time();
        d->info.parse(source);

        foreach(String fn, d->info.allBlocksOfType("color"))
        {
            add(fn, new Instance::ColorSource(d, fn));
        }
    }
    catch(Error const &er)
    {
        LOG_WARNING("Failed to read Info source:\n") << er.asText();
    }
}

void ColorBank::addFromInfo(File const &file)
{
    addFromInfo(String::fromUtf8(Block(file)));
    d->modTime = file.status().modifiedAt;
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

Bank::IData *ColorBank::loadFromSource(ISource &source)
{
    return new Instance::ColorData(static_cast<Instance::ColorSource &>(source).load());
}

} // namespace de
