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

#include "de/BuiltInExpression"
#include "de/Evaluator"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/RefValue"
#include "de/RecordValue"

using namespace de;

BuiltInExpression::BuiltInExpression(Type type, Expression* argument)
    : type_(type), arg_(argument)
{}

BuiltInExpression::~BuiltInExpression()
{
    delete arg_;
}

void BuiltInExpression::push(Evaluator& evaluator, Record*) const
{
    Expression::push(evaluator);    
    arg_->push(evaluator);
}

Value* BuiltInExpression::evaluate(Evaluator& evaluator) const
{
    std::auto_ptr<Value> value(evaluator.popResult());
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
        ArrayValue* array = new ArrayValue();
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
    
    case RECORD_MEMBERS:    
    case RECORD_SUBRECORDS:
    {
        if(argValue->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                std::string(type_ == RECORD_MEMBERS? 
                    "RECORD_MEMBERS" : "RECORD_SUBRECORDS"));
        }
        
        RecordValue* rec = dynamic_cast<RecordValue*>(argValue->elements()[1]);
        if(!rec)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a record");
        }
        DictionaryValue* dict = new DictionaryValue();
        if(type_ == RECORD_MEMBERS)
        {
            for(Record::Members::const_iterator i = rec->dereference().members().begin();
                i != rec->dereference().members().end(); ++i)
            {
                dict->add(new TextValue(i->first), new RefValue(i->second));
            }
        }
        else
        {
            for(Record::Subrecords::const_iterator i = rec->dereference().subrecords().begin();
                i != rec->dereference().subrecords().end(); ++i)
            {
                dict->add(new TextValue(i->first), new RecordValue(i->second));
            }
        }
        return dict;
    }
    
    case AS_NUMBER:
        if(argValue->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for AS_NUMBER");
        }
        return new NumberValue(argValue->elements()[1]->asNumber());

    case AS_TEXT:
        if(argValue->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for AS_TEXT");
        }
        return new TextValue(argValue->elements()[1]->asText());
        
    case LOCAL_NAMESPACE:
    {
        // Collect the namespaces to search.
        Evaluator::Namespaces spaces;
        evaluator.namespaces(spaces);
        if(argValue->size() != 1)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "No arguments expected for LOCAL_NAMESPACE");
        }
        return new RecordValue(spaces.front());
    }
        
    default:
        assert(false);
    }
    return NULL;
}

BuiltInExpression::Type BuiltInExpression::findType(const String& identifier)
{
    struct {
        const char* str;
        Type type;
    } types[] = {
        { "len",        LENGTH },
        { "dictkeys",   DICTIONARY_KEYS },
        { "dictvalues", DICTIONARY_VALUES },
        { "Text",       AS_TEXT },
        { "Number",     AS_NUMBER },
        { "locals",     LOCAL_NAMESPACE },
        { "members",    RECORD_MEMBERS },
        { "subrecords", RECORD_SUBRECORDS },
        { NULL,         NONE }
    };
    
    for(duint i = 0; types[i].str; ++i)
    {
        if(identifier == types[i].str)
        {
            return types[i].type;
        }
    }
    return NONE;
}
