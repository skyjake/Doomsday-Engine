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

#ifndef OPERATORRULE_H
#define OPERATORRULE_H

#include "rule.h"

class OperatorRule : public Rule
{
    Q_OBJECT

public:
    enum Operator {
        Equals,
        Negate,
        Half,
        Double,
        Sum,
        Subtract,
        Multiply,
        Divide
    };

public:
    explicit OperatorRule(Operator op, const Rule* unary, QObject *parent = 0);

    /**
     *  The operator rule takes ownership of the operands that have no parent.
     */
    explicit OperatorRule(Operator op, Rule* unary, QObject *parent = 0);

    explicit OperatorRule(Operator op, const Rule* left, const Rule* right, QObject *parent = 0);

    /**
     *  The operator rule takes ownership of the operands that have no parent.
     */
    explicit OperatorRule(Operator op, Rule* left, Rule* right, QObject *parent = 0);

protected:
    void update();
    void dependencyReplaced(const Rule* oldRule, const Rule* newRule);

private:
    Operator _operator;
    const Rule* _leftOperand;
    const Rule* _rightOperand;
};

#endif // OPERATORRULE_H
