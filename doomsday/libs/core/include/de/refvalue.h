/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_REFVALUE_H
#define LIBCORE_REFVALUE_H

#include "de/value.h"
#include "de/variable.h"

namespace de {

/**
 * References a Variable. Operations done on a RefValue are actually performed on
 * the variable's value.
 *
 * @ingroup data
 */
class DE_PUBLIC RefValue : public Value,
                              DE_OBSERVES(Variable, Deletion)
{
public:
    /// Attempt to dereference a NULL variable. @ingroup errors
    DE_ERROR(NullError);

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

    const Value &dereference() const;

    Text typeId() const;
    Value *duplicate() const;
    Number asNumber() const;
    Text asText() const;
    Record *memberScope() const;
    dsize size() const;
    const Value &element(const Value &index) const;
    Value &element(const Value &index);
    void setElement(const Value &index, Value *elementValue);
    bool contains(const Value &value) const;
    Value *begin();
    Value *next();
    bool isTrue() const;
    bool isFalse() const;
    dint compare(const Value &value) const;
    void negate();
    void sum(const Value &value);
    void subtract(const Value &subtrahend);
    void divide(const Value &divisor);
    void multiply(const Value &value);
    void modulo(const Value &divisor);
    void assign(Value *value);
    void call(Process &process, const Value &arguments, Value *self) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Observes Variable deletion.
    void variableBeingDeleted(Variable &variable);

public:
    Variable *_variable;
};

} // namespace de

#endif /* LIBCORE_REFVALUE_H */
