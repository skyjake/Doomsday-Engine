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

#include "de/scripting/builtinexpression.h"

#include "de/app.h"
#include "de/arrayvalue.h"
#include "de/blockvalue.h"
#include "de/dictionaryvalue.h"
#include "de/folder.h"
#include "de/math.h"
#include "de/numbervalue.h"
#include "de/reader.h"
#include "de/recordvalue.h"
#include "de/refvalue.h"
#include "de/scripting/evaluator.h"
#include "de/scripting/process.h"
#include "de/scripting/script.h"
#include "de/textvalue.h"
#include "de/timevalue.h"
#include "de/writer.h"

namespace de {

BuiltInExpression::BuiltInExpression(Type type, Expression *argument)
    : _type(type), _arg(argument)
{}

BuiltInExpression::~BuiltInExpression()
{
    delete _arg;
}

void BuiltInExpression::push(Evaluator &evaluator, Value *scope) const
{
    Expression::push(evaluator, scope);
    _arg->push(evaluator);
}

Value *BuiltInExpression::evaluate(Evaluator &evaluator) const
{
    std::unique_ptr<Value> value(evaluator.popResult());
    const ArrayValue &args = value.get()->as<ArrayValue>();

    switch (_type)
    {
    case LENGTH:
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for LENGTH");
        }
        return new NumberValue(args.at(1).size());

    case DICTIONARY_KEYS:
    case DICTIONARY_VALUES:
    {
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                String(_type == DICTIONARY_KEYS?
                       "DICTIONARY_KEYS" : "DICTIONARY_VALUES"));
        }

        const DictionaryValue *dict = dynamic_cast<const DictionaryValue *>(&args.at(1));
        if (!dict)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a dictionary");
        }

        return dict->contentsAsArray(_type == DICTIONARY_KEYS ? DictionaryValue::Keys
                                                              : DictionaryValue::Values);
    }

    case DIR:
    {
        if (args.size() > 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly at most one arguments for DIR");
        }

        const Record *ns;
        if (args.size() == 1)
        {
            ns = evaluator.localNamespace();
        }
        else
        {
            ns = &args.at(1).as<RecordValue>().dereference();
        }

        // Compose an alphabetically sorted list of the members.
        std::unique_ptr<ArrayValue> keys(new ArrayValue);
        StringList names = map<StringList>(
            ns->members(), [](const std::pair<String, Variable *> &v) { return v.first; });
        names.sort();
        for (const auto &name : names)
        {
            *keys << new TextValue(name);
        }
        return keys.release();
    }

    case RECORD_MEMBERS:
    case RECORD_SUBRECORDS:
    {
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for " +
                String(_type == RECORD_MEMBERS? "RECORD_MEMBERS" : "RECORD_SUBRECORDS"));
        }

        const RecordValue *rec = dynamic_cast<const RecordValue *>(&args.at(1));
        if (!rec)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Argument must be a record");
        }
        DictionaryValue *dict = new DictionaryValue();
        if (_type == RECORD_MEMBERS)
        {
            for (Record::Members::const_iterator i = rec->dereference().members().begin();
                i != rec->dereference().members().end(); ++i)
            {
                dict->add(new TextValue(i->first), new RefValue(i->second));
            }
        }
        else
        {
            Record::Subrecords subs = rec->dereference().subrecords();
            for (const auto &i : subs)
            {
                dict->add(new TextValue(i.first), new RecordValue(i.second));
            }
        }
        return dict;
    }

    case AS_RECORD:
    {
        if (args.size() == 1)
        {
            // No arguments: produce an owned, empty Record.
            return new RecordValue(new Record, RecordValue::OwnsRecord);
        }
        if (args.size() == 2)
        {
            std::unique_ptr<Record> returned;

            if (const auto *dict = maybeAs<DictionaryValue>(args.at(1)))
            {
                // Make an owned record out of a dictionary.
                returned.reset(new Record);
                for (auto i : dict->elements())
                {
                    returned->set(i.first.value->asText(), *i.second);
                }
            }
            else if (const auto *rec = maybeAs<RecordValue>(args.at(1)))
            {
                // Make an owned copy of a referenced record / argument.
                returned.reset(new Record(rec->dereference()));
            }
            else
            {
                throw WrongArgumentsError("BuiltInExpression::evaluate",
                                          "Argument 1 of AS_RECORD must be a record or dictionary");
            }

            return new RecordValue(returned.release(), RecordValue::OwnsRecord);
        }
        throw WrongArgumentsError("BuiltInExpression::evaluate",
                                  "Expected less than two arguments for AS_RECORD");
    }

    case AS_FILE:
    {
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_FILE");
        }
        // The only argument is an absolute path of the file.
        return new RecordValue(App::rootFolder().locate<File>(args.at(1).asText()).objectNamespace());
    }

    case AS_NUMBER:
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_NUMBER");
        }
        return new NumberValue(args.at(1).asNumber());

    case AS_TEXT:
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for AS_TEXT");
        }
        return new TextValue(args.at(1).asText());

    case AS_TIME:
        if (args.size() == 1)
        {
            // Current time.
            return new TimeValue;
        }
        if (args.size() == 2)
        {
            Time t = Time::fromText(args.at(1).asText());
            if (!t.isValid())
            {
                // Maybe just a date?
                t = Time::fromText(args.at(1).asText(), Time::ISODateOnly);
            }
            return new TimeValue(t);
        }
        throw WrongArgumentsError("BuiltInExpression::evaluate",
                                  "Expected less than two arguments for AS_TIME");

    case TIME_DELTA:
    {
        if (args.size() != 3)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly two arguments for TIME_DELTA");
        }
        const TimeValue *fromTime = dynamic_cast<const TimeValue *>(&args.at(1));
        if (!fromTime)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate", "Argument 1 of TIME_DELTA must be a time");
        }
        const TimeValue *toTime = dynamic_cast<const TimeValue *>(&args.at(2));
        if (!toTime)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate", "Argument 2 of TIME_DELTA must be a time");
        }
        return new NumberValue(toTime->time() - fromTime->time());
    }

    case LOCAL_NAMESPACE:
    {
        if (args.size() != 1)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "No arguments expected for LOCAL_NAMESPACE");
        }
        return new RecordValue(evaluator.localNamespace());
    }

    case GLOBAL_NAMESPACE:
    {
        if (args.size() != 1)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "No arguments expected for GLOBAL_NAMESPACE");
        }
        return new RecordValue(evaluator.process().globals());
    }

    case SERIALIZE:
    {
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for SERIALIZE");
        }
        std::unique_ptr<BlockValue> data(new BlockValue);
        Writer(*data.get()) << args.at(1);
        return data.release();
    }

    case DESERIALIZE:
    {
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                "Expected exactly one argument for DESERIALIZE");
        }
        const BlockValue *block = dynamic_cast<const BlockValue *>(&args.at(1));
        if (block)
        {
            Reader reader(*block);
            return Value::constructFrom(reader);
        }
        /*
        // Alternatively allow deserializing from a text value.
        const TextValue *text = dynamic_cast<const TextValue *>(&args.at(1));
        if (text)
        {
            return Value::constructFrom(Reader(Block(text->asText().toUtf8())));
        }
        */
        throw WrongArgumentsError("BuiltInExpression::evaluate",
            "deserialize() can operate only on block values");
    }

    case FLOOR:
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for FLOOR");
        }
        return new NumberValue(std::floor(args.at(1).asNumber()));

    case EVALUATE:
    {
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for EVALUATE");
        }
        // Set up a subprocess in the local namespace.
        Process subProcess(evaluator.localNamespace());
        // Parse the argument as a script.
        Script subScript(args.at(1).asText());
        subProcess.run(subScript);
        subProcess.execute();
        // A copy of the result value is returned.
        return subProcess.context().evaluator().result().duplicate();
    }

    case TYPE_OF:
        if (args.size() != 2)
        {
            throw WrongArgumentsError("BuiltInExpression::evaluate",
                                      "Expected exactly one argument for TYPE_OF");
        }
        return new TextValue(args.at(1).typeId());

    default:
        DE_ASSERT_FAIL("BuiltInExpression: invalid type");
    }
    return nullptr;
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
    if (id != BUILT_IN)
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
    _arg = nullptr;
    _arg = Expression::constructFrom(from);
}

const Hash<String, BuiltInExpression::Type> &builtinTypes()
{
    static const Hash<String, BuiltInExpression::Type> types {
        { "File",        BuiltInExpression::AS_FILE },
        { "Number",      BuiltInExpression::AS_NUMBER },
        { "Record",      BuiltInExpression::AS_RECORD },
        { "Text",        BuiltInExpression::AS_TEXT },
        { "Time",        BuiltInExpression::AS_TIME },
        { "deserialize", BuiltInExpression::DESERIALIZE },
        { "dictkeys",    BuiltInExpression::DICTIONARY_KEYS },
        { "dictvalues",  BuiltInExpression::DICTIONARY_VALUES },
        { "dir",         BuiltInExpression::DIR },
        { "eval",        BuiltInExpression::EVALUATE },
        { "floor",       BuiltInExpression::FLOOR },
        { "globals",     BuiltInExpression::GLOBAL_NAMESPACE },
        { "len",         BuiltInExpression::LENGTH },
        { "locals",      BuiltInExpression::LOCAL_NAMESPACE },
        { "members",     BuiltInExpression::RECORD_MEMBERS },
        { "serialize",   BuiltInExpression::SERIALIZE },
        { "subrecords",  BuiltInExpression::RECORD_SUBRECORDS },
        { "timedelta",   BuiltInExpression::TIME_DELTA },
        { "typeof",      BuiltInExpression::TYPE_OF },
    };
    return types;
}

BuiltInExpression::Type BuiltInExpression::findType(const String &identifier)
{
    auto found = builtinTypes().find(identifier);
    if (found != builtinTypes().end()) return found->second;
    return NONE;
}

StringList BuiltInExpression::identifiers()
{
    StringList names;
    for (const auto &t : builtinTypes())
    {
        names << t.first;
    }
    return names;
}

} // namespace de
