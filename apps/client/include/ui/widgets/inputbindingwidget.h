/** @file inputbindingwidget.h  Widget for creating input bindings.
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

#ifndef DE_CLIENT_INPUTBINDINGWIDGET_H
#define DE_CLIENT_INPUTBINDINGWIDGET_H

#include <de/auxbuttonwidget.h>

/**
 * Widget for viewing and changing an input binding.
 */
class InputBindingWidget : public de::AuxButtonWidget
{
public:
    InputBindingWidget();

    void setDefaultBinding(const de::String &eventDesc);

    void setCommand(const de::String &command);

    /**
     * Enables or disables explicitly specified modifier conditions. Bindings can be
     * specified with or without modifiers -- if modifiers are not specified, the binding
     * can be triggered regardless of modifier state.
     *
     * @param mods  @c true to includes modifiers in the binding. If @c false, modifiers
     *              can be bound like any other input.
     */
    void enableModifiers(bool mods);

    inline void setContext(const de::String &bindingContext) { setContexts({bindingContext}); }

    void setContexts(const de::StringList &contexts);

    // Events.
    void focusGained() override;
    void focusLost() override;
    bool handleEvent(const de::Event &event) override;

public:
    static InputBindingWidget *newTaskBarShortcut();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_INPUTBINDINGWIDGET_H
