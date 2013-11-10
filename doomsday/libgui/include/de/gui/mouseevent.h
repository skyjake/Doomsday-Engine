/** @file mouseevent.h  Input event from a mouse.
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

#ifndef LIBGUI_MOUSEEVENT_H
#define LIBGUI_MOUSEEVENT_H

#include <de/Event>
#include <de/Vector>

#include "libgui.h"

namespace de {

/**
 * Input event from a mouse. @ingroup ui
 */
class LIBGUI_PUBLIC MouseEvent : public Event
{
public:
    enum MotionType
    {
        Absolute = 0,
        Relative = 1,
        Wheel    = 2
    };

    enum Button
    {
        Unknown  = -1,
        Left     = 0,
        Middle   = 1,
        Right    = 2,
        XButton1 = 3,
        XButton2 = 4
    };

    enum ButtonState
    {
        Released,       ///< Released button.
        Pressed         ///< Pressed button.
    };

    enum WheelMotion
    {
        FineAngle,
        Step
    };

public:
    MouseEvent();
    MouseEvent(MotionType motion, Vector2i const &pos);
    MouseEvent(WheelMotion wheelMotion, Vector2i const &wheel, Vector2i const &pos);
    MouseEvent(Button button, ButtonState state, Vector2i const &pos);

    MotionType motion() const;
    Vector2i const &pos() const { return _pos; }
    WheelMotion wheelMotion() const { return _wheelMotion; }
    Vector2i const &wheel() const { return _wheel; }
    Button button() const { return _button; }
    ButtonState state() const { return _state; }

    void setPos(Vector2i const &p) { _pos = p; }

private:
    Vector2i _pos;
    WheelMotion _wheelMotion;
    Vector2i _wheel;
    Button _button;
    ButtonState _state;
};

} // namespace de

#endif // LIBGUI_MOUSEEVENT_H
