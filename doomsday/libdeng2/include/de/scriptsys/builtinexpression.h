/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_BUILTINEXPRESSION_H
#define LIBDENG2_BUILTINEXPRESSION_H

#include "../libdeng2.h"
#include "../Expression"
#include "../String"

namespace de {

/**
 * Evaluates a built-in function on the argument(s).
 *
 * @ingroup script
 */
class BuiltInExpression : public Expression
{
public:
    /// A wrong number of arguments is given to one of the built-in methods. @ingroup errors
    DENG2_ERROR(WrongArgumentsError);

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
        EVALUATE = 15
    };

public:
    BuiltInExpression();

    BuiltInExpression(Type type, Expression *argument);

    ~BuiltInExpression();

    void push(Evaluator &evaluator, Record *names = 0) const;

    Value *evaluate(Evaluator &evaluator) const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * Checks if the identifier is one of the built-in functions.
     */
    static Type findType(String const &identifier);

private:
    Type _type;
    Expression *_arg;
};

} // namespace de

#endif /* LIBDENG2_BUILTINEXPRESSION_H */
