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

#include "de/scripting/catchstatement.h"
#include "de/scripting/nameexpression.h"
#include "de/writer.h"
#include "de/reader.h"
#include "de/scripting/context.h"
#include "de/textvalue.h"
#include "de/refvalue.h"

using namespace de;

CatchStatement::CatchStatement(ArrayExpression *args) : _args(args)
{
    if (!_args)
    {
        _args = new ArrayExpression;
    }
}

CatchStatement::~CatchStatement()
{
    delete _args;
}

void CatchStatement::execute(Context &context) const
{
    context.proceed();
}

bool CatchStatement::isFinal() const
{
    return flags.testFlag(FinalCompound);
}

bool CatchStatement::matches(const Error &err) const
{
    if (!_args->size())
    {
        // Not specified, so catches all.
        return true;
    }

    const NameExpression &name = static_cast<const NameExpression &>(_args->at(0));

    return (name.identifier() == "Error" ||   // Generic catch-all.
            name.identifier().c_str() == err.name() || // Exact match.
            String(err.name()).endsWith("_" + name.identifier())); // Sub-error match.
}

void CatchStatement::executeCatch(Context &context, const Error &err) const
{
    if (_args->size() > 1)
    {
        // Place the error message into the specified variable.
        RefValue &ref = context.evaluator().evaluateTo<RefValue>(&_args->at(1));
        ref.assign(new TextValue(err.asText()));
    }

    // Begin the catch compound.
    context.start(_compound.firstStatement(), next());
}

void CatchStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::Catch) << duint8(flags) << *_args << _compound;
}

void CatchStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Catch)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("CatchStatement::operator <<", "Invalid ID");
    }
    duint8 f;
    from >> f;
    flags = Flags(f);
    from >> *_args >> _compound;
}
