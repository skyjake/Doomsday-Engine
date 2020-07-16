/*
 * The Doomsday Engine Project
 *
 * Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/animationrule.h"
#include "de/clock.h"

namespace de {

AnimationRule::AnimationRule(float initialValue, Animation::Style style)
    : Rule(initialValue)
    , _animation(initialValue, style)
    , _targetRule(nullptr)
{
    _flags |= Singleshot;
}

AnimationRule::AnimationRule(const Rule &target, TimeSpan transition, Animation::Style style)
    : Rule(target.value())
    , _animation(target.value(), style)
    , _targetRule(nullptr)
{
    _flags |= RestartWhenTargetChanges | DontAnimateFromZero;
    set(target, transition);
}

AnimationRule::~AnimationRule()
{
    independentOf(_targetRule);
}

void AnimationRule::set(float target, TimeSpan transition, TimeSpan delay)
{
    independentOf(_targetRule);
    _targetRule = 0;
    _animation.clock().audienceForPriorityTimeChange += this;
    _animation.setValue(target, transition, delay);
    invalidate();
}

void AnimationRule::set(const Rule &target, TimeSpan transition, TimeSpan delay)
{
    set(target.value(), transition, delay);

    // Keep a reference.
    _targetRule = &target;
    dependsOn(_targetRule);
}

void AnimationRule::setStyle(Animation::Style style)
{
    _animation.setStyle(style);
}

void AnimationRule::setStyle(Animation::Style style, float bounceSpring)
{
    _animation.setStyle(style, bounceSpring);
}

void AnimationRule::setBehavior(Behaviors behavior)
{
    _flags &= ~FlagMask;
    _flags |= behavior;
}

AnimationRule::Behaviors AnimationRule::behavior() const
{
    return Behaviors(_flags & FlagMask);
}

void AnimationRule::shift(float delta)
{
    _animation.shift(delta);
    invalidate();
}

void AnimationRule::finish()
{
    _animation.finish();
}

void AnimationRule::pause()
{
    _animation.pause();
}

void AnimationRule::resume()
{
    _animation.resume();
}

String AnimationRule::description() const
{
    DE_ASSERT(!isValid() || fequal(value(), _animation));
    
    String desc = _animation.asText();
    if (_targetRule)
    {
        desc += "=>" + _targetRule->description();
    }
    return desc;
}

void AnimationRule::update()
{
    // When using a rule for the target, keep it updated.
    if (_targetRule)
    {
        if ((_flags & Singleshot) || !_animation.done())
        {
            _animation.adjustTarget(_targetRule->value());
        }
        else
        {
            // Start a new animation with the previously used transition time.
            if (!fequal(_animation.target(), _targetRule->value()))
            {
                TimeSpan span = _animation.transitionTime();
                if ((_flags & DontAnimateFromZero) && fequal(_animation.target(), 0))
                {
                    span = 0.0;
                }
                _animation.setValue(_targetRule->value(), span);
                _animation.clock().audienceForPriorityTimeChange += this;
            }
        }
    }

    setValue(_animation);

    if (_animation.done())
    {
        _animation.clock().audienceForPriorityTimeChange -= this;
    }
}

void AnimationRule::timeChanged(const Clock &)
{
    invalidate();
}

} // namespace de
