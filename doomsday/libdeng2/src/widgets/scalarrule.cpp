/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ScalarRule"
#include "de/Clock"
#include <QDebug>

namespace de {

ScalarRule::ScalarRule(float initialValue, QObject *parent)
    : Rule(initialValue, parent), _animation(initialValue), _rule(0), _ruleOwned(false)
{}

void ScalarRule::set(float target, de::TimeDelta transition)
{
    dismissTargetRule();

    connect(&_animation.clock(), SIGNAL(timeChanged()), this, SLOT(timeChanged()));
    _animation.setValue(target, transition);
    invalidate();
}

void ScalarRule::set(Rule const *target, TimeDelta transition)
{
    set(target->value(), transition);

    _ruleOwned = false;
    _rule = target;
    dependsOn(_rule);
}

void ScalarRule::set(Rule *targetOwn, TimeDelta transition)
{
    set(targetOwn->value(), transition);

    _ruleOwned = claim(targetOwn);
    _rule = targetOwn;
    dependsOn(_rule);
}

void ScalarRule::update()
{
    setValue(_animation);
}

void ScalarRule::dependencyReplaced(Rule const *oldRule, Rule const *newRule)
{
    if(oldRule == _rule)
    {
        dismissTargetRule();
        oldRule = newRule;
    }
}

void ScalarRule::dismissTargetRule()
{
    if(_rule)
    {
        independentOf(_rule);
        if(_ruleOwned) delete _rule;
        _rule = 0;
    }
    _ruleOwned = false;
}

void ScalarRule::timeChanged()
{
    if(!_animation.done() || cachedValue() != _animation.value())
    {
        invalidate();
    }
    if(_animation.done())
    {
        disconnect(this, SLOT(timeChanged()));
    }
}

} // namespace de
