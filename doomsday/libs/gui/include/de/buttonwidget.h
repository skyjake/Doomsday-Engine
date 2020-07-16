/** @file buttonwidget.h  Clickable button widget.
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

#ifndef LIBAPPFW_BUTTONWIDGET_H
#define LIBAPPFW_BUTTONWIDGET_H

#include <de/action.h>
#include <de/observers.h>
#include <functional>

#include "de/labelwidget.h"

namespace de {

/**
 * Clickable button widget.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC ButtonWidget : public LabelWidget
{
public:
    enum State { Up, Hover, Down };
    enum HoverColorMode { ReplaceColor, ModulateColor };

    /**
     * Notified when the state of the button changes.
     */
    DE_AUDIENCE(StateChange, void buttonStateChanged(ButtonWidget &button, State state))

    /**
     * Notified immediately before the button's action is to be triggered. Will
     * occur regardless of whether an action has been set.
     */
    DE_AUDIENCE(Press, void buttonPressed(ButtonWidget &button))

    /**
     * Notified when the button's action is triggered (could be before or after
     * the action). Will not occur if no action has been defined for the
     * button.
     */
    DE_AUDIENCE(Triggered, void buttonActionTriggered(ButtonWidget &button))

public:
    ButtonWidget(const String &name = String());

    void useInfoStyle(bool yes = true);
    void useNormalStyle() { useInfoStyle(false); }
    bool isUsingInfoStyle() const;

    void       setColorTheme(ColorTheme theme);
    ColorTheme colorTheme() const;

    void setTextColor(const DotPath &colorId) override;

    /**
     * Text color to use in the Hover state. The default is to use the normal text
     * color of the button (label).
     *
     * @param hoverTextId  Style color identifier.
     * @param mode         Color hover behavior.
     */
    void setHoverTextColor(const DotPath &hoverTextId, HoverColorMode mode = ModulateColor);

    void setBackgroundColor(const DotPath &bgColorId);

    void setBorderColor(const DotPath &borderColorId);

    /**
     * Sets the action of the button. It gets triggered when the button is
     * pressed.
     *
     * @param action  Action instance. Widget holds a reference.
     */
    void setAction(RefArg<Action> action);

    /**
     * Sets the action of the button using a callback function. It gets called
     * when the button is pressed.
     *
     * @param callback  Callback function.
     *
     * @todo Rename back to setAction() when MSVC can understand that Action *
     * cannot be used to initialize a std::function<void ()>.
     */
    void setActionFn(std::function<void ()> callback);

    const Action *action() const;

    State state() const;
    void  setState(State state);

    int  shortcutKey() const;
    void setShortcutKey(int ddkey);
    bool handleShortcut(const KeyEvent &keyEvent);

    // Events.
    void update() override;
    bool handleEvent(const Event &event) override;

    /**
     * Triggers the action of the button.
     */
    void trigger();

protected:
    bool updateModelViewProjection(Mat4f &mvp) override;
    void updateStyle() override;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_BUTTONWIDGET_H
