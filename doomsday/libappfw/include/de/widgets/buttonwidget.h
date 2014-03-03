/** @file buttonwidget.h  Clickable button widget.
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

#ifndef LIBAPPFW_BUTTONWIDGET_H
#define LIBAPPFW_BUTTONWIDGET_H

#include <de/Action>
#include <de/Observers>

#include "../LabelWidget"

namespace de {

/**
 * Clickable button widget.
 *
 * @ingroup gui
 */
class LIBAPPFW_PUBLIC ButtonWidget : public LabelWidget
{
public:
    enum State {
        Up,
        Hover,
        Down
    };

    /**
     * Notified when the state of the button changes.
     */
    DENG2_DEFINE_AUDIENCE2(StateChange, void buttonStateChanged(ButtonWidget &button, State state))

    /**
     * Notified immediately before the button's action is to be triggered. Will
     * occur regardless of whether an action has been set.
     */
    DENG2_DEFINE_AUDIENCE2(Press, void buttonPressed(ButtonWidget &button))

    /**
     * Notified when the button's action is triggered (could be before or after
     * the action). Will not occur if no action has been defined for the
     * button.
     */
    DENG2_DEFINE_AUDIENCE2(Triggered, void buttonActionTriggered(ButtonWidget &button))

public:
    ButtonWidget(String const &name = "");

    enum HoverColorMode {
        ReplaceColor,
        ModulateColor
    };

    /**
     * Text color to use in the Hover state. The default is to use the normal text
     * color of the button (label).
     *
     * @param hoverTextId  Style color identifier.
     */
    void setHoverTextColor(DotPath const &hoverTextId, HoverColorMode mode = ModulateColor);

    void setBackgroundColor(DotPath const &bgColorId);

    /**
     * Sets the action of the button. It gets triggered when the button is
     * pressed.
     *
     * @param action  Action instance. Widget holds a reference.
     */
    void setAction(RefArg<Action> action);

    Action const *action() const;

    /**
     * Triggers the action of the button.
     */
    void trigger();

    State state() const;

    // Events.
    void update();
    bool handleEvent(Event const &event);


protected:
    void updateModelViewProjection(GLUniform &uMvp);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BUTTONWIDGET_H
