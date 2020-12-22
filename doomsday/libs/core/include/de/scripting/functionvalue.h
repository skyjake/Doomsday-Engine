/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_FUNCTIONVALUE_H
#define LIBCORE_FUNCTIONVALUE_H

#include "de/value.h"
#include "function.h"

namespace de {

/**
 * Holds a reference to a function and provides a way to call the function.
 *
 * @ingroup script
 */
class FunctionValue : public Value
{
public:
    FunctionValue();
    FunctionValue(Function *func);
    ~FunctionValue();

    /// Returns the function.
    const Function &function() const { return *_func; }

    Text typeId() const;
    Value *duplicate() const;
    Text asText() const;
    bool isTrue() const;
    bool isFalse() const;
    dint compare(const Value &value) const;
    void call(Process &process, const Value &arguments, Value *self) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Function *_func;
};

} // namespace de

#endif /* LIBCORE_FUNCTIONVALUE_H */
