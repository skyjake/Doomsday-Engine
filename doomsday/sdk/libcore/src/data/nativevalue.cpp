/** @file nativevalue.cpp  Value representing a native object.
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

#include "de/NativeValue"

namespace de {

NativeValue::NativeValue(ObjectPtr object, Record const *memberScope)
    : _object(object)
    , _memberScope(memberScope)
{}

NativeValue::ObjectPtr NativeValue::object() const
{
    return _object;
}

void NativeValue::setObject(ObjectPtr object)
{
    _object = object;
}

Value *NativeValue::duplicate() const
{
    return new NativeValue(_object, _memberScope);
}

Value::Text NativeValue::asText() const
{
    return QString("(native object %1)").arg(dintptr(_object), 0, 16);
}

bool NativeValue::isTrue() const
{
    return _object != nullptr;
}

Record *NativeValue::memberScope() const
{
    return const_cast<Record *>(_memberScope);
}

void NativeValue::operator >> (Writer &) const
{
    throw CannotSerializeError("NativeValue::operator >>",
                               "Cannot serialize native object references");
}

void NativeValue::operator << (Reader &)
{}

} // namespace de
