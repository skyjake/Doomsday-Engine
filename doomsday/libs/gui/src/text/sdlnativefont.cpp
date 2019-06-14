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
#include <de/Map>
#include <de/String>
#include <de/ThreadLocal>
#include <SDL_ttf.h>

namespace de {

struct FontSpec
{
    String name; // family followed by style
    int    pixelSize;

    bool operator<(const FontSpec &other) const
    {
        const int cmp = name.compareWithoutCase(other.name);
        if (cmp == 0)
        {
            return pixelSize < other.pixelSize;
        }
        return cmp < 0;
    }
};

struct FontDeleter
{
    void operator()(TTF_Font *font)
    {
        debug("[SdlNativeFont] closing font %p", font);
        TTF_CloseFont(font);
    }
};

static String fontName(TTF_Font *font)
{
    const String familyName = TTF_FontFaceFamilyName(font);
    const String styleName  = TTF_FontFaceStyleName(font);
    String name = familyName;
    if (styleName)
    {
        name += " ";
        name += styleName;
    }
    return name;
}
    
struct FontDatabase
{
    Map<String, Block, String::InsensitiveLessThan> sourceData; // TrueType

    FontDatabase()
    {
        TTF_Init();
    }
    
    bool addSource(const Block &source)
    {
        if (TTF_Font *font =
            TTF_OpenFontRW(SDL_RWFromConstMem(source.data(), int(source.size())), SDL_TRUE, 16))
        {
            const String name = fontName(font);
            if (!sourceData.contains(name))
            {
                sourceData[name] = source;
            }
            TTF_CloseFont(font);
            return true;
        }
        return false;
    }
};
    
static FontDatabase fontDb;
    
struct FontCache // thread-local
{
    Map<FontSpec, std::unique_ptr<TTF_Font, FontDeleter>> fonts; // loaded fonts
    
    TTF_Font *load(const String &name, int pixelSize)
    {
        auto found = fonts.find({name, pixelSize});
        if (found != fonts.end())
        {
            return found->second.get();
        }
        auto data = fontDb.sourceData.find(name);
        if (data == fontDb.sourceData.end())
        {
            debug("[SdlNativeFont] no source data for '%s'", name.c_str());
            return nullptr;
        }
        TTF_Font *font = TTF_OpenFontRW(
            SDL_RWFromConstMem(data->second.data(), int(data->second.size())), SDL_TRUE, pixelSize);
        DE_ASSERT(font != nullptr);
        if (!font)
        {
            debug("[SdlNativeFont] failed to open '%s' size:%d", name.c_str(), pixelSize);
            return nullptr;
        }
        debug("[SdlNativeFont] loaded %p '%s' size:%d", font, name.c_str(), pixelSize);
        fonts[{name, pixelSize}].reset(font);
        return font;
    }

    TTF_Font *getFont(const String &family, int pixelSize, NativeFont::Weight weight,
                      NativeFont::Style style)
    {
        String name = family;
        if (weight == NativeFont::Normal)
        {
            name += (style == NativeFont::Italic ? " Italic" : " Regular");
        }
        else
        {
            name += (weight >= NativeFont::Bold ? " Bold" : " Light");
            if (style == NativeFont::Italic) name += " Italic";
        }
        return load(name.c_str(), pixelSize);
    }
};

static ThreadLocal<FontCache> s_fontCache;

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
        font = s_fontCache.get().getFont(self().nativeFontName(),
                                         roundi(self().pointSize() * pixelRatio()),
                                         NativeFont::Weight(self().weight()),
                                         self().style());
        if (font)
        {
            height     = TTF_FontHeight(font);
            ascent     = TTF_FontAscent(font);
            descent    = -TTF_FontDescent(font);
            lineHeight = TTF_FontLineSkip(font);
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
        TTF_SizeUTF8(d->font, d->transform(text).c_str(), &size.x, &size.y);
        Rectanglei rect{{}, size};
        rect.move({0, -d->ascent});
        return rect;
    }
    return {};
}
    
int SdlNativeFont::nativeFontWidth(const String &text) const
{
    return int(nativeFontMeasure(d->transform(text)).width());
}    

Image SdlNativeFont::nativeFontRasterize(const String &      text,
                                         const Image::Color &foreground,
                                         const Image::Color &background) const
{
    if (!d->font) return {};

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
    SDL_LockSurface(rgba);
    const Image img{{duint(rgba->w), duint(rgba->h)}, Image::RGBA_8888,
                    Block(rgba->pixels, dsize(rgba->h * rgba->pitch))};
    SDL_UnlockSurface(rgba);
    SDL_FreeSurface(rgba);
    const int comps[4] = {0, 0, 0, 0}; // red channel used for blending each component
    return img.mixed(background, foreground, comps);
}

bool SdlNativeFont::load(const String &/*name*/, const Block &fontData) // static
{
    // `name` is read from the file.
    return fontDb.addSource(fontData);
}

} // namespace de
