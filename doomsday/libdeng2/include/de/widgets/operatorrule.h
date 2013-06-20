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
#include "../ConstantRule"

namespace de {

/**
 * Calculates a value by applying a mathematical operator to the values of one
 * or two other rules.
 * @ingroup widgets
 */
class DENG2_PUBLIC OperatorRule : public Rule
{
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
        Minimum,
        Floor
    };

public:
    OperatorRule(Operator op, Rule const &unary);

    OperatorRule(Operator op, Rule const &left, Rule const &right);

public:
    static OperatorRule &maximum(Rule const &left, Rule const &right) {
        return *refless(new OperatorRule(Maximum, left, right));
    }

    static OperatorRule &minimum(Rule const &left, Rule const &right) {
        return *refless(new OperatorRule(Minimum, left, right));
    }

    static OperatorRule &floor(Rule const &unary) {
        return *refless(new OperatorRule(Floor, unary));
    }

    static OperatorRule &clamped(Rule const &value, Rule const &low, Rule const &high) {
        return OperatorRule::minimum(OperatorRule::maximum(value, low), high);
    }

protected:
    ~OperatorRule();

    void update();

    String description() const;

private:
    Operator _operator;
    Rule const *_leftOperand;
    Rule const *_rightOperand;
};

inline OperatorRule &operator + (Rule const &left, int right) {
    return *refless(new OperatorRule(OperatorRule::Sum, left, Const(right)));
}

inline OperatorRule &operator + (Rule const &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Sum, left, Constf(right)));
}

inline OperatorRule &operator + (Rule const &left, Rule const &right) {
    return *refless(new OperatorRule(OperatorRule::Sum, left, right));
}

inline OperatorRule &operator - (Rule const &unary) {
    return *refless(new OperatorRule(OperatorRule::Negate, unary));
}

inline OperatorRule &operator - (Rule const &left, int right) {
    return *refless(new OperatorRule(OperatorRule::Subtract, left, Const(right)));
}

inline OperatorRule &operator - (Rule const &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Subtract, left, Constf(right)));
}

inline OperatorRule &operator - (Rule const &left, Rule const &right) {
    return *refless(new OperatorRule(OperatorRule::Subtract, left, right));
}

inline OperatorRule &operator * (int left, Rule const &right) {
    if(left == 2) {
        return *refless(new OperatorRule(OperatorRule::Double, right));
    }
    return *refless(new OperatorRule(OperatorRule::Multiply, Const(left), right));
}

inline OperatorRule &operator * (Rule const &left, int right) {
    if(right == 2) {
        return *refless(new OperatorRule(OperatorRule::Double, left));
    }
    return *refless(new OperatorRule(OperatorRule::Multiply, left, Constf(right)));
}

inline OperatorRule &operator * (float left, Rule const &right) {
    return *refless(new OperatorRule(OperatorRule::Multiply, Constf(left), right));
}

inline OperatorRule &operator * (Rule const &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Multiply, left, Constf(right)));
}

inline OperatorRule &operator * (Rule const &left, Rule const &right) {
    return *refless(new OperatorRule(OperatorRule::Multiply, left, right));
}

inline OperatorRule &operator / (Rule const &left, int right) {
    if(right == 2) {
        return OperatorRule::floor(*refless(new OperatorRule(OperatorRule::Half, left)));
    }
    return OperatorRule::floor(*refless(new OperatorRule(OperatorRule::Divide, left, Const(right))));
}

inline OperatorRule &operator / (Rule const &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Divide, left, Constf(right)));
}

inline OperatorRule &operator / (Rule const &left, Rule const &right) {
    return *refless(new OperatorRule(OperatorRule::Divide, left, right));
}

} // namespace de

#endif // LIBDENG2_OPERATORRULE_H
