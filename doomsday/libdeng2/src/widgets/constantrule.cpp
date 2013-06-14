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

#include "de/ConstantRule"
#include "de/math.h"

namespace de {

ConstantRule::ConstantRule() : Rule(), _pendingValue(0)
{
    // No valid value defined.
}

ConstantRule::ConstantRule(float constantValue)
    : Rule(constantValue), _pendingValue(constantValue)
{}

void ConstantRule::set(float newValue)
{
    _pendingValue = newValue;

    // Dependent values will need updating.
    invalidate();
}

String ConstantRule::description() const
{
    return String("Constant(%1)").arg(cachedValue());
}

void ConstantRule::update()
{
    setValue(_pendingValue);
}

} // namespace de
