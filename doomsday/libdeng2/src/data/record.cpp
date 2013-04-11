/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/TimeValue"
#include "de/Vector"
#include "de/String"

#include <QTextStream>
#include <QMap>
#include <QDebug>

namespace de {

/**
 * Each record is given a unique identifier, so that serialized record
 * references can be tracked to their original target.
 */
static duint32 recordIdCounter = 0;

DENG2_PIMPL(Record)
{
    Record::Members members;
    duint32 uniqueId; ///< Identifier to track serialized references.
    duint32 oldUniqueId;

    typedef QMap<duint32, Record *> RefMap;

    Instance(Public &r) : Base(r), uniqueId(++recordIdCounter), oldUniqueId(0)
    {}

    bool isSubrecord(Variable const &var) const
    {
        RecordValue const *value = dynamic_cast<RecordValue const *>(&var.value());
        return value && value->record() && value->hasOwnership();
    }

    Record::Subrecords listSubrecords() const
    {
        Subrecords subs;
        DENG2_FOR_EACH_CONST(Members, i, members)
        {
            if(isSubrecord(*i->second))
            {
                subs[i->second->name()] = static_cast<RecordValue &>(i->second->value()).record();
            }
        }
        return subs;
    }

    Variable const *findMemberByPath(String const &name)
    {
        // Path notation allows looking into subrecords.
        int pos = name.indexOf('.');
        if(pos >= 0)
        {
            String subName = name.substr(0, pos);
            String remaining = name.substr(pos + 1);
            // If it is a subrecord we can descend into it.
            return self[subName].value<RecordValue>().dereference().d->findMemberByPath(remaining);
        }

        Members::const_iterator found = members.find(name);
        if(found != members.end())
        {
            return found->second;
        }

        return 0;
    }

    /**
     * Returns the record inside which the variable identified by path @a name
     * resides. The necessary subrecords are created if they don't exist.
     *
     * @param name  Variable name or path.
     *
     * @return  Parent record for the variable.
     */
    Record &parentRecordByPath(String const &pathOrName)
    {
        int pos = pathOrName.indexOf('.');
        if(pos >= 0)
        {
            String subName = pathOrName.substr(0, pos);
            String remaining = pathOrName.substr(pos + 1);
            Record *rec = 0;

            if(!self.hasSubrecord(subName))
            {
                // Create it now.
                rec = &self.addRecord(subName);
            }
            else
            {
                rec = &self.subrecord(subName);
            }

            return rec->d->parentRecordByPath(remaining);
        }
        return self;
    }

    static String memberNameFromPath(String const &path)
    {
        return path.fileName('.');
    }

    /**
     * Reconnect record values that used to reference known records. After a
     * record has been deserialized, it may contain variables whose values
     * reference other records. The default behavior for a record is to
     * dereference records when serialized, but if the target has been
     * serialized as part of the record, we can restore the original reference
     * by looking at the IDs found in the serialized data.
     *
     * @param refMap  Known records indexes with their old IDs.
     */
    void reconnectReferencesAfterDeserialization(RefMap const &refMap)
    {
        DENG2_FOR_EACH(Members, i, members)
        {
            RecordValue *value = dynamic_cast<RecordValue *>(&i->second->value());
            if(!value || !value->record()) continue;

            // Recurse into subrecords first.
            if(value->usedToHaveOwnership())
            {
                value->record()->d->reconnectReferencesAfterDeserialization(refMap);
            }

            // After deserialization all record values own their records.
            if(value->hasOwnership() && !value->usedToHaveOwnership())
            {                
                // Do we happen to know the record from earlier?
                duint32 oldTargetId = value->record()->d->oldUniqueId;
                if(refMap.contains(oldTargetId))
                {
                    LOG_DEV_TRACE("RecordValue %p restored to reference record %i (%p)",
                                  value << oldTargetId << refMap[oldTargetId]);

                    // Relink the value to its target.
                    value->setRecord(refMap[oldTargetId]);
                }
            }
        }
    }
};

Record::Record() : d(new Instance(*this))
{}

Record::Record(Record const &other)
    : ISerializable(), LogEntry::Arg::Base(), Variable::IDeletionObserver(),
      d(new Instance(*this))
{
    copyMembersFrom(other);
}

Record::~Record()
{
    DENG2_FOR_AUDIENCE(Deletion, i) i->recordBeingDeleted(*this);
    clear();
}

void Record::clear()
{
    if(!d->members.empty())
    {
        DENG2_FOR_EACH(Members, i, d->members)
        {
            i->second->audienceForDeletion -= this;
            delete i->second;
        }
        d->members.clear();
    }
}

void Record::copyMembersFrom(Record const &other)
{
    DENG2_FOR_EACH_CONST(Members, i, other.d->members)
    {
        Variable *var = new Variable(*i->second);
        var->audienceForDeletion += this;
        d->members[i->first] = var;
    }
}

Record &Record::operator = (Record const &other)
{
    clear();
    copyMembersFrom(other);
    return *this;
}

bool Record::has(String const &name) const
{
    return hasMember(name);
}

bool Record::hasMember(String const &variableName) const
{
    return d->findMemberByPath(variableName) != 0;
}

bool Record::hasSubrecord(String const &subrecordName) const
{
    Variable const *found = d->findMemberByPath(subrecordName);
    if(found)
    {
        return d->isSubrecord(*found);
    }
    return false;
}

Variable &Record::add(Variable *variable)
{
    std::auto_ptr<Variable> var(variable);
    if(variable->name().empty())
    {
        /// @throw UnnamedError All variables in a record must have a name.
        throw UnnamedError("Record::add", "All members of a record must have a name");
    }
    if(hasMember(variable->name()))
    {
        // Delete the previous variable with this name.
        delete d->members[variable->name()];
    }
    var->audienceForDeletion += this;
    d->members[variable->name()] = var.release();
    return *variable;
}

Variable *Record::remove(Variable &variable)
{
    variable.audienceForDeletion -= this;
    d->members.erase(variable.name());
    return &variable;
}

Variable &Record::addNumber(String const &name, Value::Number const &number)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new NumberValue(number), Variable::AllowNumber));
}

Variable &Record::addBoolean(String const &name, bool booleanValue)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new NumberValue(booleanValue, NumberValue::Boolean),
                              Variable::AllowNumber));
}

Variable &Record::addText(String const &name, Value::Text const &text)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new TextValue(text), Variable::AllowText));
}

Variable &Record::addTime(String const &name, Time const &time)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new TimeValue(time), Variable::AllowTime));
}

Variable &Record::addArray(String const &name, ArrayValue *array)
{
    // Automatically create an empty array if one is not provided.
    if(!array) array = new ArrayValue;
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              array, Variable::AllowArray));
}

Variable &Record::addDictionary(String const &name)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new DictionaryValue, Variable::AllowDictionary));
}

Variable &Record::addBlock(String const &name)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new BlockValue, Variable::AllowBlock));
}

Variable &Record::addFunction(const String &name, Function *func)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new FunctionValue(func), Variable::AllowFunction));
}
    
Record &Record::add(String const &name, Record *subrecord)
{
    std::auto_ptr<Record> sub(subrecord);
    d->parentRecordByPath(name)
            .add(new Variable(Instance::memberNameFromPath(name),
                              new RecordValue(sub.release(), RecordValue::OwnsRecord)));
    return *subrecord;
}

Record &Record::addRecord(String const &name)
{
    return add(name, new Record);
}

Record *Record::remove(String const &name)
{
    Members::const_iterator found = d->members.find(name);
    if(found != d->members.end() && d->isSubrecord(*found->second))
    {
        Record *rec = static_cast<RecordValue *>(&found->second->value())->takeRecord();
        remove(*found->second);
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
    Variable const *found = d->findMemberByPath(name);
    if(found)
    {
        return *found;
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

    Members::const_iterator found = d->members.find(name);
    if(found != d->members.end() && d->isSubrecord(*found->second))
    {
        return *static_cast<RecordValue const &>(found->second->value()).record();
    }
    throw NotFoundError("Record::subrecord", "Subrecord '" + name + "' not found");
}

Record::Members const &Record::members() const
{
    return d->members;
}

Record::Subrecords Record::subrecords() const
{
    return d->listSubrecords();
}

String Record::asText(String const &prefix, List *lines) const
{
    // Recursive calls to collect all variables in the record.
    if(lines)
    {
        // Collect lines from this record.
        for(Members::const_iterator i = d->members.begin(); i != d->members.end(); ++i)
        {
            String separator = (d->isSubrecord(*i->second)? "." : ":");

            KeyValue kv(prefix + i->first + separator, i->second->value().asText());
            lines->push_back(kv);
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
        os << qSetFieldWidth(maxLength.x) << i->first << qSetFieldWidth(0) << " ";
        // Print the value line by line.
        int pos = 0;
        while(pos >= 0)
        {
            int next = i->second.indexOf('\n', pos);
            if(pos > 0)
            {
                os << qSetFieldWidth(maxLength.x) << "" << qSetFieldWidth(0) << " ";
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
    to << d->uniqueId << duint32(d->members.size());
    for(Members::const_iterator i = d->members.begin(); i != d->members.end(); ++i)
    {
        to << *i->second;
    }
}
    
void Record::operator << (Reader &from)
{
    LOG_AS("Record deserialization");

    duint32 count = 0;
    from >> d->oldUniqueId >> count;
    clear();

    Instance::RefMap refMap;
    refMap.insert(d->oldUniqueId, this);

    while(count-- > 0)
    {
        std::auto_ptr<Variable> var(new Variable());
        from >> *var.get();

        RecordValue *recVal = dynamic_cast<RecordValue *>(&var.get()->value());
        if(recVal && recVal->usedToHaveOwnership())
        {
            DENG2_ASSERT(recVal->record());

            // This record was a subrecord prior to serializing.
            // Let's remember the record for reconnecting other variables
            // that might be referencing it.
            refMap.insert(recVal->record()->d->oldUniqueId, recVal->record());
        }

        add(var.release());
    }

    // Find referenced records and relink them to their original targets.
    d->reconnectReferencesAfterDeserialization(refMap);

    // Observe all members for deletion.
    DENG2_FOR_EACH(Members, i, d->members)
    {
        i->second->audienceForDeletion += this;
    }
}

void Record::variableBeingDeleted(Variable &variable)
{
    DENG2_ASSERT(d->findMemberByPath(variable.name()) != 0);

    LOG_DEV_TRACE("Variable %p deleted, removing from Record %p", &variable << this);

    // Remove from our index.
    d->members.erase(variable.name());
}

QTextStream &operator << (QTextStream &os, Record const &record)
{
    return os << record.asText();
}

} // namespace de
