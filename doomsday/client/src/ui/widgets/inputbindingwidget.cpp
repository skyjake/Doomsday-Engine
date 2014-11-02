/** @file inputbindingwidget.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/b_main.h"
#include "ui/b_context.h"
#include <de/AuxButtonWidget>
#include <de/charsymbols.h>

using namespace de;

#ifdef MACOSX
#  define CONTROL_MOD   KeyEvent::Meta
#  define CONTROL_CHAR  DENG2_CHAR_MAC_CONTROL_KEY
#else
#  define CONTROL_MOD   KeyEvent::Control
#  define CONTROL_CHAR  DENG2_CHAR_CONTROL_KEY
#endif

DENG_GUI_PIMPL(InputBindingWidget)
, DENG2_OBSERVES(ButtonWidget, Press)
{
    String defaultEvent;
    String command;
    QStringList contexts;
    uint device;
    bool useModifiers;

    Instance(Public *i)
        : Base(i)
        , device(IDEV_KEYBOARD)
        , useModifiers(false)
    {
        //self.setTextLineAlignment(ui::AlignLeft);
        self.setSizePolicy(ui::Fixed, ui::Expand);

        self.auxiliary().setText(_E(l) + tr("Reset"));

        self.audienceForPress() += this;
        self.auxiliary().audienceForPress() += this;
    }

    String prettyKey(String const &eventDesc)
    {
        if(!eventDesc.startsWith("key-"))
        {
            // Doesn't look like a key.
            return eventDesc;
        }

        String name = eventDesc.substr(Rangei(4, eventDesc.indexOf("-", 4)));
        name = name.left(1).toUpper() + name.mid(1).toLower();

        // Any modifiers?
        int idx = eventDesc.indexOf("+");
        if(idx > 0)
        {
            String const conds = eventDesc.mid(idx + 1);
            if(conds.contains("key-alt-down"))
            {
                name = String(DENG2_CHAR_ALT_KEY) + name;
            }
            if(conds.contains("key-ctrl-down") || conds.contains("key-control-down"))
            {
                name = String(CONTROL_CHAR) + name;
            }
            if(conds.contains("key-shift-down"))
            {
                name = String(DENG2_CHAR_SHIFT_KEY) + name;
            }
        }
        return name;
    }

    /// Checks the current binding and updates the label to show which event/input
    /// is bound.
    void updateLabel()
    {
        String text = _E(l) + tr("(not bound)");

        // Check all the contexts associated with this widget.
        foreach(QString bcName, contexts)
        {
            bcontext_t const *bc = B_ContextByName(bcName.toLatin1());
            evbinding_t const *com = B_FindCommandBinding(&bc->commandBinds,
                                                          command.toLatin1(), device);
            if(com)
            {
                // This'll do.
                AutoStr *str = AutoStr_New();
                B_EventBindingToString(com, str);
                text = prettyKey(Str_Text(str));
                break;
            }
        }

        self.setText(_E(b) + text);
    }

    void bind(String const &eventDesc)
    {
        B_UnbindCommand(command.toLatin1());

        foreach(QString bcName, contexts)
        {
            String ev = String("%1:%2").arg(bcName, eventDesc);
            B_BindCommand(ev.toLatin1(), command.toLatin1());
        }
    }

    void buttonPressed(ButtonWidget &button)
    {
        if(&button == thisPublic)
        {
            if(!self.hasFocus())
            {
                focus();
            }
            else
            {
                unfocus();
            }
        }
        else
        {
            // The reset button.
            bind(defaultEvent);
            updateLabel();
        }
    }

    void focus()
    {
        root().setFocus(thisPublic);
        self.auxiliary().disable();
        self.useInvertedStyle();
    }

    void unfocus()
    {
        root().setFocus(0);
        self.auxiliary().enable();
        self.useNormalStyle();
    }
};

InputBindingWidget::InputBindingWidget() : d(new Instance(this))
{
    auxiliary().hide();
}

void InputBindingWidget::setDefaultBinding(String const &eventDesc)
{
    d->defaultEvent = eventDesc;
    auxiliary().show();
}

void InputBindingWidget::setCommand(String const &command)
{
    d->command = command;
    d->updateLabel();
}

void InputBindingWidget::enableModifiers(bool mods)
{
    d->useModifiers = mods;
}

void InputBindingWidget::setContexts(QStringList const &contexts)
{
    d->contexts = contexts;
    d->updateLabel();
}

bool InputBindingWidget::handleEvent(Event const &event)
{
    if(hasFocus())
    {
        if(KeyEvent const *key = event.maybeAs<KeyEvent>())
        {
            if(key->state() != KeyEvent::Pressed) return false;

            // Include modifier keys if they will be included in the binding.
            if(d->useModifiers && key->isModifier())
            {
                return false;
            }

            if(key->ddKey() == DDKEY_ESCAPE)
            {
                d->unfocus();
                return true;
            }

            AutoStr *name = AutoStr_New();
            ddevent_t ev;
            InputSystem::convertEvent(event, &ev);
            B_AppendEventToString(&ev, name);

            String desc = Str_Text(name);

            // Apply current modifiers as conditions.
            if(d->useModifiers)
            {
                if(key->modifiers().testFlag(KeyEvent::Shift))
                {
                    desc += " + key-shift-down";
                }
                else
                {
                    desc += " + key-shift-up";
                }

                if(key->modifiers().testFlag(KeyEvent::Alt))
                {
                    desc += " + key-alt-down";
                }
                else
                {
                    desc += " + key-alt-up";
                }

                if(key->modifiers().testFlag(CONTROL_MOD))
                {
                    desc += " + key-ctrl-down";
                }
                else
                {
                    desc += " + key-ctrl-up";
                }
            }

            d->bind(desc);
            d->updateLabel();
            d->unfocus();
            return true;
        }

        if(MouseEvent const *mouse = event.maybeAs<MouseEvent>())
        {
            if(mouse->type() == Event::MouseButton &&
               mouse->state() == MouseEvent::Released &&
               !hitTest(event))
            {
                // Clicking outside clears focus.
                d->unfocus();
                return true;
            }
        }
    }

    return AuxButtonWidget::handleEvent(event);
}

InputBindingWidget *InputBindingWidget::newTaskBarShortcut()
{
    InputBindingWidget *bind = new InputBindingWidget;
    bind->setCommand("taskbar");
    bind->setDefaultBinding("key-tilde-down + key-shift-up");
    bind->enableModifiers(true);
    bind->setContexts(QStringList() << "global" << "console");
    return bind;
}
