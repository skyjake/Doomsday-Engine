/** @file popupmenuwidget.cpp
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/popupmenuwidget.h"
#include "ui/widgets/menuwidget.h"
#include "GuiRootWidget"
#include "ContextWidgetOrganizer"
#include "ui/Item"
#include "clientapp.h"

using namespace de;

DENG2_PIMPL(PopupMenuWidget),
DENG2_OBSERVES(ButtonWidget, StateChange),
DENG2_OBSERVES(ButtonWidget, Triggered),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetCreation),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetUpdate)
{
    ButtonWidget *hover;
    int oldScrollY;

    Instance(Public *i) : Base(i), hover(0), oldScrollY(0) {}

    void widgetCreatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        // Popup menu items' background is provided by the popup.
        widget.set(Background());

        if(item.semantics().testFlag(ui::Item::Separator))
        {
            LabelWidget &lab = widget.as<LabelWidget>();
            lab.setTextColor("label.accent");
            return;
        }

        // Customize buttons for use in the popup. We will observe the button
        // state for highlighting and possibly close the popup when an action
        // gets triggered.
        if(ButtonWidget *b = widget.maybeAs<ButtonWidget>())
        {
            b->setSizePolicy(ui::Expand, ui::Expand);
            b->margins().set("unit");

            b->audienceForStateChange += this;

            // Triggered actions close the menu.
            if(item.semantics().testFlag(ui::Item::ActivationClosesPopup))
            {
                b->audienceForTriggered += this;
            }
        }
    }

    void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
    {
        if(item.semantics().testFlag(ui::Item::Separator))
        {
            // The label of a separator may change.
            if(item.label().isEmpty())
            {
                widget.margins().set("");
                widget.setFont("separator.empty");
            }
            else
            {
                widget.margins().set("halfunit");
                widget.setFont("separator.label");
            }
        }
    }

    void updateItemHitRules()
    {
        GridLayout const &layout = self.menu().layout();

        foreach(Widget *child, self.menu().childWidgets())
        {
            GuiWidget &widget = child->as<GuiWidget>();

            if(self.menu().isWidgetPartOfMenu(widget))
            {
                Vector2i cell = layout.widgetPos(widget);
                DENG2_ASSERT(cell.x >= 0 && cell.y >= 0);

                // We want items to be hittable throughout the width of the menu,
                // however restrict this to the item's column if there are multiple.
                widget.hitRule()
                        .setInput(Rule::Left,  (!cell.x? self.rule().left() :
                                                         layout.columnLeft(cell.x)))
                        .setInput(Rule::Right, (cell.x == layout.gridSize().x - 1? self.rule().right() :
                                                                                   layout.columnRight(cell.x)));
            }
        }
    }

    void buttonStateChanged(ButtonWidget &button, ButtonWidget::State state)
    {
        // Update button style.
        if(state == ButtonWidget::Up)
        {
            button.setTextColor("text");
        }
        else
        {
            button.setTextColor("inverted.text");
        }

        // Position item highlight.
        if(&button == hover && state == ButtonWidget::Up)
        {
            hover = 0;
            self.requestGeometry();
            return;
        }

        if(state == ButtonWidget::Hover || state == ButtonWidget::Down)
        {
            hover = &button;
            self.requestGeometry();
        }
    }

    Rectanglei highlightRect() const
    {
        Rectanglei hi;
        if(hover)
        {
            hi.topLeft.x     = hover->hitRule().left().valuei();
            hi.topLeft.y     = hover->hitRule().top().valuei();
            hi.bottomRight.x = hover->hitRule().right().valuei();
            hi.bottomRight.y = hover->hitRule().bottom().valuei();
        }
        // Clip the highlight to the main popup area.
        return hi & self.rule().recti();
    }

    void buttonActionTriggered(ButtonWidget &)
    {
        // The popup menu is closed when an action is triggered.
        self.close();
    }

    void updateIfScrolled()
    {
        // If the menu is scrolled, we need to update some things.
        int scrollY = self.menu().scrollPositionY().valuei();
        if(scrollY == oldScrollY)
        {
            return;
        }
        oldScrollY = scrollY;

        //qDebug() << "menu scrolling" << scrollY;

        // Resend the mouse position so the buttons realize they've moved.
        ClientApp::windowSystem().dispatchLatestMousePosition();

        self.requestGeometry();
    }
};

PopupMenuWidget::PopupMenuWidget(String const &name)
    : PopupWidget(name), d(new Instance(this))
{
    setContent(new MenuWidget(name.isEmpty()? "" : name + "-content"));

    menu().setGridSize(1, ui::Expand, 0, ui::Expand);

    menu().organizer().audienceForWidgetCreation += d;
    menu().organizer().audienceForWidgetUpdate += d;
}

MenuWidget &PopupMenuWidget::menu() const
{
    return static_cast<MenuWidget &>(content());
}

void PopupMenuWidget::update()
{
    PopupWidget::update();
    d->updateIfScrolled();
}

void PopupMenuWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    PopupWidget::glMakeGeometry(verts);

    if(d->hover && d->hover->isEnabled())
    {
        verts.makeQuad(d->highlightRect(),
                       d->hover->state() == ButtonWidget::Hover?
                           style().colors().colorf("inverted.background") :
                           style().colors().colorf("accent"),
                       root().atlas().imageRectf(root().solidWhitePixel()).middle());
    }
}

void PopupMenuWidget::preparePopupForOpening()
{
    // Redo the layout.
    menu().updateLayout();
    d->updateItemHitRules();

    // Make sure the menu doesn't go beyond the top of the view.
    if(openingDirection() == ui::Up)
    {
        menu().rule().setInput(Rule::Height,
                OperatorRule::minimum(menu().contentRule().height() + menu().margins().height(),
                                      anchorY() - menu().margins().top()));
    }

    PopupWidget::preparePopupForOpening();
}

void PopupMenuWidget::popupClosing()
{
    PopupWidget::popupClosing();

    if(d->hover)
    {
        d->hover->setTextColor("text");
        d->hover = 0;
        requestGeometry();
    }

    menu().dismissPopups();
}
