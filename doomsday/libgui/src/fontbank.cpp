/** @file fontbank.cpp  Font bank.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/FontBank"
#include "de/Font"
#include <de/ScriptedInfo>
#include <de/Block>
#include <de/Time>

namespace de {

DENG2_PIMPL(FontBank)
{
    struct FontSource : public ISource
    {
        FontBank &bank;
        String id;

        FontSource(FontBank &b, String const &fontId) : bank(b), id(fontId) {}
        Time modifiedAt() const { return bank.sourceModifiedAt(); }

        Font *load() const
        {
            Record const &def = bank[id];

            // Font family.
            QFont font(def["family"]);

            // Size.
            String size = def["size"];
            if(size.endsWith("px"))
            {
                font.setPixelSize(size.toInt(0, 10, String::AllowSuffix));
            }
            else
            {
                font.setPointSize(size.toInt(0, 10, String::AllowSuffix));
            }

            // Weight.
            String const weight = def["weight"];
            font.setWeight(weight == "light"? QFont::Light :
                           weight == "bold"?  QFont::Bold :
                                              QFont::Normal);

            // Style.
            String const style = def["style"];
            font.setStyle(style == "italic"? QFont::StyleItalic : QFont::StyleNormal);

            return new Font(font);
        }
    };

    struct FontData : public IData
    {
        Font *font; // owned

        FontData(Font *f = 0) : font(f) {}
        ~FontData() { delete font; }
    };

    Instance(Public *i) : Base(i)
    {}
};

FontBank::FontBank() : InfoBank(DisableHotStorage), d(new Instance(this))
{}

void FontBank::addFromInfo(File const &file)
{
    LOG_AS("FontBank");
    parse(file);
    addFromInfoBlocks("font");
}

Font const &FontBank::font(Path const &path) const
{
    return *static_cast<Instance::FontData &>(data(path)).font;
}

Bank::ISource *FontBank::newSourceFromInfo(String const &id)
{
    return new Instance::FontSource(*this, id);
}

Bank::IData *FontBank::loadFromSource(ISource &source)
{
    return new Instance::FontData(static_cast<Instance::FontSource &>(source).load());
}

} // namespace de
