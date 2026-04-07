/** @file style.h  User interface style.
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

#include "de/ui/style.h"
#include "de/baseguiapp.h"
#include "de/guirootwidget.h"
#include "de/labelwidget.h"
#include "de/ui/stylist.h"

#include <de/commandline.h>
#include <de/config.h>
#include <de/folder.h>
#include <de/package.h>
#include <de/record.h>
#include <de/recordvalue.h>
#include <de/scripting/scriptsystem.h>
#include <de/variable.h>

namespace de {

DE_PIMPL(Style)
, DE_OBSERVES(Variable, Change)
{
    struct EmptyContentLabelStylist : public ui::Stylist
    {
        void applyStyle(GuiWidget &widget) const override
        {
            if (auto *label = maybeAs<LabelWidget>(widget))
            {
                label->setFont("menu.empty");
                label->setOpacity(0.5f);
            }
        }
    }
    emptyContentLabelStylist;

    Record    module;
    RuleBank  rules;
    FontBank  fonts;
    ColorBank colors;
    ImageBank images;
    const Package *loadedPack;

    const Variable &pixelRatio     = ScriptSystem::get()["DisplayMode"]["PIXEL_RATIO"];
    const Variable *uiTranslucency = nullptr;
    Variable        defaultTranslucency;    
    
    Impl(Public *i)
        : Base(i)
        , rules(DE_BASE_GUI_APP->pixelRatio())
    {
        defaultTranslucency = new NumberValue(true);

        // The Style is available as a native module.
        App::scriptSystem().addNativeModule("Style", module);
        pixelRatio.audienceForChange() += this;
    }

    void clear()
    {
        rules.clear();
        fonts.clear();
        colors.clear();
        images.clear();

        module.clear();

        loadedPack = nullptr;
    }

    void updateFontSizeFactor()
    {
        float fontSize = 1.0f;
        if (CommandLine::ArgWithParams arg = App::commandLine().check("-fontsize", 1))
        {
            fontSize = arg.params.at(0).toFloat();
        }
        fonts.setFontSizeFactor(fontSize);
    }

    void load(const Package &pack)
    {
        loadedPack = &pack;

        updateFontSizeFactor();

        rules .addFromInfo(pack.root().locate<File>("rules.dei"));
        fonts .addFromInfo(pack.root().locate<File>("fonts.dei"));
        colors.addFromInfo(pack.root().locate<File>("colors.dei"));
        images.addFromInfo(pack.root().locate<File>("images.dei"));

        // Update the subrecords of the native module.
        module.add(new Variable("rules",  new RecordValue(rules),  Variable::AllowRecord));
        module.add(new Variable("fonts",  new RecordValue(fonts),  Variable::AllowRecord));
        module.add(new Variable("colors", new RecordValue(colors), Variable::AllowRecord));
        module.add(new Variable("images", new RecordValue(images), Variable::AllowRecord));
    }

    void variableValueChanged(Variable &, const Value &) override
    {
        if (loadedPack)
        {
            LOG_MSG("UI style being updated due to pixel ratio change");

#if 0
#if defined (WIN32)
            /*
             * KLUDGE: The operating system provides fonts scaled according to the desktop
             * scaling factor. The user's scaling factor affects the sizing of the fonts
             * on Windows via this value; on other platforms, the font definitions use
             * DisplayMode.PIXEL_RATIO directly. (Should do that on Windows, too?)
             */
            updateFontSizeFactor();
#endif
#endif
            self().performUpdate();
        }
    }

    DE_PIMPL_AUDIENCE(Change)
};

DE_AUDIENCE_METHOD(Style, Change)

Style::Style() : d(new Impl(this))
{}

Style::~Style()
{}

void Style::load(const Package &pack)
{
    d->clear();
    d->load(pack);
}

const RuleBank &Style::rules() const
{
    return d->rules;
}

const FontBank &Style::fonts() const
{
    return d->fonts;
}

const ColorBank &Style::colors() const
{
    return d->colors;
}

const ImageBank &Style::images() const
{
    return d->images;
}

RuleBank &Style::rules()
{
    return d->rules;
}

FontBank &Style::fonts()
{
    return d->fonts;
}

ColorBank &Style::colors()
{
    return d->colors;
}

ImageBank &Style::images()
{
    return d->images;
}

void Style::richStyleFormat(int contentStyle,
                            float &sizeFactor,
                            Font::RichFormat::Weight &fontWeight,
                            Font::RichFormat::Style &fontStyle,
                            int &colorIndex) const
{
    switch (contentStyle)
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
        sizeFactor = 1.f;
        fontWeight = Font::RichFormat::Normal;
        fontStyle  = Font::RichFormat::Regular;
        colorIndex = Font::RichFormat::DimmedColor;
        break;

    case Font::RichFormat::MetaStyle:
        sizeFactor = .8f;
        fontWeight = Font::RichFormat::Light;
        fontStyle  = Font::RichFormat::Regular;
        colorIndex = Font::RichFormat::AccentColor;
        break;

    case Font::RichFormat::MajorMetaStyle:
        sizeFactor = .8f;
        fontWeight = Font::RichFormat::Bold;
        fontStyle  = Font::RichFormat::Regular;
        colorIndex = Font::RichFormat::AccentColor;
        break;

    case Font::RichFormat::MinorMetaStyle:
        sizeFactor = .8f;
        fontWeight = Font::RichFormat::Light;
        fontStyle  = Font::RichFormat::Regular;
        colorIndex = Font::RichFormat::DimAccentColor;
        break;

    case Font::RichFormat::AuxMetaStyle:
        sizeFactor = .8f;
        fontWeight = Font::RichFormat::Light;
        fontStyle  = Font::RichFormat::OriginalStyle;
        colorIndex = Font::RichFormat::AltAccentColor;
        break;
    }
}

const Font *Style::richStyleFont(Font::RichFormat::Style fontStyle) const
{
    switch (fontStyle)
    {
    case Font::RichFormat::Monospace:
        return &fonts().font(DE_STR("monospace"));

    default:
        return nullptr;
    }
}

const ui::Stylist &Style::emptyContentLabelStylist() const
{
    return d->emptyContentLabelStylist;
}

bool Style::isBlurringAllowed() const
{
    if (!d->uiTranslucency)
    {
        d->uiTranslucency = Config::get().has("ui.translucency") ? &Config::get("ui.translucency")
                                                                 : &d->defaultTranslucency;
    }
    return d->uiTranslucency->value().isTrue();
}

GuiWidget *Style::sharedBlurWidget() const
{
    return nullptr;
}

void Style::performUpdate()
{
    d->fonts.reload();

    DE_NOTIFY(Change, i)
    {
        i->styleChanged(*this);
    }
}

static Style *theAppStyle = nullptr;

Style &Style::get()
{
    DE_ASSERT(theAppStyle != nullptr);
    return *theAppStyle;
}

void Style::setAppStyle(Style &newStyle)
{
    theAppStyle = &newStyle;
    /// @todo Notify everybody about the style change. -jk
}

} // namespace de
