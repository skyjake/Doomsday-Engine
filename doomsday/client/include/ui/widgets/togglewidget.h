/** @file togglewidget.h  Toggle widget.
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

#ifndef DENG_CLIENT_TOGGLEWIDGET_H
#define DENG_CLIENT_TOGGLEWIDGET_H

#include "buttonwidget.h"

/**
 * Toggle is a specialized button that maintains an on/off state in addition to
 * the state of a ButtonWidget.
 */
class ToggleWidget : public ButtonWidget
{
public:
    enum ToggleState {
        Active,
        Inactive
    };

    /**
     * Audience to be notified whenever the toggle is toggled.
     */
    DENG2_DEFINE_AUDIENCE(Toggle, void toggleStateChanged(ToggleWidget &toggle))

public:
    ToggleWidget(de::String const &name = "");

    /**
     * Sets the toggle state of the widget.
     */
    void setToggleState(ToggleState state, bool notify = true);

    void setActive(bool activate)     { setToggleState(activate?   Active   : Inactive); }
    void setInactive(bool deactivate) { setToggleState(deactivate? Inactive : Active  ); }

    ToggleState toggleState() const;

    bool isActive() const   { return toggleState() == Active;   }
    bool isInactive() const { return toggleState() == Inactive; }

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_TOGGLEWIDGET_H
