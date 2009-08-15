/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <iomanip>

using namespace de;

Record::Record()
{}

Record::Record(const Record& other)
{
    for(Members::const_iterator i = other.members_.begin(); i != other.members_.end(); ++i)
    {
        members_[i->first] = new Variable(*i->second);
    }
    for(Subrecords::const_iterator i = other.subrecords_.begin(); i != other.subrecords_.end(); ++i)
    {
        subrecords_[i->first] = new Record(*i->second);
    }    
}

Record::~Record()
{
    FOR_EACH_OBSERVER(o, observers) o->recordBeingDeleted(*this);
    clear();
}

void Record::clear()
{
    if(!members_.empty())
    {
        for(Members::iterator i = members_.begin(); i != members_.end(); ++i)
        {
            delete i->second;
        }
        members_.clear();
    }
    if(!subrecords_.empty())
    {
        for(Subrecords::iterator i = subrecords_.begin(); i != subrecords_.end(); ++i)
        {
            delete i->second;
        }
        subrecords_.clear();
    }
}

bool Record::has(const String& name) const
{
    return hasMember(name) || hasSubrecord(name);
}

bool Record::hasMember(const String& variableName) const
{
    Members::const_iterator found = members_.find(variableName);
    return found != members_.end();
}

bool Record::hasSubrecord(const String& subrecordName) const
{
    Subrecords::const_iterator found = subrecords_.find(subrecordName);
    return found != subrecords_.end();
}

Variable& Record::add(Variable* variable)
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
        delete members_[variable->name()];
    }
    members_[variable->name()] = var.release();
    return *variable;
}

Variable* Record::remove(Variable& variable)
{
    members_.erase(variable.name());
    return &variable;
}

Variable& Record::addNumber(const String& name, const Value::Number& number)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new NumberValue(number), Variable::NUMBER));
}

Variable& Record::addText(const String& name, const Value::Text& text)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new TextValue(text), Variable::TEXT));
}

Variable& Record::addArray(const String& name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new ArrayValue(), Variable::ARRAY));
}

Variable& Record::addDictionary(const String& name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new DictionaryValue(), Variable::DICTIONARY));
}

Variable& Record::addBlock(const String& name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new BlockValue(), Variable::BLOCK));
}
    
Record& Record::add(const String& name, Record* subrecord)
{
    std::auto_ptr<Record> sub(subrecord);
    /// @throw Variable::NameError Subrecord names must be valid variable names.
    Variable::verifyName(name);
    if(name.empty())
    {
        /// @throw UnnamedError All subrecords in a record must have a name.
        throw UnnamedError("Record::add", "All subrecords in a record must have a name");
    }
    subrecords_[name] = sub.release();
    return *subrecord;
}

Record& Record::addRecord(const String& name)
{
    return add(name, new Record());
}

Record* Record::remove(const String& name)
{
    Subrecords::iterator found = subrecords_.find(name);
    if(found != subrecords_.end())
    {
        Record* rec = found->second;
        subrecords_.erase(found);
        return rec;
    }
    throw NotFoundError("Record::remove", "Subrecord '" + name + "' not found");
}
    
Variable& Record::operator [] (const String& name)
{
    return const_cast<Variable&>((*const_cast<const Record*>(this))[name]);
}
    
const Variable& Record::operator [] (const String& name) const
{
    // Path notation allows looking into subrecords.
    String::size_type pos = name.find('.');
    if(pos != String::npos)
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
    
    Members::const_iterator found = members_.find(name);
    if(found != members_.end())
    {
        return *found->second;
    }
    throw NotFoundError("Record::operator []", "Variable '" + name + "' not found");
}

Record& Record::subrecord(const String& name)
{
    return const_cast<Record&>((const_cast<const Record*>(this))->subrecord(name));
}

const Record& Record::subrecord(const String& name) const
{
    // Path notation allows looking into subrecords.
    String::size_type pos = name.find('.');
    if(pos != String::npos)
    {
        return subrecord(name.substr(0, pos)).subrecord(name.substr(pos + 1));
    }

    Subrecords::const_iterator found = subrecords_.find(name);
    if(found != subrecords_.end())
    {
        return *found->second;
    }
    throw NotFoundError("Record::subrecords", "Subrecord '" + name + "' not found");
}
    
String Record::asText(const String& prefix, List* lines) const
{
    // Recursive calls to collect all variables in the record.
    if(lines)
    {
        // Collect lines from this record.
        for(Members::const_iterator i = members_.begin(); i != members_.end(); ++i)
        {
            KeyValue kv(prefix + i->first, i->second->value().asText());
            lines->push_back(kv);
        }
        // Collect lines from subrecords.
        for(Subrecords::const_iterator i = subrecords_.begin(); i != subrecords_.end(); ++i)
        {
            i->second->asText(i->first.concatenateMember(""), lines);
        }
        return "";
    }

    // Top level of the recursion.
    std::ostringstream os;
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
        os << std::setw(maxLength.x) << i->first << ": ";
        // Print the value line by line.
        String::size_type pos = 0;
        while(pos != String::npos)
        {
            String::size_type next = i->second.find('\n', pos);
            if(pos > 0)
            {
                os << std::setw(maxLength.x) << "" << "  ";
            }
            os << i->second.substr(pos, next != String::npos? next - pos + 1 : next);
            pos = next;
            if(pos != String::npos) pos++;
        }
    }

    return os.str();    
}

const Function* Record::function(const String& name) const
{
    try
    {
        const FunctionValue* func = dynamic_cast<const FunctionValue*>(&(*this)[name].value());
        if(func)
        {
            return &func->function();
        }
    }
    catch(NotFoundError&)
    {}
    return 0;
}
    
void Record::operator >> (Writer& to) const
{
    to << duint32(members_.size());
    for(Members::const_iterator i = members_.begin(); i != members_.end(); ++i)
    {
        to << *i->second;
    }
    // Any subrecords?
    to << duint32(subrecords_.size());
    if(!subrecords_.empty())
    {
        for(Subrecords::const_iterator i = subrecords_.begin(); i != subrecords_.end(); ++i)
        {
            to << i->first << *i->second;
        }
    }
}
    
void Record::operator << (Reader& from)
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

std::ostream& de::operator << (std::ostream& os, const Record& record)
{
    return os << record.asText();
}
