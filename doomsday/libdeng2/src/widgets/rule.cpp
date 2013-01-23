/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Rule"

namespace de {

Rule::Rule(float initialValue)
    : QObject(), _value(initialValue), _isValid(true)
{}

Rule::~Rule()
{
    // Release references to the dependent rules.
    DENG2_FOR_EACH(Dependents, i, _dependentRules)
    {
        (*i)->release();
    }
}

float Rule::value() const
{
    if(!_isValid)
    {
        // Force an update.
        const_cast<Rule *>(this)->update();
    }

    // It must be valid now, after the update.
    DENG2_ASSERT(_isValid);

    return _value;
}

void Rule::update()
{
    // This is a fixed value, so don't do anything.
    _isValid = true;
}

#if 0
void Rule::dependencyReplaced(Rule const *, Rule const *)
{
    // No dependencies.
}
#endif

float Rule::cachedValue() const
{
    return _value;
}

void Rule::setValue(float v)
{
    _value = v;
    _isValid = true;
}

#if 0
void Rule::transferDependencies(Rule *toRule)
{
    foreach(Rule *rule, _dependentRules)
    {
        // Disconnect from this rule.
        removeDependent(rule);

        // Connect to the new rule.
        toRule->addDependent(rule);

        rule->dependencyReplaced(this, toRule);
        rule->invalidate();
    }

    DENG2_ASSERT(_dependentRules.isEmpty());
}
#endif

void Rule::dependsOn(Rule const *dependency)
{
    DENG2_ASSERT(dependency != 0);

    const_cast<Rule *>(dependency)->addDependent(this);
}

void Rule::independentOf(Rule const *dependency)
{
    DENG2_ASSERT(dependency != 0);

    const_cast<Rule *>(dependency)->removeDependent(this);
}

void Rule::addDependent(Rule *rule)
{
    DENG2_ASSERT(!_dependentRules.contains(rule));

    //connect(rule, SIGNAL(destroyed(QObject *)), this, SLOT(ruleDestroyed(QObject *)));
    connect(this, SIGNAL(valueInvalidated()), rule, SLOT(invalidate()));

    // Acquire a reference.
    _dependentRules.insert(de::holdRef(rule));
}

void Rule::removeDependent(Rule *rule)
{
    DENG2_ASSERT(_dependentRules.contains(rule));

    disconnect(rule, SLOT(invalidate()));

    _dependentRules.remove(rule);
    de::releaseRef(rule);

    //rule->disconnect(this, SLOT(ruleDestroyed(QObject *)));
}

void Rule::invalidate()
{
    if(_isValid)
    {
        _isValid = false;
        emit valueInvalidated();
    }
}

#if 0
void Rule::ruleDestroyed(QObject *rule)
{
    removeDependent(static_cast<Rule *>(rule));
}
#endif

} // namespace de
