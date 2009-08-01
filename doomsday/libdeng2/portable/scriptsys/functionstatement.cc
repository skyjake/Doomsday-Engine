/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/FunctionStatement"
#include "de/Function"
#include "de/Context"
#include "de/NameExpression"
#include "de/ConstantExpression"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/FunctionValue"
#include "de/RefValue"

using namespace de;

FunctionStatement::FunctionStatement(Expression* identifier)
    : identifier_(identifier)
{
    function_ = new Function();
}

FunctionStatement::~FunctionStatement()
{
    delete identifier_;
    function_->release();
}

Compound& FunctionStatement::compound()
{
    return function_->compound();
}

void FunctionStatement::addArgument(const String& argName, Expression* defaultValue)
{
    function_->arguments().push_back(argName);
    if(defaultValue)
    {
        defaults_.add(new ConstantExpression(new TextValue(argName)), defaultValue);
    }
}

void FunctionStatement::execute(Context& context) const
{
    Evaluator& eval = context.evaluator();

    // Variable that will store the function.
    eval.evaluateTo<RefValue>(identifier_);
    std::auto_ptr<RefValue> ref(eval.popResultAs<RefValue>());

    // Evaluate the argument default values.
    DictionaryValue& dict = eval.evaluateTo<DictionaryValue>(&defaults_);
    for(DictionaryValue::Elements::const_iterator i = dict.elements().begin(); 
        i != dict.elements().end(); ++i)
    {
        function_->defaults()[i->first.value->asText()] = i->second->duplicate();
    }
        
    // The value takes a reference to the function.
    ref->assign(new FunctionValue(function_));
    
    context.proceed();
}
