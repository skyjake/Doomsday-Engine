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

#ifndef LIBCORE_ID_H
#define LIBCORE_ID_H

#include "de/libcore.h"
#include "de/iserializable.h"
#include "de/value.h"
#include "de/log.h"

namespace de {

class String;

/**
 * Unique identifier number. Zero is not a valid identifier, as it reserved
 * for the "no identifier" special case.
 * @ingroup core
 */
class DE_PUBLIC Id : public ISerializable, public LogEntry::Arg::Base
{
public:
    typedef duint32 Type;

    /// The special "no identifier".
    enum { None = 0 };

public:
    /**
     * Constructs a new identifier. It is automatically unique (until the duint32 range
     * is depleted).
     */
    Id();

    Id(const Type &idValue) : _id(idValue) {}

    /**
     * Constructs an identifier from the text representation.
     *
     * @param text  Text representation of an identifier, such as returned by asText().
     */
    Id(const String &text);

    ~Id();

    bool isNone() const { return _id == None; }

    bool operator < (const Id &other) const { return _id < other._id; }

    bool operator == (const Id &other) const { return _id == other._id; }

    bool operator != (const Id &other) const { return _id != other._id; }

    operator bool () const { return _id != None; }

    operator Type () const { return _id; }

    /// Converts the Id to a text string.
    operator String () const;

    operator Value::Number () const;

    /// Converts the Id to a text string, using the format "{id}".
    String asText() const;

    ddouble asDouble() const;

    inline duint32 asUInt32() const { return _id; }

    dint64 asInt64() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::StringArgument; }

public:
    static Id none() { return Id::None; }

    static void resetGenerator(Type largestKnownId);

private:
    Type _id;
};

DE_PUBLIC std::ostream &operator<<(std::ostream &os, const Id &id);

/**
 * Utility for declaring identifiers that are initially uninitialized.
 */
class DE_PUBLIC NoneId : public Id
{
public:
    NoneId() : Id(None) {}
    NoneId(const Id &other) : Id(other) {}
};

inline bool operator!(const Id &id)
{
    return Id::Type(id) == Id::None;
}

} // namespace de

namespace std
{
    template<>
    struct hash<de::Id> {
        std::size_t operator()(const de::Id &key) const {
            return hash<uint32_t>()(key.asUInt32());
        }
    };
}

#endif // LIBCORE_ID_H
