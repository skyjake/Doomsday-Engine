/** @file inputbindingwidget.h  Widget for creating input bindings.
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

#ifndef DENG_CLIENT_INPUTBINDINGWIDGET_H
#define DENG_CLIENT_INPUTBINDINGWIDGET_H

#include <de/AuxButtonWidget>

/**
 * Widget for viewing and changing an input binding.
 */
class InputBindingWidget : public de::AuxButtonWidget
{
public:
    InputBindingWidget();

    void setDefaultBinding(de::String const &eventDesc);

    void setCommand(de::String const &command);

    /**
     * Enables or disables explicitly specified modifier conditions. Bindings can be
     * specified with or without modifiers -- if modifiers are not specified, the binding
     * can be triggered regardless of modifier state.
     *
     * @param mods  @c true to includes modifiers in the binding. If @c false, modifiers
     *              can be bound like any other input.
     */
    void enableModifiers(bool mods);

    void setContext(QString const &bindingContext) {
        setContexts(QStringList() << bindingContext);
    }

    void setContexts(QStringList const &contexts);

    // Events.
    bool handleEvent(de::Event const &event);

public:
    static InputBindingWidget *newTaskBarShortcut();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_INPUTBINDINGWIDGET_H
