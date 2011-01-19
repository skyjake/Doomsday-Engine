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

#include "rule.h"
#include "visual.h"
#include <QDebug>

Rule::Rule(QObject* parent)
    : QObject(parent), _value(0), _isValid(true)
{}

Rule::Rule(float initialValue, QObject* parent)
    : QObject(parent), _value(initialValue), _isValid(true)
{}

Rule::~Rule()
{
    // Notify of a dependency going away.
    foreach(Rule* rule, _dependentRules)
    {
        rule->dependencyReplaced(this, 0);
    }
}

float Rule::value() const
{
    if(!_isValid)
    {
        // Force an update.
        const_cast<Rule*>(this)->update();
    }

    Q_ASSERT(_isValid);
    return _value;
}

/*
Visual* Rule::visual() const
{
    return dynamic_cast<Visual*>(parent());
}
*/

void Rule::update()
{
    // This is a fixed value, so don't do anything.
    _isValid = true;
}

void Rule::dependencyReplaced(const Rule* /*oldRule*/, const Rule* /*newRule*/)
{
    // No dependencies.
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

void Rule::replace(Rule* newRule)
{
    foreach(Rule* rule, _dependentRules)
    {
        // Disconnect from this rule.
        removeDependent(rule);

        // Connect to the new rule.
        newRule->addDependent(rule);

        rule->dependencyReplaced(this, newRule);
        rule->invalidate();
    }

    Q_ASSERT(_dependentRules.isEmpty());
}

void Rule::dependsOn(const Rule* dependency)
{
    Q_ASSERT(dependency != 0);

    const_cast<Rule*>(dependency)->addDependent(this);
}

void Rule::addDependent(Rule* rule)
{
    Q_ASSERT(!_dependentRules.contains(rule));

    connect(rule, SIGNAL(destroyed(QObject*)), this, SLOT(ruleDestroyed(QObject*)));
    connect(this, SIGNAL(valueInvalidated()), rule, SLOT(invalidate()));

    _dependentRules.insert(rule);
}

void Rule::removeDependent(Rule* rule)
{
    Q_ASSERT(_dependentRules.contains(rule));

    _dependentRules.remove(rule);

    disconnect(rule, SLOT(invalidate()));
    rule->disconnect(this, SLOT(ruleDestroyed(QObject*)));
}

void Rule::invalidate()
{
    if(_isValid)
    {
        _isValid = false;
        emit valueInvalidated();
    }
}

void Rule::ruleDestroyed(QObject *rule)
{
    removeDependent(static_cast<Rule*>(rule));
}

void Rule::claim(Rule* child)
{
    if(!child->parent())
    {
        child->setParent(this);
    }
}
