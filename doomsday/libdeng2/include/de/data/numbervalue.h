/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_NUMBERVALUE_H
#define LIBDENG2_NUMBERVALUE_H

#include "../Value"

#include <QFlags>

namespace de {

/**
 * The NumberValue class is a subclass of Value that holds a single double
 * precision floating point number. All numbers are stored using ddouble.
 *
 * Note that all 32-bit integer values can be represented exactly with doubles,
 * however all 64-bit integers cannot be:
 * http://en.wikipedia.org/wiki/Floating_point#IEEE_754:_floating_point_in_modern_computers
 *
 * @ingroup data
 */
class DENG2_PUBLIC NumberValue : public Value
{
public:
    /// Truth values for logical operations. They are treated as
    /// number values.
    enum {
        False = 0,
        True  = 1
    };

    enum SemanticHint {
        Boolean = 0x1,      ///< The number is intended to be a boolean value.
        Generic = 0         ///< Generic number.
    };
    Q_DECLARE_FLAGS(SemanticHints, SemanticHint)

public:
    NumberValue(Number initialValue = 0, SemanticHints semantic = Generic);
    NumberValue(dsize initialSize);
    NumberValue(dint initialInteger);
    NumberValue(duint initialUnsignedInteger);
    NumberValue(bool initialBoolean);

    /**
     * Conversion template that forces a cast to another type.
     */
    template <typename Type>
    Type as() const { return Type(_value); }

    Value *duplicate() const;
    Number asNumber() const;
    Text asText() const;
    bool isTrue() const;
    dint compare(Value const &value) const;
    void negate();
    void sum(Value const &value);
    void subtract(Value const &value);
    void divide(Value const &divisor);
    void multiply(Value const &value);
    void modulo(Value const &divisor);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Number _value;
    SemanticHints _semantic;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(NumberValue::SemanticHints)

} // namespace de

#endif /* LIBDENG2_NUMBERVALUE_H */
