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

#include "de/Rule"
#include "de/math.h"
#include "de/PointerSet"

namespace de {

bool Rule::_invalidRulesExist = false;

DE_PIMPL_NOREF(Rule)
{
    typedef PointerSetT<Rule> Dependencies;
    Dependencies dependencies; // ref'd

    /// Current value of the rule.
    float value;

    /// The value is valid.
    bool isValid;

    Impl() : value(0), isValid(false)
    {}

    Impl(float initialValue) : value(initialValue), isValid(true)
    {}

    ~Impl()
    {
        DE_ASSERT(dependencies.isEmpty());
    }
};

Rule::Rule() : d(new Impl)
{}

Rule::Rule(float initialValue) : d(new Impl(initialValue))
{}

Rule::~Rule()
{}

float Rule::value() const
{
    if (!d->isValid)
    {
        // Force an update.
        const_cast<Rule *>(this)->update();
    }

    // It must be valid now, after the update.
    DE_ASSERT(d->isValid);

    return d->value;
}

int Rule::valuei() const
{
    return de::floor(value());
}

void Rule::update()
{
    // This is a fixed value, so don't do anything.
    d->isValid = true;
}

bool Rule::isValid() const
{
    return d->isValid;
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

void Rule::ruleInvalidated()
{
    // A dependency was invalidated, also invalidate this value.
    invalidate();
}

void Rule::setValue(float v)
{
    d->value = v;
    d->isValid = true;
}

void Rule::dependsOn(Rule const &dependency)
{
    DE_ASSERT(!d->dependencies.contains(&dependency));
    d->dependencies.insert(de::holdRef(&dependency));

    dependency.audienceForRuleInvalidation += this;
}

void Rule::dependsOn(Rule const *dependencyOrNull)
{
    if (dependencyOrNull) dependsOn(*dependencyOrNull);
}

void Rule::independentOf(Rule const &dependency)
{
    dependency.audienceForRuleInvalidation -= this;

    DE_ASSERT(d->dependencies.contains(&dependency));
    d->dependencies.remove(&dependency);
    dependency.release();
}

void Rule::independentOf(Rule const *dependencyOrNull)
{
    if (dependencyOrNull) independentOf(*dependencyOrNull);
}

void Rule::invalidate()
{
    if (d->isValid)
    {
        d->isValid = false;

        // Also set the global flag.
        Rule::_invalidRulesExist = true;

        DE_FOR_AUDIENCE(RuleInvalidation, i) i->ruleInvalidated();
    }
}

} // namespace de
