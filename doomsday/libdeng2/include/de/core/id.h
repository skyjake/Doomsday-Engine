/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_ID_H
#define LIBDENG2_ID_H

#include "../libdeng2.h"
#include "../ISerializable"
#include "../Value"
#include "../Log"

namespace de {

class String;

/**
 * Unique identifier number. Zero is not a valid identifier, as it reserved
 * for the "no identifier" special case.
 */
class DENG2_PUBLIC Id : public ISerializable, public LogEntry::Arg::Base
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

    Id(Type const &idValue) : _id(idValue) {}

    /**
     * Constructs an identifier from the text representation.
     *
     * @param text  Text representation of an identifier, such as returned by asText().
     */
    Id(String const &text);

    ~Id();

    bool isNone() const { return _id == None; }

    bool operator < (Id const &other) const { return _id < other._id; }

    bool operator == (Id const &other) const { return _id == other._id; }

    bool operator != (Id const &other) const { return _id != other._id; }

    operator bool () const { return _id != None; }

    operator Type () const { return _id; }

    /// Converts the Id to a text string.
    operator String () const;

    operator Value::Number () const;

    /// Converts the Id to a text string, using the format "{id}".
    String asText() const;

    ddouble asDouble() const;

    dint64 asInt64() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::STRING; }

private:
    Type _id;

    static Type _generator;
};

DENG2_PUBLIC QTextStream &operator << (QTextStream &os, Id const &id);

inline uint qHash(Id const &id) { return id; }

} // namespace de

#endif // LIBDENG2_ID_H
