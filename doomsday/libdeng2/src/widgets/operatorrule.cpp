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

#include "de/OperatorRule"
#include "de/math.h"

namespace de {

OperatorRule::OperatorRule(Operator op, Rule const *unary)
    : Rule(), _operator(op), _leftOperand(unary), _rightOperand(0)
{
    DENG2_ASSERT(_leftOperand != 0);

    dependsOn(_leftOperand);
    invalidate();
}

OperatorRule::OperatorRule(Operator op, Rule const *left, Rule const *right)
    : Rule(), _operator(op), _leftOperand(left), _rightOperand(right)
{
    DENG2_ASSERT(_leftOperand != 0);
    DENG2_ASSERT(_rightOperand != 0);

    dependsOn(_leftOperand);
    dependsOn(_rightOperand);
    invalidate();
}

OperatorRule::~OperatorRule()
{
    independentOf(_leftOperand);
    independentOf(_rightOperand);
}

void OperatorRule::update()
{
    float leftValue  = (_leftOperand?  _leftOperand->value()  : 0);
    float rightValue = (_rightOperand? _rightOperand->value() : 0);
    float v = leftValue;

    switch(_operator)
    {
    case Equals:
        v = leftValue;
        break;

    case Negate:
        v = -leftValue;
        break;

    case Half:
        v = leftValue / 2;
        break;

    case Double:
        v = leftValue * 2;

    case Sum:
        v = leftValue + rightValue;
        break;

    case Subtract:
        v = leftValue - rightValue;
        break;

    case Multiply:
        v = leftValue * rightValue;
        break;

    case Divide:
        v = leftValue / rightValue;
        break;

    case Maximum:
        v = de::max(leftValue, rightValue);
        break;

    case Minimum:
        v = de::min(leftValue, rightValue);
        break;
    }

    setValue(v);
}

} // namespace de
