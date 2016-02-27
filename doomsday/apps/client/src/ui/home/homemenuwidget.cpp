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
    int selectedIndex = -1;

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
    layout().setRowPadding(rule("gap"));
}

void HomeMenuWidget::unselectAll()
{
    if(d->selectedIndex >= 0)
    {
        d->selectedIndex = -1;

        // Unselect all items.
        for(auto *w : childWidgets())
        {
            if(auto *item = w->maybeAs<HomeItemWidget>())
            {
                item->setSelected(false);
            }
        }
    }
}

int HomeMenuWidget::selectedIndex() const
{
    return d->selectedIndex;
}

void HomeMenuWidget::setSelectedIndex(int index)
{
    if(index >= 0 && index < childWidgets().size())
    {
        unselectAll();
        d->selectedIndex = index;

        HomeItemWidget &widget = childWidgets().at(index)->as<HomeItemWidget>();
        widget.setSelected(true);
    }
}

void HomeMenuWidget::mouseActivityInItem()
{
    auto *clickedItem = dynamic_cast<HomeItemWidget *>(sender());

    // Radio button behavior: other items will be deselected.
    for(int i = 0; i < childWidgets().size(); ++i)
    {
        if(auto *item = childWidgets().at(i)->maybeAs<HomeItemWidget>())
        {
            item->setSelected(item == clickedItem);

            if(item == clickedItem)
            {
                d->selectedIndex = i;
            }
        }
    }
}
