/** @file axisinputcontrol.h  Axis control for a logical input device.
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

#ifndef CLIENT_UI_AXISINPUTCONTROL_H
#define CLIENT_UI_AXISINPUTCONTROL_H

#include <de/legacy/types.h>
#include <de/string.h>
#include "inputdevice.h"

// Input device axis flags for use with cvars:
#define IDA_DISABLED    0x1  ///< Axis is always zero.
#define IDA_INVERT      0x2  ///< Real input data should be inverted.
#define IDA_RAW         0x4  ///< Do not smooth the input values; always use latest received value.

/**
 * Models an axis control on a "physical" input device (e.g., mouse along one axis).
 *
 * @ingroup ui
 */
class AxisInputControl : public InputDevice::Control
{
public:
    enum Type {
        Stick,   ///< Joysticks, gamepads
        Pointer  ///< Mouse
    };

public:
    /**
     * @param name  Symbolic name of the axis.
     * @param type  Logical axis type.
     */
    AxisInputControl(const de::String &name, Type type);
    virtual ~AxisInputControl();

    Type type() const;
    void setRawInput(bool yes = true);

    bool isActive() const;
    bool isInverted() const;

    /**
     * Returns the current position of the axis.
     */
    double position() const;
    void setPosition(double newPosition);

    void markPosition();
    double markedPosition() const;

    /**
     * Update the position of the axis control from a "real" position.
     *
     * @param newPosition  New position to be applied (maybe filtered, normalized, etc...).
     */
    void applyRealPosition(float newPosition);

    float translateRealPosition(float rawPosition) const;

    /**
     * Returns the current dead zone (0..1) limit for the axis.
     */
    float deadZone() const;
    void setDeadZone(float newDeadZone);

    /**
     * Returns the current position scaling factor (applied to "real" positions).
     */
    float scale() const;
    void setScale(float newScale);

    float offset() const;
    void setOffset(float newOffset);

    /**
     * When the state of the control last changed, in milliseconds since app init.
     */
    de::duint time() const;

    void update(timespan_t ticLength);

    de::String description() const;
    bool inDefaultState() const;
    void reset();
    void consoleRegister();

private:
    DE_PRIVATE(d)
};

#endif // CLIENT_UI_AXISINPUTCONTROL_H
