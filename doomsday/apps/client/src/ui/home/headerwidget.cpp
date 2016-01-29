/** @file headerwidget.cpp  Home column header.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/LabelWidget>
#include <de/ButtonWidget>
#include <de/PanelWidget>
#include <de/CallbackAction>

using namespace de;

DENG_GUI_PIMPL(HeaderWidget)
{
    LabelWidget *logo;
    LabelWidget *title;
    PanelWidget *infoPanel;
    LabelWidget *info;
    ButtonWidget *menuButton;

    Instance(Public *i) : Base(i)
    {
        self.add(logo  = new LabelWidget);
        self.add(title = new LabelWidget);
        self.add(infoPanel = new PanelWidget);
        info  = new LabelWidget;
        infoPanel->setContent(info);
        self.add(menuButton = new ButtonWidget);
    }
};

HeaderWidget::HeaderWidget()
    : d(new Instance(this))
{
    AutoRef<Rule> logoHeight(new ConstantRule(2 * 120));

    margins().setTop("gap");

    d->title->setAlignment(ui::AlignLeft);
    d->title->setTextLineAlignment(ui::AlignLeft);
    d->title->setFont("title");
    d->title->setSizePolicy(ui::Fixed, ui::Fixed);
    d->title->margins().setLeft("");

    d->logo->setSizePolicy(ui::Filled, ui::Filled);
    d->logo->setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
    d->logo->set(Background(Vector4f(0, 0, 0, 1)));

    d->info->setFont("small");
    d->info->setAlignment(ui::AlignLeft);
    d->info->setTextLineAlignment(ui::AlignLeft);
    d->info->setMaximumTextWidth(rule().width());
    d->info->setSizePolicy(ui::Fixed, ui::Expand);
    d->info->margins().setLeft("").setRight("");

    d->menuButton->setSizePolicy(ui::Expand, ui::Expand);
    d->menuButton->setFont("small");
    d->menuButton->setText("...");
    d->menuButton->margins().setTopBottom("unit");
    d->menuButton->setAction(new CallbackAction([this] ()
    {
        if(d->infoPanel->isOpen())
        {
            d->infoPanel->close(0);
        }
        else
        {
            d->infoPanel->open();
        }
    }));

    d->logo->rule()
            .setInput(Rule::Height, logoHeight)
            .setInput(Rule::Width,  Const(0))
            .setInput(Rule::Top,    rule().top() + margins().top())
            .setInput(Rule::Left,   rule().left());
    d->title->rule()
            .setInput(Rule::Height, logoHeight)
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Top,    d->logo->rule().top())
            .setInput(Rule::Left,   d->logo->rule().right());
    d->menuButton->rule()
            .setInput(Rule::Bottom, d->title->rule().bottom())
            .setInput(Rule::Left,   d->title->rule().left() +
                                    d->title->margins().left());
    d->infoPanel->rule()
            .setInput(Rule::Top,    d->title->rule().bottom())
            .setInput(Rule::Left,   rule().left());
    d->info->rule()
            .setInput(Rule::Width,  rule().width());

    rule().setInput(Rule::Height, logoHeight + d->infoPanel->rule().height());
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

void HeaderWidget::setLogoImage(const DotPath &imageId)
{
    d->logo->setImage(style().images().image(imageId));
    d->logo->rule().setInput(Rule::Width, Const(2 * 160));
    d->title->margins().setLeft("gap");
}
