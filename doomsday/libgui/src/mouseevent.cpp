/** @file mouseevent.cpp
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

#include "de/MouseEvent"

namespace de {

MouseEvent::MouseEvent()
    : Event(MouseButton),
      _wheelMotion(FineAngle),
      _button(Unknown),
      _state(Released)
{}

MouseEvent::MouseEvent(MotionType motion, Vector2i const &pos)
    : Event(motion == Absolute? MousePosition :
            motion == Relative? MouseMotion :
                                MouseWheel),
      _pos(pos),
      _wheelMotion(FineAngle),
      _button(Unknown),
      _state(Released)
{
    if(motion == Wheel)
    {
        _pos = Vector2i();
        _wheel = pos;
    }
}

MouseEvent::MouseEvent(WheelMotion wheelMotion, Vector2i const &wheel, Vector2i const &pos)
    : Event(MouseWheel),
      _pos(pos),
      _wheelMotion(wheelMotion),
      _wheel(wheel),
      _button(Unknown),
      _state(Released)
{}

MouseEvent::MouseEvent(Button button, ButtonState state, Vector2i const &pos)
    : Event(MouseButton),
      _pos(pos),
      _button(button),
      _state(state)
{}

MouseEvent::MotionType MouseEvent::motion() const
{
    return type() == MousePosition? Absolute :
           type() == MouseMotion?   Relative :
                                    Wheel;
}

} // namespace de
