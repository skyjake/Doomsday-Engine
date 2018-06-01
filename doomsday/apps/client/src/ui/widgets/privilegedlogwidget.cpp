/** @file privilegedlogwidget.cpp
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/privilegedlogwidget.h"
#include "ui/styledlogsinkformatter.h"
#include <de/LogWidget>
#include <de/ButtonWidget>

using namespace de;

DE_GUI_PIMPL(PrivilegedLogWidget)
{
    StyledLogSinkFormatter formatter;
    LogWidget *log = nullptr;
    ButtonWidget *closeButton = nullptr;

    Impl(Public *i) : Base(i)
    {
        log = new LogWidget("privlog");
        log->setLogFormatter(formatter);
        log->setPrivilegedEntries(true);
        self().add(log);

        closeButton = new ButtonWidget;
        closeButton->setSizePolicy(ui::Expand, ui::Expand);
        self().add(closeButton);

        updateStyle();
    }

    void updateStyle()
    {
        closeButton->setImageScale(.25f);
        closeButton->setImage(style().images().image("close.ringless"));
        closeButton->setImageColor(style().colors().colorf("altaccent"));
    }
};

PrivilegedLogWidget::PrivilegedLogWidget()
    : d(new Impl(this))
{
    auto const &gap = rule("gap");

    hide();
    connect(d->log, SIGNAL(contentHeightIncreased(int)), this, SLOT(showLog()));
    connect(d->closeButton, SIGNAL(pressed()), this, SLOT(hideLog()));

    d->log->rule()
            .setInput(Rule::Left, rule().left() + gap)
            .setInput(Rule::Top,  rule().top() + gap)
            .setInput(Rule::Right, (rule().left() + rule().midX()) / 2)
            .setInput(Rule::Bottom, rule().midY() - gap);

    d->closeButton->rule()
            .setInput(Rule::Top, d->log->rule().top())
            .setInput(Rule::Right, d->log->rule().right());
}

void PrivilegedLogWidget::showLog()
{
    show();
}

void PrivilegedLogWidget::hideLog()
{
    hide();
    d->log->clear();
}

void PrivilegedLogWidget::updateStyle()
{
    GuiWidget::updateStyle();
    d->updateStyle();
}
