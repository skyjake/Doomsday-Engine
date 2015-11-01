/** @file nativevalue.h  Value representing a native object.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_NATIVEVALUE_H
#define LIBDENG2_NATIVEVALUE_H

#include "../Value"
#include "../Record"
#include "../Deletable"

namespace de {

/**
 * Reference to a native object. Only stores a pointer, and observes the deletion of the
 * referenced object.
 *
 * The referenced objects must be derived from Deletable, because scripts may duplicate
 * values and the values may get copied into any Variable. All NativeValue instances
 * referencing a native object must be changed to point to @c nullptr if the native
 * object gets deleted.
 */
class DENG2_PUBLIC NativeValue : public Value
{
public:
    typedef Deletable Object;

public:
    NativeValue(Object *object, Record const *memberScope = nullptr);

    Object *object() const;
    void setObject(Object *object);

    template <typename Type>
    Type *nativeObject() const {
        return static_cast<Type *>(object());
    }

    // Implementations of pure virtual methods.
    Text typeId() const;
    Value *duplicate() const;
    Text asText() const;
    bool isTrue() const;

    // Virtual Value methods.
    Record *memberScope() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBDENG2_NATIVEVALUE_H

