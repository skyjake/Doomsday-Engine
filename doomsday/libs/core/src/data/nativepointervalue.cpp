/** @file nativevalue.cpp  Value representing a native object.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/nativepointervalue.h"

namespace de {

DE_PIMPL_NOREF(NativePointerValue)
, DE_OBSERVES(Deletable, Deletion)
{
    Object *object = nullptr;
    const Record *memberScope = nullptr;

    ~Impl()
    {
        setObject(nullptr);
    }

    void setObject(Object *obj)
    {
        if (object) object->audienceForDeletion -= this;
        object = obj;
        if (object) object->audienceForDeletion += this;
    }

    void objectWasDeleted(Deletable *obj)
    {
        DE_UNUSED(obj);
        DE_ASSERT(object == obj);
        object = nullptr;
    }
};

NativePointerValue::NativePointerValue(Object *object, const Record *memberScope)
    : d(new Impl)
{
    d->memberScope = memberScope;
    d->setObject(object);
}

NativePointerValue::Object *NativePointerValue::object() const
{
    return d->object;
}

void NativePointerValue::setObject(Object *object)
{
    d->object = object;
}

Value::Text NativePointerValue::typeId() const
{
    return "Native";
}

Value *NativePointerValue::duplicate() const
{
    return new NativePointerValue(d->object, d->memberScope);
}

Value::Text NativePointerValue::asText() const
{
    return Stringf("(native object %p)", d->object);
}

bool NativePointerValue::isTrue() const
{
    return d->object != nullptr;
}

Record *NativePointerValue::memberScope() const
{
    return const_cast<Record *>(d->memberScope);
}

void NativePointerValue::operator >> (Writer &to) const
{
    // Native pointers cannot be serialized.
    to << SerialId(NONE);
}

void NativePointerValue::operator << (Reader &)
{
    throw CannotSerializeError("NativePointerValue::operator <<",
                               "Cannot deserialize native object references");
}

} // namespace de
