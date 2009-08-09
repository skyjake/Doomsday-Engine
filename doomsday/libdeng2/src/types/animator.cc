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
    : clock_(&App::app()), motion_(EASE_OUT), start_(initialValue), startTime_(0), 
      transition_(0), transitionTime_(0), observer_(0)
{}

Animator::Animator(const IClock& clock, ValueType initialValue)
    : clock_(&clock), motion_(EASE_OUT), start_(initialValue), startTime_(0), 
      transition_(0), transitionTime_(0), observer_(0)
{}

Animator::Animator(const Animator& other)
    : clock_(other.clock_), motion_(other.motion_), 
      start_(other.start_), 
      startTime_(other.startTime_), 
      transition_(other.transition_), 
      transitionTime_(other.transitionTime_),
      observer_(0),
      status_(other.status_)
{}

Animator::~Animator()
{}

void Animator::setClock(const IClock& clock)
{
    clock_ = &clock;
}

void Animator::set(ValueType targetValue, const Time::Delta& transition)
{
    ValueType oldTarget = target();
    
    if(!clock_)
    {
        // Default to the application.        
        clock_ = &App::app();
    }
    
    if(transition > 0.0)
    {
        start_ = now();
        startTime_ = clock_->now();
        transition_ = targetValue - start_;
        transitionTime_ = transition;
        status_.set(ANIMATING);
    }
    else
    {
        start_ = targetValue;
        startTime_ = clock_->now();
        transition_ = 0;
        transitionTime_ = 0;
        status_.set(ANIMATING, false);
    }

    // Let the observer know.
    if(observer_)
    {
        observer_->animatorValueSet(*this, oldTarget);
    }
}

Animator& Animator::operator = (const ValueType& immediatelyAssignedValue)
{
    set(immediatelyAssignedValue);
    return *this;
}

Animator::ValueType Animator::now() const
{
    if(!status_.test(ANIMATING))
    {
        return start_;
    }

    if(!clock_)
    {
        throw ClockMissingError("Animator::set", "Animator has no clock");
    }
    
    const Time& nowTime = clock_->now();
    ddouble t = (nowTime - startTime_) / transitionTime_;
    
    if(t >= 1.0)
    {
        // The animation is complete.
        status_.set(ANIMATING, false);
        start_ += transition_;
        return start_;
    }
    
    switch(motion_)
    {
    case LINEAR:
        return start_ + transition_ * t;
        
    case EASE_OUT:
        return start_ + transition_ * std::sin(PI * t / 2.0);
        
    default:
        return start_;
    }
}

Animator::ValueType Animator::target() const
{
    return start_ + transition_;
}

Animator Animator::operator - () const
{
    Animator inversed = Animator(*this);
    inversed.start_ = -inversed.start_;
    inversed.transition_ = -inversed.transition_;
    return inversed;
}

Animator Animator::operator * (ddouble scalar) const
{
    Animator scaled = Animator(*this);
    scaled.start_ = scalar * scaled.start_;
    scaled.transition_ = scalar * scaled.transition_;
    return scaled;
}

Animator Animator::operator / (ddouble scalar) const
{
    Animator scaled = Animator(*this);
    scaled.start_ = scaled.start_ / scalar;
    scaled.transition_ = scaled.transition_ / scalar;
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
    start_ += offset;
    return *this;
}

Animator& Animator::operator -= (const ValueType& offset)
{
    start_ -= offset;
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
    to << duint8(motion_) << dfloat(target()) << dfloat(transitionTime_);
}

void Animator::operator << (Reader& from)
{
    duint8 motionType;
    dfloat targetValue;
    float transTime;
    from >> motionType >> targetValue >> transTime;
    motion_ = Motion(motionType);
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
