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

#ifndef LIBDENG2_OPERATORRULE_H
#define LIBDENG2_OPERATORRULE_H

#include "../Rule"

namespace de {

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
        Divide,
        Maximum,
        Minimum
    };

public:
    explicit OperatorRule(Operator op, Rule const *unary, QObject *parent = 0);

    /**
     *  The operator rule takes ownership of the operands that have no parent.
     */
    explicit OperatorRule(Operator op, Rule *unaryOwn, QObject *parent = 0);

    explicit OperatorRule(Operator op, Rule const *left, Rule const *right, QObject *parent = 0);
    explicit OperatorRule(Operator op, Rule const *left, Rule *rightOwn, QObject *parent = 0);
    explicit OperatorRule(Operator op, Rule *leftOwn, Rule const *right, QObject *parent = 0);

    /**
     *  The operator rule takes ownership of the operands that have no parent.
     */
    explicit OperatorRule(Operator op, Rule *leftOwn, Rule *rightOwn, QObject *parent = 0);

protected:
    void update();
    void dependencyReplaced(Rule const *oldRule, Rule const *newRule);

private:
    Operator _operator;
    Rule const *_leftOperand;
    Rule const *_rightOperand;
};

} // namespace de

#endif // LIBDENG2_OPERATORRULE_H
