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

#ifndef LIBCORE_STATEMENT_H
#define LIBCORE_STATEMENT_H

#include "de/iserializable.h"

namespace de {

class Context;

/**
 * The abstract base class for all statements.
 *
 * @ingroup script
 */
class Statement : public ISerializable
{
public:
    /// Deserialization of a statement failed. @ingroup errors
    DE_ERROR(DeserializationError);

public:
    Statement();

    virtual ~Statement();

    virtual void execute(Context &context) const = 0;

    Statement *next() const { return _next; }

    void setNext(Statement *statement) { _next = statement; }

    void setLineNumber(duint line);

    duint lineNumber() const;

public:
    /**
     * Constructs a statement by deserializing one from a reader.
     *
     * @param from  Reader.
     *
     * @return  The deserialized statement. Caller gets ownership.
     */
    static Statement *constructFrom(Reader &from);

protected:
    enum class SerialId : dbyte
    {
        Assign,
        Catch,
        Expression,
        Flow,
        For,
        Function,
        If,
        Print,
        Try,
        While,
        Delete,
        Scope,
    };

private:
    /// Pointer to the statement that follows this one, or NULL if
    /// this is the final statement.
    Statement *_next;

    duint _lineNumber;
};

} // namespace de

#endif /* LIBCORE_STATEMENT_H */
