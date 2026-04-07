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

#ifndef LIBCORE_BUILTINEXPRESSION_H
#define LIBCORE_BUILTINEXPRESSION_H

#include "../libcore.h"
#include "../string.h"
#include "expression.h"

namespace de {

/**
 * Evaluates a built-in function on the argument(s).
 *
 * @ingroup script
 */
class DE_PUBLIC BuiltInExpression : public Expression
{
public:
    /// A wrong number of arguments is given to one of the built-in methods. @ingroup errors
    DE_ERROR(WrongArgumentsError);

    /// Type of the built-in expression.
    /// @note  These are serialied as is, so do not change the existing values.
    enum Type {
        NONE = 0,
        LENGTH = 1, ///< Evaluate the length of an value (by calling size()).
        DICTIONARY_KEYS = 2,
        DICTIONARY_VALUES = 3,
        RECORD_MEMBERS = 4,
        RECORD_SUBRECORDS = 5,
        AS_TEXT = 6,
        AS_NUMBER = 7,
        LOCAL_NAMESPACE = 8,
        SERIALIZE = 9,
        DESERIALIZE = 10,
        AS_TIME = 11,
        TIME_DELTA = 12,
        AS_RECORD = 13,
        FLOOR = 14,
        EVALUATE = 15,
        DIR = 16,
        AS_FILE = 17,
        GLOBAL_NAMESPACE = 18,
        TYPE_OF = 19
    };

public:
    BuiltInExpression() : _type(NONE), _arg(nullptr)
    {}

    BuiltInExpression(Type type, Expression *argument);

    ~BuiltInExpression();

    void push(Evaluator &evaluator, Value *scope = nullptr) const;

    Value *evaluate(Evaluator &evaluator) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * Checks if the identifier is one of the built-in functions.
     */
    static Type findType(const String &identifier);

    /**
     * Returns a list of all the built-in functions.
     */
    static StringList identifiers();

private:
    Type _type;
    Expression *_arg;
};

} // namespace de

#endif /* LIBCORE_BUILTINEXPRESSION_H */
