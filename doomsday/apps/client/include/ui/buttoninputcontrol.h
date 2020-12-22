/** @file buttoninputcontrol.h  Button control for a logical input device.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_UI_BUTTONINPUTCONTROL_H
#define CLIENT_UI_BUTTONINPUTCONTROL_H

#include <de/string.h>
#include "inputdevice.h"

/**
 * Models a button control on a "physical" input device (e.g., key on a keyboard).
 *
 * @ingroup ui
 */
class ButtonInputControl : public InputDevice::Control
{
public:
    explicit ButtonInputControl(const de::String &name = de::String());
    virtual ~ButtonInputControl();

    /**
     * Returns @c true if the button is currently in the down (i.e., pressed) state.
     */
    bool isDown() const;

    /**
     * Change the "down" state of the button.
     */
    void setDown(bool yes);

    /**
     * When the state of the control last changed, in milliseconds since app init.
     */
    de::duint time() const;

    de::String description() const;
    bool inDefaultState() const;
    void reset();

private:
    bool _isDown = false; ///< @c true= currently depressed.
    de::duint _time = 0;
};

#endif // CLIENT_UI_BUTTONINPUTCONTROL_H
