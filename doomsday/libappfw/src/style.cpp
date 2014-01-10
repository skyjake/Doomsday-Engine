/** @file style.h  User interface style.
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

#include "ui/style.h"

#include <de/App>
#include <de/ScriptSystem>
#include <de/Record>
#include <de/Variable>
#include <de/RecordValue>

using namespace de;

DENG2_PIMPL(Style)
{
    Record module;
    RuleBank rules;
    FontBank fonts;
    ColorBank colors;
    ImageBank images;

    Instance(Public *i) : Base(i)
    {
        // The Style is available as a native module.
        App::scriptSystem().addNativeModule("Style", module);
    }

    void clear()
    {
        rules.clear();
        fonts.clear();
        colors.clear();
        images.clear();

        module.clear();
    }

    void load(String const &path)
    {
        Folder const &pack = App::rootFolder().locate<Folder>(path);

        if(CommandLine::ArgWithParams arg = App::commandLine().check("-fontsize", 1))
        {
            fonts.setFontSizeFactor(arg.params.at(0).toFloat());
        }

        rules.addFromInfo(pack.locate<File>("rules.dei"));
        fonts.addFromInfo(pack.locate<File>("fonts.dei"));
        colors.addFromInfo(pack.locate<File>("colors.dei"));
        images.addFromInfo(pack.locate<File>("images.dei"));

        // Update the subrecords of the native module.
        module.add(new Variable("rules",  new RecordValue(rules.names()),  Variable::AllowRecord));
        module.add(new Variable("fonts",  new RecordValue(fonts.names()),  Variable::AllowRecord));
        module.add(new Variable("colors", new RecordValue(colors.names()), Variable::AllowRecord));
        module.add(new Variable("images", new RecordValue(images.names()), Variable::AllowRecord));
    }
};

Style::Style() : d(new Instance(this))
{}

void Style::load(String const &pack)
{
    d->clear();
    d->load(pack);
}

RuleBank const &Style::rules() const
{
    return d->rules;
}

FontBank const &Style::fonts() const
{
    return d->fonts;
}

ColorBank const &Style::colors() const
{
    return d->colors;
}

ImageBank const &Style::images() const
{
    return d->images;
}

void Style::richStyleFormat(int contentStyle,
                            float &sizeFactor,
                            Font::RichFormat::Weight &fontWeight,
                            Font::RichFormat::Style &fontStyle,
                            int &colorIndex) const
{
    switch(contentStyle)
    {
    default:
    case Font::RichFormat::NormalStyle:
        sizeFactor = 1.f;
        fontWeight = Font::RichFormat::OriginalWeight;
        fontStyle  = Font::RichFormat::OriginalStyle;
        colorIndex = Font::RichFormat::OriginalColor;
        break;

    case Font::RichFormat::MajorStyle:
        sizeFactor = 1.f;
        fontWeight = Font::RichFormat::Bold;
        fontStyle  = Font::RichFormat::Regular;
        colorIndex = Font::RichFormat::HighlightColor;
        break;

    case Font::RichFormat::MinorStyle:
        sizeFactor = .8f;
        fontWeight = Font::RichFormat::Normal;
        fontStyle  = Font::RichFormat::Regular;
        colorIndex = Font::RichFormat::DimmedColor;
        break;

    case Font::RichFormat::MetaStyle:
        sizeFactor = .9f;
        fontWeight = Font::RichFormat::Light;
        fontStyle  = Font::RichFormat::Italic;
        colorIndex = Font::RichFormat::AccentColor;
        break;

    case Font::RichFormat::MajorMetaStyle:
        sizeFactor = .9f;
        fontWeight = Font::RichFormat::Bold;
        fontStyle  = Font::RichFormat::Italic;
        colorIndex = Font::RichFormat::AccentColor;
        break;

    case Font::RichFormat::MinorMetaStyle:
        sizeFactor = .8f;
        fontWeight = Font::RichFormat::Light;
        fontStyle  = Font::RichFormat::Italic;
        colorIndex = Font::RichFormat::DimAccentColor;
        break;

    case Font::RichFormat::AuxMetaStyle:
        sizeFactor = .8f;
        fontWeight = Font::RichFormat::Light;
        fontStyle  = Font::RichFormat::OriginalStyle;
        colorIndex = Font::RichFormat::DimmedColor;
        break;
    }
}

Font const *Style::richStyleFont(Font::RichFormat::Style fontStyle) const
{
    switch(fontStyle)
    {
    case Font::RichFormat::Monospace:
        return &fonts().font("monospace");

    default:
        return 0;
    }
}

static Style *appStyle = 0;

Style &Style::appStyle()
{
    DENG2_ASSERT(appStyle != 0);
    return *appStyle;
}

void Style::setAppStyle(Style &newStyle)
{
    appStyle = &newStyle;
    /// @todo Notify everybody about the style change. -jk
}
