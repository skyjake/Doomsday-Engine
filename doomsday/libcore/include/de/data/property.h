/** @file property.h  Utility for observable properties.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_PROPERTY_H
#define LIBDENG2_PROPERTY_H

#include "../Observers"

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
    BaseProperty() {}
    BaseProperty(ValueType const &value) : _value(value) {}

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
#define DENG2_DEFINE_PROPERTY(PropName, ValueType) \
    class PropName : public de::BaseProperty<ValueType> { \
    public: \
        typedef de::BaseProperty<ValueType> Base; \
        PropName() : Base() {} \
        PropName(ValueType const &value) : Base(value) {} \
        PropName(PropName const &other) : Base(other._value) {} \
        void setValue(ValueType const &v) { \
            if(_value == v) return; \
            _value = v; \
            DENG2_FOR_AUDIENCE2(Change, i) i->valueOf ## PropName ## Changed(); \
        } \
        PropName &operator = (ValueType const &v) { setValue(v); return *this; } \
        PropName &operator += (ValueType const &v) { setValue(_value + v); return *this; } \
        PropName &operator -= (ValueType const &v) { setValue(_value - v); return *this; } \
        DENG2_DEFINE_AUDIENCE_INLINE(Change, void valueOf ## PropName ## Changed()) \
    };

#define DENG2_PROPERTY(PropName, ValueType) \
    DENG2_DEFINE_PROPERTY(PropName, ValueType) \
    PropName p ## PropName;

#define DENG2_STATIC_PROPERTY(PropName, ValueType) \
    DENG2_DEFINE_PROPERTY(PropName, ValueType) \
    static PropName p ## PropName;

} // namespace de

#endif // LIBDENG2_PROPERTY_H
