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

bool Rule::_invalidRulesExist = false;

struct Rule::Instance
{
    typedef QSet<Rule const *> Dependencies;
    Dependencies dependencies; // ref'd

    /// Current value of the rule.
    float value;

    /// The value is valid.
    bool isValid;

    Instance(float initialValue) : value(initialValue), isValid(true)
    {}

    ~Instance()
    {
        // Auto-release remaining references to dependencies.
        DENG2_FOR_EACH(Dependencies, i, dependencies)
        {
            Rule const *rule = *i;
            de::releaseRef(rule);
        }
    }
};

Rule::Rule(float initialValue) : d(new Instance(initialValue))
{}

Rule::~Rule()
{
    delete d;
}

float Rule::value() const
{
    if(!d->isValid)
    {
        // Force an update.
        const_cast<Rule *>(this)->update();
    }

    // It must be valid now, after the update.
    DENG2_ASSERT(d->isValid);

    return d->value;
}

void Rule::update()
{
    // This is a fixed value, so don't do anything.
    d->isValid = true;
}

void Rule::markRulesValid()
{
    _invalidRulesExist = false;
}

bool Rule::invalidRulesExist()
{
    return _invalidRulesExist;
}

float Rule::cachedValue() const
{
    return d->value;
}

void Rule::setValue(float v)
{
    d->value = v;
    d->isValid = true;
}

void Rule::dependsOn(Rule const *dependency)
{
    if(dependency)
    {
        DENG2_ASSERT(!d->dependencies.contains(dependency));
        d->dependencies.insert(de::holdRef(dependency));

        connect(dependency, SIGNAL(valueInvalidated()), this, SLOT(invalidate()));
    }
}

void Rule::independentOf(Rule const *dependency)
{
    if(dependency)
    {
        disconnect(dependency, SIGNAL(valueInvalidated()), this, SLOT(invalidate()));

        DENG2_ASSERT(d->dependencies.contains(dependency));
        d->dependencies.remove(dependency);
        de::releaseRef(dependency);
    }
}

void Rule::invalidate()
{
    if(d->isValid)
    {
        d->isValid = false;

        // Also set the global flag.
        Rule::_invalidRulesExist = true;

        emit valueInvalidated();
    }
}

} // namespace de
