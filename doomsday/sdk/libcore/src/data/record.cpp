/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <functional>

namespace de {

/// When converting records to a human-readable text representation, this is the
/// maximum number of lines that a subrecord can have before it is shown as a short
/// excerpt.
int const SUBRECORD_CONTENT_EXCERPT_THRESHOLD = 100; // lines

String const Record::VAR_SUPER = "__super__";
String const Record::VAR_FILE  = "__file__";
String const Record::VAR_NATIVE_SELF = "__self__";

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

    typedef QHash<duint32, Record *> RefMap;

    Impl(Public &r)
        : Base(r)
        , uniqueId(++recordIdCounter)
        , oldUniqueId(0)
    {}

    struct ExcludeByBehavior {
        Behavior behavior;
        ExcludeByBehavior(Behavior b) : behavior(b) {}
        bool operator () (Variable const &member) {
            return (behavior == IgnoreDoubleUnderscoreMembers &&
                    member.name().startsWith("__"));
        }
    };

    struct ExcludeByRegExp {
        QRegExp omitted;
        ExcludeByRegExp(QRegExp const &omit) : omitted(omit) {}
        bool operator () (Variable const &member) {
            return omitted.exactMatch(member.name());
        }
    };

    template <typename Predicate>
    void clear(Predicate excluded)
    {
        if (!members.empty())
        {
            Record::Members remaining; // Contains all members that are not removed.

            DENG2_FOR_EACH(Members, i, members)
            {
                if (excluded(*i.value()))
                {
                    remaining.insert(i.key(), i.value());
                    continue;
                }

                DENG2_FOR_PUBLIC_AUDIENCE2(Removal, o) o->recordMemberRemoved(self, **i);

                i.value()->audienceForDeletion() -= self;
                delete i.value();
            }

            members = remaining;
        }
    }

    template <typename Predicate>
    void copyMembersFrom(Record const &other, Predicate excluded)
    {
        DENG2_FOR_EACH_CONST(Members, i, other.d->members)
        {
            if (excluded(*i.value())) continue;

            bool const alreadyExists = members.contains(i.key());

            Variable *var = new Variable(*i.value());
            var->audienceForDeletion() += self;
            members[i.key()] = var;

            if (!alreadyExists)
            {
                // Notify about newly added members.
                DENG2_FOR_PUBLIC_AUDIENCE2(Addition, i) i->recordMemberAdded(self, *var);
            }

            /// @todo Should also notify if the value of an existing variable changes. -jk
        }
    }

    bool isRecord(Variable const &var) const
    {
        RecordValue const *value = var.value().maybeAs<RecordValue>();
        return value && value->record();
    }

    bool isSubrecord(Variable const &var) const
    {
        // Subrecords are owned by this record.
        RecordValue const *value = var.value().maybeAs<RecordValue>();
        return value && value->record() && value->hasOwnership();
    }

    LoopResult forSubrecords(std::function<LoopResult (String const &, Record &)> func) const
    {
        Members const unmodifiedMembers = members; // In case a callback removes members.
        DENG2_FOR_EACH_CONST(Members, i, unmodifiedMembers)
        {
            Variable const &member = *i.value();
            if (isSubrecord(member))
            {
                Record *rec = member.value().as<RecordValue>().record();
                DENG2_ASSERT(rec != 0); // subrecords are owned, so cannot have been deleted

                if (auto result = func(i.key(), *rec))
                {
                    return result;
                }
            }
        }
        return LoopContinue;
    }

    Record::Subrecords listSubrecords(std::function<bool (Record const &)> filter) const
    {
        Subrecords subs;
        forSubrecords([&subs, filter] (String const &name, Record &rec)
        {
            // Must pass the filter.
            if (filter(rec))
            {
                subs.insert(name, &rec);
            }
            return LoopContinue;
        });
        return subs;
    }

    Variable const *findMemberByPath(String const &name) const
    {
        // Path notation allows looking into subrecords.
        int pos = name.indexOf('.');
        if (pos >= 0)
        {
            String subName = name.substr(0, pos);
            String remaining = name.substr(pos + 1);
            // If it is a subrecord we can descend into it.
            if (!self.hasRecord(subName)) return 0;
            return self[subName].value<RecordValue>().dereference().d->findMemberByPath(remaining);
        }

        Members::const_iterator found = members.constFind(name);
        if (found != members.constEnd())
        {
            return found.value();
        }

        return 0;
    }

    /**
     * Returns the record inside which the variable identified by path @a name
     * resides. The necessary subrecords are created if they don't exist.
     *
     * @param pathOrName  Variable name or path.
     *
     * @return  Parent record for the variable.
     */
    Record &parentRecordByPath(String const &pathOrName)
    {
        int pos = pathOrName.indexOf('.');
        if (pos >= 0)
        {
            String subName = pathOrName.substr(0, pos);
            String remaining = pathOrName.substr(pos + 1);
            Record *rec = 0;

            if (!self.hasSubrecord(subName))
            {
                // Create it now.
                rec = &self.addSubrecord(subName);
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
            RecordValue *value = dynamic_cast<RecordValue *>(&i.value()->value());
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

    DENG2_PIMPL_AUDIENCE(Deletion)
    DENG2_PIMPL_AUDIENCE(Addition)
    DENG2_PIMPL_AUDIENCE(Removal)
};

DENG2_AUDIENCE_METHOD(Record, Deletion)
DENG2_AUDIENCE_METHOD(Record, Addition)
DENG2_AUDIENCE_METHOD(Record, Removal)

Record::Record() : RecordAccessor(this), d(new Impl(*this))
{}

Record::Record(Record const &other, Behavior behavior)
    : RecordAccessor(this)
    , d(new Impl(*this))
{
    copyMembersFrom(other, behavior);
}

Record::~Record()
{
    // Notify before deleting members so that observers have full visibility
    // to the record prior to deletion.
    DENG2_FOR_AUDIENCE2(Deletion, i) i->recordBeingDeleted(*this);

    clear();
}

void Record::clear(Behavior behavior)
{
    d->clear(Impl::ExcludeByBehavior(behavior));
}

void Record::copyMembersFrom(Record const &other, Behavior behavior)
{
    d->copyMembersFrom(other, Impl::ExcludeByBehavior(behavior));
}

Record &Record::operator = (Record const &other)
{
    return assign(other);
}

Record &Record::assign(Record const &other, Behavior behavior)
{
    clear(behavior);
    copyMembersFrom(other, behavior);
    return *this;
}

Record &Record::assign(Record const &other, QRegExp const &excluded)
{
    d->clear(Impl::ExcludeByRegExp(excluded));
    d->copyMembersFrom(other, Impl::ExcludeByRegExp(excluded));
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
    return found? d->isSubrecord(*found) : false;
}

bool Record::hasRecord(String const &recordName) const
{
    Variable const *found = d->findMemberByPath(recordName);
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
    if (hasMember(variable->name()))
    {
        // Delete the previous variable with this name.
        delete d->members[variable->name()];
    }
    var->audienceForDeletion() += this;
    d->members[variable->name()] = var.release();

    DENG2_FOR_AUDIENCE2(Addition, i) i->recordMemberAdded(*this, *variable);

    return *variable;
}

Variable *Record::remove(Variable &variable)
{
    variable.audienceForDeletion() -= this;
    d->members.remove(variable.name());

    DENG2_FOR_AUDIENCE2(Removal, i) i->recordMemberRemoved(*this, variable);

    return &variable;
}

Variable *Record::remove(String const &variableName)
{
    return remove((*this)[variableName]);
}

Variable &Record::add(String const &name)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name)));
}

Variable &Record::addNumber(String const &name, Value::Number const &number)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new NumberValue(number), Variable::AllowNumber));
}

Variable &Record::addBoolean(String const &name, bool booleanValue)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new NumberValue(booleanValue, NumberValue::Boolean),
                              Variable::AllowNumber));
}

Variable &Record::addText(String const &name, Value::Text const &text)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new TextValue(text), Variable::AllowText));
}

Variable &Record::addTime(String const &name, Time const &time)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new TimeValue(time), Variable::AllowTime));
}

Variable &Record::addArray(String const &name, ArrayValue *array)
{
    // Automatically create an empty array if one is not provided.
    if (!array) array = new ArrayValue;
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              array, Variable::AllowArray));
}

Variable &Record::addDictionary(String const &name)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new DictionaryValue, Variable::AllowDictionary));
}

Variable &Record::addBlock(String const &name)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new BlockValue, Variable::AllowBlock));
}

Variable &Record::addFunction(const String &name, Function *func)
{
    return d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new FunctionValue(func), Variable::AllowFunction));
}

Record &Record::add(String const &name, Record *subrecord)
{
    std::unique_ptr<Record> sub(subrecord);
    d->parentRecordByPath(name)
            .add(new Variable(Impl::memberNameFromPath(name),
                              new RecordValue(sub.release(), RecordValue::OwnsRecord)));
    return *subrecord;
}

Record &Record::addSubrecord(String const &name, SubrecordAdditionBehavior behavior)
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

Record *Record::removeSubrecord(String const &name)
{
    Members::const_iterator found = d->members.find(name);
    if (found != d->members.end() && d->isSubrecord(*found.value()))
    {
        Record *returnedToCaller = found.value()->value().as<RecordValue>().takeRecord();
        remove(*found.value());
        return returnedToCaller;
    }
    throw NotFoundError("Record::remove", "Subrecord '" + name + "' not found");
}

Variable &Record::set(String const &name, bool value)
{
    if (hasMember(name))
    {
        return (*this)[name].set(NumberValue(value));
    }
    return addBoolean(name, value);
}

Variable &Record::set(String const &name, char const *value)
{
    if (hasMember(name))
    {
        return (*this)[name].set(TextValue(value));
    }
    return addText(name, value);
}

Variable &Record::set(String const &name, Value::Text const &value)
{
    if (hasMember(name))
    {
        return (*this)[name].set(TextValue(value));
    }
    return addText(name, value);
}

Variable &Record::set(String const &name, Value::Number const &value)
{
    if (hasMember(name))
    {
        return (*this)[name].set(NumberValue(value));
    }
    return addNumber(name, value);
}

Variable &Record::set(String const &name, dint32 value)
{
    return set(name, Value::Number(value));
}

Variable &Record::set(String const &name, duint32 value)
{
    return set(name, Value::Number(value));
}

Variable &Record::set(String const &name, dsize value)
{
    return set(name, Value::Number(value));
}

Variable &Record::set(String const &name, ArrayValue *value)
{
    if (hasMember(name))
    {
        return (*this)[name].set(value);
    }
    return addArray(name, value);
}

Variable &Record::appendWord(String const &name, String const &word, String const &separator)
{
    String value = gets(name, "");
    if (!value.isEmpty()) value.append(separator);
    set(name, value + word);
    return (*this)[name];
}

Variable &Record::appendUniqueWord(String const &name, String const &word, String const &separator)
{
    QRegExp re(QString("\\b%1\\b").arg(word));
    String const value = gets(name, "");
    if (re.indexIn(value) < 0)
    {
        appendWord(name, word, separator);
    }
    return (*this)[name];
}

Variable &Record::operator [] (String const &name)
{
    return const_cast<Variable &>((*const_cast<Record const *>(this))[name]);
}

Variable const &Record::operator [] (String const &name) const
{
    // Path notation allows looking into subrecords.
    Variable const *found = d->findMemberByPath(name);
    if (found)
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
    if (pos >= 0)
    {
        return subrecord(name.substr(0, pos)).subrecord(name.substr(pos + 1));
    }

    Members::const_iterator found = d->members.find(name);
    if (found != d->members.end() && d->isSubrecord(*found.value()))
    {
        return *found.value()->value().as<RecordValue>().record();
    }
    throw NotFoundError("Record::subrecord", "Subrecord '" + name + "' not found");
}

Record::Members const &Record::members() const
{
    return d->members;
}

Record::Subrecords Record::subrecords() const
{
    return d->listSubrecords([] (Record const &) { return true; /* unfiltered */ });
}

Record::Subrecords Record::subrecords(std::function<bool (Record const &)> filter) const
{
    return d->listSubrecords([&] (Record const &rec)
    {
        return filter(rec);
    });
}

LoopResult Record::forSubrecords(std::function<LoopResult (String const &, Record &)> func)
{
    return d->forSubrecords(func);
}

LoopResult Record::forSubrecords(std::function<LoopResult (String const &, Record const &)> func) const
{
    return d->forSubrecords([func] (String const &name, Record &rec)
    {
        return func(name, rec);
    });
}

String Record::asText(String const &prefix, List *lines) const
{
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
            String separator = (d->isSubrecord(*i.value())? "." : ":");
            String subContent = i.value()->value().asText();

            // If the content is very long, shorten it.
            int numberOfLines = subContent.count(QChar('\n'));
            if (numberOfLines > SUBRECORD_CONTENT_EXCERPT_THRESHOLD)
            {
                subContent = QString("(%1 lines)").arg(numberOfLines);
            }

            KeyValue kv(prefix + i.key() + separator, subContent);
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
    qSort(allLines);
    for (List::iterator i = allLines.begin(); i != allLines.end(); ++i)
    {
        maxLength = maxLength.max(Vector2ui(i->first.size(), i->second.size()));
    }

    os.setFieldAlignment(QTextStream::AlignLeft);

    // Print aligned.
    for (List::iterator i = allLines.begin(); i != allLines.end(); ++i)
    {
        int extra = 0;
        if (i != allLines.begin()) os << "\n";
        os << qSetFieldWidth(maxLength.x) << i->first << qSetFieldWidth(0);
        // Print the value line by line.
        int pos = 0;
        while (pos >= 0)
        {
            int next = i->second.indexOf('\n', pos);
            if (pos > 0)
            {
                os << qSetFieldWidth(maxLength.x + extra) << "" << qSetFieldWidth(0);
            }
            os << i->second.substr(pos, next != String::npos? next - pos + 1 : next);
            pos = next;
            if (pos != String::npos) pos++;
        }
    }

    return result;
}

Function const &Record::function(String const &name) const
{
    return (*this)[name].value<FunctionValue>().function();
}

void Record::addSuperRecord(Value *superValue)
{
    if (!has(VAR_SUPER))
    {
        addArray(VAR_SUPER);
    }
    (*this)[VAR_SUPER].array().add(superValue);
}

void Record::addSuperRecord(Record const &superRecord)
{
    addSuperRecord(new RecordValue(superRecord));
}

void Record::operator >> (Writer &to) const
{
    to << d->uniqueId << duint32(d->members.size());
    DENG2_FOR_EACH_CONST(Members, i, d->members)
    {
        to << *i.value();
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
        QScopedPointer<Variable> var(new Variable());
        from >> *var;

        RecordValue *recVal = dynamic_cast<RecordValue *>(&var->value());
        if (recVal && recVal->usedToHaveOwnership())
        {
            DENG2_ASSERT(recVal->record());

            // This record was a subrecord prior to serializing.
            // Let's remember the record for reconnecting other variables
            // that might be referencing it.
            refMap.insert(recVal->record()->d->oldUniqueId, recVal->record());
        }

        add(var.take());
    }

    // Find referenced records and relink them to their original targets.
    d->reconnectReferencesAfterDeserialization(refMap);

#ifdef DENG2_DEBUG
    DENG2_FOR_EACH(Members, i, d->members)
    {
        DENG2_ASSERT(i.value()->audienceForDeletion().contains(this));
    }
#endif
}

void Record::variableBeingDeleted(Variable &variable)
{
    DENG2_ASSERT(d->findMemberByPath(variable.name()) != 0);

    LOG_TRACE_DEBUGONLY("Variable %p deleted, removing from Record %p", &variable << this);

    // Remove from our index.
    d->members.remove(variable.name());
}

Record &Record::operator << (NativeFunctionSpec const &spec)
{
    addFunction(spec.name(), refless(spec.make())).setReadOnly();
    return *this;
}

Record const &Record::parentRecordForMember(String const &name) const
{
    String const lastOmitted = name.fileNamePath('.');
    if (lastOmitted.isEmpty()) return *this;

    // Omit the final segment of the dotted path to find out the parent record.
    return (*this)[lastOmitted];
}

QTextStream &operator << (QTextStream &os, Record const &record)
{
    return os << record.asText();
}

} // namespace de
