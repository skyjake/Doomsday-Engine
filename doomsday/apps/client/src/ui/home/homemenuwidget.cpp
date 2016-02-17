/** @file homemenuwidget.cpp  Menu for the Home.
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

#include "ui/home/homemenuwidget.h"
#include "ui/home/panelbuttonwidget.h"

using namespace de;

DENG_GUI_PIMPL(HomeMenuWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
{
    Instance(Public *i) : Base(i)
    {
        self.organizer().audienceForWidgetCreation() += this;
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
    {
        QObject::connect(&widget, SIGNAL(mouseActivity()),
                         thisPublic, SLOT(mouseActivityInItem()));
    }
};

HomeMenuWidget::HomeMenuWidget(String const &name)
    : MenuWidget(name)
    , d(new Instance(this))
{
    enableScrolling(false);
    enablePageKeys(false);
    setGridSize(1, ui::Filled, 0, ui::Expand);
    margins().setLeftRight("");
    layout().setRowPadding(style().rules().rule("gap"));
}

void HomeMenuWidget::unselectAll()
{
    // Unselect all items.
    for(auto *w : childWidgets())
    {
        if(auto *item = w->maybeAs<HomeItemWidget>())
        {
            item->setSelected(false);
        }
    }
}

void HomeMenuWidget::mouseActivityInItem()
{
    auto *clickedItem = dynamic_cast<HomeItemWidget *>(sender());

    // Radio button behavior: other items will be deselected.
    for(auto *w : childWidgets())
    {
        if(auto *item = w->maybeAs<HomeItemWidget>())
        {
            item->setSelected(item == clickedItem);
        }
    }
}
