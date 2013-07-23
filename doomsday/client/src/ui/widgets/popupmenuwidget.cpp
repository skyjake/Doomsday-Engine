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

using namespace de;

DENG2_PIMPL(PopupMenuWidget),
DENG2_OBSERVES(ButtonWidget, StateChange),
DENG2_OBSERVES(ButtonWidget, Triggered)
{
    ButtonWidget *hover;
    Rectanglei hoverHighlightRect;

    Instance(Public *i) : Base(i), hover(0)
    {}

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
}

MenuWidget &PopupMenuWidget::menu() const
{
    return static_cast<MenuWidget &>(content());
}

ButtonWidget *PopupMenuWidget::addItem(String const &styledText, Action *action, bool dismissOnTriggered)
{
    ButtonWidget *b = menu().addItem(styledText, action);
    b->setSizePolicy(ui::Expand, ui::Expand);
    b->setMargin("unit");
    b->set(Background());

    b->audienceForStateChange += d;
    if(dismissOnTriggered)
    {
        b->audienceForTriggered += d;
    }

    // We want items to be hittable throughtout the width of the menu.
    b->hitRule()
            .setInput(Rule::Left,  rule().left())
            .setInput(Rule::Right, rule().right());

    return b;
}

GuiWidget *PopupMenuWidget::addSeparator(String const &optionalLabel)
{
    GuiWidget *sep = menu().addSeparator(optionalLabel);
    if(optionalLabel.isEmpty())
    {
        sep->setMargin("");
        sep->setFont("separator.empty");
    }
    else
    {
        sep->setMargin("halfunit");
        sep->setFont("separator.label");
    }
    sep->setTextColor("label.accent");
    sep->set(Background());
    return sep;
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
    PopupWidget::preparePopupForOpening();

    // Redo the layout.
    menu().updateLayout();
    menu().rule().setInput(Rule::Width,
                           *refless(menu().newColumnWidthRule(0)) + 2 * margin());
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
}
