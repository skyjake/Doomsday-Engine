/** @file coretextnativefont_macx.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "coretextnativefont_macx.h"

#include <QFont>
#include <QColor>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

namespace de {

struct CoreTextFontCache : public Lockable
{
    struct Key {
        String name;
        dfloat size;

        Key(String const &n = "", dfloat s = 12.f) : name(n), size(s) {}
        bool operator < (Key const &other) const {
            if(name == other.name) return size < other.size;
            if(fequal(size, other.size)) return false;
            return name < other.name;
        }
    };

    typedef QMap<Key, CTFontRef> Fonts;
    Fonts fonts;

    ~CoreTextFontCache()
    {
        clear();
    }

    void clear()
    {
        DENG2_GUARD(this);

        foreach(CTFontRef ref, fonts.values())
        {
            CFRelease(ref);
        }
    }

    CTFontRef getFont(String const &postScriptName, dfloat pointSize)
    {
        DENG2_GUARD(this);

        Key const key(postScriptName, pointSize);
        if(fonts.contains(key))
        {
            // Already got it.
            return fonts[key];
        }

        // Get a reference to the font.
        CFStringRef name = CFStringCreateWithCharacters(nil, (UniChar *) postScriptName.data(),
                                                        postScriptName.size());
        CTFontRef font = CTFontCreateWithName(name, pointSize, nil);
        CFRelease(name);

        qDebug() << "cached" << postScriptName << pointSize;

        fonts.insert(key, font);
        return font;
    }
};

static CoreTextFontCache fontCache;

DENG2_PIMPL(CoreTextNativeFont)
{
    CTFontRef font;
    float ascent;
    float descent;
    float height;
    float lineSpacing;

    String lineText;
    CTLineRef line;

    Instance(Public *i)
        : Base(i)
        , font(0)
        , ascent(0)
        , descent(0)
        , height(0)
        , lineSpacing(0)
        , line(0)
    {}

    Instance(Public *i, Instance const &other)
        : Base(i)
        , font(other.font)
        , ascent(other.ascent)
        , descent(other.descent)
        , height(other.height)
        , lineSpacing(other.lineSpacing)
        , line(0)
    {}

    ~Instance()
    {
        release();
    }

    void release()
    {
        font = 0;
        releaseLine();
    }

    void releaseLine()
    {
        if(line)
        {
            CFRelease(line);
            line = 0;
        }
        lineText.clear();
    }

    void updateFontAndMetrics()
    {
        release();

        // Get a reference to the font.
        font = fontCache.getFont(self.nativeFontName(), self.size());

        // Get basic metrics about the font.
        ascent      = CTFontGetAscent(font);
        descent     = CTFontGetDescent(font);
        height      = ascent + descent;
        lineSpacing = height + CTFontGetLeading(font);
    }

    void makeLine(String const &text)
    {
        if(lineText == text) return; // Already got it.

        releaseLine();
        lineText = text;

        void const *keys[]   = { kCTFontAttributeName };
        void const *values[] = { font };
        CFDictionaryRef attribs = CFDictionaryCreate(nil, keys, values, 1, nil, nil);
                                                     //kCFTypeDictionaryKeyCallBacks,
                                                     //kCFTypeDictionaryValueCallBacks);

        CFStringRef textStr = CFStringCreateWithCharacters(nil, (UniChar *) text.data(), text.size());
        CFAttributedStringRef as = CFAttributedStringCreate(0, textStr, attribs);
        line = CTLineCreateWithAttributedString(as);

        CFRelease(attribs);
        CFRelease(textStr);
        CFRelease(as);
    }
};

} // namespace de

namespace de {

CoreTextNativeFont::CoreTextNativeFont(String const &family)
    : NativeFont(family), d(new Instance(this))
{}

CoreTextNativeFont::CoreTextNativeFont(QFont const &font)
    : NativeFont(font.family()), d(new Instance(this))
{
    setSize(font.pointSizeF());
    setWeight(font.weight());
    setStyle(font.italic()? Italic : Regular);
}

CoreTextNativeFont::CoreTextNativeFont(CoreTextNativeFont const &other)
    : NativeFont(other), d(new Instance(this, *other.d))
{
    // If the other is ready, this will be too.
    setState(other.state());
}

CoreTextNativeFont &CoreTextNativeFont::operator = (CoreTextNativeFont const &other)
{
    NativeFont::operator = (other);
    d.reset(new Instance(this, *other.d));
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
    return roundi(d->ascent);
}

int CoreTextNativeFont::nativeFontDescent() const
{
    return roundi(d->descent);
}

int CoreTextNativeFont::nativeFontHeight() const
{
    return roundi(d->height);
}

int CoreTextNativeFont::nativeFontLineSpacing() const
{
    return roundi(d->lineSpacing);
}

Rectanglei CoreTextNativeFont::nativeFontMeasure(String const &text) const
{
    d->makeLine(text);

    //CGLineGetImageBounds(d->line, d->gc);

    Rectanglei rect(Vector2i(0, -d->ascent),
                    Vector2i(CTLineGetTypographicBounds(d->line, NULL, NULL, NULL),
                             d->descent));

    return rect;
}

int CoreTextNativeFont::nativeFontWidth(String const &text) const
{
    d->makeLine(text);
    return CTLineGetTypographicBounds(d->line, NULL, NULL, NULL);
}

QImage CoreTextNativeFont::nativeFontRasterize(String const &text,
                                               Vector4ub const &foreground,
                                               Vector4ub const &background) const
{
    d->makeLine(text);

    Rectanglei const bounds = measure(text);

    //qDebug() << text << foreground.asText() << background.asText();

    QImage backbuffer(QSize(bounds.width(), bounds.height()), QImage::Format_ARGB32);//_Premultiplied);
    backbuffer.fill(QColor(background.x, background.y, background.z, background.w).rgba());

    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGContextRef gc = CGBitmapContextCreate(backbuffer.bits(),
                                            backbuffer.width(),
                                            backbuffer.height(),
                                            8, 4 * backbuffer.width(),
                                            colorspace,
                                            kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorspace);

    Vector4f const fg = foreground.toVector4f() * 255.f;
    Vector4f const bg = background.toVector4f() * 255.f;

    //CGContextSetRGBStrokeColor(gc, fg.x, fg.y, fg.z, fg.w);
    //CGContextSetRGBFillColor(gc, fg.x, fg.y, fg.z, fg.w);
    CGContextSetTextPosition(gc, 0, d->descent);
    CTLineDraw(d->line, gc);

    return backbuffer;
}

} // namespace de
