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
#include "de/Vector"

#include <list>
#include <iomanip>

using namespace de;

Record::Record()
{}

Record::~Record()
{
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

Variable& Record::add(Variable* variable)
{
    std::auto_ptr<Variable> var(variable);
    if(variable->name().empty())
    {
        /// @throw UnnamedError All variables in a record must have a name.
        throw UnnamedError("Record::add", "All variables in a record must have a name");
    }
    members_[variable->name()] = var.release();
    return *variable;
}

Variable* Record::remove(Variable& variable)
{
    members_.erase(variable.name());
    return &variable;
}

Variable& Record::addNumber(const std::string& name, const Value::Number& number)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new NumberValue(number), Variable::NUMBER));
}

Variable& Record::addText(const std::string& name, const Value::Text& text)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new TextValue(text), Variable::TEXT));
}

Variable& Record::addArray(const std::string& name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new ArrayValue(), Variable::ARRAY));
}

Variable& Record::addDictionary(const std::string& name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new DictionaryValue(), Variable::DICTIONARY));
}

Variable& Record::addBlock(const std::string& name)
{
    /// @throw Variable::NameError @a name is not a valid variable name.
    Variable::verifyName(name);
    return add(new Variable(name, new BlockValue(), Variable::BLOCK));
}
    
Record& Record::add(const std::string& name, Record* subrecord)
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

Record& Record::addRecord(const std::string& name)
{
    return add(name, new Record());
}

Record* Record::remove(const std::string& name)
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
    
Variable& Record::operator [] (const std::string& name)
{
    return const_cast<Variable&>((*const_cast<const Record*>(this))[name]);
}
    
const Variable& Record::operator [] (const std::string& name) const
{
    // Path notation allows looking into subrecords.
    std::string::size_type pos = name.find('.');
    if(pos != std::string::npos)
    {
        return subrecord(name.substr(0, pos))[name.substr(pos + 1)];
    }
    
    Members::const_iterator found = members_.find(name);
    if(found != members_.end())
    {
        return *found->second;
    }
    throw NotFoundError("Record::operator []", "Variable '" + name + "' not found");
}

Record& Record::subrecord(const std::string& name)
{
    return const_cast<Record&>((const_cast<const Record*>(this))->subrecord(name));
}

const Record& Record::subrecord(const std::string& name) const
{
    // Path notation allows looking into subrecords.
    std::string::size_type pos = name.find('.');
    if(pos != std::string::npos)
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
    typedef std::pair<std::string, std::string> KeyValue;
    typedef std::list<KeyValue> List;
    
    List lines;
    Vector2ui maxLength;
    
    for(Record::Members::const_iterator i = record.members().begin(); 
        i != record.members().end(); ++i)
    {
        KeyValue kv(i->first, i->second->value().asText());
        lines.push_back(kv);
        maxLength = maxLength.max(Vector2ui(kv.first.size(), kv.second.size()));
    }
    
    for(List::iterator i = lines.begin(); i != lines.end(); ++i)
    {
        os << std::setw(maxLength.x) << i->first << ": " << i->second << "\n";
    }

    for(Record::Subrecords::const_iterator i = record.subrecords().begin();
        i != record.subrecords().end(); ++i)
    {
        os << "Subrecord '" + i->first + "':\n";
        os << *i->second;
        os << "End subrecord '" + i->first + "'\n";
    }
    return os;
}
