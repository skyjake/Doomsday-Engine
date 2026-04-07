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

#include "de/scripting/printstatement.h"
#include "de/scripting/context.h"
#include "de/scripting/arrayexpression.h"
#include "de/arrayvalue.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/log.h"

using namespace de;

PrintStatement::PrintStatement(ArrayExpression *arguments) : _arg(arguments)
{
    if (!_arg)
    {
        _arg = new ArrayExpression();
    }
}

PrintStatement::~PrintStatement()
{
    delete _arg;
}

void PrintStatement::execute(Context &context) const
{
    ArrayValue &value = context.evaluator().evaluateTo<ArrayValue>(_arg);

    String msg;
    bool isFirst = true;

    for (const auto &i : value.elements())
    {
       if (!isFirst)
       {
           msg += " ";
       }
       else
       {
           isFirst = false;
       }
       msg += i->asText();
    }

    LOG_SCR_MSG(_E(m)) << msg;

    context.proceed();
}

void PrintStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::Print) << *_arg;
}

void PrintStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Print)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("PrintStatement::operator <<", "Invalid ID");
    }
    from >> *_arg;
}
