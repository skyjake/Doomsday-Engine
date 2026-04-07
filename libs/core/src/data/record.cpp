/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/record.h"
#include "de/arrayvalue.h"
#include "de/blockvalue.h"
#include "de/dictionaryvalue.h"
#include "de/scripting/functionvalue.h"
#include "de/info.h"
#include "de/logbuffer.h"
#include "de/numbervalue.h"
#include "de/reader.h"
#include "de/recordvalue.h"
#include "de/regexp.h"
#include "de/string.h"
#include "de/textvalue.h"
#include "de/timevalue.h"
#include "de/variable.h"
#include "de/vector.h"
#include "de/writer.h"

#include "de/compiledrecord.h"

#include <functional>
#include <atomic>
#include <iomanip>

namespace de {

/// When converting records to a human-readable text representation, this is the
/// maximum number of lines that a subrecord can have before it is shown as a short
/// excerpt.
const int SUBRECORD_CONTENT_EXCERPT_THRESHOLD = 100; // lines

const char *Record::VAR_SUPER       = "__super__";
const char *Record::VAR_FILE        = "__file__";
const char *Record::VAR_INIT        = "__init__";
const char *Record::VAR_NATIVE_SELF = "__self__";

/**
 * Each record is given a unique identifier, so that serialized record
 * references can be tracked to their original target.
 */
static std::atomic_uint recordIdCounter;

DE_PIMPL(Record)
, public Lockable
, DE_OBSERVES(Variable, Deletion)
{
    Record::Members members;
    duint32 uniqueId; ///< Identifier to track serialized references.
    duint32 oldUniqueId;
    Flags flags = DefaultFlags;

    using RefMap = Hash<duint32, Record *>;

    Impl(Public &r)
        : Base(r)
        , uniqueId(++recordIdCounter)
        , oldUniqueId(0)
    {}

    struct ExcludeByBehavior {
        Behavior behavior;
        ExcludeByBehavior(Behavior b) : behavior(b) {}
        bool operator () (const Variable &member) {
            return (behavior == IgnoreDoubleUnderscoreMembers &&
                    member.name().beginsWith("__"));
        }
    };

    struct ExcludeByRegExp {
        RegExp omitted;
        ExcludeByRegExp(const RegExp &omit) : omitted(omit) {}
        bool operator()(const Variable &member) {
            return omitted.exactMatch(member.name());
        }
    };

    void clear(const std::function<bool (const Variable &)>& excluded)
    {
        if (!members.empty())
        {
            Record::Members remaining; // Contains all members that are not removed.
            for (auto &i : members)
            {
                if (excluded(*i.second))
                {
                    remaining.insert(i.first, i.second);
                    continue;
                }
                DE_NOTIFY_PUBLIC(Removal, o)
                {
                    o->recordMemberRemoved(self(), *i.second);
                }
                i.second->audienceForDeletion() -= this;
                delete i.second;
            }
            members = std::move(remaining);
        }
    }

    void copyMembersFrom(const Record &other, const std::function<bool (const Variable &)>& excluded)
    {
        const auto *other_d = other.d.getConst();
        DE_GUARD(other_d);

        for (auto &i : other_d->members)
        {
            const auto &i_key = i.first;
            const auto *i_value = i.second;

            if (!excluded(*i_value))
            {
                bool alreadyExists;
                Variable *var;
                {
                    DE_GUARD(this);
                    var = new Variable(*i_value);
                    var->audienceForDeletion() += this;
                    auto iter = members.find(i_key);
                    alreadyExists = (iter != members.end());
                    if (alreadyExists)
                    {
                        iter->second->audienceForDeletion() -= this;
                        delete iter->second;
                        iter->second = var;
                    }
                    else
                    {
                        members[i_key] = var;
                    }
                }

                if (!alreadyExists)
                {
                    // Notify about newly added members.
                    DE_NOTIFY_PUBLIC(Addition, i) i->recordMemberAdded(self(), *var);
                }

                /// @todo Should also notify if the value of an existing variable changes. -jk
            }
        }
    }

    void assignPreservingVariables(const Record &other, std::function<bool (const Variable &)> excluded)
    {
        const auto *other_d = other.d.getConst();
        DE_GUARD(other_d);

        // Add variables or update existing ones.
        for (auto i = other_d->members.begin(); i != other_d->members.end(); ++i)
        {
            if (!excluded(*i->second))
            {
                Variable *var = nullptr;

                // Already have a variable with this name?
                {
                    DE_GUARD(this);
                    auto found = members.find(i->first);
                    if (found != members.end())
                    {
                        var = found->second;
                    }
                }

                // Change the existing value.
                if (var)
                {
                    if (isSubrecord(*i->second) && isSubrecord(*var))
                    {
                        // Recurse to subrecords.
                        var->valueAsRecord().d->assignPreservingVariables
                                (i->second->valueAsRecord(), excluded);
                    }
                    else
                    {
                        // Ignore read-only flags.
                        const Flags oldFlags = var->flags();
                        var->setFlags(Variable::ReadOnly, false);

                        // Just make a copy.
                        var->set(i->second->value());

                        var->setFlags(oldFlags, ReplaceFlags);
                    }
                }
                else
                {
                    // Add a new one.
                    DE_GUARD(this);
                    var = new Variable(*i->second);
                    var->audienceForDeletion() += this;
                    members[i->first] = var;
                }
            }
        }

        // Remove variables not present in the other.
        DE_GUARD(this);
        MutableHashIterator<String, Variable *> iter(members);
        while (iter.hasNext())
        {
            iter.next();
            if (!excluded(*iter.value()) && !other.hasMember(iter.key()))
            {
                Variable *var = iter.value();
                iter.remove();
                var->audienceForDeletion() -= this;
                delete var;
            }
        }
    }

    static bool isRecord(const Variable &var)
    {
        const auto *value = maybeAs<RecordValue>(var.value());
        return value && value->record();
    }

    static bool isSubrecord(const Variable &var)
    {
        // Subrecords are owned by this record.
        // Note: Non-owned Records are likely imports from other modules.
        const auto *value = maybeAs<RecordValue>(var.value());
        return value && value->record() && value->hasOwnership();
    }

    LoopResult forSubrecords(std::function<LoopResult (const String &, Record &)> func) const
    {
        const Members unmodifiedMembers = members; // In case a callback removes members.
        for (auto &i : unmodifiedMembers)
        {
            const Variable &member = *i.second;
            if (isSubrecord(member))
            {
                Record *rec = member.value<RecordValue>().record();
                DE_ASSERT(rec != nullptr); // subrecords are owned, so cannot have been deleted

                if (auto result = func(i.first, *rec))
                {
                    return result;
                }
            }
        }
        return LoopContinue;
    }

    Record::Subrecords listSubrecords(std::function<bool (const Record &)> filter) const
    {
        DE_GUARD(this);

        Subrecords subs;
        forSubrecords([&subs, filter] (const String &name, Record &rec)
        {
            // Must pass the filter.
            if (filter(rec))
            {
                subs.insert(String(name), &rec);
            }
            return LoopContinue;
        });
        return subs;
    }

    const Variable *findMemberByPath(const String &name) const
    {
        // Path notation allows looking into subrecords.
        if (auto pos = name.indexOf('.'))
        {
            String subName   = name.left(pos);
            String remaining = name.substr(pos + 1);
            // If it is a subrecord we can descend into it.
            if (!self().hasRecord(subName)) return nullptr;
            return self()[subName].value<RecordValue>().dereference().d->findMemberByPath(remaining);
        }

        DE_GUARD(this);
        auto found = members.find(name);
        if (found != members.end())
        {
            return found->second;
        }
        return nullptr;
    }

    /**
     * Returns the record inside which the variable identified by path @a name
     * resides. The necessary subrecords are created if they don't exist.
     *
     * @param pathOrName  Variable name or path.
     *
     * @return  Parent record for the variable.
     */
    Record &parentRecordByPath(const String &pathOrName)
    {
        DE_GUARD(this);

        if (auto pos = pathOrName.indexOf('.'))
        {
            String  subName   = pathOrName.left(pos);
            String  remaining = pathOrName.substr(pos + 1);
            Record *rec       = nullptr;

            if (!self().hasSubrecord(subName))
            {
                // Create it now.
                rec = &self().addSubrecord(subName);
            }
            else
            {
                rec = &self().subrecord(subName);
            }

            return rec->d->parentRecordByPath(remaining);
        }
        return self();
    }

    // Observes Variable deletion.
    void variableBeingDeleted(Variable &variable)
    {
        DE_ASSERT(findMemberByPath(variable.name()));

        LOG_TRACE_DEBUGONLY("Variable %p deleted, removing from Record %p", &variable << thisPublic);

        // Remove from our index.
        DE_GUARD(this);
        members.remove(variable.name());
    }

    static String memberNameFromPath(const String &path)
    {
        return String(path.fileName('.'));
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
    void reconnectReferencesAfterDeserialization(const RefMap &refMap)
    {
        for (auto &i : members)
        {
            RecordValue *value = dynamic_cast<RecordValue *>(&i.second->value());
            if (!value || !value->record()) continue;

            // Recurse into subrecords first.
            if (value->usedToHaveOwnership())
            {
                value->record()->d->reconnectReferencesAfterDeserialization(refMap);
            }

            // After deserialization all record values own their records.
            if (value->hasOwnership() && !value->usedToHaveOwnership())
            {
                // Do we happen to know the record from earlier?
                duint32 oldTargetId = value->record()->d->oldUniqueId;
                if (refMap.contains(oldTargetId))
                {
                    LOG_TRACE_DEBUGONLY("RecordValue %p restored to reference record %i (%p)",
                                        value << oldTargetId << refMap[oldTargetId]);

                    // Relink the value to its target.
                    value->setRecord(refMap[oldTargetId]);
                }
            }
        }
    }

    DE_PIMPL_AUDIENCE(Deletion)
    DE_PIMPL_AUDIENCE(Addition)
    DE_PIMPL_AUDIENCE(Removal)
};

DE_AUDIENCE_METHOD(Record, Deletion)
DE_AUDIENCE_METHOD(Record, Addition)
DE_AUDIENCE_METHOD(Record, Removal)

Record::Record() : RecordAccessor(this), d(new Impl(*this))
{}

Record::Record(const Record &other, Behavior behavior)
    : RecordAccessor(this)
    , d(new Impl(*this))
{
    copyMembersFrom(other, behavior);
}

Record::Record(Record &&moved)
    : RecordAccessor(this)
    , d(std::move(moved.d))
{
    DE_ASSERT(moved.d.isNull());

    d->thisPublic = this;
}

Record::~Record()
{
    if (!d.isNull()) // will be nullptr if moved away
    {
        // Notify before deleting members so that observers have full visibility
        // to the record prior to deletion.
        DE_NOTIFY(Deletion, i) i->recordBeingDeleted(*this);

        clear();
    }
}

Record &Record::setFlags(Flags flags, FlagOpArg op)
{
    applyFlagOperation(d->flags, flags, op);
    return *this;
}

Flags Record::flags() const
{
    return d->flags;
}

void Record::clear(Behavior behavior)
{
    DE_GUARD(d);
    d->clear(Impl::ExcludeByBehavior(behavior));
}

void Record::copyMembersFrom(const Record &other, Behavior behavior)
{
    d->copyMembersFrom(other, Impl::ExcludeByBehavior(behavior));
}

void Record::assignPreservingVariables(const Record &from, Behavior behavior)
{
    d->assignPreservingVariables(from, Impl::ExcludeByBehavior(behavior));
}

Record &Record::operator = (const Record &other)
{
    return assign(other);
}

Record &Record::operator = (Record &&moved)
{
    d = std::move(moved.d);
    d->thisPublic = this;
    return *this;
}

Record &Record::assign(const Record &other, Behavior behavior)
{
    if (this == &other) return *this;

    DE_GUARD(d);

    clear(behavior);
    copyMembersFrom(other, behavior);
    return *this;
}

Record &Record::assign(const Record &other, const RegExp &excluded)
{
    DE_GUARD(d);

    d->clear(Impl::ExcludeByRegExp(excluded));
    d->copyMembersFrom(other, Impl::ExcludeByRegExp(excluded));
    return *this;
}

bool Record::has(const String &name) const
{
    return hasMember(name);
}

bool Record::hasMember(const String &variableName) const
{
    return d->findMemberByPath(variableName) != nullptr;
}

bool Record::hasSubrecord(const String &subrecordName) const
{
    const Variable *found = d->findMemberByPath(subrecordName);
    return found? d->isSubrecord(*found) : false;
}

bool Record::hasRecord(const String &recordName) const
{
    const Variable *found = d->findMemberByPath(recordName);
    return found? d->isRecord(*found) : false;
}

Variable &Record::add(Variable *variable)
{
    std::unique_ptr<Variable> var(variable);
    if (variable->name().empty())
    {
        /// @throw UnnamedError All variables in a record must have a name.
        throw UnnamedError("Record::add", "All members of a record must have a name");
    }

    {
        DE_GUARD(d);
        if (hasMember(variable->name()))
        {
            // Delete the previous variable with this name.
            delete d->members[variable->name()];
        }
        var->audienceForDeletion() += d;
        d->members[variable->name()] = var.release();
    }

    DE_NOTIFY(Addition, i) i->recordMemberAdded(*this, *variable);

    return *variable;
}

Variable *Record::remove(Variable &variable)
{
    {
        DE_GUARD(d);
        variable.audienceForDeletion() -= d;
        d->members.remove(variable.name());
    }

    DE_NOTIFY(Removal, i) i->recordMemberRemoved(*this, variable);

    return &variable;
}

Variable *Record::remove(const String &variableName)
{
    return remove((*this)[variableName]);
}

Variable *Record::tryRemove(const String &variableName)
{
    if (has(variableName))
    {
        return remove(variableName);
    }
    return nullptr;
}

void Record::removeMembersWithPrefix(const String &prefix)
{
    for (const String &name : members().keys())
    {
        if (name.beginsWith(prefix))
        {
            remove(name);
        }
    }
}

Variable &Record::add(const String &name, Flags variableFlags)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name), nullptr, variableFlags));
}

Variable &Record::addNumber(const String &name, Value::Number number)
{
    return add(name, Variable::AllowNumber).set(NumberValue(number));
}

Variable &Record::addBoolean(const String &name, bool booleanValue)
{
    return add(name, Variable::AllowNumber).set(NumberValue(booleanValue, NumberValue::Boolean));
}

Variable &Record::addText(const String &name, const Value::Text &text)
{
    return add(name, Variable::AllowText).set(TextValue(text));
}

Variable &Record::addTime(const String &name, const Time &time)
{
    return add(name, Variable::AllowTime).set(TimeValue(time));
}

Variable &Record::addArray(const String &name, ArrayValue *array)
{
    // Automatically create an empty array if one is not provided.
    if (!array) array = new ArrayValue;
    return add(name, Variable::AllowArray).set(array);
}

Variable &Record::addDictionary(const String &name)
{
    return add(name, Variable::AllowDictionary).set(new DictionaryValue);
}

Variable &Record::addBlock(const String &name)
{
    return add(name, Variable::AllowBlock).set(new BlockValue);
}

Variable &Record::addFunction(const String &name, Function *func)
{
    return add(name, Variable::AllowFunction).set(new FunctionValue(func));
}

Record &Record::add(const String &name, Record *subrecord)
{
    std::unique_ptr<Record> sub(subrecord);
    add(name).set(RecordValue::takeRecord(sub.release()));
    return *subrecord;
}

Record &Record::addSubrecord(const String &name, SubrecordAdditionBehavior behavior)
{
    if (behavior == KeepExisting)
    {
        if (name.isEmpty())
        {
            return *this;
        }
        if (hasSubrecord(name))
        {
            return subrecord(name);
        }
    }
    return add(name, new Record);
}

Record *Record::removeSubrecord(const String &name)
{
    auto found = d->members.find(String(name));
    if (found != d->members.end() && d->isSubrecord(*found->second))
    {
        Record *returnedToCaller = found->second->value<RecordValue>().takeRecord();
        remove(*found->second);
        return returnedToCaller;
    }
    throw NotFoundError("Record::remove", "Subrecord '" + name + "' not found");
}

Variable &Record::set(const String &name, bool value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(NumberValue(value));
    }
    return addBoolean(name, value);
}

Variable &Record::set(const String &name, const char *value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(TextValue(value));
    }
    return addText(name, value);
}

Variable &Record::set(const String &name, const Value::Text &value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(TextValue(value));
    }
    return addText(name, value);
}

Variable &Record::set(const String &name, Value::Number value)
{
    return set(name, NumberValue(value));
}

Variable &Record::set(const String &name, const NumberValue &value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(value);
    }
    return add(name, Variable::AllowNumber).set(value);
}

Variable &Record::set(const String &name, dint32 value)
{
    return set(name, NumberValue(value));
}

Variable &Record::set(const String &name, duint32 value)
{
    return set(name, NumberValue(value));
}

Variable &Record::set(const String &name, dint64 value)
{
  return set(name, NumberValue(value));
}

Variable &Record::set(const String &name, duint64 value)
{
    return set(name, NumberValue(value));
}

// Variable &Record::set(const String &name, unsigned long value)
// {
//     return set(name, NumberValue(value));
// }

Variable &Record::set(const String &name, const Time &value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(TimeValue(value));
    }
    return addTime(name, value);
}

Variable &Record::set(const String &name, const Block &value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(BlockValue(value));
    }
    Variable &var = addBlock(name);
    var.value<BlockValue>().block() = value;
    return var;
}

Variable &Record::set(const String &name, const Record &value)
{
    DE_GUARD(d);

    std::unique_ptr<Record> dup(new Record(value));
    if (hasMember(name))
    {
        return (*this)[name].set(RecordValue::takeRecord(dup.release()));
    }
    Variable &var = add(name);
    var.set(RecordValue::takeRecord(dup.release()));
    return var;
}

Variable &Record::set(const String &name, ArrayValue *value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(value);
    }
    return addArray(name, value);
}

Variable &Record::set(const String &name, Value *value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(value);
    }
    return add(name).set(value);
}

Variable &Record::set(const String &name, const Value &value)
{
    DE_GUARD(d);

    if (hasMember(name))
    {
        return (*this)[name].set(value);
    }
    return add(name).set(value);
}

Variable &Record::appendWord(const String &name, const String &word, const String &separator)
{
    DE_GUARD(d);

    String value = gets(name, "");
    if (!value.isEmpty()) value.append(separator);
    set(name, value + word);
    return (*this)[name];
}

Variable &Record::appendUniqueWord(const String &name, const String &word, const String &separator)
{
    DE_GUARD(d);

    const String value = gets(name, "");
    if (!value.containsWord(word))
    {
        appendWord(name, word, separator);
    }
    return (*this)[name];
}

Variable &Record::appendMultipleUniqueWords(const String &name, const String &words, const String &separator)
{
    for (const String &word : words.split(separator))
    {
        if (word)
        {
            appendUniqueWord(name, word, separator);
        }
    }
    return (*this)[name];
}

Variable &Record::appendToArray(const String &name, Value *value)
{
    DE_GUARD(d);

    if (!has(name))
    {
        return addArray(name, new ArrayValue({ value }));
    }

    Variable &var = (*this)[name];
    DE_ASSERT(is<ArrayValue>(var.value()));
    var.value<ArrayValue>().add(value);
    return var;
}

Variable &Record::insertToSortedArray(const String &name, Value *value)
{
    DE_GUARD(d);

    if (!has(name))
    {
        return appendToArray(name, value);
    }

    Variable &var = (*this)[name];
    ArrayValue &array = var.value<ArrayValue>();
    // O(n) insertion sort.
    for (dsize i = 0; i < array.size(); ++i)
    {
        if (value->compare(array.at(i)) <= 0)
        {
            array.insert(i, value);
            return var;
        }
    }
    // Value is larger than everything in the array.
    array.add(value);
    return var;
}

Variable &Record::operator [] (const String &name)
{
    return const_cast<Variable &>((*const_cast<const Record *>(this))[name]);
}

const Variable &Record::operator [] (const String &name) const
{
    // Path notation allows looking into subrecords.
    const Variable *found = d->findMemberByPath(name);
    if (found)
    {
        return *found;
    }
    throw NotFoundError("Record::operator []", "Variable '" + name + "' not found");
}

Variable *Record::tryFind(const String &name)
{
    return const_cast<Variable *>(d->findMemberByPath(name));
}

const Variable *Record::tryFind(const String &name) const
{
    return d->findMemberByPath(name);
}

Record &Record::subrecord(const String &name)
{
    return const_cast<Record &>((const_cast<const Record *>(this))->subrecord(name));
}

const Record &Record::subrecord(const String &name) const
{
    // Path notation allows looking into subrecords.
    if (auto pos = name.indexOf('.'))
    {
        return subrecord(name.left(pos)).subrecord(name.substr(pos + 1));
    }

    Members::const_iterator found = d->members.find(name);
    if (found != d->members.end() && d->isSubrecord(*found->second))
    {
        return *found->second->value<RecordValue>().record();
    }
    throw NotFoundError("Record::subrecord", stringf("Subrecord '%s' not found", name.c_str()));
}

dsize Record::size() const
{
    return dsize(d->members.size());
}

const Record::Members &Record::members() const
{
    return d->members;
}

LoopResult Record::forMembers(std::function<LoopResult (const String &, Variable &)> func)
{
    for (auto i = d->members.begin(); i != d->members.end(); ++i)
    {
        if (auto result = func(i->first, *i->second))
        {
            return result;
        }
    }
    return LoopContinue;
}

LoopResult Record::forMembers(std::function<LoopResult (const String &, const Variable &)> func) const
{
    for (Members::const_iterator i = d->members.begin(); i != d->members.end(); ++i)
    {
        if (auto result = func(i->first, *i->second))
        {
            return result;
        }
    }
    return LoopContinue;
}

Record::Subrecords Record::subrecords() const
{
    return d->listSubrecords([] (const Record &) { return true; /* unfiltered */ });
}

Record::Subrecords Record::subrecords(std::function<bool (const Record &)> filter) const
{
    return d->listSubrecords([&] (const Record &rec)
    {
        return filter(rec);
    });
}

LoopResult Record::forSubrecords(std::function<LoopResult (const String &, Record &)> func)
{
    return d->forSubrecords(func);
}

LoopResult Record::forSubrecords(std::function<LoopResult (const String &, const Record &)> func) const
{
    return d->forSubrecords([func] (const String &name, Record &rec)
    {
        return func(name, rec);
    });
}

bool Record::anyMembersChanged() const
{
    DE_GUARD(d);

    const auto *const_d = d.getConst();
    for (auto i = const_d->members.begin(); i != const_d->members.end(); ++i)
    {
        if (d->isSubrecord(*i->second))
        {
            if (i->second->valueAsRecord().anyMembersChanged())
            {
                return true;
            }
        }
        else if (i->second->flags() & Variable::ValueHasChanged)
        {
            return true;
        }
    }
    return false;
}

void Record::markAllMembersUnchanged()
{
    DE_GUARD(d);

    for (auto i = d->members.begin(); i != d->members.end(); ++i)
    {
        i->second->setFlags(Variable::ValueHasChanged, UnsetFlags);

        if (d->isSubrecord(*i->second))
        {
            i->second->valueAsRecord().markAllMembersUnchanged();
        }
    }
}

String Record::asText(const String &prefix, List<KeyValue> *lines) const
{
    DE_GUARD(d);

    /*
    // If this is a module, don't print out the entire contents.
    /// @todo Should only apply to actual modules. -jk
    if (!gets(VAR_FILE, "").isEmpty())
    {
        return QString("(Record imported from \"%1\")").arg(gets(VAR_FILE));
    }*/

    // Recursive calls to collect all variables in the record.
    if (lines)
    {
        // Collect lines from this record.
        for (Members::const_iterator i = d->members.begin(); i != d->members.end(); ++i)
        {
            const char *separator = d->isSubrecord(*i->second)? "." : ":";
            String subContent = i->second->value().asText();

            // If the content is very long, shorten it.
            int numberOfLines = subContent.count('\n');
            if (numberOfLines > SUBRECORD_CONTENT_EXCERPT_THRESHOLD)
            {
                subContent = Stringf("(%i lines)", numberOfLines);
            }

            KeyValue kv(prefix + i->first + separator, subContent);
            lines->push_back(kv);
        }
        return "";
    }

    // Top level of the recursion.
    List<KeyValue> allLines;
    Vec2ui maxLength;

    // Collect.
    asText(prefix, &allLines);

    // Sort and find maximum length.
    std::sort(allLines.begin(), allLines.end());
    for (auto i = allLines.cbegin(); i != allLines.cend(); ++i)
    {
        maxLength = maxLength.max(Vec2ui(i->first.size(), i->second.size()));
    }

    std::ostringstream os;
    //os << std::left << std::setfill(' ');

    const String indent = "\n" + String(maxLength.x, ' ');

    // Print aligned.
    bool first = true;
    for (auto i = allLines.begin(); i != allLines.end(); ++i)
    {
        if (first)
        {
            first = false;
        }
        else
        {
            os << "\n";
        }

        // The key.
        os << i->first;
        if (i->first.size() < maxLength.x)
        {
            os << String(maxLength.x - i->first.size(), ' ');
        }

        // Print the value line by line.
        for (const CString &valueLine : i->second.splitRef('\n'))
        {
            if (valueLine.begin() > i->second.data())
            {
                os << indent;
            }
            os << valueLine;
        }

        /*String::BytePos pos{0};
        while (pos)
        {
            auto next = i->second.indexOf("\n", pos);
            if (pos > 0)
            {
                os << std::setw(maxLength.x + extra) << "";
            }
            os << i->second.substr(pos, (next ? (next - pos + 1) : next).index);
            pos = next;
            if (pos) pos++;
        }*/

    }

    return os.str();
}

const Function &Record::function(const String &name) const
{
    return (*this)[name].value<FunctionValue>().function();
}

void Record::addSuperRecord(Value *superValue)
{
    DE_GUARD(d);

    if (!has(VAR_SUPER))
    {
        addArray(VAR_SUPER);
    }
    (*this)[VAR_SUPER].array().add(superValue);
}

void Record::addSuperRecord(const Record &superRecord)
{
    addSuperRecord(new RecordValue(superRecord));
}

void Record::operator >> (Writer &to) const
{
    DE_GUARD(d);

    to << d->uniqueId << duint32(d->members.size());
    for (const auto &i : d->members)
    {
        to << *i.second;
    }
}

void Record::operator << (Reader &from)
{
    LOG_AS("Record deserialization");

    duint32 count = 0;
    from >> d->oldUniqueId >> count;
    clear();

    Impl::RefMap refMap;
    refMap.insert(d->oldUniqueId, this);

    while (count-- > 0)
    {
        std::unique_ptr<Variable> var(new Variable());
        from >> *var;

        RecordValue *recVal = dynamic_cast<RecordValue *>(&var->value());
        if (recVal && recVal->usedToHaveOwnership())
        {
            DE_ASSERT(recVal->record());

            // This record was a subrecord prior to serializing.
            // Let's remember the record for reconnecting other variables
            // that might be referencing it.
            refMap.insert(recVal->record()->d->oldUniqueId, recVal->record());
        }

        add(var.release());
    }

    // Find referenced records and relink them to their original targets.
    d->reconnectReferencesAfterDeserialization(refMap);

#ifdef DE_DEBUG
    DE_FOR_EACH(Members, i, d->members)
    {
        DE_ASSERT(i->second->audienceForDeletion().contains(d));
    }
#endif
}

Record &Record::operator << (const NativeFunctionSpec &spec)
{
    addFunction(spec.name(), refless(spec.make())).setReadOnly();
    return *this;
}

const Record &Record::parentRecordForMember(const String &name) const
{
    const String lastOmitted = name.fileNamePath('.');
    if (lastOmitted.isEmpty()) return *this;

    // Omit the final segment of the dotted path to find out the parent record.
    return (*this)[lastOmitted];
}

String Record::asInfo() const
{
    String out;
    for (const auto &i : d->members)
    {
        if (out) out += "\n";

        const Variable &var = *i.second;
        String src = i.first;

        if (is<RecordValue>(var.value()))
        {
            src += " {\n" + var.valueAsRecord().asInfo();
            src.replace("\n", "\n    ");
            src += "\n}";
        }
        else if (is<ArrayValue>(var.value()))
        {
            src += " " + var.value<ArrayValue>().asInfo();
        }
        else
        {
            String valueText = var.value().asText();
            if (valueText.contains("\n"))
            {
                src += " = " + Info::quoteString(var.value().asText());
            }
            else
            {
                src += ": " + valueText;
            }
        }        
    }
    return out;
}

std::ostream &operator<<(std::ostream &os, const Record &record)
{
    return os << record.asText();
}

CompiledRecord::~CompiledRecord()
{}

} // namespace de
