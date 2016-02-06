/** @file drawerbuttonwidget.cpp  Button with an extensible drawer.
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

#include "ui/home/drawerbuttonwidget.h"

#include <de/PanelWidget>
#include <de/ButtonWidget>

using namespace de;

DENG_GUI_PIMPL(DrawerButtonWidget)
{
    LabelWidget *icon;
    LabelWidget *label;
    PanelWidget *drawer;
    QList<ButtonWidget *> buttons;
    bool selected = false;

    Instance(Public *i) : Base(i)
    {
        self.add(icon   = new LabelWidget);
        self.add(label  = new LabelWidget);
        self.add(drawer = new PanelWidget);

        label->setSizePolicy(ui::Fixed, ui::Expand);
        label->setTextLineAlignment(ui::AlignLeft);
        label->setAlignment(ui::AlignLeft);

        icon->set(Background(style().colors().colorf("text")));

        drawer->set(Background(Vector4f(0, 0, 0, .3f)));
        //drawer->margins().setZero();
    }
};

DrawerButtonWidget::DrawerButtonWidget()
    : d(new Instance(this))
{
    Rule const &iconSize = d->label->rule().height();

    d->icon->rule()
            .setSize(iconSize, iconSize)
            .setInput(Rule::Left, rule().left())
            .setInput(Rule::Top,  rule().top());

    d->label->rule()
            .setInput(Rule::Top,   rule().top())
            .setInput(Rule::Left,  d->icon->rule().right())
            .setInput(Rule::Right, rule().right()); // adjust when buttons added

    d->drawer->rule()
            .setInput(Rule::Top,  d->label->rule().bottom())
            .setInput(Rule::Left, rule().left());

    rule().setInput(Rule::Height, d->label->rule().height() +
                                  d->drawer->rule().height());
}

LabelWidget &DrawerButtonWidget::icon()
{
    return *d->icon;
}

LabelWidget &DrawerButtonWidget::label()
{
    return *d->label;
}

PanelWidget &DrawerButtonWidget::drawer()
{
    return *d->drawer;
}

void DrawerButtonWidget::setSelected(bool selected)
{
    d->selected = selected;
    if(selected)
    {
        d->label->set(Background(style().colors().colorf("background")));
    }
    else
    {
        d->label->set(Background());
    }
}

bool DrawerButtonWidget::isSelected() const
{
    return d->selected;
}

bool DrawerButtonWidget::handleEvent(Event const &event)
{
    switch(handleMouseClick(event))
    {
    case MouseClickUnrelated:
        return false;

    case MouseClickStarted:
    case MouseClickAborted:
        return true;

    case MouseClickFinished:
        emit clicked();
        return true;
    }
    return false;
}
