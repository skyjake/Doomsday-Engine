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

#include "de/DerivedRule"

namespace de {

DerivedRule::DerivedRule(Rule const *source)
    : ConstantRule(0), _source(source)
{
    DENG2_ASSERT(source != 0);

    dependsOn(source);
    invalidate();
}

void DerivedRule::update()
{
    DENG2_ASSERT(_source != 0);

    // The value gets updated by the source.
    const_cast<Rule *>(_source)->update();

    ConstantRule::update();
}

/*
void DerivedRule::dependencyReplaced(Rule const *oldRule, Rule const *newRule)
{
    if(_source == oldRule)
    {
        _source = newRule;
    }
}
*/

} // namespace de
