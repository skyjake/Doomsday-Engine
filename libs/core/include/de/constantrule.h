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

#ifndef LIBCORE_CONSTANTRULE_H
#define LIBCORE_CONSTANTRULE_H

#include "de/rule.h"

namespace de {

/**
 * The value of a constant rule never changes unless manually changed.
 * @ingroup widgets
 *
 * @see AnimationRule
 */
class DE_PUBLIC ConstantRule : public Rule
{
public:
    /**
     * Utility template for constructing refless ConstantRule instances.
     * Instead of: <pre>   *refless(new ConstantRule(10))</pre> one can just
     * write: <pre>   Const(10)</pre> (see typedefs de::Const and de::Constf).
     */
    template <typename NumberType>
    class Builder
    {
    public:
        Builder(const NumberType &number) : _number(number) {}

        /**
         * Returns a new, refless ConstantRule instance with @a _number as
         * value. Caller is responsible for making sure a reference is taken
         * to the returned rule.
         */
        operator const Rule &() const;

        operator RefArg<Rule>() const;

    private:
        NumberType _number;
    };

public:
    ConstantRule();

    explicit ConstantRule(float constantValue);

    /**
     * Changes the value of the constant in the rule.
     */
    void set(float newValue);

    String description() const override;

public:
    static const ConstantRule &zero();

protected:
    void update() override;

private:
    float _pendingValue;
};

template <typename Type>
ConstantRule::Builder<Type>::operator const Rule &() const
{
    if (fequal(float(_number), 0)) return ConstantRule::zero();
    return *refless(new ConstantRule(_number));
}

template <typename Type>
ConstantRule::Builder<Type>::operator RefArg<Rule>() const
{
    if (fequal(float(_number), 0)) return RefArg<Rule>(ConstantRule::zero());
    return RefArg<Rule>(new ConstantRule(_number));
}

typedef ConstantRule::Builder<int32_t>  Const;
typedef ConstantRule::Builder<uint32_t> Constu;
typedef ConstantRule::Builder<float>    Constf;

} // namespace de

#endif // LIBCORE_CONSTANTRULE_H
