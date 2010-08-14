/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
 
#include "de/Animator"
#include "de/App"
#include "de/math.h"
#include "de/Writer"
#include "de/Reader"

#include <cmath>

using namespace de;

Animator::Animator(ValueType initialValue)
    : _clock(0), _motion(EASE_OUT), _start(initialValue), _startTime(0), 
      _transition(0), _transitionTime(0), _observer(0)
{}

Animator::Animator(const IClock& clock, ValueType initialValue)
    : _clock(&clock), _motion(EASE_OUT), _start(initialValue), _startTime(0), 
      _transition(0), _transitionTime(0), _observer(0)
{}

Animator::Animator(const Animator& other)
    : _clock(other._clock), _motion(other._motion), 
      _start(other._start), 
      _startTime(other._startTime), 
      _transition(other._transition), 
      _transitionTime(other._transitionTime),
      _observer(0),
      _status(other._status)
{}

Animator::~Animator()
{}

void Animator::setClock(const IClock& clock)
{
    _clock = &clock;
}

void Animator::set(ValueType targetValue, const Time::Delta& transition)
{
    ValueType oldTarget = target();
    
    if(!_clock)
    {
        // Default to the application.        
        _clock = &App::app();
    }
    
    if(transition > 0.0)
    {
        _start = now();
        _startTime = _clock->now();
        _transition = targetValue - _start;
        _transitionTime = transition;
        _status.set(ANIMATING_BIT);
    }
    else
    {
        _start = targetValue;
        _startTime = _clock->now();
        _transition = 0;
        _transitionTime = 0;
        _status.set(ANIMATING_BIT, false);
    }

    // Let the observer know.
    if(_observer)
    {
        _observer->animatorValueSet(*this, oldTarget);
    }
}

Animator& Animator::operator = (const ValueType& immediatelyAssignedValue)
{
    set(immediatelyAssignedValue);
    return *this;
}

Animator::ValueType Animator::now() const
{
    if(!_status[ANIMATING_BIT])
    {
        return _start;
    }

    if(!_clock)
    {
        throw ClockMissingError("Animator::set", "Animator has no clock");
    }
    
    const Time& nowTime = _clock->now();
    ddouble t = (nowTime - _startTime) / _transitionTime;
    
    if(t >= 1.0)
    {
        // The animation is complete.
        _status.set(ANIMATING_BIT, false);
        _start += _transition;
        return _start;
    }
    
    switch(_motion)
    {
    case LINEAR:
        return _start + _transition * t;
        
    case EASE_OUT:
        return _start + _transition * std::sin(PI * t / 2.0);
        
    default:
        return _start;
    }
}

Animator::ValueType Animator::target() const
{
    return _start + _transition;
}

Animator Animator::operator - () const
{
    Animator inversed = Animator(*this);
    inversed._start = -inversed._start;
    inversed._transition = -inversed._transition;
    return inversed;
}

Animator Animator::operator * (ddouble scalar) const
{
    Animator scaled = Animator(*this);
    scaled._start = scalar * scaled._start;
    scaled._transition = scalar * scaled._transition;
    return scaled;
}

Animator Animator::operator / (ddouble scalar) const
{
    Animator scaled = Animator(*this);
    scaled._start = scaled._start / scalar;
    scaled._transition = scaled._transition / scalar;
    return scaled;
}

Animator Animator::operator + (const ValueType& offset) const
{
    Animator shifted = Animator(*this);
    shifted += offset;
    return shifted;
}

Animator Animator::operator - (const ValueType& offset) const
{
    Animator shifted = Animator(*this);
    shifted -= offset;
    return shifted;
}

Animator& Animator::operator += (const ValueType& offset)
{
    _start += offset;
    return *this;
}

Animator& Animator::operator -= (const ValueType& offset)
{
    _start -= offset;
    return *this;
}

bool Animator::operator < (const ValueType& offset) const
{
    return now() < offset;
}

bool Animator::operator > (const ValueType& offset) const
{
    return now() > offset;
}

void Animator::operator >> (Writer& to) const
{
    to << duint8(_motion) << dfloat(target()) << dfloat(_transitionTime);
}

void Animator::operator << (Reader& from)
{
    duint8 motionType;
    dfloat targetValue;
    float transTime;
    from >> motionType >> targetValue >> transTime;
    _motion = Motion(motionType);
    set(targetValue, transTime);
}

AnimatorVector2 AnimatorVector2::operator + (const Vector2<Animator::ValueType>& offset) const
{
    Animator shiftedX = x + offset.x;
    Animator shiftedY = y + offset.y;
    return AnimatorVector2(shiftedX, shiftedY);
}

AnimatorVector2 AnimatorVector2::operator - (const Vector2<Animator::ValueType>& offset) const
{
    Animator shiftedX = x - offset.x;
    Animator shiftedY = y - offset.y;
    return AnimatorVector2(shiftedX, shiftedY);
}

void AnimatorVector2::setObserver(Animator::IObserver* observer)
{
    x.setObserver(observer);
    y.setObserver(observer);
}

std::ostream& de::operator << (std::ostream& os, const Animator& anim)
{
    os << anim.now() << " (=> " << anim.target() << ")";
    return os;
}
