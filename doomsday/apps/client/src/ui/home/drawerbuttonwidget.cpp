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

    Instance(Public *i) : Base(i)
    {
        self.add(icon  = new LabelWidget);
        self.add(label = new LabelWidget);
        self.add(drawer = new PanelWidget);

        label->setSizePolicy(ui::Fixed, ui::Expand);
        label->setTextLineAlignment(ui::AlignLeft);
        label->setAlignment(ui::AlignLeft);

        icon->set(Background(style().colors().colorf("text")));
    }
};

DrawerButtonWidget::DrawerButtonWidget()
    : d(new Instance(this))
{
    AutoRef<Rule> iconSize(new ConstantRule(2 * 50));

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

    rule().setInput(Rule::Height, d->label->rule().height());
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
