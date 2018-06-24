/** @file mouseevent.h  Input event from a mouse.
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

#ifndef LIBGUI_MOUSEEVENT_H
#define LIBGUI_MOUSEEVENT_H

#include <de/Event>
#include <de/Vector>

#include "../gui/libgui.h"

namespace de {

/**
 * Input event from a mouse. @ingroup gui
 */
class LIBGUI_PUBLIC MouseEvent : public Event
{
public:
    enum MotionType { Absolute = 0, Relative = 1, Wheel = 2 };

    enum Button {
        Unknown  = -1,
        Left     = 0,
        Middle   = 1,
        Right    = 2,
        XButton1 = 3,
        XButton2 = 4
    };

    enum ButtonState {
        Released, ///< Released button.
        Pressed,  ///< Pressed button.
        DoubleClick
    };

    enum WheelMotion { FineAngle, Step };

public:
    MouseEvent();
    MouseEvent(MotionType motion, Vec2i const &pos);
    MouseEvent(WheelMotion wheelMotion, Vec2i const &wheel, Vec2i const &pos);
    MouseEvent(Button button, ButtonState state, Vec2i const &pos);

    MotionType   motion() const;
    Vec2i const &pos() const { return _pos; }
    WheelMotion  wheelMotion() const { return _wheelMotion; }
    Vec2i const &wheel() const { return _wheel; }
    Button       button() const { return _button; }
    ButtonState  state() const { return _state; }

    void setPos(Vec2i const &p) { _pos = p; }

private:
    Vec2i       _pos;
    WheelMotion _wheelMotion;
    Vec2i       _wheel;
    Button      _button;
    ButtonState _state;
};

} // namespace de

#endif // LIBGUI_MOUSEEVENT_H
