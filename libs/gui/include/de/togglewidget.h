/** @file togglewidget.h  Toggle widget.
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

#ifndef LIBAPPFW_TOGGLEWIDGET_H
#define LIBAPPFW_TOGGLEWIDGET_H

#include "de/buttonwidget.h"

namespace de {

/**
 * Toggle is a specialized button that maintains an on/off state in addition to
 * the state of a ButtonWidget.
 *
 * @ingroup guiWidgets
 */
class LIBGUI_PUBLIC ToggleWidget : public ButtonWidget
{
public:
    enum Flag {
        DefaultFlags     = 0,
        WithoutIndicator = 0x1,
    };

    enum ToggleState { Active, Inactive };

    /**
     * Audience to be notified whenever the toggle is toggled.
     */
    DE_AUDIENCE(Toggle, void toggleStateChanged(ToggleWidget &toggle))

    DE_AUDIENCE(UserToggle, void toggleStateChangedByUser(ToggleState active))

public:
    ToggleWidget(const Flags &flags = DefaultFlags, const String &name = String());

    /**
     * Sets the toggle state of the widget.
     */
    void setToggleState(ToggleState state, bool notify = true);

    void setActive(bool activate)     { setToggleState(activate?   Active   : Inactive); }
    void setInactive(bool deactivate) { setToggleState(deactivate? Inactive : Active  ); }

    ToggleState toggleState() const;

    bool isActive() const   { return toggleState() == Active;   }
    bool isInactive() const { return toggleState() == Inactive; }

    /**
     * Completes an ongoing state change animation.
     */
    void finishAnimation();

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_TOGGLEWIDGET_H
