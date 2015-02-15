/** @file buttonwidget.cpp  Clickable button widget.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/ButtonWidget"
#include "de/GuiRootWidget"

#include <de/MouseEvent>
#include <de/Animation>

namespace de {

DENG_GUI_PIMPL(ButtonWidget),
DENG2_OBSERVES(Action, Triggered)
{
    State state;
    DotPath hoverTextColor;
    DotPath originalTextColor;
    Vector4f originalTextModColor;
    DotPath bgColorId;
    HoverColorMode hoverColorMode;
    bool infoStyle;
    Action *action;
    Animation scale;
    Animation frameOpacity;
    bool animating;

    Instance(Public *i)
        : Base(i)
        , state(Up)
        , bgColorId("background")
        , hoverColorMode(ReplaceColor)
        , infoStyle(false)
        , action(0)
        , scale(1.f)
        , frameOpacity(.08f, Animation::Linear)
        , animating(false)
    {
        setDefaultBackground();
    }

    ~Instance()
    {
        if(action) action->audienceForTriggered() -= this;
        releaseRef(action);
    }

    void setState(State st)
    {
        if(state == st) return;

        if(st == Hover && state == Up)
        {
            // Remember the original text color.
            originalTextColor = self.textColorId();
            originalTextModColor = self.textModulationColorf();
        }

        State const prev = state;
        state = st;
        animating = true;

        switch(st)
        {
        case Up:
            scale.setValue(1.f, .3f);
            scale.setStyle(prev == Down? Animation::Bounce : Animation::EaseOut);
            frameOpacity.setValue(.08f, .6f);
            if(!hoverTextColor.isEmpty())
            {
                // Restore old color.
                switch(hoverColorMode)
                {
                case ModulateColor:
                    self.setTextModulationColorf(originalTextModColor);
                    break;
                case ReplaceColor:
                    self.setTextColor(originalTextColor);
                    break;
                }
            }
            break;

        case Hover:
            frameOpacity.setValue(.4f, .15f);
            if(!hoverTextColor.isEmpty())
            {
                switch(hoverColorMode)
                {
                case ModulateColor:
                    self.setTextModulationColorf(style().colors().colorf(hoverTextColor));
                    break;
                case ReplaceColor:
                    self.setTextColor(hoverTextColor);
                    break;
                }
            }
            break;

        case Down:
            scale.setValue(.95f);
            frameOpacity.setValue(0);
            break;
        }

        DENG2_FOR_PUBLIC_AUDIENCE2(StateChange, i)
        {
            i->buttonStateChanged(self, state);
        }
    }

    void updateHover(Vector2i const &pos)
    {
        if(state == Down) return;
        if(self.isDisabled())
        {
            setState(Up);
            return;
        }

        if(self.hitTest(pos))
        {
            if(state == Up) setState(Hover);
        }
        else if(state == Hover)
        {
            setState(Up);
        }
    }

    void setDefaultBackground()
    {
        self.set(Background(style().colors().colorf(bgColorId),
                            Background::GradientFrame, Vector4f(1, 1, 1, frameOpacity), 6));
    }

    void updateBackground()
    {
        Background bg = self.background();
        if(bg.type == Background::GradientFrame)
        {
            bg.solidFill = style().colors().colorf(bgColorId);
            /// @todo Make this Style-able. -jk
            bg.color = infoStyle? Vector4f(0, 0, 0, frameOpacity) :
                                  Vector4f(1, 1, 1, frameOpacity);
            self.set(bg);
        }
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

    void actionTriggered(Action &)
    {
        DENG2_FOR_PUBLIC_AUDIENCE2(Triggered, i)
        {
            i->buttonActionTriggered(self);
        }
    }

    DENG2_PIMPL_AUDIENCE(StateChange)
    DENG2_PIMPL_AUDIENCE(Press)
    DENG2_PIMPL_AUDIENCE(Triggered)
};

DENG2_AUDIENCE_METHOD(ButtonWidget, StateChange)
DENG2_AUDIENCE_METHOD(ButtonWidget, Press)
DENG2_AUDIENCE_METHOD(ButtonWidget, Triggered)

ButtonWidget::ButtonWidget(String const &name) : LabelWidget(name), d(new Instance(this))
{}

void ButtonWidget::useInfoStyle(bool yes)
{
    d->infoStyle = yes;
    if(yes)
    {        
        d->originalTextColor = "inverted.text";
        setHoverTextColor("inverted.text", ReplaceColor);
        setBackgroundColor("inverted.background");
    }
    else
    {
        d->originalTextColor = "text";
        setHoverTextColor("text", ReplaceColor);
        setBackgroundColor("background");
    }
    setTextColor(d->originalTextColor);
    d->originalTextModColor = Vector4f(1, 1, 1, 1);
    setTextModulationColorf(d->originalTextModColor);
    updateStyle();
}

bool ButtonWidget::isUsingInfoStyle() const
{
    return d->infoStyle;
}

void ButtonWidget::setHoverTextColor(DotPath const &hoverTextId, HoverColorMode mode)
{
    d->hoverTextColor = hoverTextId;
    d->hoverColorMode = mode;
}

void ButtonWidget::setBackgroundColor(DotPath const &bgColorId)
{
    d->bgColorId = bgColorId;
    d->updateBackground();
}

void ButtonWidget::setAction(RefArg<Action> action)
{
    if(d->action)
    {
        d->action->audienceForTriggered() -= d;
    }

    changeRef(d->action, action);

    if(action)
    {
        action->audienceForTriggered() += d;
    }
}

Action const *ButtonWidget::action() const
{
    return d->action;
}

void ButtonWidget::trigger()
{
    // Hold an extra ref so the action isn't deleted by triggering.
    AutoRef<Action> held = holdRef(d->action);

    // Notify.
    emit pressed();
    DENG2_FOR_AUDIENCE2(Press, i) i->buttonPressed(*this);

    if(held)
    {
        held->trigger();
    }
}

ButtonWidget::State ButtonWidget::state() const
{
    return d->state;
}

bool ButtonWidget::handleEvent(Event const &event)
{
    if(isDisabled()) return false;

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
                d->setState(Down);
                return true;

            case MouseClickFinished:
                d->setState(Up);
                d->updateHover(mouse.pos());
                if(hitTest(mouse.pos()))
                {
                    trigger();
                }
                return true;

            case MouseClickAborted:
                d->setState(Up);
                return true;

            default:
                break;
            }
        }
    }

    return LabelWidget::handleEvent(event);
}

void ButtonWidget::updateModelViewProjection(GLUniform &uMvp)
{
    uMvp = root().projMatrix2D();

    if(!fequal(d->scale, 1.f))
    {
        Rectanglef const &pos = rule().rect();

        // Apply a scale animation to indicate button response.
        uMvp = uMvp.toMatrix4f() *
                Matrix4f::scaleThenTranslate(d->scale, pos.middle()) *
                Matrix4f::translate(-pos.middle());
    }
}

void ButtonWidget::updateStyle()
{
    LabelWidget::updateStyle();
    d->updateBackground();
}

void ButtonWidget::update()
{
    LabelWidget::update();
    d->updateAnimation();
}

} // namespace de
