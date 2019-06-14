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

#include <de/Map>
#include <de/String>
#include <de/ThreadLocal>
#include <de/NativePath>

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
    Map<String, Block, String::InsensitiveLessThan> sourceData; // TrueType files

    FontDatabase()
    {
//        TTF_Init();
    }

    bool addSource(const String &name, const Block &source)
    {
//        if (TTF_Font *font =
//            TTF_OpenFontRW(SDL_RWFromConstMem(source.data(), int(source.size())), SDL_TRUE, 16))
//        {
//            const String name = fontName(font);
//            if (!sourceData.contains(name))
//            {
//                sourceData[name] = source;
//            }
//            TTF_CloseFont(font);
//            return true;
//        }
        if (!sourceData.contains(name))
        {
            sourceData[name] = source;
        }
        return false;
    }
};

static FontDatabase fontDb;

struct FontCache // thread-local
{
    Map<FontSpec, stbtt_fontinfo> fonts; // loaded fonts

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
            debug("[StbTtNativeFont] no source data for '%s'", name.c_str());
            return nullptr;
        }
        stbtt_fontinfo *font = &fonts[key];
        if (!stbtt_InitFont(font, data->second.data(), 0))
        {
            fonts.erase(key);
            debug("[StbTtNativeFont] error initializing '%s'", name.c_str());
            return nullptr;
        }
        debug("[StbTtNativeFont] initialized %p '%s'", font, name.c_str());
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
            height    = roundi(self().pointSize() * pixelRatio());
            fontScale = stbtt_ScaleForPixelHeight(font, height);

            int fontAscent, fontDescent, fontLineGap;
            stbtt_GetFontVMetrics(font, &fontAscent, &fontDescent, &fontLineGap);

            ascent     = roundi(fontScale * fontAscent);
            descent    = roundi(-fontScale * fontDescent);
            lineHeight = roundi(fontScale * (fontAscent - fontDescent + fontLineGap));
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
    Rectanglef rasterize(const String &text,
                         Image *       image,
                         const Vec2f   imageOffset = {},
                         Image::Color  foreground = {},
                         Image::Color  background = {})
    {
        if (image)
        {
            image->fill(background);
        }
        Rectanglef bounds;
        Vec2f pos = imageOffset;
        int previousUcp = 0;
        for (Char ch : text)
        {
            const int ucp = int(ch.unicode());
            if (previousUcp)
            {
                pos.x += fontScale * stbtt_GetCodepointKernAdvance(font, previousUcp, ucp);
            }

            int advance;
            int leftSideBearing;
            Vec2i glyphPoint[2];
            float xShift = pos.x - std::floor(pos.x);
            stbtt_GetCodepointHMetrics(font, ucp, &advance, &leftSideBearing);
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
            glyphBounds.move({int(pos.x), 0});
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
                glyphBounds.move({0, ascent});

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
                    duint32 *dst = image->row32(y);
                    for (int x = glyphBounds.left(), sx = 0; x < glyphBounds.right(); ++x, ++sx)
                    {
                        const duint8 cval = src[sx];
                        dst[x] = Image::packColor(Image::mix(Image::unpackColor(dst[x]),
                                                             foreground,
                                                             Image::Color(cval, cval, cval, cval)));
                    }
                }
            }

            pos.x += fontScale * advance;
            previousUcp = ucp;
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
        return d->rasterize(text, nullptr).toRectanglei();
    }
    return {};
}

int StbTtNativeFont::nativeFontWidth(const String &text) const
{
    return int(nativeFontMeasure(d->transform(text)).right());
}

Image StbTtNativeFont::nativeFontRasterize(const String &      text,
                                           const Image::Color &foreground,
                                           const Image::Color &background) const
{
    if (!d->font) return {};
/*
    SDL_Surface *pal =
        TTF_RenderUTF8_Shaded(d->font,
                              d->transform(text).c_str(),
                              SDL_Color{255, 255, 255, 255},
                              SDL_Color{000, 000, 000, 255});
    if (!pal)
    {
        return {};
    }
    DE_ASSERT(pal->w && pal->h);
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(pal, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(pal);
    SDL_LockSurface(rgba);*/

    const Rectanglef bounds = d->rasterize(text, nullptr).toRectanglei();
    Image img{Vec2i(ceil(bounds.width()) + 1, ceil(bounds.height()) + 1).toVec2ui(), Image::RGBA_8888};
//    img.fill(background);
//                    Block(rgba->pixels, dsize(rgba->h * rgba->pitch))};
//    SDL_UnlockSurface(rgba);
//    SDL_FreeSurface(rgba);
//    const int comps[4] = {0, 0, 0, 0}; // red channel used for blending each component
//    return img.mixed(background, foreground, comps);
    d->rasterize(text, &img, -bounds.topLeft, foreground, background);
    img.save(Stringf("raster_%p.png", cstr_String(text)));
    return img;
}

bool StbTtNativeFont::load(const String &fontName, const Block &fontData) // static
{
    return fontDb.addSource(fontName, fontData);
}

} // namespace de
