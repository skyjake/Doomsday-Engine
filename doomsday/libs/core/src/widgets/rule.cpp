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

#include "de/rule.h"
#include "de/math.h"

namespace de {

bool Rule::_invalidRulesExist = false;

float Rule::value() const
{
    if (!(_flags & Valid))
    {
        // Force an update.
        const_cast<Rule *>(this)->update();
    }

    // It must be valid now, after the update.
    DE_ASSERT(isValid());

    return _value;
}

void Rule::markRulesValid()
{
    _invalidRulesExist = false;
}

bool Rule::invalidRulesExist()
{
    return _invalidRulesExist;
}

void Rule::ruleInvalidated()
{
    // A dependency was invalidated, also invalidate this value.
    invalidate();
}

void Rule::dependsOn(const Rule &dependency)
{
    DE_ASSERT(!_dependencies.contains(&dependency));
    _dependencies.insert(holdRef(&dependency));

    dependency.audienceForRuleInvalidation += this;
}

void Rule::dependsOn(const Rule *dependencyOrNull)
{
    if (dependencyOrNull) dependsOn(*dependencyOrNull);
}

void Rule::independentOf(const Rule &dependency)
{
    dependency.audienceForRuleInvalidation -= this;

    DE_ASSERT(_dependencies.contains(&dependency));
    _dependencies.remove(&dependency);
    dependency.release();
}

void Rule::independentOf(const Rule *dependencyOrNull)
{
    if (dependencyOrNull) independentOf(*dependencyOrNull);
}

void Rule::invalidate()
{
    if (isValid())
    {
        _flags &= ~Valid;

        // Also set the global flag.
        Rule::_invalidRulesExist = true;

        DE_NOTIFY_VAR(RuleInvalidation, i) i->ruleInvalidated();
    }
}

} // namespace de
