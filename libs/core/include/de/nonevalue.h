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

#ifndef LIBCORE_NONEVALUE_H
#define LIBCORE_NONEVALUE_H

#include "de/value.h"

namespace de {

/**
 * The NoneValue class is a subclass of Value that does not contain any actual data.
 *
 * @ingroup data
 */
class DE_PUBLIC NoneValue : public Value
{
public:
    NoneValue();

    Text typeId() const;
    Value *duplicate() const;
    Text asText() const;
    bool isTrue() const;
    dint compare(const Value &value) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    static const NoneValue &none();
};

} // namespace de

#endif /* LIBCORE_NONEVALUE_H */
