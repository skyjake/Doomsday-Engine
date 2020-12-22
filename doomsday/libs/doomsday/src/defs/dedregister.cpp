/** @file dedregister.h  Register of definitions.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "doomsday/defs/dedregister.h"
#include "doomsday/defs/definition.h"

#include <de/arrayvalue.h>
#include <de/dictionaryvalue.h>
#include <de/textvalue.h>
#include <de/recordvalue.h>
#include <de/regexp.h>
#include <de/set.h>
#include <de/keymap.h>

using namespace de;

static const String VAR_ORDER = "order";

DE_PIMPL(DEDRegister)
, DE_OBSERVES(Record, Deletion)
, DE_OBSERVES(Record, Addition)
, DE_OBSERVES(Record, Removal)
, DE_OBSERVES(Variable, ChangeFrom)
{
    Record *names;
    ArrayValue *orderArray;
    struct Key {
        LookupFlags flags;
        Key(const LookupFlags &f = DefaultLookup) : flags(f) {}
    };
    typedef KeyMap<String, Key> Keys;
    Keys keys;
    KeyMap<Variable *, Record *> parents;

    Impl(Public *i, Record &rec) : Base(i), names(&rec)
    {
        names->audienceForDeletion() += this;

        // The definitions will be stored here in the original order.
        orderArray = &names->addArray(VAR_ORDER).array();
    }

    void recordBeingDeleted(Record &DE_DEBUG_ONLY(record))
    {
        DE_ASSERT(names == &record);
        names = nullptr;
        orderArray = nullptr;
    }

    void clear()
    {
        // As a side-effect, the lookups will be cleared, too, as the members of
        // each definition record are deleted.
        order().clear();

#ifdef DE_DEBUG
        DE_ASSERT(parents.isEmpty());
        for (const auto &k : keys)
        {
            DE_ASSERT(lookup(k.first).size() == 0);
        }
#endif
    }

    void addKey(const String &name, const LookupFlags &flags)
    {
        keys.insert(name, Key(flags));
        names->addDictionary(name + "Lookup");
    }

    ArrayValue &order()
    {
        DE_ASSERT(orderArray != nullptr);
        return *orderArray;
    }

    const ArrayValue &order() const
    {
        DE_ASSERT(orderArray != nullptr);
        return *orderArray;
    }

    DictionaryValue &lookup(const String &keyName)
    {
        return (*names)[keyName + "Lookup"].value<DictionaryValue>();
    }

    const DictionaryValue &lookup(const String &keyName) const
    {
        return (*names)[keyName + "Lookup"].value<DictionaryValue>();
    }

    template <typename Type>
    Type lookupOperation(const String &key, String value,
                         std::function<Type (const DictionaryValue &, String)> operation) const
    {
        auto foundKey = keys.find(key);
        if (foundKey == keys.end()) return Type{0};

        if (!foundKey->second.flags.testFlag(CaseSensitive))
        {
            // Case insensitive lookup is done in lower case.
            value = value.lower();
        }

        return operation(lookup(key), value);
    }

    const Record *tryFind(const String &key, const String &value) const
    {
        return lookupOperation<const Record *>(key, value,
            [] (const DictionaryValue &lut, String v) -> const Record * {
                TextValue const val(v);
                auto i = lut.elements().find(DictionaryValue::ValueRef(&val));
                if (i == lut.elements().end()) return nullptr; // Value not in dictionary.
                return i->second->as<RecordValue>().record();
            });
    }

    bool has(const String &key, const String &value) const
    {
        return lookupOperation<bool>(key, value, [] (const DictionaryValue &lut, String v) {
            return lut.contains(TextValue(v));
        });
    }

    Record &append()
    {
        Record *sub = new Record;

        // Let each subrecord know their ordinal.
        sub->set(defn::Definition::VAR_ORDER, int(order().size())).setReadOnly();

        // Observe what goes into this record.
        //sub->audienceForDeletion() += this;
        sub->audienceForAddition() += this;
        sub->audienceForRemoval()  += this;

        order().add(new RecordValue(sub, RecordValue::OwnsRecord));
        return *sub;
    }

    bool isEmptyKeyValue(const Value &value) const
    {
        return is<TextValue>(value) && value.asText().isEmpty();
    }

    bool isValidKeyValue(const Value &value) const
    {
        // Empty strings are not indexable.
        if (isEmptyKeyValue(value)) return false;
        return true;
    }

    /// Returns @c true if the value was added.
    bool addToLookup(const String &key, const Value &value, Record &def)
    {
        if (!isValidKeyValue(value))
            return false;

        String valText = value.asText();
        DE_ASSERT(!valText.isEmpty());
        DE_ASSERT(keys.contains(key));

        if (!keys[key].flags.testFlag(CaseSensitive))
        {
            valText = valText.lower();
        }

        DictionaryValue &dict = lookup(key);

        if (keys[key].flags.testFlag(OnlyFirst))
        {
            // Only index the first one that is found.
            if (dict.contains(TextValue(valText))) return false;
        }

        // Index definition using its current value.
        dict.add(new TextValue(valText), new RecordValue(&def));
        return true;
    }

    bool removeFromLookup(const String &key, const Value &value, Record &def)
    {
        //qDebug() << "removeFromLookup" << key << value.asText() << &def;

        if (!isValidKeyValue(value))
            return false;

        String valText = value.asText();
        DE_ASSERT(!valText.isEmpty());
        DE_ASSERT(keys.contains(key));

        if (!keys[key].flags.testFlag(CaseSensitive))
        {
            valText = valText.lower();
        }

        DictionaryValue &dict = lookup(key);

        // Remove from the index.
        if (dict.contains(TextValue(valText)))
        {
            const Value &indexed = dict.element(TextValue(valText));
            //qDebug() << " -" << indexed.as<RecordValue>().record() << &def;
            const Record *indexedDef = indexed.as<RecordValue>().record();
            if (!indexedDef || indexedDef == &def)
            {
                // This is the definition that was indexed using the key value.
                // Let's remove it.
                dict.remove(TextValue(valText));

                /// @todo Should now index any other definitions with this key value;
                /// needs to add a lookup of which other definitions have this value.
                return true;
            }
        }

        // There was some other definition indexed using this key.
        return false;
    }

    void recordMemberAdded(Record &def, Variable &key)
    {
        // Keys must be observed so that they are indexed in the lookup table.
        if (keys.contains(key.name()))
        {
            // Index definition using its current value.
            // Observe empty keys so we'll get the key's value when it's set.
            if (addToLookup(key.name(), key.value(), def) ||
               isEmptyKeyValue(key.value()))
            {
                parents.insert(&key, &def);
                key.audienceForChangeFrom() += this;
            }
        }
    }

    void recordMemberRemoved(Record &def, Variable &key)
    {
        if (keys.contains(key.name()))
        {
            key.audienceForChangeFrom() -= this;
            parents.remove(&key);
            removeFromLookup(key.name(), key.value(), def);
        }
    }

    void variableValueChangedFrom(Variable &key, const Value &oldValue, const Value &newValue)
    {
        //qDebug() << "changed" << key.name() << "from" << oldValue.asText() << "to"
        //         << newValue.asText();

        DE_ASSERT(parents.contains(&key));

        // The value of a key has changed, so it needs to be reindexed.
        removeFromLookup(key.name(), oldValue, *parents[&key]);
        addToLookup(key.name(), newValue, *parents[&key]);
    }
};

DEDRegister::DEDRegister(Record &names) : d(new Impl(this, names))
{}

void DEDRegister::addLookupKey(const String &variableName, const LookupFlags &flags)
{
    d->addKey(variableName, flags);
}

void DEDRegister::clear()
{
    d->clear();
}

Record &DEDRegister::append()
{
    return d->append();
}

Record &DEDRegister::appendCopy(int index)
{
    return copy(index, append());
}

Record &DEDRegister::copy(int fromIndex, Record &to)
{
    StringList omitted;
    omitted << "__.*"; // double-underscore

    // By default lookup keys are not copied, as they are used as identifiers and
    // therefore duplicates should not occur.
    for (const auto &i : d->keys)
    {
        if (i.second.flags.testFlag(AllowCopy)) continue;
        omitted << i.first;
    }

    return to.assign((*this)[fromIndex], RegExp(String::join(omitted, "|")));
}

int DEDRegister::size() const
{
    return d->order().size();
}

bool DEDRegister::has(const String &key, const String &value) const
{
    return d->has(key, value);
}

Record &DEDRegister::operator [] (int index)
{
    return *d->order().at(index).as<RecordValue>().record();
}

const Record &DEDRegister::operator [] (int index) const
{
    return *d->order().at(index).as<RecordValue>().record();
}

Record *DEDRegister::tryFind(const String &key, const String &value)
{
    return const_cast<Record *>(d->tryFind(key, value));
}

const Record *DEDRegister::tryFind(const String &key, const String &value) const
{
    return d->tryFind(key, value);
}

Record &DEDRegister::find(const String &key, const String &value)
{
    return const_cast<Record &>(const_cast<const DEDRegister *>(this)->find(key, value));
}

const Record &DEDRegister::find(const String &key, const String &value) const
{
    if (!d->keys.contains(key))
    {
        throw UndefinedKeyError("DEDRegister::find", "Key '" + key + "' not defined");
    }
    const Record *rec = tryFind(key, value);
    if (!rec)
    {
        throw NotFoundError("DEDRegister::find", key + " '" + value + "' not found");
    }
    return *rec;
}

const DictionaryValue &DEDRegister::lookup(const String &key) const
{
    if (!d->keys.contains(key))
    {
        throw UndefinedKeyError("DEDRegister::lookup", "Key '" + key + "' not defined");
    }
    return d->lookup(key);
}
