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

#ifndef LIBDENG2_CONSTANTRULE_H
#define LIBDENG2_CONSTANTRULE_H

#include "../Rule"

namespace de {

/**
 * The value of a constant rule never changes unless manually changed.
 * @ingroup widgets
 *
 * @see ScalarRule
 */
class DENG2_PUBLIC ConstantRule : public Rule
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
        Builder(NumberType const &number) : _number(number) {}

        /**
         * Returns a new, refless ConstantRule instance with @a _number as
         * value. Caller is responsible for making sure a reference is taken
         * to the returned rule.
         */
        operator Rule const & () const;

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

protected:
    void update();

private:
    float _pendingValue;
};

template <typename Type>
ConstantRule::Builder<Type>::operator Rule const & () const {
    return *refless(new ConstantRule(_number));
}

typedef ConstantRule::Builder<int>   Const;
typedef ConstantRule::Builder<float> Constf;

} // namespace de

#endif // LIBDENG2_CONSTANTRULE_H
