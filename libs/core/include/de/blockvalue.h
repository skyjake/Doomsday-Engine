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

#ifndef LIBCORE_BLOCKVALUE_H
#define LIBCORE_BLOCKVALUE_H

#include "de/value.h"
#include "de/block.h"

namespace de {

/**
 * The BlockValue class is a subclass of Value that holds a data block.
 *
 * @ingroup data
 */
class DE_PUBLIC BlockValue : public Value
{
public:
    BlockValue();

    /// Copies the content of an existing block.
    BlockValue(const Block &block);

    /// Converts the BlockValue to a plain byte array (non-modifiable).
    operator const IByteArray &() const;

    /// Converts the BlockValue to a plain byte array.
    operator IByteArray &();

    Block &block();
    const Block &block() const;

    /// Empties the block value.
    void clear();

    Text typeId() const;
    Value *duplicate() const;
    Text asText() const;
    dsize size() const;
    bool isTrue() const;
    void sum(const Value &value);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

private:
    Block _value;
};

} // namespace de

#endif /* LIBCORE_BLOCKVALUE_H */
