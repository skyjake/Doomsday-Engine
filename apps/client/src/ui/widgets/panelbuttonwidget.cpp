/** @file panelbuttonwidget.cpp  Button with an extensible drawer.
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

#include "ui/widgets/panelbuttonwidget.h"

using namespace de;

DE_GUI_PIMPL(PanelButtonWidget)
{
    PanelWidget *drawer;

    Impl(Public *i) : Base(i)
    {
        self().add(drawer = new PanelWidget);
        drawer->set(Background(Vec4f(0, 0, 0, .15f)));
        drawer->setEatMouseEvents(false);
    }
};

PanelButtonWidget::PanelButtonWidget(Flags flags, const String &name)
    : HomeItemWidget(flags, name)
    , d(new Impl(this))
{
    setBehavior(Focusable);

    d->drawer->rule()
            .setInput(Rule::Top,  icon().rule().bottom())
            .setInput(Rule::Left, rule().left());

    if (flags & AnimatedHeight)
    {
        rule().setInput(Rule::Height, new AnimationRule(label().rule().height() +
                                                        d->drawer->rule().height(), 0.3));
    }
    else
    {
        rule().setInput(Rule::Height, label().rule().height() + d->drawer->rule().height());
    }
}

PanelWidget &PanelButtonWidget::panel()
{
    return *d->drawer;
}

void PanelButtonWidget::setSelected(bool selected)
{
    HomeItemWidget::setSelected(selected);

    if (selected)
    {
        d->drawer->set(Background(Vec4f(0, 0, 0, .4f)));
    }
    else
    {
        d->drawer->set(Background(Vec4f(0, 0, 0, .15f)));
    }
}
