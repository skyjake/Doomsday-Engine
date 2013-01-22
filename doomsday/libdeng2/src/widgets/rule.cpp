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

Rule::Rule(QObject *parent)
    : QObject(parent), _value(0), _isValid(true)
{}

Rule::Rule(float initialValue, QObject *parent)
    : QObject(parent), _value(initialValue), _isValid(true)
{}

Rule::~Rule()
{
    // Notify of a dependency going away.
    foreach(Rule *rule, _dependentRules)
    {
        rule->dependencyReplaced(this, 0);
    }
}

float Rule::value() const
{
    if(!_isValid)
    {
        // Force an update.
        const_cast<Rule *>(this)->update();
    }
    DENG2_ASSERT(_isValid);

    return _value;
}

void Rule::update()
{
    // This is a fixed value, so don't do anything.
    _isValid = true;
}

bool Rule::isValid() const
{
    return _isValid;
}

void Rule::dependencyReplaced(Rule const *, Rule const *)
{
    // No dependencies.
}

void Rule::invalidateSilently()
{
    _isValid = false;
}

float Rule::cachedValue() const
{
    return _value;
}

void Rule::setValue(float v)
{
    _value = v;
    _isValid = true;
}

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

    connect(rule, SIGNAL(destroyed(QObject *)), this, SLOT(ruleDestroyed(QObject *)));
    connect(this, SIGNAL(valueInvalidated()), rule, SLOT(invalidate()));

    _dependentRules.insert(rule);
}

void Rule::removeDependent(Rule *rule)
{
    DENG2_ASSERT(_dependentRules.contains(rule));

    _dependentRules.remove(rule);

    disconnect(rule, SLOT(invalidate()));
    rule->disconnect(this, SLOT(ruleDestroyed(QObject *)));
}

void Rule::invalidate()
{
    if(_isValid)
    {
        invalidateSilently();
        emit valueInvalidated();
    }
}

void Rule::ruleDestroyed(QObject *rule)
{
    removeDependent(static_cast<Rule *>(rule));
}

bool Rule::claim(Rule *child)
{
    if(!child->parent())
    {
        child->setParent(this);
    }
    return (child->parent() == this);
}

} // namespace de
