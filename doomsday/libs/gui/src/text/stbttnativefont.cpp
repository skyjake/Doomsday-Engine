/** @file stbttnativefont.cpp  Text rendering with stb_truetype.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "stbttnativefont.h"

#include <de/keymap.h>
#include <de/string.h>
#include <de/threadlocal.h>
#include <de/nativepath.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace de {

struct FontSpec
{
    String name; // family followed by style
//    int    pixelSize;

    bool operator<(const FontSpec &other) const
    {
        const int cmp = name.compareWithoutCase(other.name);
//        if (cmp == 0)
//        {
//            return pixelSize < other.pixelSize;
//        }
        return cmp < 0;
    }
};

//struct FontDeleter
//{
//    void operator()(TTF_Font *font)
//    {
//        debug("[StbTtNativeFont] closing font %p", font);
//        TTF_CloseFont(font);
//    }
//};

//static String fontName(TTF_Font *font)
//{
//    const String familyName = TTF_FontFaceFamilyName(font);
//    const String styleName  = TTF_FontFaceStyleName(font);
//    String name = familyName;
//    if (styleName)
//    {
//        name += " ";
//        name += styleName;
//    }
//    return name;
//}

struct FontDatabase
{
    KeyMap<String, Block, String::InsensitiveLessThan> sourceData; // TrueType files

    FontDatabase()
    {
//        TTF_Init();
    }

    bool addSource(const String &name, const Block &source)
    {
        if (sourceData.contains(name)) return true; // already got it (reloading?)

        stbtt_fontinfo info;
        if (stbtt_InitFont(&info, source.data(), 0))
        {
            sourceData[name] = source;
            return true;
        }
        return false;
    }
};

static FontDatabase fontDb;

struct FontCache // thread-local
{
    KeyMap<FontSpec, stbtt_fontinfo> fonts; // loaded fonts

    const stbtt_fontinfo *load(const String &name)
    {
        const FontSpec key{name};
        auto found = fonts.find(key);
        if (found != fonts.end())
        {
            return &found->second;
        }
        auto data = fontDb.sourceData.find(name);
        if (data == fontDb.sourceData.end())
        {
            LOG_RES_ERROR("[StbTtNativeFont] no source data for '%s'") << name;
            return nullptr;
        }
        stbtt_fontinfo *font = &fonts[key];
        if (!stbtt_InitFont(font, data->second.data(), 0))
        {
            fonts.erase(key);
            LOG_RES_ERROR("[StbTtNativeFont] error initializing '%s'") << name;
            return nullptr;
        }
        LOG_RES_VERBOSE("[StbTtNativeFont] initialized %p '%s'") << font << name;
        return font;
    }

    const stbtt_fontinfo *getFont(const String &family, NativeFont::Weight weight,
                                  NativeFont::Style style)
    {
        String name = family;
        if (weight == NativeFont::Normal)
        {
            name += (style == NativeFont::Italic ? "-Italic" : "-Regular");
        }
        else
        {
            name += (weight >= NativeFont::Bold ? "-Bold" : "-Light");
            if (style == NativeFont::Italic)
            {
                name += "Italic";
            }
        }
        return load(name.c_str()/*, pixelSize*/);
    }
};

static ThreadLocal<FontCache> s_fontCache;

DE_PIMPL(StbTtNativeFont)
{
    const stbtt_fontinfo *font      = nullptr;
    float                 fontScale = 1.0f;

    int height = 0;
    int ascent = 0;
    int descent = 0;
    int lineHeight = 0;

    Impl(Public *i) : Base(i)
    {}

    Impl(Public *i, const Impl &d)
        : Base(i)
        , font(d.font)
        , height(d.height)
        , ascent(d.ascent)
        , descent(d.descent)
        , lineHeight(d.lineHeight)
    {}

    void updateFontAndMetrics()
    {
        font = s_fontCache.get().getFont(
            self().nativeFontName(), NativeFont::Weight(self().weight()), self().style());

        if (font)
        {
            fontScale = stbtt_ScaleForMappingEmToPixels(font, roundi(self().pointSize() * pixelRatio()));

            int fontAscent, fontDescent, fontLineGap;
            stbtt_GetFontVMetrics(font, &fontAscent, &fontDescent, &fontLineGap);

            // TODO: There should be a proper setting for scaling the vertical metrics
            // independent of the font scaling itself.

            const float lineSpacingFactor = 0.82f;
            float asc  = fontAscent * lineSpacingFactor;
            float desc = fontDescent * lineSpacingFactor;

            height     = roundi(fontScale * (asc - desc));
            ascent     = roundi(fontScale * asc);
            descent    = roundi(-fontScale * desc);
            lineHeight = roundi(fontScale * (asc - desc + fontLineGap));
        }
        else
        {
            height = ascent = descent = lineHeight = 0;
        }
    }

    String transform(const String &str) const
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

    /**
     * Rasterize or just measure a text string.
     *
     * @param image  Output image. Set to null if only measuring.
     *
     * @return Bounding box for the string. The origin (0,0) is at the baseline, at the
     * left edge of the first character.
     */
    Rectanglei rasterize(const String &text,
                         int *         advanceWidth,
                         Image *       image,
                         const Vec2i   imageOrigin = {},
                         Image::Color  foreground = {},
                         Image::Color  background = {})
    {
        if (image)
        {
            image->fill(background);
        }
        Rectanglei bounds;
        float xPos = 0.0f;
        int previousUcp = 0;
        for (Char ch : text)
        {
            const int ucp = int(ch.unicode());
            if (previousUcp)
            {
                xPos += fontScale * stbtt_GetCodepointKernAdvance(font, previousUcp, ucp);
            }

            int advance;
            int leftSideBearing;
            Vec2i glyphPoint[2];
            stbtt_GetCodepointHMetrics(font, ucp, &advance, &leftSideBearing);
            // Why the LSB*0.5? Don't know, but it seems to work nicely...
            float xLeft  = xPos - fontScale * leftSideBearing * 0.5f;
            float xShift = xLeft - std::floor(xLeft);
            stbtt_GetCodepointBitmapBoxSubpixel(font,
                                                ucp,
                                                fontScale,
                                                fontScale,
                                                xShift,
                                                0.0f,
                                                &glyphPoint[0].x,
                                                &glyphPoint[0].y,
                                                &glyphPoint[1].x,
                                                &glyphPoint[1].y);
            Rectanglei glyphBounds{glyphPoint[0], glyphPoint[1]};
            glyphBounds.move({int(xLeft), 0});
            if (bounds.isNull())
            {
                bounds = glyphBounds;
            }
            else
            {
                bounds |= glyphBounds;
            }

            if (image)
            {
                // Rasterize the glyph.
                Block raster(glyphBounds.area());
                stbtt_MakeCodepointBitmapSubpixel(font,
                                                  raster.data(),
                                                  glyphBounds.width(),
                                                  glyphBounds.height(),
                                                  glyphBounds.width(),
                                                  fontScale,
                                                  fontScale,
                                                  xShift,
                                                  0.0f,
                                                  ucp);

                for (int y = glyphBounds.top(), sy = 0; y < glyphBounds.bottom(); ++y, ++sy)
                {
                    const duint8 *src = &raster[sy * glyphBounds.width()];
                    duint32 *dst = image->row32(imageOrigin.y + y);
                    for (int x = glyphBounds.left(), sx = 0; x < glyphBounds.right(); ++x, ++sx)
                    {
                        DE_ASSERT(duint(imageOrigin.x + x) < image->width());
                        const duint8 cval = src[sx];
                        duint32 *    out  = &dst[imageOrigin.x + x];
                        *out = Image::packColor(Image::mix(Image::unpackColor(*out),
                                                           foreground,
                                                           Image::Color(cval, cval, cval, cval)));
                    }
                }
            }

            xPos += fontScale * advance;
            
            previousUcp = ucp;
        }
        if (advanceWidth)
        {
            *advanceWidth = roundi(xPos);
        }
        return bounds;
    }
};

StbTtNativeFont::StbTtNativeFont(const String &family)
    : NativeFont(family), d(new Impl(this))
{}

StbTtNativeFont::StbTtNativeFont(const StbTtNativeFont &other)
    : NativeFont(other), d(new Impl(this, *other.d))
{}

StbTtNativeFont &StbTtNativeFont::operator=(const StbTtNativeFont &other)
{
    NativeFont::operator=(other);
    d.reset(new Impl(this, *other.d));
    return *this;
}

void StbTtNativeFont::commit() const
{
    d->updateFontAndMetrics();
}

int StbTtNativeFont::nativeFontAscent() const
{
    return d->ascent;
}

int StbTtNativeFont::nativeFontDescent() const
{
    return d->descent;
}

int StbTtNativeFont::nativeFontHeight() const
{
    return d->height;
}

int StbTtNativeFont::nativeFontLineSpacing() const
{
    return d->lineHeight;
}

Rectanglei StbTtNativeFont::nativeFontMeasure(const String &text) const
{
    if (d->font)
    {
        return d->rasterize(d->transform(text), nullptr, nullptr);
    }
    return {};
}

int StbTtNativeFont::nativeFontAdvanceWidth(const String &text) const
{
    int advance = 0;
    d->rasterize(d->transform(text), &advance, nullptr);
    return advance;
}

Image StbTtNativeFont::nativeFontRasterize(const String &      text,
                                           const Image::Color &foreground,
                                           const Image::Color &background) const
{
    if (!d->font) return {};

    const String displayText = d->transform(text);
    Rectanglei bounds = d->rasterize(displayText, nullptr, nullptr);
    Image img{bounds.size(), Image::RGBA_8888};
    d->rasterize(displayText, nullptr, &img, -bounds.topLeft, foreground, background);
    img.setOrigin(bounds.topLeft);
//    img.save(Stringf("raster_%p.png", cstr_String(text)));
    return img;
}

bool StbTtNativeFont::load(const String &fontName, const Block &fontData) // static
{
    return fontDb.addSource(fontName, fontData);
}

} // namespace de
