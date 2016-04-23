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

#include "ui/widgets/homemenuwidget.h"
#include "ui/widgets/homeitemwidget.h"

using namespace de;

DENG_GUI_PIMPL(HomeMenuWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(Asset, StateChange)
{
    int selectedIndex = -1;
    int previousSelectedIndex = 0;

    Instance(Public *i) : Base(i)
    {
        self.organizer().audienceForWidgetCreation() += this;
    }

    ~Instance()
    {
        self.assets().audienceForStateChange() -= this;
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
    {
        if(widget.is<HomeItemWidget>())
        {
            QObject::connect(&widget, SIGNAL(mouseActivity()),
                             thisPublic, SLOT(mouseActivityInItem()));

            QObject::connect(&widget, SIGNAL(selected()), thisPublic, SLOT(itemSelectionChanged()));
            //QObject::connect(&widget, SIGNAL(deselected()), thisPublic, SLOT(itemSelectionChanged()));
        }
    }

    void assetStateChanged(Asset &asset)
    {
        if(asset.state() == Asset::Ready)
        {
            self.assets().audienceForStateChange() -= this; // only scroll once
            scrollToSelected();
        }
    }

    void scrollToSelected()
    {
        if(selectedIndex >= 0 && self.hasRoot())
        {
            self.findTopmostScrollable().scrollToWidget(
                        self.childWidgets().at(selectedIndex)->as<GuiWidget>());
        }
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
                // Never deselect the currently focused item.
                if(root().focus() != item)
                {
                    item->setSelected(false);
                }
            }
        }
    }
}

void HomeMenuWidget::restorePreviousSelection()
{
    setSelectedIndex(d->previousSelectedIndex);
}

int HomeMenuWidget::selectedIndex() const
{
    return d->selectedIndex;
}

void HomeMenuWidget::setSelectedIndex(int index)
{
    if(index >= 0 && index < childWidgets().size())
    {
        if(HomeItemWidget *widget = childWidgets().at(index)->maybeAs<HomeItemWidget>())
        {
            widget->acquireFocus();

            // Check if we can scroll to the selected widget right away.
            // If not, we are observing the asset and will scroll when it is ready.
            if(assets().isReady())
            {
                qDebug() << "immediate scroll";
                d->scrollToSelected();
            }
            else
            {
                qDebug() << "deferred scroll";
                assets().audienceForStateChange() += d;
            }
        }
    }
}

void HomeMenuWidget::mouseActivityInItem()
{
    auto *clickedItem = dynamic_cast<HomeItemWidget *>(sender());

    emit itemClicked(childWidgets().indexOf(clickedItem));
/*
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
    }*/
}

void HomeMenuWidget::itemSelectionChanged()
{
    if(auto *clickedItem = dynamic_cast<HomeItemWidget *>(sender()))
    {
        int const newSelection = childWidgets().indexOf(clickedItem);
        if(d->selectedIndex != newSelection)
        {
            if(d->selectedIndex >= 0)
            {
                auto children = childWidgets();
                if(d->selectedIndex < children.size())
                {
                    // Deselect the previous selection.
                    if(auto *item = children.at(d->selectedIndex)->maybeAs<HomeItemWidget>())
                    {
                        item->setSelected(false);
                    }
                }
            }
            d->selectedIndex = d->previousSelectedIndex = newSelection;

            emit selectedIndexChanged(d->selectedIndex);
        }
    }
}
