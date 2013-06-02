/** @file buttonwidget.cpp  Clickable button widget.
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

#include "ui/widgets/buttonwidget.h"
#include "ui/widgets/guirootwidget.h"

#include <de/MouseEvent>
#include <de/Animation>

using namespace de;

DENG2_PIMPL(ButtonWidget)
{
    enum State {
        Up,
        Hover,
        Down
    };
    State state;
    QScopedPointer<Action> action;
    Animation scale;
    Animation frameOpacity;
    bool animating;

    Instance(Public *i)
        : Base(i), state(Up),
          scale(1.f),
          frameOpacity(.15f, Animation::Linear),
          animating(false)
    {
        updateBackground();
    }

    void setState(State st)
    {
        if(state == st) return;

        State const prev = state;
        state = st;
        animating = true;

        switch(st)
        {
        case Up:
            scale.setStyle(prev == Down? Animation::Bounce : Animation::EaseIn);
            scale.setValue(1.f, .3f);
            frameOpacity.setValue(.15f, .6f);
            break;

        case Hover:
            //scale.setStyle(Animation::EaseIn);
            //scale.setValue(1.1f, .15f);
            frameOpacity.setValue(.4f, .15f);
            break;

        case Down:
            scale.setValue(.9f);
            frameOpacity.setValue(0);//.5f);
            break;
        }
    }

    void updateHover(Vector2i const &pos)
    {
        if(state == Down) return;

        if(self.hitTest(pos))
        {
            if(state == Up) setState(Hover);
        }
        else if(state == Hover)
        {
            setState(Up);
        }
    }

    void updateBackground()
    {
        self.set(Background(self.style().colors().colorf("background"),
                            Background::GradientFrame, Vector4f(1, 1, 1, frameOpacity), 6));
    }

    void updateAnimation()
    {
        if(animating)
        {
            updateBackground();
            self.requestGeometry();
            if(scale.done() && frameOpacity.done())
            {
                animating = false;
            }
        }
    }
};

ButtonWidget::ButtonWidget(String const &name) : LabelWidget(name), d(new Instance(this))
{}

void ButtonWidget::setAction(Action *action)
{
    d->action.reset(action);
}

bool ButtonWidget::handleEvent(Event const &event)
{
    if(event.isMouse())
    {
        MouseEvent const &mouse = event.as<MouseEvent>();

        if(mouse.type() == Event::MousePosition)
        {
            d->updateHover(mouse.pos());
        }
        else if(mouse.type() == Event::MouseButton)
        {
            switch(handleMouseClick(event))
            {
            case MouseClickStarted:
                d->setState(Instance::Down);
                return true;

            case MouseClickFinished:
                d->setState(Instance::Up);
                if(!d->action.isNull() && hitTest(mouse.pos()))
                {
                    d->action->trigger();
                }
                return true;

            case MouseClickAborted:
                d->setState(Instance::Up);
                return true;

            default:
                break;
            }
        }
    }
    return false;
}

void ButtonWidget::updateModelViewProjection(GLUniform &uMvp)
{
    Rectanglef const &pos = rule().rect();

    // Apply a scale animation to indicate button response.
    uMvp = root().projMatrix2D() *
            Matrix4f::scaleThenTranslate(d->scale, pos.middle()) *
            Matrix4f::translate(-pos.middle());
}

void ButtonWidget::update()
{
    LabelWidget::update();

    d->updateAnimation();
}
