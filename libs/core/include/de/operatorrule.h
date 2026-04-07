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

#ifndef LIBCORE_OPERATORRULE_H
#define LIBCORE_OPERATORRULE_H

#include "de/rule.h"
#include "de/constantrule.h"

namespace de {

/**
 * Calculates a value by applying a mathematical operator to the values of one
 * or two other rules.
 * @ingroup widgets
 */
class DE_PUBLIC OperatorRule : public Rule
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
        Floor,
        Select, ///< Negative selects left, positive selects right.
    };

public:
    OperatorRule(Operator op, const Rule &unary);

    OperatorRule(Operator op, const Rule &left, const Rule &right);

    OperatorRule(Operator op, const Rule &left, const Rule &right, const Rule &condition);       

public:
    static OperatorRule &maximum(const Rule &left, const Rule &right) {
        return *refless(new OperatorRule(Maximum, left, right));
    }

    static OperatorRule &maximum(const Rule &a, const Rule &b, const Rule &c) {
        return maximum(a, maximum(b, c));
    }

    static const Rule &maximum(const Rule &left, const Rule *rightOrNull) {
        if (rightOrNull) return *refless(new OperatorRule(Maximum, left, *rightOrNull));
        return left;
    }

    static OperatorRule &minimum(const Rule &left, const Rule &right) {
        return *refless(new OperatorRule(Minimum, left, right));
    }

    static OperatorRule &minimum(const Rule &a, const Rule &b, const Rule &c) {
        return minimum(a, minimum(b, c));
    }

    static OperatorRule &floor(const Rule &unary) {
        return *refless(new OperatorRule(Floor, unary));
    }

    static OperatorRule &clamped(const Rule &value, const Rule &low, const Rule &high) {
        return OperatorRule::minimum(OperatorRule::maximum(value, low), high);
    }

    static OperatorRule &select(const Rule &ifLessThanZero,
                                const Rule &ifGreaterThanOrEqualToZero,
                                const Rule &selection) {
        return *refless(new OperatorRule(Select,
                                         ifLessThanZero,
                                         ifGreaterThanOrEqualToZero,
                                         selection));
    }

protected:
    ~OperatorRule() override;

    inline Operator op() const { return Operator((_flags >> BaseFlagsShift) & 0xf); }

    void   update() override;
    String description() const override;

private:
    const Rule *_leftOperand;
    const Rule *_rightOperand;
    const Rule *_condition;
};

inline OperatorRule &operator + (const Rule &left, int right) {
    return *refless(new OperatorRule(OperatorRule::Sum, left, Const(right)));
}

inline OperatorRule &operator + (const Rule &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Sum, left, Constf(right)));
}

inline OperatorRule &operator + (const Rule &left, const Rule &right) {
    return *refless(new OperatorRule(OperatorRule::Sum, left, right));
}

inline OperatorRule &operator - (const Rule &unary) {
    return *refless(new OperatorRule(OperatorRule::Negate, unary));
}

inline OperatorRule &operator - (const Rule &left, int right) {
    return *refless(new OperatorRule(OperatorRule::Subtract, left, Const(right)));
}

inline OperatorRule &operator - (const Rule &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Subtract, left, Constf(right)));
}

inline OperatorRule &operator - (const Rule &left, const Rule &right) {
    return *refless(new OperatorRule(OperatorRule::Subtract, left, right));
}

inline OperatorRule &operator * (int left, const Rule &right) {
    if (left == 2) {
        return *refless(new OperatorRule(OperatorRule::Double, right));
    }
    return *refless(new OperatorRule(OperatorRule::Multiply, Const(left), right));
}

inline OperatorRule &operator * (const Rule &left, int right) {
    if (right == 2) {
        return *refless(new OperatorRule(OperatorRule::Double, left));
    }
    return *refless(new OperatorRule(OperatorRule::Multiply, left, Constf(right)));
}

inline OperatorRule &operator * (float left, const Rule &right) {
    return *refless(new OperatorRule(OperatorRule::Multiply, Constf(left), right));
}

inline OperatorRule &operator * (const Rule &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Multiply, left, Constf(right)));
}

inline OperatorRule &operator * (const Rule &left, const Rule &right) {
    return *refless(new OperatorRule(OperatorRule::Multiply, left, right));
}

inline OperatorRule &operator / (const Rule &left, int right) {
    if (right == 2) {
        return OperatorRule::floor(*refless(new OperatorRule(OperatorRule::Half, left)));
    }
    return OperatorRule::floor(*refless(new OperatorRule(OperatorRule::Divide, left, Const(right))));
}

inline OperatorRule &operator / (const Rule &left, dsize right) {
    if (right == 2) {
        return OperatorRule::floor(*refless(new OperatorRule(OperatorRule::Half, left)));
    }
    return OperatorRule::floor(*refless(new OperatorRule(OperatorRule::Divide, left, Constu(right))));
}

inline OperatorRule &operator / (const Rule &left, float right) {
    return *refless(new OperatorRule(OperatorRule::Divide, left, Constf(right)));
}

inline OperatorRule &operator / (const Rule &left, const Rule &right) {
    return *refless(new OperatorRule(OperatorRule::Divide, left, right));
}

template <typename RuleType>
inline void sumInto(const RuleType *&sum, const Rule &value) {
    if (!sum) { sum = holdRef(value); }
    else { changeRef(sum, *sum + value); }
}

template <typename RuleType>
inline void maxInto(const RuleType *&maximum, const Rule &value) {
    if (!maximum) { maximum = holdRef(value); }
    else { changeRef(maximum, OperatorRule::maximum(*maximum, value)); }
}

} // namespace de

#endif // LIBCORE_OPERATORRULE_H
