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

#include "de/constantrule.h"
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
    if (!fequal(_pendingValue, newValue))
    {
        _pendingValue = newValue;

        // Dependent values will need updating.
        invalidate();
    }
}

String ConstantRule::description() const
{
    return Stringf("%f", cachedValue());
}

const ConstantRule &ConstantRule::zero()
{
    static ConstantRule *zeroRule = nullptr;
    if (!zeroRule)
    {
        zeroRule = new ConstantRule(0); // won't be deleted ever
#ifdef DE_DEBUG
        Counted::totalCount--;
#endif
    }
    return *zeroRule;
}

void ConstantRule::update()
{
    setValue(_pendingValue);
}

} // namespace de
