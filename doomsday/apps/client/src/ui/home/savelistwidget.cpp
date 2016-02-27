/** @file savelistwidget.cpp  List showing the available saves of a game.
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

#include "ui/home/savelistwidget.h"
#include "ui/home/gamepanelbuttonwidget.h"
#include "ui/savedsessionlistdata.h"

#include <doomsday/game.h>

#include <de/CallbackAction>

using namespace de;

DENG_GUI_PIMPL(SaveListWidget)
, DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
{
    /**
     * Handles mouse button doubleclicks on the save items.
     */
    struct DoubleClickHandler : public GuiWidget::IEventHandler
    {
        SaveListWidget::Instance *d;

        DoubleClickHandler(SaveListWidget::Instance *d) : d(d) {}

        bool handleEvent(GuiWidget &button, Event const &event)
        {
            if(event.type() == Event::MouseButton)
            {
                MouseEvent const &mouse = event.as<MouseEvent>();
                if(button.hitTest(mouse) && mouse.state() == MouseEvent::DoubleClick)
                {
                    emit d->self.doubleClicked(d->self.items().find(
                            *d->self.organizer().findItemForWidget(button)));
                    return true;
                }
            }
            return false;
        }
    };

    GamePanelButtonWidget &owner;
    ui::DataPos selected = ui::Data::InvalidPos;

    Instance(Public *i, GamePanelButtonWidget &owner) : Base(i), owner(owner)
    {
        self.organizer().audienceForWidgetUpdate() += this;
    }

    void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        auto &button = widget.as<ButtonWidget>();
        button.setTextAlignment(ui::AlignRight);
        button.setAlignment(ui::AlignLeft);
        button.setTextLineAlignment(ui::AlignLeft);
        button.setSizePolicy(ui::Filled, ui::Expand);
        button.setText(item.label());
        button.margins().set("dialog.gap");
        button.set(Background(Vector4f()));

        button.setActionFn([this, &button] () {
            toggleSelectedItem(button);
            emit owner.mouseActivity();
        });
        button.addEventHandler(new DoubleClickHandler(this));

        auto const &saveItem = item.as<SavedSessionListData::SaveItem>();
        button.setImage(style().images().image(Game::logoImageForId(saveItem.gameId())));
    }

    void toggleSelectedItem(ButtonWidget &button)
    {
        auto const buttonItemPos = self.items().find(*self.organizer().findItemForWidget(button));

        if(selected == buttonItemPos)
        {
            // Unselect.
            selected = ui::Data::InvalidPos;
            updateItemHighlights(nullptr);
        }
        else
        {
            selected = buttonItemPos;
            updateItemHighlights(&button);
        }

        emit self.selectionChanged(selected);
    }

    void updateItemHighlights(ButtonWidget *selectedButton)
    {
        for(auto *w : self.childWidgets())
        {
            if(auto *bw = w->maybeAs<ButtonWidget>())
            {
                if(selectedButton == bw)
                {
                    bw->useInfoStyle();
                    bw->set(Background(style().colors().colorf("inverted.background")));
                }
                else
                {
                    bw->useNormalStyle();
                    bw->set(Background());
                }
            }
        }
    }
};

SaveListWidget::SaveListWidget(GamePanelButtonWidget &owner)
    : d(new Instance(this, owner))
{
    setGridSize(1, ui::Filled, 0, ui::Expand);
    enableScrolling(false);
    enablePageKeys(false);
}

ui::DataPos SaveListWidget::selectedPos() const
{
    return d->selected;
}

void SaveListWidget::setSelectedPos(ui::DataPos pos)
{
    if(d->selected != pos)
    {
        d->selected = pos;
        d->updateItemHighlights(&itemWidget<ButtonWidget>(items().at(pos)));
        emit selectionChanged(d->selected);
    }
}

void SaveListWidget::clearSelection()
{
    if(d->selected != ui::Data::InvalidPos)
    {
        d->selected = ui::Data::InvalidPos;
        d->updateItemHighlights(nullptr);
        emit selectionChanged(d->selected);
    }
}
