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

#include "de/scripting/flowstatement.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/context.h"
#include "de/scripting/process.h"
#include "de/scripting/expression.h"
#include "de/value.h"
#include "de/writer.h"
#include "de/reader.h"

using namespace de;

#define HAS_ARG     0x80
#define TYPE_MASK   0x7f

FlowStatement::FlowStatement() : _type(PASS), _arg(0)
{}

FlowStatement::FlowStatement(Type type, Expression *countArgument)
    : _type(type), _arg(countArgument)
{}

FlowStatement::~FlowStatement()
{
    delete _arg;
}

void FlowStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();

    switch (_type)
    {
    case PASS:
        context.proceed();
        break;

    case CONTINUE:
        context.jumpContinue();
        break;

    case BREAK:
        if (_arg)
        {
            context.jumpBreak(dint(eval.evaluate(_arg).asNumber()));
        }
        else
        {
            context.jumpBreak();
        }
        break;

    case RETURN:
        if (_arg)
        {
            eval.evaluate(_arg);
            context.process().finish(eval.popResult());
        }
        else
        {
            context.process().finish();
        }
        break;

    case THROW:
        if (_arg)
        {
            throw Error("script", eval.evaluate(_arg).asText());
        }
        else
        {
            /// @todo  Rethrow the current error.
            context.proceed();
        }
    }
}

void FlowStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::Flow);
    duint8 header = duint8(_type);
    if (_arg)
    {
        header |= HAS_ARG;
    }
    to << header;
    if (_arg)
    {
        to << *_arg;
    }
}

void FlowStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Flow)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("FlowStatement::operator <<", "Invalid ID");
    }
    duint8 header;
    from >> header;
    _type = Type(header & TYPE_MASK);
    if (header & HAS_ARG)
    {
        delete _arg;
        _arg = 0;
        _arg = Expression::constructFrom(from);
    }
}
