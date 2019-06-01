/** @file font.cpp  Font with metrics.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Font"

#include <de/ConstantRule>
#include <de/Hash>
#include <de/ThreadLocal>

#if (defined(MACOSX) && defined(MACOS_10_7)) || defined (DE_IOS)
#  include "../src/text/coretextnativefont_macx.h"
namespace de { typedef CoreTextNativeFont PlatformFont; }
#else
#  define DE_USING_SDL_TTF
#  include "../src/text/sdlnativefont.h"
namespace de { typedef SdlNativeFont PlatformFont; }
#endif

namespace std {
template<>
struct hash<de::FontParams> {
    std::size_t operator()(const de::FontParams &fp) const {
        return hash<de::String>()(fp.family)
             ^ hash<int>()(int(100 * fp.size))
             ^ hash<int>()(fp.spec.weight)
             ^ hash<int>()(int(fp.spec.style))
             ^ hash<int>()(fp.spec.transform);
    }
};
} // namespace std

namespace de {
namespace internal {

    struct ThreadFonts
    {
        PlatformFont font;
        Hash<FontParams, PlatformFont *> fontMods;

        ~ThreadFonts() { fontMods.deleteAll(); }
    };

} // namespace internal

/**
 * Each thread uses its own independent set of native font instances. This allows
 * background threads to freely measure and render text using the native font
 * instances without any synchronization. Also note that these background threads are
 * pooled, so there is only a fixed total number of threads accessing these objects.
 */
static ThreadLocal<Hash<Font *, internal::ThreadFonts>> fontsForThread;

DE_PIMPL(Font)
{
    FontParams fontParams;
    internal::ThreadFonts *threadFonts = nullptr;

    ConstantRule *heightRule;
    ConstantRule *ascentRule;
    ConstantRule *descentRule;
    ConstantRule *lineSpacingRule;
    int ascent = 0;

    Impl(Public *i) : Base(i)
    {
        createRules();
    }

    Impl(Public * i, const FontParams &fp)
        : Base(i)
        , fontParams(fp)
    {
        createRules();
        updateMetrics();
    }

    ~Impl()
    {
        fontsForThread.get().remove(thisPublic); // FIXME: not removed by all threads...

        releaseRef(heightRule);
        releaseRef(ascentRule);
        releaseRef(descentRule);
        releaseRef(lineSpacingRule);
    }

    void createRules()
    {
        heightRule      = new ConstantRule(0);
        ascentRule      = new ConstantRule(0);
        descentRule     = new ConstantRule(0);
        lineSpacingRule = new ConstantRule(0);
    }

    /**
     * Initializes the current thread's platform fonts for this Font.
     */
    internal::ThreadFonts &getThreadFonts()
    {
        auto &hash = fontsForThread.get();
        auto found = hash.find(thisPublic);
        if (found != hash.end())
        {
            auto &tf = found->second;
            if (fequal(tf.font.size(), fontParams.size))
            {
                return tf;
            }
            // Size has changed, re-initialize the font.
            tf.fontMods.deleteAll();
            tf.fontMods.clear();
        }
        auto &font = hash[thisPublic].font;
        font.setFamily(fontParams.family);
        font.setSize(fontParams.size);
        font.setStyle(fontParams.spec.style);
        font.setTransform(fontParams.spec.transform);
        font.setWeight(fontParams.spec.weight);
        return hash[thisPublic];
    }

    void updateMetrics()
    {
        auto &plat = getThreadFonts();

        ascent = plat.font.ascent();
        if (plat.font.weight() != NativeFont::Normal)
        {
            // Use the ascent of the normal weight for non-normal weights;
            // we need to align content to baseline regardless of weight.
            PlatformFont normalized(plat.font);
            normalized.setWeight(NativeFont::Normal);
            ascent = normalized.ascent();
        }

        ascentRule     ->set(ascent);
        descentRule    ->set(plat.font.descent());
        heightRule     ->set(plat.font.height());
        lineSpacingRule->set(plat.font.lineSpacing());
    }

    static PlatformFont makePlatformFont(const FontParams &params)
    {
        PlatformFont pf;
        pf.setFamily(params.family);
        pf.setSize(params.size);
        pf.setStyle(params.spec.style);
        pf.setWeight(params.spec.weight);
        pf.setTransform(params.spec.transform);
        return pf;
    }

    PlatformFont &getFontMod(FontParams const &params)
    {
        auto &plat = getThreadFonts();

        auto found = plat.fontMods.find(params);
        if (found != plat.fontMods.end())
        {
            return *found->second;
        }

        PlatformFont *mod = new PlatformFont(makePlatformFont(params));
        plat.fontMods.insert(params, mod);
        return *mod;
    }

    /**
     * Produces a font based on this one but with the attribute modifications applied
     * from a rich format range.
     *
     * @param rich  Rich formatting.
     *
     * @return  Font with applied formatting.
     */
    PlatformFont const &alteredFont(RichFormat::Iterator const &rich)
    {
        auto &plat = getThreadFonts();

        if (!rich.isDefault())
        {
            FontParams modParams(plat.font);

            // Size change.
            if (!fequal(rich.sizeFactor(), 1.f))
            {
                modParams.size *= rich.sizeFactor();
            }

            // Style change (including monospace).
            switch (rich.style())
            {
            case RichFormat::OriginalStyle:
                break;

            case RichFormat::Regular:
                modParams.family = plat.font.family();
                modParams.spec.style = NativeFont::Regular;
                break;

            case RichFormat::Italic:
                modParams.family = plat.font.family();
                modParams.spec.style = NativeFont::Italic;
                break;

            case RichFormat::Monospace:
                if (rich.format.format().hasStyle())
                {
                    if (Font const *altFont = rich.format.format().style().richStyleFont(rich.style()))
                    {
                        modParams = FontParams(altFont->d->getThreadFonts().font);
                    }
                }
                break;
            }

            // Weight change.
            if (rich.weight() != RichFormat::OriginalWeight)
            {
                modParams.spec.weight = (rich.weight() == RichFormat::Normal? NativeFont::Normal :
                                         rich.weight() == RichFormat::Bold?   NativeFont::Bold   :
                                                                              NativeFont::Light);
            }
            return getFontMod(modParams);
        }

        // No alterations applied.
        return plat.font;
    }
};

Font::Font()
    : d(new Impl(this))
{}

Font::Font(Font const &other)
    : d(new Impl(this, other.d->fontParams))
{}

Font::Font(const FontParams &params)
    : d(new Impl(this, params))
{}

void Font::initialize(const FontParams &params)
{
    d->fontParams = params;
    d->updateMetrics();
}

Rectanglei Font::measure(String const &textLine) const
{
    return measure(RichFormat::fromPlainText(textLine));
}

Rectanglei Font::measure(RichFormatRef const &format) const
{
    Rectanglei bounds;
    int advance = 0;

    RichFormat::Iterator iter(format);
    while (iter.hasNext())
    {
        iter.next();
        if (iter.range().isEmpty()) continue;

        PlatformFont const &altFont = d->alteredFont(iter);

        const String part = iter.range();
        Rectanglei rect = altFont.measure(part);

        // Combine to the total bounds.
        rect.moveTopLeft(Vec2i(advance, rect.top()));
        bounds |= rect;

        advance += altFont.width(part);
    }

    return bounds;
}

int Font::advanceWidth(String const &textLine) const
{
    return advanceWidth(RichFormat::fromPlainText(textLine));
}

int Font::advanceWidth(RichFormatRef const &format) const
{
    int advance = 0;
    RichFormat::Iterator iter(format);
    while (iter.hasNext())
    {
        iter.next();
        if (!iter.range().isEmpty())
        {
            advance += d->alteredFont(iter).width(iter.range());
        }
    }
    return advance;
}

Image Font::rasterize(String const &      textLine,
                      Image::Color const &foreground,
                      Image::Color const &background) const
{
    if (textLine.isEmpty()) return {};
    return rasterize(RichFormat::fromPlainText(textLine), foreground, background);
}

Image Font::rasterize(RichFormatRef const &format,
                      Image::Color const & foreground,
                      Image::Color const & background) const
{
    auto const &plat = d->getThreadFonts();

#ifdef LIBGUI_ACCURATE_TEXT_BOUNDS
    Rectanglei const bounds = measure(format);
#else
    Rectanglei const bounds(0, 0,
                            advanceWidth(textLine, format),
                            plat.font.height());
#endif

    Image::Color bgColor(background.x, background.y, background.z, background.w);

    Image::Color fg = foreground;
    Image::Color bg = background;

    Image img(Image::Size(bounds.width(), de::max(duint(plat.font.height()), bounds.height())),
               Image::RGBA_8888);
    img.fill(bgColor);

    // Composite the final image by drawing each rich range first into a separate
    // bitmap and then drawing those into the final image.
    int advance = 0;
    RichFormat::Iterator iter(format);
    while (iter.hasNext())
    {
        iter.next();
        if (iter.range().isEmpty()) continue;

        const auto *font = &plat.font;

        if (iter.isDefault())
        {
            fg = foreground;
            bg = background;
        }
        else
        {
            font = &d->alteredFont(iter);

            if (iter.colorIndex() != RichFormat::OriginalColor)
            {
                fg = iter.color();
                bg = Vec4ub(fg, 0);
            }
            else
            {
                fg = foreground;
                bg = background;
            }
        }

        const String part = iter.range();

        /*
#ifdef WIN32
        // Kludge: No light-weight fonts available, so reduce opacity to give the
        // illusion of thinness.
        if (iter.weight() == RichFormat::Light)
        {
            if (Vec3ub(60, 60, 60) > fg) // dark
                fg.w *= .66f;
            else if (Vec3ub(230, 230, 230) < fg) // light
                fg.w *= .85f;
            else
                fg.w *= .925f;
        }
#endif
         */

        Image fragment = font->rasterize(part, fg, bg);
        Rectanglei const bounds = font->measure(part);

        img.draw(fragment, Vec2i(advance + bounds.left(), d->ascent + bounds.top()));
        advance += font->width(part);
    }
    return img;
}

Rule const &Font::height() const
{
    return *d->heightRule;
}

Rule const &Font::ascent() const
{
    return *d->ascentRule;
}

Rule const &Font::descent() const
{
    return *d->descentRule;
}

Rule const &Font::lineSpacing() const
{
    return *d->lineSpacingRule;
}

bool Font::load(const Block &data) // static
{
#if defined (DE_USING_SDL_TTF)
    return SdlNativeFont::load(data);
#else
    warning("Font::load() is not implemented");
    DE_UNUSED(data);
    return false;
#endif
}

//------------------------------------------------------------------------------------------------

FontParams::FontParams()
{}

FontParams::FontParams(const NativeFont &font)
{
    family         = font.family();
    size           = font.size();
    spec.weight    = font.weight();
    spec.style     = font.style();
    spec.transform = font.transform();
}

} // namespace de
