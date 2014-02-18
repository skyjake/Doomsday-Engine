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

using namespace de;

DENG2_PIMPL(GameFilterWidget)
{
    ButtonWidget *sp;
    ButtonWidget *mp;
    ButtonWidget *all;
    LabelWidget *sortLabel;
    ChoiceWidget *sortBy;
    DialogContentStylist stylist;

    Instance(Public *i) : Base(i)
    {
        stylist.setContainer(self);

        // Create widgets.
        self.add(sp = new ButtonWidget);
        self.add(mp = new ButtonWidget);
        self.add(all = new ButtonWidget);
        sortLabel = LabelWidget::newWithText(tr("Sort By:"), &self);
        self.add(sortBy = new ChoiceWidget);

        sp->setText(tr("Singleplayer"));
        mp->setText(tr("Multiplayer"));
        all->setText(tr("All"));
        sortLabel->setTextColor("inverted.text");
        sortBy->items()
                << new ChoiceItem(tr("Title"),        SortByTitle)
                << new ChoiceItem(tr("Identity key"), SortByIdentityKey);

        SequentialLayout layout(self.rule().right(), self.rule().top(), ui::Left);
        layout << *sortBy << *sortLabel;

        AutoRef<Rule> sum(sp->rule().width() + mp->rule().width() + all->rule().width());
        SequentialLayout blay(self.rule().left() + self.rule().width() / 2 - sum / 2,
                              self.rule().top(), ui::Right);
        blay << *sp << *mp << *all;
    }
};

GameFilterWidget::GameFilterWidget(String const &name)
    : GuiWidget(name), d(new Instance(this))
{
    rule().setInput(Rule::Height, d->sp->rule().height());
}

