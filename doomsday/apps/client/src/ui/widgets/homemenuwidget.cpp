/** @file homemenuwidget.cpp  Menu for the Home.
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

#include "ui/widgets/homemenuwidget.h"
#include "ui/widgets/homeitemwidget.h"
#include "ui/home/columnwidget.h"

#include <de/FocusWidget>

using namespace de;

DENG_GUI_PIMPL(HomeMenuWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
, DENG2_OBSERVES(Asset, StateChange)
{
    ui::DataPos selectedIndex = ui::Data::InvalidPos;
    ui::DataPos previousSelectedIndex = 0;
    ui::Item const *interacted = nullptr;
    ui::Item const *interactedAction = nullptr;

    Impl(Public *i) : Base(i)
    {
        self().organizer().audienceForWidgetCreation() += this;
    }

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
    {
        if (is<HomeItemWidget>(widget))
        {
            QObject::connect(&widget, SIGNAL(mouseActivity()),
                             thisPublic, SLOT(mouseActivityInItem()));

            QObject::connect(&widget, SIGNAL(selected()),
                             thisPublic, SLOT(itemSelectionChanged()));
        }
    }

    void assetStateChanged(Asset &asset)
    {
        if (asset.state() == Asset::Ready)
        {
            self().assets().audienceForStateChange() -= this; // only scroll once
            scrollToSelected();
        }
    }

    void scrollToSelected()
    {
        if (self().hasRoot())
        {
            if (auto *widget = self().itemWidget<GuiWidget>(selectedIndex))
            {
                self().findTopmostScrollable().scrollToWidget(*widget);
            }
        }
    }
};

HomeMenuWidget::HomeMenuWidget(String const &name)
    : MenuWidget(name)
    , d(new Impl(this))
{
    enableScrolling(false);
    enablePageKeys(false);
    setGridSize(1, ui::Filled, 0, ui::Expand);
    margins().setLeftRight("");
    layout().setRowPadding(rule("gap"));
}

void HomeMenuWidget::unselectAll()
{
    if (d->selectedIndex < items().size())
    {
        d->selectedIndex = ui::Data::InvalidPos;

        // Unselect all items.
        for (auto *w : childWidgets())
        {
            if (auto *item = maybeAs<HomeItemWidget>(w))
            {
                // Never deselect the currently focused item.
                if (root().focus() != item)
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

ui::DataPos HomeMenuWidget::selectedIndex() const
{
    return d->selectedIndex;
}

const ui::Item *HomeMenuWidget::selectedItem() const
{
    if (selectedIndex() < items().size())
    {
        return &items().at(selectedIndex());
    }
    return nullptr;
}

void HomeMenuWidget::setSelectedIndex(ui::DataPos index)
{
    DENG2_ASSERT(hasRoot());

    if (auto *widget = itemWidget<HomeItemWidget>(index))
    {
        if (d->selectedIndex != index)
        {
            root().setFocus(nullptr);
            unselectAll();

            widget->acquireFocus();
        }

        // Focus-based scrolling is only enabled when keyboard focus is in use.
        if (root().focusIndicator().isKeyboardFocusActive())
        {
            // Check if we can scroll to the selected widget right away.
            // If not, we are observing the asset and will scroll when it is ready.
            if (assets().isReady())
            {
                d->scrollToSelected();
            }
            else
            {
                assets().audienceForStateChange() += d;
            }
        }
    }
}

void HomeMenuWidget::setInteractedItem(ui::Item const *menuItem, ui::Item const *actionItem)
{
    d->interacted       = menuItem;
    d->interactedAction = actionItem;
}

ColumnWidget *HomeMenuWidget::parentColumn() const
{
    for (Widget *i = parentWidget(); i; i = i->parent())
    {
        if (ColumnWidget *column = maybeAs<ColumnWidget>(i))
        {
            return column;
        }
    }
    return nullptr;
}

ui::Item const *HomeMenuWidget::interactedItem() const
{
    return d->interacted;
}

ui::Item const *HomeMenuWidget::actionItem() const
{
    return d->interactedAction;
}

void HomeMenuWidget::mouseActivityInItem()
{
    if (auto *clickedWidget = dynamic_cast<HomeItemWidget *>(sender()))
    {
        emit itemClicked(findItem(*clickedWidget));
    }

    if (auto *column = parentColumn())
    {
        emit column->mouseActivity(column);
    }
}

void HomeMenuWidget::itemSelectionChanged()
{
    if (auto *clickedItem = dynamic_cast<HomeItemWidget *>(sender()))
    {
        ui::DataPos const newSelection = findItem(*clickedItem);
        if (d->selectedIndex != newSelection)
        {
            // Deselect the previous selection.
            if (auto *item = itemWidget<HomeItemWidget>(d->selectedIndex))
            {
                item->setSelected(false);
            }

            d->selectedIndex = d->previousSelectedIndex = newSelection;

            emit selectedIndexChanged(d->selectedIndex);
        }
    }
}
