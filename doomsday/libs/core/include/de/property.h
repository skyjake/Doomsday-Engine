/** @file property.h  Utility for observable properties.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_PROPERTY_H
#define LIBCORE_PROPERTY_H

#include "de/observers.h"

namespace de {

/**
 * Base class for properties.
 *
 * libcore properties are a utility for conveniently defining observable objects that
 * automatically send out a notification when their value changes.
 *
 * @ingroup data
 */
template <typename ValueType>
class BaseProperty
{
public:
    BaseProperty() = default;
    BaseProperty(const ValueType &value) : _value(value) {}

    inline ValueType value() const { return _value; }
    inline operator ValueType() const { return value(); }

protected:
    ValueType _value;
};

/**
 * Define a new property.
 *
 * @param PropName   Name of the property class.
 * @param ValueType  Value type of the property.
 *
 * When the value of the property changes, the audience notification method
 * <code>valueOf[PropName]Changed()</code> is called.
 *
 * Unlike script variables, properties deal with native value types and cannot accept
 * more than one type of value.
 *
 * @ingroup data
 */
#define DE_DEFINE_PROPERTY(PropName, ValueType) \
    class PropName : public de::BaseProperty<ValueType> { \
    public: \
        typedef de::BaseProperty<ValueType> Base; \
        PropName() : Base() {} \
        PropName(const ValueType &value) : Base(value) {} \
        PropName(const PropName &other) : Base(other._value) {} \
        void setValue(const ValueType &v) { \
            if (_value == v) return; \
            _value = v; \
            DE_NOTIFY(Change, i) i->valueOf ## PropName ## Changed(); \
        } \
        PropName &operator = (const ValueType &v) { setValue(v); return *this; } \
        PropName &operator += (const ValueType &v) { setValue(_value + v); return *this; } \
        PropName &operator -= (const ValueType &v) { setValue(_value - v); return *this; } \
        DE_AUDIENCE_INLINE(Change, void valueOf ## PropName ## Changed()) \
    };

#define DE_PROPERTY(PropName, ValueType) \
    DE_DEFINE_PROPERTY(PropName, ValueType) \
    PropName p ## PropName;

#define DE_STATIC_PROPERTY(PropName, ValueType) \
    DE_DEFINE_PROPERTY(PropName, ValueType) \
    static PropName p ## PropName;

} // namespace de

#endif // LIBCORE_PROPERTY_H
