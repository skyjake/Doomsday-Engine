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

namespace de {

/**
 * Reference to a native object. Only stores a pointer, and is not informed about the
 * deletion of the referenced object. The creator of NativeValue instances must ensure
 * that the values are not used after the referenced object has been destroyed.
 */
class DENG2_PUBLIC NativeValue : public Value
{
public:
    typedef void * ObjectPtr;

public:
    NativeValue(ObjectPtr object, Record const *memberScope);

    ObjectPtr object() const;
    void setObject(ObjectPtr object);

    // Implementations of pure virtual methods.
    Value *duplicate() const;
    Text asText() const;
    bool isTrue() const;

    // Virtual Value methods.
    Record *memberScope() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    ObjectPtr _object;
    Record const *_memberScope;
};

} // namespace de

#endif // LIBDENG2_NATIVEVALUE_H

