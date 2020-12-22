/** @file headerwidget.cpp  Home column header.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/home/headerwidget.h"

#include <de/config.h>
#include <de/labelwidget.h>
#include <de/buttonwidget.h>
#include <de/panelwidget.h>
#include <de/callbackaction.h>

using namespace de;

DE_GUI_PIMPL(HeaderWidget)
, DE_OBSERVES(Variable, Change)
{
    LabelWidget *logo;
    LabelWidget *logoBg; /// @todo Backgrounds should support ProceduralImages. -jk
    LabelWidget *title;
    PanelWidget *infoPanel;
    LabelWidget *info;
    PopupButtonWidget *menuButton;

    Impl(Public *i) : Base(i)
    {
        self().add(logoBg    = new LabelWidget);
        self().add(logo      = new LabelWidget);
        self().add(title     = new LabelWidget);
        self().add(infoPanel = new PanelWidget);

        info  = new LabelWidget;
        infoPanel->setContent(info);
        if (showDescriptionVar().value().isTrue())
        {
            infoPanel->open();
        }
        self().add(menuButton = new PopupButtonWidget);

        showDescriptionVar().audienceForChange() += this;
    }

    void variableValueChanged(Variable &, const Value &newValue)
    {
        if (newValue.isTrue())
        {
            infoPanel->open();
        }
        else
        {
            infoPanel->close();
        }
    }

    static const Variable &showDescriptionVar()
    {
        return Config::get("home.showColumnDescription");
    }
};

HeaderWidget::HeaderWidget()
    : d(new Impl(this))
{
    const Rule &logoHeight = rule("home.header.logo.height");

    margins().setTop("gap");

    d->title->setAlignment(ui::AlignLeft);
    d->title->setTextLineAlignment(ui::AlignLeft);
    d->title->setFont("title");
    d->title->setSizePolicy(ui::Fixed, ui::Expand);
    d->title->margins()
            .setLeft("")
            .setBottom(style().fonts().font("title").descent());

    d->logo->setSizePolicy(ui::Filled, ui::Filled);
    d->logo->setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
    //d->logo->set(Background(Vec4f(0, 0, 0, 1)));

    d->logoBg->setSizePolicy(ui::Filled, ui::Filled);
    d->logoBg->setImageFit(ui::FitToSize);
    d->logoBg->margins().setZero();

    //d->info->setFont("small");
    d->info->setTextColor("label.dimmed");
    d->info->setAlignment(ui::AlignLeft);
    d->info->setTextLineAlignment(ui::AlignLeft);
    d->info->setMaximumTextWidth(rule().width());
    d->info->setSizePolicy(ui::Fixed, ui::Expand);
    d->info->margins().setZero().setTop("gap");

    d->menuButton->setSizePolicy(ui::Expand, ui::Expand);
    d->menuButton->setFont("small");
    d->menuButton->setText("...");
    d->menuButton->margins().setTopBottom(RuleBank::UNIT);
    /*d->menuButton->setAction(new CallbackAction([this] ()
    {
        if (d->infoPanel->isOpen())
        {
            d->infoPanel->close(0);
        }
        else
        {
            d->infoPanel->open();
        }
    }));*/

    d->logoBg->rule().setRect(d->logo->rule());
    d->logo->rule()
            .setInput(Rule::Height, logoHeight)
            .setInput(Rule::Width,  Const(0))
            .setInput(Rule::Top,    rule().top() + margins().top())
            .setInput(Rule::Left,   rule().left());
    d->menuButton->rule()
            .setInput(Rule::Bottom, d->logo->rule().bottom())
            .setInput(Rule::Left,   d->logo->rule().right() +
                                    d->title->margins().left());
    d->title->rule()
            //.setInput(Rule::Height, logoHeight)
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, d->menuButton->rule().top())
            .setInput(Rule::Left,   d->logo->rule().right());
    d->infoPanel->rule()
            .setInput(Rule::Top,    d->logo->rule().bottom())
            .setInput(Rule::Left,   rule().left());
    d->info->rule()
            .setInput(Rule::Width,  rule().width());

    rule().setInput(Rule::Height, logoHeight + d->infoPanel->rule().height() +
                    margins().height());
}

LabelWidget &HeaderWidget::logo()
{
    return *d->logo;
}

LabelWidget &HeaderWidget::title()
{
    return *d->title;
}

LabelWidget &HeaderWidget::info()
{
    return *d->info;
}

PanelWidget &HeaderWidget::infoPanel()
{
    return *d->infoPanel;
}

PopupButtonWidget &HeaderWidget::menuButton()
{
    return *d->menuButton;
}

void HeaderWidget::setLogoImage(const DotPath &imageId)
{
    d->logo->setStyleImage(imageId);
    d->logo->rule().setInput(Rule::Width, rule("home.header.logo.width"));
    d->title->margins().setLeft("gap");
}

void HeaderWidget::setLogoBackground(const DotPath &imageId)
{
    d->logoBg->setStyleImage(imageId);
}
