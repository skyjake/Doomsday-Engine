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

#include "de/font.h"

#include <de/constantrule.h>
#include <de/hash.h>
#include <de/threadlocal.h>

#include "../src/text/stbttnativefont.h"

namespace de { using PlatformFont = StbTtNativeFont; }

namespace std {
template<>
struct hash<de::FontParams> {
    std::size_t operator()(const de::FontParams &fp) const {
        return hash<de::String>()(fp.family)
             ^ hash<int>()(int(100 * fp.pointSize + 0.5f))
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

    Impl(Public *i, const FontParams &fp)
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
            if (fequal(tf.font.pointSize(), fontParams.pointSize))
            {
                return tf;
            }
            // Size has changed, re-initialize the font.
            tf.fontMods.deleteAll();
            tf.fontMods.clear();
        }
        auto &font = hash[thisPublic].font;
        font.setFamily(fontParams.family);
        font.setPointSize(fontParams.pointSize);
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
        pf.setPointSize(params.pointSize);
        pf.setStyle(params.spec.style);
        pf.setWeight(params.spec.weight);
        pf.setTransform(params.spec.transform);
        return pf;
    }

    PlatformFont &getFontMod(const FontParams &params)
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
    const PlatformFont &alteredFont(const RichFormat::Iterator &rich)
    {
        auto &plat = getThreadFonts();

        if (!rich.isDefault())
        {
            FontParams modParams(plat.font);

            // Size change.
            if (!fequal(rich.sizeFactor(), 1.f))
            {
                modParams.pointSize *= rich.sizeFactor();
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
                    if (const Font *altFont = rich.format.format().style().richStyleFont(rich.style()))
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

Font::Font(const Font &other)
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

Rectanglei Font::measure(const String &textLine) const
{
    return measure(RichFormat::fromPlainText(textLine));
}

Rectanglei Font::measure(const RichFormatRef &format) const
{
    Rectanglei bounds;
    int advance = 0;

    RichFormat::Iterator iter(format);
    while (iter.hasNext())
    {
        iter.next();
        if (iter.range().isEmpty()) continue;

        const PlatformFont &altFont = d->alteredFont(iter);

        const String part = iter.range();
        Rectanglei rect = altFont.measure(part);

        // Combine to the total bounds.
        rect.move(Vec2i(advance, 0));
        bounds |= rect;

        advance += altFont.advanceWidth(part);
    }

    return bounds;
}

int Font::advanceWidth(const String &textLine) const
{
    return advanceWidth(RichFormat::fromPlainText(textLine));
}

int Font::advanceWidth(const RichFormatRef &format) const
{
    int advance = 0;
    RichFormat::Iterator iter(format);
    while (iter.hasNext())
    {
        iter.next();
        if (!iter.range().isEmpty())
        {
            advance += d->alteredFont(iter).advanceWidth(iter.range());
        }
    }
    return advance;
}

Image Font::rasterize(const String &      textLine,
                      const Image::Color &foreground,
                      const Image::Color &background) const
{
    if (textLine.isEmpty()) return {};
    return rasterize(RichFormat::fromPlainText(textLine), foreground, background);
}

Image Font::rasterize(const RichFormatRef &format,
                      const Image::Color & foreground,
                      const Image::Color & background) const
{
    const auto &plat = d->getThreadFonts();

    const Rectanglei bounds = measure(format);

    Image::Color bgColor(background.x, background.y, background.z, background.w);

    Image::Color fg = foreground;
    Image::Color bg = background;

    const Vec2i imgOrigin = -bounds.topLeft;
    Image img(bounds.size(), Image::RGBA_8888);
    img.fill(bgColor);
    img.setOrigin(bounds.topLeft);

    // Composite the final image by drawing each rich range first into a separate
    // bitmap and then drawing those into the final image.
    int advance = 0;
    int ascent = 0;
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
        const Image raster = font->rasterize(part, fg, bg);
        ascent = de::max(ascent, font->ascent());
        img.draw(raster, imgOrigin + raster.origin() + Vec2i(advance, 0));
        advance += font->advanceWidth(part);
    }
    img.setOrigin(img.origin() + Vec2i(0, ascent));
    return img;
}

const Rule &Font::height() const
{
    return *d->heightRule;
}

const Rule &Font::ascent() const
{
    return *d->ascentRule;
}

const Rule &Font::descent() const
{
    return *d->descentRule;
}

const Rule &Font::lineSpacing() const
{
    return *d->lineSpacingRule;
}

bool Font::load(const String &name, const Block &data) // static
{
    return PlatformFont::load(name, data);
}

//------------------------------------------------------------------------------------------------

FontParams::FontParams()
{}

FontParams::FontParams(const NativeFont &font)
{
    family         = font.family();
    pointSize      = font.pointSize();
    spec.weight    = font.weight();
    spec.style     = font.style();
    spec.transform = font.transform();
}

} // namespace de
