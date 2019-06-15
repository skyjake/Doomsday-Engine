/** @file coretextnativefont_macx.cpp
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

#include "coretextnativefont_macx.h"
#include "fonts_macx.h"
#include <de/Log>
#include <de/Thread>
#include <de/Map>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <atomic>

namespace de {

struct CoreTextFontCache : public Lockable
{
    struct Key {
        String name;
        float pointSize;

        Key(const String &n = {}, float s = 12.f)
            : name(n)
            , pointSize(s)
        {}

        bool operator<(Key const &other) const
        {
            if (name == other.name) {
                return pointSize < other.pointSize && !fequal(pointSize, other.pointSize);
            }
            return name < other.name;
        }
    };

    Map<Key, CTFontRef> fonts;
    CGColorSpaceRef     _colorspace; ///< Shared by all fonts.

    CoreTextFontCache() : _colorspace(nullptr)
    {}

    ~CoreTextFontCache()
    {
        clear();
        if (_colorspace)
        {
            CGColorSpaceRelease(_colorspace);
        }
    }

    CGColorSpaceRef colorspace()
    {
        if (!_colorspace)
        {
            _colorspace = CGColorSpaceCreateDeviceRGB();
        }
        return _colorspace;
    }

    void clear()
    {
        DE_GUARD(this);
        for (const auto &ref : fonts)
        {
            CFRelease(ref.second);
        }
    }

    CTFontRef getFont(const String postScriptName, float pointSize)
    {
        CTFontRef font;

        // Only lock the cache while accessing the fonts database. If we keep the
        // lock while printing log output, a flush might occur, which might in turn
        // lead to text rendering and need for font information -- causing a hang.
        {
            DE_GUARD(this);

            const Key key(postScriptName, pointSize);
            if (fonts.contains(key))
            {
                // Already got it.
                return fonts[key];
            }

            // System fonts.
            if (postScriptName.beginsWith("#"))
            {
                int weight = 50;
                if (postScriptName.contains("-bold")) weight = 75;
                else if (postScriptName.contains("-light")) weight = 25;
                else if (postScriptName.contains("-black")) weight = 100;
                else if (postScriptName.contains("-ultralight")) weight = 0;

                const int italic = postScriptName.contains("-italic");

                void *nsFont = nullptr;
                if (postScriptName.beginsWith("#system"))
                {
                    nsFont = Apple_CreateSystemFont(pointSize, weight, italic);
                }
                else
                {
                    nsFont = Apple_CreateMonospaceSystemFont(pointSize, weight, italic);
                }
                font = reinterpret_cast<CTFontRef>(nsFont);                
                CFRetain(font);
            }
            else
            {
                CFStringRef name =
                    CFStringCreateWithCString(nil, postScriptName.c_str(), kCFStringEncodingUTF8);
                font = CTFontCreateWithName(name, pointSize, nil);
                CFRelease(name);
            }

            fonts.insert(key, font);
        }

        LOG_GL_VERBOSE("Cached native font '%s' size %.1f") << postScriptName << pointSize;

        return font;
    }

#if 0
    float fontSize(CTFontRef font) const
    {
        DE_FOR_EACH_CONST(Fonts, i, fonts)
        {
            if (i.value() == font) return i.key().size;
        }
        DE_ASSERT_FAIL("Font not in cache");
        return 0;
    }

    int fontWeight(CTFontRef font) const
    {
        DE_FOR_EACH_CONST(Fonts, i, fonts)
        {
            if (i.value() == font)
            {
                if (i.key().name.contains("Light")) return 25;
                if (i.key().name.contains("Bold")) return 75;
                return 50;
            }
        }
        DE_ASSERT_FAIL("Font not in cache");
        return 0;
    }
#endif
};

static CoreTextFontCache fontCache;

DE_PIMPL(CoreTextNativeFont)
{
    CTFontRef font;
    float ascent;
    float descent;
    float height;
    float lineSpacing;

    struct CachedLine
    {
        String lineText;
        CTLineRef line = nullptr;

        ~CachedLine()
        {
            release();
        }

        void release()
        {
            if (line)
            {
                CFRelease(line);
                line = nullptr;
            }
            lineText.clear();
        }
    };
    CachedLine cache;

    Impl(Public *i)
        : Base(i)
        , font(nullptr)
        , ascent(0)
        , descent(0)
        , height(0)
        , lineSpacing(0)
    {}

    Impl(Public *i, Impl const &other)
        : Base(i)
        , font(other.font)
        , ascent(other.ascent)
        , descent(other.descent)
        , height(other.height)
        , lineSpacing(other.lineSpacing)
    {}

    ~Impl()
    {
        release();
    }

    String applyTransformation(String const &str) const
    {
        switch (self().transform())
        {
        case Uppercase:
            return str.upper();

        case Lowercase:
            return str.lower();

        default:
            break;
        }
        return str;
    }

    void release()
    {
        font = nullptr;
        cache.release();
    }

    void updateFontAndMetrics()
    {
        release();

        // Get a reference to the font.
        font = fontCache.getFont(self().nativeFontName(), self().pointSize());

        // Get basic metrics about the font (in points).
        ascent      = float(CTFontGetAscent(font));
        descent     = float(CTFontGetDescent(font));
        height      = ascent + descent;
        lineSpacing = height + float(CTFontGetLeading(font));
    }

    CachedLine &makeLine(String const &text, CGColorRef color = nullptr)
    {
        if (cache.lineText == text)
        {
            return cache; // Already got it.
        }

        cache.release();
        cache.lineText = text;

        void const *keys[]   = { kCTFontAttributeName, kCTForegroundColorAttributeName };
        void const *values[] = { font, color };
        CFDictionaryRef attribs = CFDictionaryCreate(nil, keys, values,
                                                     color? 2 : 1, nil, nil);

        CFStringRef textStr = CFStringCreateWithCString(nil, text.c_str(), kCFStringEncodingUTF8);
        DE_ASSERT(textStr != NULL);
        CFAttributedStringRef as = CFAttributedStringCreate(nullptr, textStr, attribs);
        cache.line = CTLineCreateWithAttributedString(as);

        CFRelease(attribs);
        CFRelease(textStr);
        CFRelease(as);
        return cache;
    }
};

} // namespace de

namespace de {

CoreTextNativeFont::CoreTextNativeFont(String const &family)
    : NativeFont(family), d(new Impl(this))
{}

CoreTextNativeFont::CoreTextNativeFont(CoreTextNativeFont const &other)
    : NativeFont(other)
    , d(new Impl(this, *other.d))
{
    // If the other is ready, this will be too.
    setState(other.state());
}

CoreTextNativeFont &CoreTextNativeFont::operator=(CoreTextNativeFont const &other)
{
    NativeFont::operator=(other);
    d.reset(new Impl(this, *other.d));
    // If the other is ready, this will be too.
    setState(other.state());
    return *this;
}

void CoreTextNativeFont::commit() const
{
    d->updateFontAndMetrics();
}

int CoreTextNativeFont::nativeFontAscent() const
{
    return roundi(d->ascent * pixelRatio());
}

int CoreTextNativeFont::nativeFontDescent() const
{
    return roundi(d->descent * pixelRatio());
}

int CoreTextNativeFont::nativeFontHeight() const
{
    return roundi(d->height * pixelRatio());
}

int CoreTextNativeFont::nativeFontLineSpacing() const
{
    return roundi(d->lineSpacing * pixelRatio());
}

Rectanglei CoreTextNativeFont::nativeFontMeasure(String const &text) const
{
    d->makeLine(d->applyTransformation(text));

    //CGLineGetImageBounds(d->line, d->gc); // more accurate but slow

    Rectanglei rect(
        Vec2i(0, roundi(-d->ascent * pixelRatio())),
        Vec2i(roundi(CTLineGetTypographicBounds(d->cache.line, NULL, NULL, NULL) * pixelRatio()),
              roundi(d->descent * pixelRatio())));

    return rect;
}

int CoreTextNativeFont::nativeFontAdvanceWidth(String const &text) const
{
    auto &cachedLine = d->makeLine(d->applyTransformation(text));
    return roundi(CTLineGetTypographicBounds(cachedLine.line, NULL, NULL, NULL) * pixelRatio());
}

Image CoreTextNativeFont::nativeFontRasterize(const String &text,
                                              const Image::Color &foreground,
                                              const Image::Color &background) const
{
#if 0
    DE_ASSERT(fequal(fontCache.fontSize(d->font), size()));
    DE_ASSERT(fontCache.fontWeight(d->font) == weight());
#endif

    // Text color.
    const Vec4d fg = foreground.toVec4f() / 255.f;
    CGFloat fgValues[4] = { fg.x, fg.y, fg.z, fg.w };
    CGColorRef fgColor = CGColorCreate(fontCache.colorspace(), fgValues);

    // Ensure the color is used by recreating the attributed line string.
    d->cache.release();
    d->makeLine(d->applyTransformation(text), fgColor);

    // Set up the bitmap for drawing into.
    const Rectanglei bounds = measure(d->cache.lineText);
    Image backbuffer(bounds.size(), Image::RGBA_8888);
    backbuffer.setOrigin(bounds.topLeft);
    backbuffer.fill(background);

    CGContextRef gc = CGBitmapContextCreate(backbuffer.bits(),
                                            backbuffer.width(),
                                            backbuffer.height(),
                                            8, 4 * backbuffer.width(),
                                            fontCache.colorspace(),
                                            kCGImageAlphaPremultipliedLast);
    CGContextScaleCTM(gc, pixelRatio(), pixelRatio());

    CGContextSetTextPosition(gc, -bounds.topLeft.x, -bounds.topLeft.y);
    CTLineDraw(d->cache.line, gc);

    CGColorRelease(fgColor);
    CGContextRelease(gc);
    d->cache.release();

    return backbuffer;
}

bool CoreTextNativeFont::load(const String &, const Block &)
{
    warning("[CoreTextNativeFont] loading font data is not supported");
    return false;
}

} // namespace de
