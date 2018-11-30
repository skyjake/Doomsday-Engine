/** @file fontbank.cpp  Font bank.
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

#include "de/FontBank"
#include "de/Font"
#include <de/ScriptedInfo>
#include <de/Block>
#include <de/Time>
#include <de/Config>

namespace de {

static const String BLOCK_FONT = "font";

DE_PIMPL(FontBank)
{
    struct FontSource : public ISource
    {
        FontBank &bank;
        String    id;

        FontSource(FontBank &b, String const &fontId)
            : bank(b)
            , id(fontId)
        {}

        Time modifiedAt() const { return bank.sourceModifiedAt(); }

        void initParams(FontParams &font) const
        {
            const Record &def = bank[id];

            // Font family.
            font.family = def["family"];

            // Size.
            String size = def["size"];
            if (size.endsWith("px"))
            {
                DE_ASSERT_FAIL("Convert font size from pixels to points");
                //font.setPixelSize(size.toInt(0, 10, String::AllowSuffix) * bank.d->fontSizeFactor);
            }
            else
            {
                font.size = size.toInt(nullptr, 10, String::AllowSuffix) * bank.d->fontSizeFactor;
            }

            // Weight.
            String const weight = def["weight"];
            font.spec.weight = (weight == "light"? NativeFont::Light :
                                weight == "bold"?  NativeFont::Bold :
                                                   NativeFont::Normal);

            // Style.
            String const style = def["style"];
            font.spec.style = (style == "italic"? NativeFont::Italic : NativeFont::Regular);

            // Transformation function.
            String const caps = def.gets("transform", "normal");
            font.spec.transform = (caps == "uppercase"? NativeFont::Uppercase :
                                   caps == "lowercase"? NativeFont::Lowercase :
                                                        NativeFont::NoTransform);
        }

        Font *load() const
        {
            FontParams params;
            initParams(params);
            return new Font(params);
        }

        void reload(Font &font)
        {           
            // Only the size can change when reloading.
            FontParams params;
            initParams(params);
            font.initialize(params);
        }
    };

    struct FontData : public IData
    {
        Font *font; // owned

        FontData(Font *f = nullptr)
            : font(f)
        {}

        ~FontData() { delete font; }
    };

    SafePtr<const File> sourceFile;
    float               fontSizeFactor;

    Impl(Public *i)
        : Base(i)
        , fontSizeFactor(1.f)
    {}
};

FontBank::FontBank()
    : InfoBank("FontBank", DisableHotStorage)
    , d(new Impl(this))
{}

void FontBank::addFromInfo(const File &file)
{
    LOG_AS("FontBank");
    d->sourceFile.reset(&file);
    parse(file);
    addFromInfoBlocks(BLOCK_FONT);
}

Font const &FontBank::font(const DotPath &path) const
{
    return *data(path).as<Impl::FontData>().font;
}

void FontBank::setFontSizeFactor(float sizeFactor)
{
    // The overall UI scalefactor affects fonts.
    d->fontSizeFactor = clamp(.1f, sizeFactor, 20.f);

#if defined (WIN32)
    /*
     * On Windows, fonts are automatically scaled by the operating system according
     * to the display scaling factor (pixel ratio). Therefore, defaultstyle.pack
     * does not scale fonts on Windows based on PIXEL_RATIO, and we need to apply
     * the user's UI scaling here.
     */
    d->fontSizeFactor *= Config::get().getf("ui.scaleFactor", 1.f);
#endif
}

void FontBank::reload()
{
    if (d->sourceFile)
    {
        objectNamespace().clear();
        parse(*d->sourceFile);
        for (const String &id : info().allBlocksOfType(BLOCK_FONT))
        {
            source(id).as<Impl::FontSource>().reload(*data(id).as<Impl::FontData>().font);
        }
    }
}

Bank::ISource *FontBank::newSourceFromInfo(String const &id)
{
    return new Impl::FontSource(*this, id);
}

Bank::IData *FontBank::loadFromSource(ISource &source)
{
    return new Impl::FontData(source.as<Impl::FontSource>().load());
}

} // namespace de
