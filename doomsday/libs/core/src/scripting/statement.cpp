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

#include "de/scripting/statement.h"
#include "de/scripting/assignstatement.h"
#include "de/scripting/catchstatement.h"
#include "de/scripting/deletestatement.h"
#include "de/scripting/expressionstatement.h"
#include "de/scripting/forstatement.h"
#include "de/scripting/functionstatement.h"
#include "de/scripting/ifstatement.h"
#include "de/scripting/flowstatement.h"
#include "de/scripting/printstatement.h"
#include "de/scripting/scopestatement.h"
#include "de/scripting/trystatement.h"
#include "de/scripting/whilestatement.h"
#include "de/reader.h"

using namespace de;

Statement::Statement() : _next(nullptr), _lineNumber(0)
{}

Statement::~Statement()
{}

Statement *Statement::constructFrom(Reader &reader)
{
    SerialId id;
    reader.mark();
    reader.readAs<dbyte>(id);
    reader.rewind();

    std::unique_ptr<Statement> result;
    switch (id)
    {
    case SerialId::Assign:
        result.reset(new AssignStatement);
        break;

    case SerialId::Catch:
        result.reset(new CatchStatement);
        break;

    case SerialId::Delete:
        result.reset(new DeleteStatement);
        break;

    case SerialId::Expression:
        result.reset(new ExpressionStatement);
        break;

    case SerialId::Flow:
        result.reset(new FlowStatement);
        break;

    case SerialId::For:
        result.reset(new ForStatement);
        break;

    case SerialId::Function:
        result.reset(new FunctionStatement);
        break;

    case SerialId::If:
        result.reset(new IfStatement);
        break;

    case SerialId::Print:
        result.reset(new PrintStatement);
        break;

    case SerialId::Try:
        result.reset(new TryStatement);
        break;

    case SerialId::While:
        result.reset(new WhileStatement);
        break;

    case SerialId::Scope:
        result.reset(new ScopeStatement);
        break;

    default:
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("Statement::constructFrom", "Invalid statement identifier");
    }

    // Deserialize it.
    reader >> *result.get();
    return result.release();
}

void Statement::setLineNumber(duint line)
{
    _lineNumber = line;
}

duint Statement::lineNumber() const
{
    return _lineNumber;
}
