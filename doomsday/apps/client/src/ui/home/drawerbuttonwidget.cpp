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
#include <de/SequentialLayout>

using namespace de;

DENG_GUI_PIMPL(DrawerButtonWidget)
{
    struct ClickHandler : public GuiWidget::IEventHandler
    {
        DrawerButtonWidget &owner;

        ClickHandler(DrawerButtonWidget &owner)
            : owner(owner) {}

        bool handleEvent(GuiWidget &, Event const &event)
        {
            if(event.type() == Event::MouseButton)
            {
                MouseEvent const &mouse = event.as<MouseEvent>();
                if(owner.hitTest(event) && mouse.state() == MouseEvent::Pressed)
                {
                    owner.root().setFocus(owner.d->background);
                    emit owner.mouseActivity();
                }
            }
            return false;
        }
    };

    LabelWidget *background;
    LabelWidget *icon;
    LabelWidget *label;
    PanelWidget *drawer;
    QList<ButtonWidget *> buttons;
    ScalarRule *labelRight;
    Rule const *buttonsWidth = nullptr;
    bool selected = false;

    Instance(Public *i) : Base(i)
    {
        labelRight = new ScalarRule(0);

        self.add(background = new LabelWidget);
        self.add(icon   = new LabelWidget);
        self.add(label  = new LabelWidget);
        self.add(drawer = new PanelWidget);

        label->setSizePolicy(ui::Fixed, ui::Expand);
        label->setTextLineAlignment(ui::AlignLeft);
        label->setAlignment(ui::AlignLeft);

        icon->set(Background(style().colors().colorf("text")));

        drawer->set(Background(Vector4f(0, 0, 0, .15f)));
        //drawer->margins().setZero();

        background->setBehavior(Focusable);
        background->addEventHandler(new ClickHandler(self));
    }

    ~Instance()
    {
        releaseRef(labelRight);
        releaseRef(buttonsWidth);
    }

    void updateButtonLayout()
    {
        SequentialLayout layout(*labelRight, label->rule().top(), ui::Right);
        for(auto *button : buttons)
        {
            layout << *button;
            button->rule().setMidAnchorY(label->rule().midY());
        }
        changeRef(buttonsWidth, layout.width() + label->margins().right());
    }

    void showButtons(bool show)
    {
        if(!buttonsWidth) return;

        TimeDelta const SPAN = .5;
        if(show)
        {
            labelRight->set(self.rule().right() - *buttonsWidth, SPAN);
        }
        else
        {
            labelRight->set(self.rule().right(), SPAN);
        }
    }
};

DrawerButtonWidget::DrawerButtonWidget()
    : d(new Instance(this))
{
    setBehavior(Focusable);

    Rule const &iconSize = d->label->rule().height();

    d->background->rule()
            .setInput(Rule::Top,    rule().top())
            .setInput(Rule::Left,   d->icon->rule().right())
            .setInput(Rule::Right,  rule().right())
            .setInput(Rule::Bottom, d->label->rule().bottom());

    d->icon->rule()
            .setSize(iconSize, iconSize)
            .setInput(Rule::Left, rule().left())
            .setInput(Rule::Top,  rule().top());

    d->labelRight->set(rule().right());
    d->label->rule()
            .setInput(Rule::Top,   rule().top())
            .setInput(Rule::Left,  d->icon->rule().right())
            .setInput(Rule::Right, *d->labelRight);

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
        d->drawer->set(Background(Vector4f(0, 0, 0, .4f)));
        d->background->set(Background(style().colors().colorf("background")));
        d->showButtons(true);
    }
    else
    {
        d->drawer->set(Background(Vector4f(0, 0, 0, .15f)));
        d->background->set(Background());
        d->showButtons(false);
    }
}

bool DrawerButtonWidget::isSelected() const
{
    return d->selected;
}

void DrawerButtonWidget::addButton(ButtonWidget *button)
{
    d->buttons << button;
    add(button);
    d->updateButtonLayout();
}

/*bool DrawerButtonWidget::handleEvent(Event const &event)
{
    if(event.isMouse())
    {
        switch(handleMouseClick(event))
        {
        case MouseClickUnrelated:
            return false;

        case MouseClickStarted:
        case MouseClickAborted:
            return false;

        case MouseClickFinished:
            root().setFocus(this);
            emit clicked();
            return false;
        }
    }
    return false;
}*/
/*
bool DrawerButtonWidget::dispatchEvent(Event const &event, bool (Widget::*memberFunc)(Event const &))
{
    // Observe mouse clicks occurring in the column.
    if(isEnabled() && event.isMouse() && event.type() == Event::MouseButton)
    {
        MouseEvent const &mouse = event.as<MouseEvent>();
        if(mouse.state() == MouseEvent::Released &&
           contentRect().contains(mouse.pos()))
        {
            root().setFocus(d->background);
            emit mouseActivity();
        }
    }

    return GuiWidget::dispatchEvent(event, memberFunc);
}
*/
