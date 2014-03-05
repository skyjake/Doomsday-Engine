/** @file nativefont.cpp  Abstraction of a native font.
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

#include "de/NativeFont"

namespace de
{
    typedef QMap<String, NativeFont::StyleMapping> Families;
    static Families families;

    DENG2_PIMPL(NativeFont)
    {
        String family;
        dfloat size;
        Style style;
        int weight;

        String cachedText; // Measuring is done repeatedly
        Rectanglei cachedMeasure;

        Instance(Public *i)
            : Base(i)
            , size(12.f)
            , style(Regular)
            , weight(Normal)
        {}

        void prepare()
        {
            if(!self.isReady())
            {
                self.commit();
                cachedText.clear();
                self.setState(Ready);
            }
        }

        void markNotReady()
        {
            self.setState(NotReady);
            cachedText.clear();
        }
    };
}

namespace de {

void NativeFont::defineMapping(String const &family, StyleMapping const &mapping)
{
    families.insert(family, mapping);
}

NativeFont::NativeFont(String const &family) : d(new Instance(this))
{
    setFamily(family);
}

NativeFont::NativeFont(NativeFont const &other) : Asset(other), d(new Instance(this))
{
    *this = other;
}

NativeFont &NativeFont::operator = (NativeFont const &other)
{
    d->family = other.d->family;
    d->style  = other.d->style;
    d->size   = other.d->size;
    d->weight = other.d->weight;
    d->markNotReady();
    return *this;
}

void NativeFont::setFamily(String const &family)
{
    d->family = family;
    d->markNotReady();
}

void NativeFont::setSize(dfloat size)
{
    d->size = size;
    d->markNotReady();
}

void NativeFont::setStyle(Style style)
{
    d->style = style;
    d->markNotReady();
}

void NativeFont::setWeight(dint weight)
{
    d->weight = weight;
    d->markNotReady();
}

String NativeFont::family() const
{
    return d->family;
}

dfloat NativeFont::size() const
{
    return d->size;
}

NativeFont::Style NativeFont::style() const
{
    return d->style;
}

dint NativeFont::weight() const
{
    return d->weight;
}

String NativeFont::nativeFontName() const
{
    // Check the defined mappings.
    if(families.contains(d->family))
    {
        StyleMapping const &map = families[d->family];
        Spec const spec(d->style, d->weight);
        if(map.contains(spec))
        {
            return map[spec];
        }
    }
    return d->family;
}

int NativeFont::ascent() const
{
    d->prepare();
    return nativeFontAscent();
}

int NativeFont::descent() const
{
    d->prepare();
    return nativeFontDescent();
}

int NativeFont::height() const
{
    d->prepare();
    return nativeFontHeight();
}

int NativeFont::lineSpacing() const
{
    d->prepare();
    return nativeFontLineSpacing();
}

Rectanglei NativeFont::measure(String const &text) const
{
    d->prepare();

    if(d->cachedText == text)
    {
        return d->cachedMeasure;
    }

    Rectanglei const bounds = nativeFontMeasure(text);

    // Remember this for later.
    d->cachedText = text;
    d->cachedMeasure = bounds;

    return bounds;
}

int NativeFont::width(String const &text) const
{
    d->prepare();
    return nativeFontWidth(text);
}

QImage NativeFont::rasterize(String const &text,
                             Vector4ub const &foreground,
                             Vector4ub const &background) const
{
    d->prepare();
    return nativeFontRasterize(text, foreground, background);
}

} // namespace de
