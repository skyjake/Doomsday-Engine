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

#include "de/BuiltInExpression"
#include "de/Evaluator"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/RefValue"
#include "de/RecordValue"
#include "de/BlockValue"
#include "de/TimeValue"
#include "de/Writer"
#include "de/Reader"
#include "de/math.h"

using namespace de;

BuiltInExpression::BuiltInExpression() : _type(NONE), _arg(0)
{}

BuiltInExpression::BuiltInExpression(Type type, Expression *argument)
    : _type(type), _arg(argument)
{}

BuiltInExpression::~BuiltInExpression()
{
    delete _arg;
}

void BuiltInExpression::push(Evaluator &evaluator, Record *) const
{
    Expression::push(evaluator);    
    _arg->push(evaluator);
}

Value *BuiltInExpression::evaluate(Evaluator &evaluator) const
{
    std::auto_ptr<Value> value(evaluator.popResult());
    ArrayValue *args = dynamic_cast<ArrayValue *>(value.get());
    DENG2_ASSERT(args != NULL); // must be an array
    
    switch(_type)
    {
    case LENGTH:
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for LENGTH");
        }
        return new NumberValue(args->at(1).size());
        
    case DICTIONARY_KEYS:
    case DICTIONARY_VALUES:
    {
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                String(_type == DICTIONARY_KEYS?
                       "DICTIONARY_KEYS" : "DICTIONARY_VALUES"));
        }
        
        DictionaryValue const *dict = dynamic_cast<DictionaryValue const *>(&args->at(1));
        if(!dict)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a dictionary");
        }
        ArrayValue *array = new ArrayValue();
        for(DictionaryValue::Elements::const_iterator i = dict->elements().begin();
            i != dict->elements().end(); ++i)
        {
            if(_type == DICTIONARY_KEYS)
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
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                String(_type == RECORD_MEMBERS? "RECORD_MEMBERS" : "RECORD_SUBRECORDS"));
        }
        
        RecordValue const *rec = dynamic_cast<RecordValue const *>(&args->at(1));
        if(!rec)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a record");
        }
        DictionaryValue *dict = new DictionaryValue();
        if(_type == RECORD_MEMBERS)
        {
            for(Record::Members::const_iterator i = rec->dereference().members().begin();
                i != rec->dereference().members().end(); ++i)
            {
                dict->add(new TextValue(i->first), new RefValue(i->second));
            }
        }
        else
        {
            Record::Subrecords subs = rec->dereference().subrecords();
            DENG2_FOR_EACH(Record::Subrecords, i, subs)
            {
                dict->add(new TextValue(i->first), new RecordValue(i->second));
            }
        }
        return dict;
    }

    case AS_RECORD:
    {
        if(args->size() == 1)
        {
            // No arguments: produce an owned, empty Record.
            return new RecordValue(new Record, RecordValue::OwnsRecord);
        }
        if(args->size() == 2)
        {
            // One argument: make an owned copy of a referenced record.
            RecordValue const *rec = dynamic_cast<RecordValue const *>(&args->at(1));
            if(!rec)
            {
                throw WrongArgumentsError("BuiltInExpression::evaluate",
                                          "Argument 1 of AS_RECORD must be a record");
            }
            return new RecordValue(new Record(*rec->record()), RecordValue::OwnsRecord);
        }
        throw WrongArgumentsError("BuiltInExpression::evaluate",
                                  "Expected less than two arguments for AS_RECORD");
    }
    
    case AS_NUMBER:
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_NUMBER");
        }
        return new NumberValue(args->at(1).asNumber());

    case AS_TEXT:
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_TEXT");
        }
        return new TextValue(args->at(1).asText());

    case AS_TIME:
        if(args->size() == 1)
        {
            // Current time.
            return new TimeValue;
        }
        if(args->size() == 2)
        {
            Time t = Time::fromText(args->at(1).asText());
            if(!t.isValid())
            {
                // Maybe just a date?
                t = Time::fromText(args->at(1).asText(), Time::ISODateOnly);
            }
            return new TimeValue(t);
        }
        throw WrongArgumentsError("BuiltInExpression::evaluate",
                                  "Expected less than two arguments for AS_TIME");

    case TIME_DELTA:
    {
        if(args->size() != 3)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly two arguments for TIME_DELTA");
        }
        TimeValue const *fromTime = dynamic_cast<TimeValue const *>(&args->at(1));
        if(!fromTime)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate", "Argument 1 of TIME_DELTA must be a time");
        }
        TimeValue const *toTime = dynamic_cast<TimeValue const *>(&args->at(2));
        if(!toTime)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate", "Argument 2 of TIME_DELTA must be a time");
        }
        return new NumberValue(toTime->time() - fromTime->time());
    }

    case LOCAL_NAMESPACE:
    {
        // Collect the namespaces to search.
        Evaluator::Namespaces spaces;
        evaluator.namespaces(spaces);
        if(args->size() != 1)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "No arguments expected for LOCAL_NAMESPACE");
        }
        return new RecordValue(spaces.front());
    }
    
    case SERIALIZE:
    {
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for SERIALIZE");
        }
        std::auto_ptr<BlockValue> data(new BlockValue);
        Writer(*data.get()) << args->at(1);
        return data.release();
    }
    
    case DESERIALIZE:
    {
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for DESERIALIZE");
        }
        BlockValue const *block = dynamic_cast<BlockValue const *>(&args->at(1));
        if(block)
        {
            Reader reader(*block);
            return Value::constructFrom(reader);
        }
        /*
        // Alternatively allow deserializing from a text value.
        TextValue const *text = dynamic_cast<TextValue const *>(&args->at(1));
        if(text)
        {
            return Value::constructFrom(Reader(Block(text->asText().toUtf8())));
        }
        */
        throw WrongArgumentsError("BuiltInExpression::evaluate",
            "deserialize() can operate only on block values");
    }
        
    case FLOOR:
        if(args->size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for FLOOR");
        }
        return new NumberValue(std::floor(args->at(1).asNumber()));

    default:
        DENG2_ASSERT(false);
    }
    return NULL;
}

void BuiltInExpression::operator >> (Writer &to) const
{
    to << SerialId(BUILT_IN);

    Expression::operator >> (to);

    to << duint8(_type) << *_arg;
}

void BuiltInExpression::operator << (Reader &from)
{
    SerialId id;
    from >> id;
    if(id != BUILT_IN)
    {
        /// @throw DeserializationError The identifier that species the type of the 
        /// serialized expression was invalid.
        throw DeserializationError("BuiltInExpression::operator <<", "Invalid ID");
    }

    Expression::operator << (from);

    duint8 t;
    from >> t;
    _type = Type(t);
    delete _arg;
    _arg = 0;
    _arg = Expression::constructFrom(from);
}

BuiltInExpression::Type BuiltInExpression::findType(String const &identifier)
{
    struct {
        char const *str;
        Type type;
    } types[] = {
        { "len",         LENGTH },
        { "dictkeys",    DICTIONARY_KEYS },
        { "dictvalues",  DICTIONARY_VALUES },
        { "Text",        AS_TEXT },
        { "Number",      AS_NUMBER },
        { "locals",      LOCAL_NAMESPACE },
        { "members",     RECORD_MEMBERS },
        { "subrecords",  RECORD_SUBRECORDS },
        { "serialize",   SERIALIZE },
        { "deserialize", DESERIALIZE },
        { "Time",        AS_TIME },
        { "timedelta",   TIME_DELTA },
        { "Record",      AS_RECORD },
        { "floor",       FLOOR },
        { NULL,          NONE }
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
