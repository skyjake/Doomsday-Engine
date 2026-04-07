/** @file variant.h  Opaque owned pointer of any Deletable type.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#pragma once

#include "de/deletable.h"

namespace de {

/**
 * Opaque owned pointer of any Deletable type.
 *
 * @ingroup data
 */
class DE_PUBLIC Variant
{
public:
    Variant() : _object(nullptr) {}

    template <typename Type>
    Variant(const Type &object)
        : _object(new Type(object))
    {}

    template <typename Type>
    Variant(Type *object)
        : _object(object)
    {
        DE_ASSERT(object != nullptr);
    }

    Variant(const Variant &) = delete;
    Variant &operator=(const Variant &) = delete;

    Variant(Variant &&moved)
        : _object(moved._object)
    {
        moved._object = nullptr;
    }

    Variant &operator=(Variant &&moved)
    {
        delete _object;
        _object = moved._object;
        moved._object = nullptr;
        return *this;
    }

    ~Variant()
    {
        delete _object;
    }

    inline explicit operator bool() const { return _object != nullptr; }

    template <typename Type>
    Type &value()
    {
        return *static_cast<Type *>(_object);
    }

    template <typename Type>
    const Type &value() const
    {
        return *static_cast<const Type *>(_object);
    }

private:
    Deletable *_object;
};

} // namespace de
