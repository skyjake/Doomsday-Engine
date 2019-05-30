/** @file sdlnativefont.cpp
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

#include "sdlnativefont.h"
#include <SDL_ttf.h>
#include <de/Map>
#include <de/String>

namespace de {

struct FontSpec
{
    String name;
    int    size;
    int    ttfStyle; // bold or italic

    bool operator<(const FontSpec &other) const
    {
        const int c = name.compareWithoutCase(other.name);
        if (c == 0)
        {
            if (size == other.size)
            {
                return cmp(ttfStyle, other.ttfStyle);
            }
            return cmp(size, other.size);
        }
        return c;
    }
};

struct FontDeleter
{
    void operator()(TTF_Font *font)
    {
        TTF_CloseFont(font);
    }
};

struct FontCache
{
    Map<String, Block, String::InsensitiveLessThan> sourceData; // TrueType
    Map<FontSpec, std::unique_ptr<TTF_Font, FontDeleter>> fonts; // loaded fonts

    FontCache()
    {
        TTF_Init();
    }

    ~FontCache()
    {
        fonts.clear();
        TTF_Quit();
    }

    bool addSource(const Block &source)
    {
        const int size = 16;
        if (TTF_Font *font =
                TTF_OpenFontRW(SDL_RWFromConstMem(source.data(), int(source.size())), 0, size))
        {
            const String familyName = TTF_FontFaceFamilyName(font);
            fonts[{familyName, size, TTF_STYLE_NORMAL}].reset(font);
            sourceData[familyName] = source;
            return true;
        }
        return false;
    }

    TTF_Font *load(const String &family, int size, int ttfStyle)
    {
        auto found = fonts.find({family, size, ttfStyle});
        if (found != fonts.end())
        {
            return found->second.get();
        }
        auto data = sourceData.find(family);
        if (data == sourceData.end())
        {
            return nullptr;
        }
        TTF_Font *font = TTF_OpenFontRW(
            SDL_RWFromConstMem(data->second.data(), int(data->second.size())), 0, size);
        DE_ASSERT(font != nullptr);
        if (!font)
        {
            return nullptr;
        }
        TTF_SetFontStyle(font, ttfStyle);
        fonts[{family, size, ttfStyle}].reset(font);
        return font;
    }

    TTF_Font *getFont(const String &family, int size, NativeFont::Weight weight,
                      NativeFont::Style style)
    {
        int ttfStyle = TTF_STYLE_NORMAL;
        if (weight >= NativeFont::Bold) ttfStyle |= TTF_STYLE_BOLD;
        if (style == NativeFont::Italic) ttfStyle |= TTF_STYLE_ITALIC;

        return load(family, size, ttfStyle);
    }
};

static FontCache s_fontCache;

DE_PIMPL(SdlNativeFont)
{
    TTF_Font *font = nullptr;
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
        font = s_fontCache.getFont(self().nativeFontName(),
                                   de::roundi(self().size()),
                                   NativeFont::Weight(self().weight()),
                                   self().style());
        if (font)
        {
            height     = TTF_FontHeight(font);
            ascent     = TTF_FontAscent(font);
            descent    = TTF_FontDescent(font);
            lineHeight = TTF_FontLineSkip(font);
        }
        else
        {
            height = ascent = descent = lineHeight = 0;
        }
    }
};

SdlNativeFont::SdlNativeFont(const String &family)
    : NativeFont(family), d(new Impl(this))
{}

SdlNativeFont::SdlNativeFont(const SdlNativeFont &other)
    : NativeFont(other), d(new Impl(this, *other.d))
{}

SdlNativeFont &SdlNativeFont::operator=(const SdlNativeFont &other)
{
    NativeFont::operator=(other);
    d.reset(new Impl(this, *other.d));
    return *this;
}

void SdlNativeFont::commit() const
{
    d->updateFontAndMetrics();
}

int SdlNativeFont::nativeFontAscent() const
{
    return d->ascent;
}
    
int SdlNativeFont::nativeFontDescent() const
{
    return d->descent;
}
    
int SdlNativeFont::nativeFontHeight() const
{
    return d->height;
}
    
int SdlNativeFont::nativeFontLineSpacing() const
{
    return d->lineHeight;
}
   
Rectanglei SdlNativeFont::nativeFontMeasure(const String &text) const
{    
    if (d->font)
    {
        Vec2i size;
        TTF_SizeUTF8(d->font, text.c_str(), &size.x, &size.y);
        return {{}, size};
    }
    return {};
}
    
int SdlNativeFont::nativeFontWidth(const String &text) const
{
    if (d->font)
    {
        int w = 0;
        TTF_SizeUTF8(d->font, text.c_str(), &w, nullptr);
        return w;
    }
    return 0;
}    

Image SdlNativeFont::nativeFontRasterize(const String &      text,
                                         const Image::Color &foreground,
                                         const Image::Color &background) const
{
    if (!d->font) return {};

    SDL_Surface *pal =
        TTF_RenderUTF8_Shaded(d->font,
                              text.c_str(),
                              SDL_Color{foreground.x, foreground.y, foreground.z, foreground.w},
                              SDL_Color{background.x, background.y, background.z, background.w});
    if (!pal)
    {
        return {};
    }
    SDL_Surface *rgba = SDL_ConvertSurfaceFormat(pal, SDL_PIXELFORMAT_ABGR32, 0);
    SDL_FreeSurface(pal);
    SDL_LockSurface(rgba);
    Image img(Image::Size(duint(rgba->w), duint(rgba->h)),
              Image::RGBA_8888,
              ByteRefArray(rgba->pixels, dsize(rgba->h * rgba->pitch)));
    SDL_UnlockSurface(rgba);
    SDL_FreeSurface(rgba);
    return img;
}

bool SdlNativeFont::load(const Block &fontData) // static
{
    return s_fontCache.addSource(fontData);
}

} // namespace de
