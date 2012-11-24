/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/Record"
#include "de/Variable"
#include "de/Reader"
#include "de/Writer"
#include "de/NumberValue"
#include "de/TextValue"
#include "de/ArrayValue"
#include "de/DictionaryValue"
#include "de/BlockValue"
#include "de/RecordValue"
#include "de/FunctionValue"
#include "de/Vector"

#include <QTextStream>

using namespace de;

Record::Record()
{}

Record::Record(Record const &other) : ISerializable(), LogEntry::Arg::Base()
{
    for(Members::const_iterator i = other._members.begin(); i != other._members.end(); ++i)
    {
        _members[i->first] = new Variable(*i->second);
    }
    for(Subrecords::const_iterator i = other._subrecords.begin(); i != other._subrecords.end(); ++i)
    {
        _subrecords[i->first] = new Record(*i->second);
    }    
}

Record::~Record()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->recordBeingDeleted(*this);
    clear();
}

void Record::clear()
{
    if(!_members.empty())
    {
        for(Members::iterator i = _members.begin(); i != _members.end(); ++i)
        {
            delete i->second;
        }
        _members.clear();
    }
    if(!_subrecords.empty())
    {
        for(Subrecords::iterator i = _subrecords.begin(); i != _subrecords.end(); ++i)
        {
            delete i->second;
        }
        _subrecords.clear();
    }
}

bool Record::has(String const &name) const
{
    return hasMember(name) || hasSubrecord(name);
}

bool Record::hasMember(String const &variableName) const
{
    Members::const_iterator found = _members.find(variableName);
    return found != _members.end();
}

bool Record::hasSubrecord(String const &subrecordName) const
{
    Subrecords::const_iterator found = _subrecords.find(subrecordName);
    return found != _subrecords.end();
}

Variable &Record::add(Variable *variable)
{
    std::auto_ptr<Variable> var(variable);
    if(variable->name().empty())
    {
        /// @throw UnnamedError All variables in a record must have a name.
        throw UnnamedError("Record::add", "All variables in a record must have a name");
    }
    if(hasMember(variable->name()))
    {
        // Delete the previous variable with this name.
        delete _members[variable->name()];
    }
    _members[variable->name()] = var.release();
    return *variable;
}

Variable *Record::remove(Variable &variable)
{
    _members.erase(variable.name());
    return &variable;
}

Variable &Record::addNumber(String const &name, Value::Number const &number)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new NumberValue(number), Variable::AllowNumber));
}

Variable &Record::addText(String const &name, Value::Text const &text)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new TextValue(text), Variable::AllowText));
}

Variable &Record::addArray(String const &name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new ArrayValue(), Variable::AllowArray));
}

Variable &Record::addDictionary(String const &name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new DictionaryValue(), Variable::AllowDictionary));
}

Variable &Record::addBlock(String const &name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new BlockValue(), Variable::AllowBlock));
}
    
Record &Record::add(String const &name, Record *subrecord)
{
    std::auto_ptr<Record> sub(subrecord);
    /// @throw Variable::NameError Subrecord names must be valid variable names.
    Variable::verifyName(name);
    if(name.empty())
    {
        /// @throw UnnamedError All subrecords in a record must have a name.
        throw UnnamedError("Record::add", "All subrecords in a record must have a name");
    }
    _subrecords[name] = sub.release();
    return *subrecord;
}

Record &Record::addRecord(String const &name)
{
    return add(name, new Record());
}

Record *Record::remove(String const &name)
{
    Subrecords::iterator found = _subrecords.find(name);
    if(found != _subrecords.end())
    {
        Record *rec = found->second;
        _subrecords.erase(found);
        return rec;
    }
    throw NotFoundError("Record::remove", "Subrecord '" + name + "' not found");
}
    
Variable &Record::operator [] (String const &name)
{
    return const_cast<Variable &>((*const_cast<Record const *>(this))[name]);
}
    
Variable const &Record::operator [] (String const &name) const
{
    // Path notation allows looking into subrecords.
    int pos = name.indexOf('.');
    if(pos >= 0)
    {
        String subName = name.substr(0, pos);
        String remaining = name.substr(pos + 1);
        if(hasMember(subName))
        {
            // It is a variable. If it has a RecordValue then we can descend into it.
            return value<RecordValue>(subName).dereference()[remaining];
        }
        return subrecord(subName)[remaining];
    }
    
    Members::const_iterator found = _members.find(name);
    if(found != _members.end())
    {
        return *found->second;
    }
    throw NotFoundError("Record::operator []", "Variable '" + name + "' not found");
}

Record &Record::subrecord(String const &name)
{
    return const_cast<Record &>((const_cast<Record const *>(this))->subrecord(name));
}

Record const &Record::subrecord(String const &name) const
{
    // Path notation allows looking into subrecords.
    int pos = name.indexOf('.');
    if(pos >= 0)
    {
        return subrecord(name.substr(0, pos)).subrecord(name.substr(pos + 1));
    }

    Subrecords::const_iterator found = _subrecords.find(name);
    if(found != _subrecords.end())
    {
        return *found->second;
    }
    throw NotFoundError("Record::subrecords", "Subrecord '" + name + "' not found");
}
    
String Record::asText(String const &prefix, List *lines) const
{
    // Recursive calls to collect all variables in the record.
    if(lines)
    {
        // Collect lines from this record.
        for(Members::const_iterator i = _members.begin(); i != _members.end(); ++i)
        {
            KeyValue kv(prefix + i->first, i->second->value().asText());
            lines->push_back(kv);
        }
        // Collect lines from subrecords.
        for(Subrecords::const_iterator i = _subrecords.begin(); i != _subrecords.end(); ++i)
        {
            i->second->asText(i->first.concatenateMember(""), lines);
        }
        return "";
    }

    // Top level of the recursion.
    QString result;
    QTextStream os(&result);
    List allLines;
    Vector2ui maxLength;

    // Collect.
    asText(prefix, &allLines);
    
    // Sort and find maximum length.
    allLines.sort();
    for(List::iterator i = allLines.begin(); i != allLines.end(); ++i)
    {
        maxLength = maxLength.max(Vector2ui(i->first.size(), i->second.size()));
    }
    
    // Print aligned.
    for(List::iterator i = allLines.begin(); i != allLines.end(); ++i)
    {
        if(i != allLines.begin()) os << "\n";
        os << qSetFieldWidth(maxLength.x) << i->first << qSetFieldWidth(0) << ": ";
        // Print the value line by line.
        int pos = 0;
        while(pos >= 0)
        {
            int next = i->second.indexOf('\n', pos);
            if(pos > 0)
            {
                os << qSetFieldWidth(maxLength.x) << "" << qSetFieldWidth(0) << "  ";
            }
            os << i->second.substr(pos, next != String::npos? next - pos + 1 : next);
            pos = next;
            if(pos != String::npos) pos++;
        }
    }

    return result;
}

Function const *Record::function(String const &name) const
{
    try
    {
        FunctionValue const *func = dynamic_cast<FunctionValue const *>(&(*this)[name].value());
        if(func)
        {
            return &func->function();
        }
    }
    catch(NotFoundError &) {}    
    return 0;
}
    
void Record::operator >> (Writer &to) const
{
    to << duint32(_members.size());
    for(Members::const_iterator i = _members.begin(); i != _members.end(); ++i)
    {
        to << *i->second;
    }
    // Any subrecords?
    to << duint32(_subrecords.size());
    if(!_subrecords.empty())
    {
        for(Subrecords::const_iterator i = _subrecords.begin(); i != _subrecords.end(); ++i)
        {
            to << i->first << *i->second;
        }
    }
}
    
void Record::operator << (Reader &from)
{
    duint32 count = 0;
    from >> count;
    clear();
    while(count-- > 0)
    {
        std::auto_ptr<Variable> var(new Variable());
        from >> *var.get();
        add(var.release());
    }
    // Also subrecords.
    from >> count;
    while(count-- > 0)
    {
        std::auto_ptr<Record> sub(new Record());
        String subName;
        from >> subName >> *sub.get();
        add(subName, sub.release());
    }
}

QTextStream &de::operator << (QTextStream &os, Record const &record)
{
    return os << record.asText();
}
