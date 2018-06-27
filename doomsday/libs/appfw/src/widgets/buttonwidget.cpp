#include <utility>

/** @file buttonwidget.cpp  Clickable button widget.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/CallbackAction"

#include <de/FocusWidget>
#include <de/MouseEvent>
#include <de/Animation>

namespace de {

DE_GUI_PIMPL(ButtonWidget)
, DE_OBSERVES(Action, Triggered)
{
    State            state{Up};
    DotPath          bgColorId{"background"};
    DotPath          borderColorId{"text"};
    HoverColorMode   hoverColorMode{ReplaceColor};
    ColorTheme       colorTheme{Normal};
    Background::Type bgType{Background::GradientFrame};
    Action *         action{nullptr};
    Animation        scale{1.f};
    Animation        frameOpacity{.08f, Animation::Linear};
    bool             animating{false};
    DotPath          hoverTextColor;
    DotPath          originalTextColor;
    Vec4f            originalTextModColor;
    String           shortcut;

    Impl(Public *i) : Base(i)
    {
        setDefaultBackground();
    }

    ~Impl()
    {
        releaseRef(action);
    }

    void setState(State st)
    {
        if (state == st) return;

        State const prev = state;
        state = st;
        animating = true;

        switch (st)
        {
        case Up:
            scale.setValue(1.f, .3f);
            scale.setStyle(prev == Down? Animation::Bounce : Animation::EaseOut);
            frameOpacity.setValue(.08f, .6f);
            if (!hoverTextColor.isEmpty())
            {
                // Restore old color.
                switch (hoverColorMode)
                {
                case ModulateColor:
                    self().setTextModulationColorf(originalTextModColor);
                    break;
                case ReplaceColor:
                    setTemporaryTextColor(originalTextColor);
                    break;
                }
            }
            break;

        case Hover:
            frameOpacity.setValue(.4f, .15f);
            if (!hoverTextColor.isEmpty())
            {
                switch (hoverColorMode)
                {
                case ModulateColor:
                    self().setTextModulationColorf(style().colors().colorf(hoverTextColor));
                    break;
                case ReplaceColor:
                    setTemporaryTextColor(hoverTextColor);
                    break;
                }
            }
            break;

        case Down:
            scale.setValue(.95f);
            frameOpacity.setValue(0);
            break;
        }

        DE_FOR_PUBLIC_AUDIENCE2(StateChange, i)
        {
            i->buttonStateChanged(self(), state);
        }
    }

    void updateHover(Vec2i const &pos)
    {
        if (state == Down) return;
        if (self().isDisabled())
        {
            setState(Up);
            return;
        }

        if (self().hitTest(pos))
        {
            if (state == Up) setState(Hover);
        }
        else if (state == Hover)
        {
            setState(Up);
        }
    }

    Vec4f borderColor() const
    {
        return style().colors().colorf(borderColorId) *
               Vec4f(1, 1, 1, frameOpacity);
    }

    void setDefaultBackground()
    {
        self().set(Background(style().colors().colorf(bgColorId),
                            bgType, borderColor(), 6));
    }

    void updateBackground()
    {
        Background bg = self().background();
        if (bg.type == Background::GradientFrame ||
            bg.type == Background::GradientFrameWithRoundedFill)
        {
            bg.solidFill = style().colors().colorf(bgColorId);
            bg.color = borderColor();
            self().set(bg);
        }
    }

    void updateAnimation()
    {
        if (animating)
        {
            updateBackground();
            self().requestGeometry();
            if (scale.done() && frameOpacity.done())
            {
                animating = false;
            }
        }
    }

    void actionTriggered(Action &)
    {
        DE_FOR_PUBLIC_AUDIENCE2(Triggered, i)
        {
            i->buttonActionTriggered(self());
        }
    }

    void setTemporaryTextColor(DotPath const &id)
    {
        DotPath const original = originalTextColor;
        self().setTextColor(id); // originalTextColor changes...
        originalTextColor = original;
    }

    DE_PIMPL_AUDIENCE(StateChange)
    DE_PIMPL_AUDIENCE(Press)
    DE_PIMPL_AUDIENCE(Triggered)
};

DE_AUDIENCE_METHOD(ButtonWidget, StateChange)
DE_AUDIENCE_METHOD(ButtonWidget, Press)
DE_AUDIENCE_METHOD(ButtonWidget, Triggered)

ButtonWidget::ButtonWidget(String const &name) : LabelWidget(name), d(new Impl(this))
{
    setBehavior(Focusable);
    setColorTheme(Normal);
}

void ButtonWidget::useInfoStyle(bool yes)
{
    setColorTheme(yes? Inverted : Normal);
}

bool ButtonWidget::isUsingInfoStyle() const
{
    return d->colorTheme == Inverted;
}

void ButtonWidget::setColorTheme(ColorTheme theme)
{
    auto bg = background();

    d->colorTheme = theme;
    setTextModulationColorf(Vec4f(1, 1, 1, 1));
    d->originalTextModColor = textModulationColorf();
    if (theme == Inverted)
    {
        d->bgType = Background::GradientFrameWithRoundedFill;
        if (bg.type == Background::GradientFrame) bg.type = d->bgType;
        setTextColor("inverted.text");
        setHoverTextColor("inverted.text", ReplaceColor);
        setBorderColor("inverted.text");
        setBackgroundColor("inverted.background");
    }
    else
    {
        d->bgType = Background::GradientFrame;
        if (bg.type == Background::GradientFrameWithRoundedFill) bg.type = d->bgType;
        setTextColor("text");
        setHoverTextColor("text", ReplaceColor);
        setBorderColor("text");
        setBackgroundColor("background");
    }
    set(bg);
    setImageColor(textColorf());
    updateStyle();
}

GuiWidget::ColorTheme ButtonWidget::colorTheme() const
{
    return d->colorTheme;
}

void ButtonWidget::setTextColor(DotPath const &colorId)
{
    LabelWidget::setTextColor(colorId);
    d->originalTextColor = colorId;
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

void ButtonWidget::setBorderColor(DotPath const &borderColorId)
{
    d->borderColorId = borderColorId;
    d->updateBackground();
}

void ButtonWidget::setAction(RefArg<Action> action)
{
    if (d->action)
    {
        d->action->audienceForTriggered() -= d;
    }

    changeRef(d->action, action);

    if (action)
    {
        action->audienceForTriggered() += d;
    }
}

void ButtonWidget::setActionFn(std::function<void ()> callback)
{
    setAction(new CallbackAction(std::move(callback)));
}

Action const *ButtonWidget::action() const
{
    return d->action;
}

void ButtonWidget::trigger()
{
    if (behavior().testFlag(Focusable))
    {
        root().setFocus(this);
    }

    // Hold an extra ref so the action isn't deleted by triggering.
    AutoRef<Action> held = holdRef(d->action);

    // Notify.
    DE_FOR_AUDIENCE2(Press, i) i->buttonPressed(*this);

    if (held)
    {
        held->trigger();
    }
}

ButtonWidget::State ButtonWidget::state() const
{
    return d->state;
}

void ButtonWidget::setState(State state)
{
    d->setState(state);
}

void ButtonWidget::setShortcutKey(String const &key)
{
    d->shortcut = key;
}

String ButtonWidget::shortcutKey() const
{
    return d->shortcut;
}

bool ButtonWidget::handleShortcut(KeyEvent const &keyEvent)
{
    if (keyEvent.text().isEmpty())
    {
        return false;
    }
    if ((!d->shortcut.isEmpty() && d->shortcut.beginsWith(keyEvent.text(), CaseInsensitive)) ||
        text().beginsWith(keyEvent.text(), CaseInsensitive))
    {
        trigger();
        return true;
    }
    return false;
}

bool ButtonWidget::handleEvent(Event const &event)
{
    if (isDisabled()) return false;

    if (event.isKey() && hasFocus())
    {
        KeyEvent const &key = event.as<KeyEvent>();
        if (key.ddKey() == DDKEY_RETURN ||
            key.ddKey() == DDKEY_ENTER  ||
            key.ddKey() == ' ')
        {
            if (key.isKeyDown())
            {
                root().focusIndicator().fadeIn();
                trigger();
            }
            return true;
        }
    }

    if (event.isMouse())
    {
        MouseEvent const &mouse = event.as<MouseEvent>();

        if (mouse.type() == Event::MousePosition)
        {
            d->updateHover(mouse.pos());
        }
        else if (mouse.type() == Event::MouseButton)
        {
            switch (handleMouseClick(event))
            {
            case MouseClickStarted:
                d->setState(Down);
                return true;

            case MouseClickFinished:
                d->setState(Up);
                d->updateHover(mouse.pos());
                if (hitTest(mouse.pos()))
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

bool ButtonWidget::updateModelViewProjection(Mat4f &mvp)
{
    if (!fequal(d->scale, 1.f))
    {
        Rectanglef const &pos = rule().rect();

        // Apply a scale animation to indicate button response.
        mvp = root().projMatrix2D() *
              Mat4f::scaleThenTranslate(d->scale, pos.middle()) *
              Mat4f::translate(-pos.middle());
        return true;
    }
    return false;
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
