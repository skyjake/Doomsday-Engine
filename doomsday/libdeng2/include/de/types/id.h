/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "../deng.h"
#include "../ISerializable"
#include "../Value"
#include "../Log"

namespace de
{
    class String;
    
    /**
     * Unique identifier number. Zero is not a valid identifier, as it reserved
     * for the "no identifier" special case.
     */
    class LIBDENG2_API Id : public ISerializable, public LogEntry::Arg::Base
    {
    public:
        typedef duint Type;

        /// The special "no identifier".
        enum { NONE = 0 };
        
    public:
        /**
         * Constructs a new identifier. It is automatically unique (until the duint range
         * is depleted).
         */
        Id();
        
        Id(const Type& idValue) : _id(idValue) {}
        
        /**
         * Constructs an identifier from the text representation.
         *
         * @param text  Text representation of an identifier, such as returned by asText().
         */
        Id(const String& text);

        ~Id();

        bool operator < (const Id& other) const { return _id < other._id; }

        bool operator == (const Id& other) const { return _id == other._id; }

        bool operator != (const Id& other) const { return _id != other._id; }
        
        operator bool () const { return _id != NONE; }

        operator Type () const { return _id; }

        /// Converts the Id to a text string.
        operator String () const;
        
        operator Value::Number () const;
        
        /// Converts the Id to a text string.
        String asText() const;
        
        // Implements ISerializable.
        void operator >> (Writer& to) const;
        void operator << (Reader& from);
        
        // Implements LogEntry::Arg::Base.
        LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::STRING; }
        
    private:
        Type _id;
        static Type _generator;
    };
    
    LIBDENG2_API QTextStream& operator << (QTextStream& os, const Id& id);
}

#endif /* LIBDENG2_ID_H */
