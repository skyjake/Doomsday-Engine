/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_REFVALUE_H
#define LIBDENG2_REFVALUE_H

#include "../Value"
#include "../Variable"

namespace de {

/**
 * References a Variable. Operations done on a RefValue are actually performed on
 * the variable's value.
 *
 * @ingroup data
 */
class DENG2_PUBLIC RefValue : public Value,
                              DENG2_OBSERVES(Variable, Deletion)
{
public:
    /// Attempt to dereference a NULL variable. @ingroup errors
    DENG2_ERROR(NullError);

public:
    /**
     * Constructs a new reference to a variable.
     *
     * @param variable  Variable.
     */
    RefValue(Variable *variable = 0);

    virtual ~RefValue();

    /**
     * Returns the variable this reference points to.
     */
    Variable *variable() const { return _variable; }

    void verify() const;

    Value &dereference();

    Value const &dereference() const;

    Value *duplicate() const;
    Number asNumber() const;
    Text asText() const;
    dsize size() const;
    Value const &element(Value const &index) const;
    Value &element(Value const &index);
    void setElement(Value const &index, Value *elementValue);
    bool contains(Value const &value) const;
    Value *begin();
    Value *next();
    bool isTrue() const;
    bool isFalse() const;
    dint compare(Value const &value) const;
    void negate();
    void sum(Value const &value);
    void subtract(Value const &subtrahend);
    void divide(Value const &divisor);
    void multiply(Value const &value);
    void modulo(Value const &divisor);
    void assign(Value *value);
    void call(Process &process, Value const &arguments) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Observes Variable deletion.
    void variableBeingDeleted(Variable &variable);

public:
    Variable *_variable;
};

} // namespace de

#endif /* LIBDENG2_REFVALUE_H */
