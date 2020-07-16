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

#include "de/scripting/functionstatement.h"
#include "de/scripting/function.h"
#include "de/scripting/context.h"
#include "de/scripting/constantexpression.h"
#include "de/scripting/functionvalue.h"
#include "de/scripting/process.h"
#include "de/textvalue.h"
#include "de/arrayvalue.h"
#include "de/dictionaryvalue.h"
#include "de/writer.h"
#include "de/refvalue.h"
#include "de/reader.h"

using namespace de;

FunctionStatement::FunctionStatement(Expression *identifier)
    : _identifier(identifier)
{
    _function = new Function();
}

FunctionStatement::~FunctionStatement()
{
    delete _identifier;
    de::releaseRef(_function);
}

Compound &FunctionStatement::compound()
{
    return _function->compound();
}

void FunctionStatement::addArgument(const String &argName, Expression *defaultValue)
{
    _function->arguments().push_back(argName);
    if (defaultValue)
    {
        _defaults.add(new ConstantExpression(new TextValue(argName)), defaultValue);
    }
}

void FunctionStatement::execute(Context &context) const
{
    Evaluator &eval = context.evaluator();

    // Set the function's namespace.
    Record &globals = context.process().globals();
    _function->setGlobals(&globals);

    // Variable that will store the function.
    eval.evaluateTo<RefValue>(_identifier);
    std::unique_ptr<RefValue> ref(eval.popResultAs<RefValue>());

    // Evaluate the argument default values.
    DictionaryValue &dict = eval.evaluateTo<DictionaryValue>(&_defaults);
    DE_FOR_EACH_CONST(DictionaryValue::Elements, i, dict.elements())
    {
        _function->defaults()[i->first.value->asText()] = i->second->duplicate();
    }

    // The value takes a reference to the function.
    ref->assign(new FunctionValue(_function));

    context.proceed();
}

void FunctionStatement::operator >> (Writer &to) const
{
    to << dbyte(SerialId::Function) << *_identifier << *_function << _defaults;
}

void FunctionStatement::operator << (Reader &from)
{
    SerialId id;
    from.readAs<dbyte>(id);
    if (id != SerialId::Function)
    {
        /// @throw DeserializationError The identifier that species the type of the
        /// serialized statement was invalid.
        throw DeserializationError("FunctionStatement::operator <<", "Invalid ID");
    }
    delete _identifier;
    _identifier = 0;
    _identifier = Expression::constructFrom(from);

    from >> *_function >> _defaults;
}
