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

#include "de/scripting/expression.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/arrayexpression.h"
#include "de/scripting/builtinexpression.h"
#include "de/scripting/constantexpression.h"
#include "de/scripting/dictionaryexpression.h"
#include "de/scripting/nameexpression.h"
#include "de/scripting/operatorexpression.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

Expression::Expression()
{}

Expression::~Expression()
{}

void Expression::push(Evaluator &evaluator, Value *scope) const
{
    evaluator.push(this, scope);
}

Expression *Expression::constructFrom(Reader &reader)
{
    SerialId id;
    reader.mark();
    reader >> id;
    reader.rewind();

    std::unique_ptr<Expression> result;
    switch (id)
    {
    case ARRAY:
        result.reset(new ArrayExpression());
        break;

    case BUILT_IN:
        result.reset(new BuiltInExpression());
        break;

    case CONSTANT:
        result.reset(new ConstantExpression());
        break;

    case DICTIONARY:
        result.reset(new DictionaryExpression());
        break;

    case NAME:
        result.reset(new NameExpression());
        break;

    case OPERATOR:
        result.reset(new OperatorExpression());
        break;

    default:
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized expression was invalid.
        throw DeserializationError("Expression::constructFrom", "Invalid expression identifier");
    }

    // Deserialize it.
    reader >> *result.get();
    return result.release();
}

const Flags &Expression::flags() const
{
    return _flags;
}

void Expression::setFlags(Flags f, FlagOp operation)
{
    applyFlagOperation(_flags, f, operation);
}

void Expression::operator >> (Writer &to) const
{
    // Save the flags.
    to << duint16(_flags);
}

void Expression::operator << (Reader &from)
{
    // Restore the flags.
    duint16 f;
    from >> f;
    _flags = Flags(f);
}
