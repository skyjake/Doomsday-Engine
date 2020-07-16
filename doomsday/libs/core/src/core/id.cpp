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

#include "de/id.h"
#include "de/string.h"
#include "de/writer.h"
#include "de/reader.h"

#include <atomic>
#include <cstdlib>
#include <ostream>

namespace de {

// The Id generator starts from one.
static std::atomic_uint idGenerator { 1 };

Id::Id() : _id(idGenerator++)
{
    if (_id == None)
    {
        ++_id;
    }
}

Id::Id(const String &text) : _id(None)
{
    if (text.beginsWith("{") && text.endsWith("}"))
    {
        _id = std::strtoul(text.substr(BytePos{1}, text.sizeu() - 2), nullptr, 16);
    }
}

Id::~Id()
{}

Id::operator String () const
{
    return asText();
}

Id::operator Value::Number () const
{
    return static_cast<Value::Number>(_id);
}

String Id::asText() const
{
    return Stringf("{%08x}", _id);
}

ddouble Id::asDouble() const
{
    return _id;
}

dint64 Id::asInt64() const
{
    return _id;
}

std::ostream &operator << (std::ostream &os, const Id &id)
{
    os << id.asText().c_str();
    return os;
}

void Id::operator >> (Writer &to) const
{
    to << _id;
}

void Id::operator << (Reader &from)
{
    from >> _id;

    resetGenerator(_id); // Try to avoid overlaps.
}

void Id::resetGenerator(Id::Type largestKnownId)
{
    if (idGenerator <= largestKnownId)
    {
        idGenerator = largestKnownId + 1;
    }
}

} // namespace de
