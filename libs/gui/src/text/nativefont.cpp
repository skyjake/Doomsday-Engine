/** @file nativefont.cpp  Abstraction of a native font.
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

#include "de/nativefont.h"
#include <de/keymap.h>
#include <de/hash.h>
#include <de/property.h>

namespace de {

static Hash<String, NativeFont::StyleMapping> s_families;

static constexpr int MAX_CACHE_STRING_LENGTH = 200;
static constexpr int MAX_CACHE_STRINGS       = 500;

DE_STATIC_PROPERTY(NativeFontPixelRatio, float)

DE_PIMPL(NativeFont)
, DE_OBSERVES(NativeFontPixelRatio, Change)
{
    String    family;
    float     pointSize;
    Style     style;
    int       weight;
    Transform transform;

    Hash<String, Rectanglei> measureCache;

    Impl(Public *i)
        : Base(i)
        , pointSize(12.0f)
        , style(Regular)
        , weight(Normal)
        , transform(NoTransform)
    {
        pNativeFontPixelRatio.audienceForChange() += this;
    }

    void prepare()
    {
        if (!self().isReady())
        {
            self().commit();
            measureCache.clear();
            self().setState(Ready);
        }
    }

    void markNotReady()
    {
        self().setState(NotReady);
        measureCache.clear();
    }

    void valueOfNativeFontPixelRatioChanged() override
    {
        markNotReady();
    }
};

void NativeFont::defineMapping(const String &family, const StyleMapping &mapping)
{
    s_families.insert(family, mapping);
}

NativeFont::NativeFont(const String &family) : d(new Impl(this))
{
    setFamily(family);
}

NativeFont::NativeFont(const NativeFont &other) : Asset(other), d(new Impl(this))
{
    *this = other;
}

NativeFont &NativeFont::operator=(const NativeFont &other)
{
    d->family    = other.d->family;
    d->style     = other.d->style;
    d->pointSize = other.d->pointSize;
    d->weight    = other.d->weight;
    d->transform = other.d->transform;
    d->markNotReady();
    return *this;
}

void NativeFont::setFamily(const String &family)
{
    d->family = family;
    d->markNotReady();
}

void NativeFont::setPointSize(float pointSize)
{
    d->pointSize = pointSize;
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

void NativeFont::setTransform(de::NativeFont::Transform transform)
{
    d->transform = transform;
    d->markNotReady();
}

String NativeFont::family() const
{
    return d->family;
}

float NativeFont::pointSize() const
{
    return d->pointSize;
}

NativeFont::Style NativeFont::style() const
{
    return d->style;
}

dint NativeFont::weight() const
{
    return d->weight;
}

NativeFont::Transform NativeFont::transform() const
{
    return d->transform;
}

String NativeFont::nativeFontName() const
{
    // Check the defined mappings.
    auto found = s_families.find(d->family);
    if (found != s_families.end())
    {
        const auto &styleMapping = found->second;
        const Spec spec(d->style, d->weight);
        auto mapped = styleMapping.find(spec);
        if (mapped != styleMapping.end())
        {
            return mapped->second;
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

Rectanglei NativeFont::measure(const String &text) const
{
    d->prepare();

    if (text.size() < MAX_CACHE_STRING_LENGTH)
    {
        auto foundInCache = d->measureCache.find(text);
        if (foundInCache != d->measureCache.end())
        {
            return foundInCache->second;
        }
    }

    const Rectanglei bounds = nativeFontMeasure(text);

    // Remember this for later.
    if (d->measureCache.size() > MAX_CACHE_STRINGS)
    {
        // Too many strings, forget everything.
        d->measureCache.clear();
    }
    d->measureCache.insert(text, bounds);

    return bounds;
}

int NativeFont::advanceWidth(const String &text) const
{
    d->prepare();
    return nativeFontAdvanceWidth(text);
}

Image NativeFont::rasterize(const String &      text,
                            const Image::Color &foreground,
                            const Image::Color &background) const
{
    d->prepare();
    return nativeFontRasterize(text, foreground, background);
}

void NativeFont::setPixelRatio(float pixelRatio)
{
    pNativeFontPixelRatio.setValue(pixelRatio);    
}

float NativeFont::pixelRatio()
{
    return pNativeFontPixelRatio.value();
}

} // namespace de
