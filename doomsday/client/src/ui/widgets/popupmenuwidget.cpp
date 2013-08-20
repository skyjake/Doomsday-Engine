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
#include "ui/widgets/guirootwidget.h"
#include "ui/widgets/menuwidget.h"
#include "ui/widgets/contextwidgetorganizer.h"
#include "ui/widgets/item.h"

using namespace de;

DENG2_PIMPL(PopupMenuWidget),
DENG2_OBSERVES(ButtonWidget, StateChange),
DENG2_OBSERVES(ButtonWidget, Triggered),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetCreation),
DENG2_OBSERVES(ContextWidgetOrganizer, WidgetUpdate)
{
    ButtonWidget *hover;
    Rectanglei hoverHighlightRect;

    Instance(Public *i) : Base(i), hover(0)
    {}

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

        // We want items to be hittable throughout the width of the menu.
        widget.hitRule()
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Right, self.rule().right());

        // Customize buttons for use in the popup. We will observe the button
        // state for highlighting and possibly close the popup when an action
        // gets triggered.
        if(ButtonWidget *b = widget.maybeAs<ButtonWidget>())
        {
            b->setSizePolicy(ui::Expand, ui::Expand);
            b->setMargin("unit");

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
                widget.setMargin("");
                widget.setFont("separator.empty");
            }
            else
            {
                widget.setMargin("halfunit");
                widget.setFont("separator.label");
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
            hi.topLeft.x     = self.rule().left().valuei();
            hi.topLeft.y     = hover->rule().top().valuei();
            hi.bottomRight.x = self.rule().right().valuei();
            hi.bottomRight.y = hover->rule().bottom().valuei();
        }
        return hi;
    }

    void buttonActionTriggered(ButtonWidget &)
    {
        // The popup menu is closed when an action is triggered.
        self.close();
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

void PopupMenuWidget::glMakeGeometry(DefaultVertexBuf::Builder &verts)
{
    PopupWidget::glMakeGeometry(verts);

    if(d->hover)
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

    PopupWidget::preparePopupForOpening();

    //menu().rule().setInput(Rule::Width, menu().layout().width() + 2 * margin());
    //menu().rule().setInput(Rule::Height, menu().layout().height() + 2 * margin());
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
