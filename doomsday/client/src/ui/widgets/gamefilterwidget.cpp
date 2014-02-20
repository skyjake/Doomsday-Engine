/** @file gamefilterwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/gamefilterwidget.h"

#include <de/ButtonWidget>
#include <de/ChoiceWidget>
#include <de/SequentialLayout>
#include <de/DialogContentStylist>
#include <de/TabWidget>

using namespace de;

DENG2_PIMPL(GameFilterWidget)
{
    TabWidget *tabs;
    LabelWidget *sortLabel;
    ChoiceWidget *sortBy;
    DialogContentStylist stylist;

    Instance(Public *i) : Base(i)
    {
        stylist.setContainer(self);

        // Create widgets.
        self.add(tabs = new TabWidget);
        sortLabel = LabelWidget::newWithText(tr("Sort By:"), &self);
        self.add(sortBy = new ChoiceWidget);

        tabs->items()
                << new TabItem(tr("Singleplayer"), Singleplayer)
                << new TabItem(tr("Multiplayer"),  Multiplayer)
                << new TabItem(tr("All Games"),    AllGames);

        sortLabel->setFont("small");
        sortBy->setFont("tab.label");
        sortBy->setOpeningDirection(ui::Down);
        sortBy->items()
                << new ChoiceItem(tr("Title"),        SortByTitle)
                << new ChoiceItem(tr("Identity key"), SortByIdentityKey);

        SequentialLayout layout(self.rule().right(), self.rule().top(), ui::Left);
        layout << *sortBy << *sortLabel;

        tabs->rule()
                .setInput(Rule::Width, self.rule().width())
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Top,   self.rule().top());
    }
};

GameFilterWidget::GameFilterWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    connect(d->tabs, SIGNAL(currentTabChanged()), this, SIGNAL(filterChanged()));
    connect(d->sortBy, SIGNAL(selectionChangedByUser(uint)), this, SIGNAL(sortOrderChanged()));

    rule().setInput(Rule::Height, d->tabs->rule().height());
}

void GameFilterWidget::useInvertedStyle()
{
    d->sortLabel->setTextColor("inverted.text");
}

GameFilterWidget::Filter GameFilterWidget::filter() const
{
    return Filter(d->tabs->currentItem().data().toUInt());
}

GameFilterWidget::SortOrder GameFilterWidget::sortOrder() const
{
    return SortOrder(d->sortBy->selectedItem().data().toInt());
}

