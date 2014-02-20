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
    //ButtonWidget *sp;
    //ButtonWidget *mp;
    //ButtonWidget *all;
    LabelWidget *sortLabel;
    ChoiceWidget *sortBy;
    DialogContentStylist stylist;

    Instance(Public *i) : Base(i)
    {
        stylist.setContainer(self);

        // Create widgets.
        //self.add(sp = new ButtonWidget);
        //self.add(mp = new ButtonWidget);
        //self.add(all = new ButtonWidget);
        self.add(tabs = new TabWidget);
        sortLabel = LabelWidget::newWithText(tr("Sort By:"), &self);
        self.add(sortBy = new ChoiceWidget);

        tabs->items()
                << new TabItem(tr("Singleplayer"))
                << new TabItem(tr("Multiplayer"))
                << new TabItem(tr("All Games"));

        sortLabel->setTextColor("inverted.text");
        sortLabel->setFont("small");
        sortBy->setFont("small");
        sortBy->setOpeningDirection(ui::Down);
        sortBy->items()
                << new ChoiceItem(tr("Title"),        SortByTitle)
                << new ChoiceItem(tr("Identity key"), SortByIdentityKey);

        SequentialLayout layout(self.rule().right(), self.rule().top(), ui::Left);
        layout << *sortBy << *sortLabel;

        //AutoRef<Rule> sum(sp->rule().width() + mp->rule().width() + all->rule().width());
        //SequentialLayout blay(self.rule().left() + self.rule().width() / 2 - sum / 2,
//                              self.rule().top(), ui::Right);
  //      blay << *sp << *mp << *all;

        tabs->rule()
                .setInput(Rule::Width, self.rule().width())
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Top,   self.rule().top());
    }
};

GameFilterWidget::GameFilterWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    rule().setInput(Rule::Height, d->tabs->rule().height());
}

