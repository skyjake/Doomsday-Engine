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

#include "operatorrule.h"

OperatorRule::OperatorRule(Operator op, Rule* unary, QObject* parent)
    : Rule(parent), _operator(op), _leftOperand(unary), _rightOperand(0)
{
    Q_ASSERT(_leftOperand != 0);

    setup();
}

OperatorRule::OperatorRule(Operator op, Rule* left, Rule* right, QObject* parent)
    : Rule(parent), _operator(op), _leftOperand(left), _rightOperand(right)
{
    Q_ASSERT(_leftOperand != 0);
    Q_ASSERT(_rightOperand != 0);

    setup();
}

void OperatorRule::setup()
{
    if(!_leftOperand->parent())
    {
        _leftOperand->setParent(this);
    }
    dependsOn(_leftOperand);

    if(_rightOperand)
    {
        if(!_rightOperand->parent())
        {
            _rightOperand->setParent(this);
        }
        dependsOn(_rightOperand);
    }
}

void OperatorRule::update()
{
    float leftValue = (_leftOperand? _leftOperand->value() : 0);
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
    }

    setValue(v);
}

void OperatorRule::dependencyReplaced(Rule* oldRule, Rule* newRule)
{
    if(_leftOperand == oldRule)
    {
        _leftOperand = newRule;
    }
    if(_rightOperand == oldRule)
    {
        _rightOperand = newRule;
    }
    invalidate();
}
