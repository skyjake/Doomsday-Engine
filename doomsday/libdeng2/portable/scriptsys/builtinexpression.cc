/*
 * The Doomsday Engine Project -- Hawthorn
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

#include "dbuiltinexpression.hh"
#include "devaluator.hh"
#include "dnumbervalue.hh"
#include "dtextvalue.hh"
#include "darrayvalue.hh"
#include "ddictionaryvalue.hh"
#include "dprocess.hh"

#include <memory>

using namespace de;
using std::auto_ptr;

BuiltInExpression::BuiltInExpression(Type type, Expression* argument)
    : type_(type), arg_(argument)
{}

BuiltInExpression::~BuiltInExpression()
{
    delete arg_;
}

void BuiltInExpression::push(Evaluator& evaluator, Object* names) const
{
	Expression::push(evaluator);	
    arg_->push(evaluator);
}

Value* BuiltInExpression::evaluate(Evaluator& evaluator) const
{
    auto_ptr<Value> value(evaluator.popResult());

    ArrayValue* argValue = dynamic_cast<ArrayValue*>(value.get());
    assert(argValue != NULL); // must be an array
    
    switch(type_)
    {
    case LENGTH:
        if(argValue->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for LENGTH");
        }
        return new NumberValue(argValue->elements()[1]->size());
        
    case DICTIONARY_KEYS:
    case DICTIONARY_VALUES:
        {
            if(argValue->size() != 2)
            {
                throw WrongArgumentsError("BuiltInExpression::evaluate",
                    "Expected exactly one argument for " +
                    std::string(type_ == DICTIONARY_KEYS? 
                        "DICTIONARY_KEYS" : "DICTIONARY_VALUES"));
            }
            
            DictionaryValue* dict = dynamic_cast<DictionaryValue*>(argValue->elements()[1]);
            if(!dict)
            {
                throw WrongArgumentsError("BuiltInExpression::evaluate",
                    "Argument must be a dictionary");
            }
            ArrayValue* array = new ArrayValue;
            for(DictionaryValue::Elements::const_iterator i = dict->elements().begin();
                i != dict->elements().end(); ++i)
            {
                if(type_ == DICTIONARY_KEYS)
                {
                    array->add(i->first.value->duplicate());
                }
                else
                {
                    array->add(i->second->duplicate());
                }
            }
            return array;
        }
        
    default:
        throw std::logic_error("Built-in method not implemented");
    }
}
