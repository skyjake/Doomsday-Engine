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

DENG2_PIMPL_NOREF(NativeValue)
, DENG2_OBSERVES(Deletable, Deletion)
{
    Object *object = nullptr;
    Record const *memberScope = nullptr;

    ~Instance()
    {
        setObject(nullptr);
    }

    void setObject(Object *obj)
    {
        if(object) object->audienceForDeletion -= this;
        object = obj;
        if(object) object->audienceForDeletion += this;
    }

    void objectWasDeleted(Deletable *obj)
    {
        DENG2_ASSERT(object == obj);
        object = nullptr;
    }
};

NativeValue::NativeValue(Object *object, Record const *memberScope)
    : d(new Instance)
{
    d->memberScope = memberScope;
    d->setObject(object);
}

NativeValue::Object *NativeValue::object() const
{
    return d->object;
}

void NativeValue::setObject(Object *object)
{
    d->object = object;
}

Value::Text NativeValue::typeId() const
{
    return "Native";
}

Value *NativeValue::duplicate() const
{
    return new NativeValue(d->object, d->memberScope);
}

Value::Text NativeValue::asText() const
{
    return QString("(native object 0x%1)").arg(dintptr(d->object), 0, 16);
}

bool NativeValue::isTrue() const
{
    return d->object != nullptr;
}

Record *NativeValue::memberScope() const
{
    return const_cast<Record *>(d->memberScope);
}

void NativeValue::operator >> (Writer &) const
{
    throw CannotSerializeError("NativeValue::operator >>",
                               "Cannot serialize native object references");
}

void NativeValue::operator << (Reader &)
{}

} // namespace de
