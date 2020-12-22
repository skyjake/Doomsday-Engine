/** @file inputbindingwidget.cpp
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/inputbindingwidget.h"

#include "clientapp.h"
#include "ui/bindcontext.h"
#include "ui/commandbinding.h"
// #include "ui/impulsebinding.h"
#include "ui/inputsystem.h"
#include "ui/b_util.h"

#include <de/charsymbols.h>
#include <de/auxbuttonwidget.h>

using namespace de;

#ifdef MACOSX
#  define CONTROL_MOD   KeyEvent::Meta
#  define CONTROL_CHAR  DE_CHAR_MAC_CONTROL_KEY
#else
#  define CONTROL_MOD   KeyEvent::Control
#  define CONTROL_CHAR  DE_CHAR_CONTROL_KEY
#endif

DE_GUI_PIMPL(InputBindingWidget)
, DE_OBSERVES(ButtonWidget, Press)
{
    String     defaultEvent;
    String     command;
    StringList contexts;
    int        device       = IDEV_KEYBOARD;
    bool       useModifiers = false;

    Impl(Public *i) : Base(i)
    {
        self().setSizePolicy(ui::Fixed, ui::Expand);

        self().auxiliary().setText(_E(l) "Reset");

        //self().audienceForPress() += this;
        self().auxiliary().audienceForPress() += this;
    }

    String prettyKey(const String &eventDesc)
    {
        if (!eventDesc.beginsWith("key-"))
        {
            // Doesn't look like a key.
            return eventDesc;
        }

        String name = eventDesc.substr({BytePos(4), eventDesc.indexOf("-", BytePos(4))});
        name = name.upperFirstChar(); //name.left(BytePos(1)).upper() + name.substr(BytePos(1)).lower();

        // Any modifiers?
        auto idx = eventDesc.indexOf("+");
        if (idx > 0)
        {
            const String conds = eventDesc.substr(idx + 1);
            if (conds.contains("key-alt-down"))
            {
                name = String(DE_CHAR_ALT_KEY) + name;
            }
            if (conds.contains("key-ctrl-down") || conds.contains("key-control-down"))
            {
                name = String(CONTROL_CHAR) + name;
            }
            if (conds.contains("key-shift-down"))
            {
                name = String(DE_CHAR_SHIFT_KEY) + name;
            }
        }
        return name;
    }

    /// Checks the current binding and updates the label to show which event/input
    /// is bound.
    void updateLabel()
    {
        String text = _E(l) "(not bound)";

        // Check all the contexts associated with this widget.
        for (const auto &bcName : contexts)
        {
            if (!InputSystem::get().hasContext(bcName)) continue;
            const BindContext &context = InputSystem::get().context(bcName);

            if (const Record *rec = context.findCommandBinding(command, device))
            {
                // This'll do.
                CommandBinding bind(*rec);
                text = prettyKey(bind.composeDescriptor());
                break;
            }
        }

        self().setText(_E(b) + text);
    }

    void bindCommand(const String &eventDesc)
    {
        InputSystem::get().forAllContexts([this] (BindContext &context)
        {
            while (Record *bind = context.findCommandBinding(command))
            {
                context.deleteBinding(bind->geti("id"));
            }
            return LoopContinue;
        });
        for (const auto &bcName : contexts)
        {
            String ev = Stringf("%s:%s", bcName.c_str(), eventDesc.c_str());
            InputSystem::get().bindCommand(ev, command);
        }
    }

    void buttonPressed(ButtonWidget &)
    {
        /*if (&button == thisPublic)
        {
            if (!self().hasFocus())
            {
                focus();
            }
            else
            {
                unfocus();
            }
        }
        else*/
        {
            // The reset button.
            bindCommand(defaultEvent);
            updateLabel();
        }
    }
};

InputBindingWidget::InputBindingWidget() : d(new Impl(this))
{
    setBehavior(Focusable);
    auxiliary().hide();
}

void InputBindingWidget::setDefaultBinding(const String &eventDesc)
{
    d->defaultEvent = eventDesc;
    auxiliary().show();
}

void InputBindingWidget::setCommand(const String &command)
{
    d->command = command;
    d->updateLabel();
}

void InputBindingWidget::enableModifiers(bool mods)
{
    d->useModifiers = mods;
}

void InputBindingWidget::setContexts(const StringList &contexts)
{
    d->contexts = contexts;
    d->updateLabel();
}

void InputBindingWidget::focusGained()
{
    AuxButtonWidget::focusGained();
    auxiliary().disable();
    invertStyle();
}

void InputBindingWidget::focusLost()
{
    AuxButtonWidget::focusLost();
    auxiliary().enable();
    invertStyle();
}

bool InputBindingWidget::handleEvent(const Event &event)
{
    if (hasFocus())
    {
        if (const KeyEvent *key = maybeAs<KeyEvent>(event))
        {
            if (key->state() != KeyEvent::Pressed) return false;

            // Include modifier keys if they will be included in the binding.
            if (d->useModifiers && key->isModifier())
            {
                return false;
            }

            if (key->ddKey() == DDKEY_ESCAPE)
            {
                root().popFocus();
                return true;
            }

            ddevent_t ev;
            InputSystem::convertEvent(event, ev);
            String desc = B_EventToString(ev);

            // Apply current modifiers as conditions.
            if (d->useModifiers)
            {
                if (key->modifiers().testFlag(KeyEvent::Shift))
                {
                    desc += " + key-shift-down";
                }
                else
                {
                    desc += " + key-shift-up";
                }

                if (key->modifiers().testFlag(KeyEvent::Alt))
                {
                    desc += " + key-alt-down";
                }
                else
                {
                    desc += " + key-alt-up";
                }

                if (key->modifiers().testFlag(CONTROL_MOD))
                {
                    desc += " + key-ctrl-down";
                }
                else
                {
                    desc += " + key-ctrl-up";
                }
            }

            d->bindCommand(desc);
            d->updateLabel();
            root().popFocus();
            return true;
        }

        /*if (const MouseEvent *mouse = maybeAs<MouseEvent>(event))
        {
            if (mouse->type()  == Event::MouseButton &&
                mouse->state() == MouseEvent::Released &&
                !hitTest(event))
            {
                // Clicking outside clears focus.
                d->unfocus();
                return true;
            }
        }*/
    }

    return AuxButtonWidget::handleEvent(event);
}

InputBindingWidget *InputBindingWidget::newTaskBarShortcut()
{
    InputBindingWidget *bind = new InputBindingWidget;
    bind->setCommand("taskbar");
    bind->setDefaultBinding("key-tilde-down + key-shift-up");
    bind->enableModifiers(true);
    bind->setContexts({"global", "console"});
    return bind;
}
