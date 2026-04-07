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

#ifndef LIBCORE_RECORD_H
#define LIBCORE_RECORD_H

#include "de/observers.h"
#include "de/hash.h"
#include "de/iserializable.h"
#include "de/list.h"
#include "de/log.h"
#include "de/recordaccessor.h"
#include "de/cstring.h"
#include "de/value.h"
#include "de/variable.h"

#include <functional>

namespace de {

class ArrayValue;
class Function;
class NativeFunctionSpec;

#define DE_ADD_NUMBER_CONSTANT(record, constant) \
    (record).addNumber(#constant, constant).setReadOnly()

/**
 * A set of variables. A record may have any number of subrecords. Note
 * that the members of a record do not have an order.
 *
 * A @em subrecord is a record that is owned by one of the members of the
 * main record. The ownership chain is as follows: Record -> Variable ->
 * RecordValue -> Record.
 *
 * @see http://en.wikipedia.org/wiki/Record_(computer_science)
 *
 * @par Thread-safety
 *
 * One Record instance can be accessed simultaneously from multiple threads.
 * Record locks its internal state when calling its methods.
 *
 * @ingroup data
 */
class DE_PUBLIC Record
        : public RecordAccessor
        , public ISerializable
        , public LogEntry::Arg::Base
{
public:
    /// Unknown variable name was given. @ingroup errors
    DE_ERROR(NotFoundError);

    /// All variables and subrecords in the record must have a name. @ingroup errors
    DE_ERROR(UnnamedError);

    static const char *VAR_SUPER; ///< Special variable that specifies super records.
    static const char *VAR_FILE; ///< Special variable that identifies the source file.
    static const char *VAR_INIT;
    static const char *VAR_NATIVE_SELF;

    typedef Hash<String, Variable *> Members;  // unordered
    typedef Hash<String, Record *> Subrecords; // unordered
    typedef std::pair<String, String> KeyValue;

    enum Behavior {
        AllMembers,
        IgnoreDoubleUnderscoreMembers
    };

    enum Flag
    {
        /// Assume that the Record will not be deleted until the application is terminated.
        /// Other objects will not need to observe the Record for deletion. Use this only
        /// for optimization purposes so that large audiences can be avoided.
        WontBeDeleted = 0x1,

        DefaultFlags = 0,
    };

    DE_AUDIENCE(Addition, void recordMemberAdded(Record &record, Variable &member))
    DE_AUDIENCE(Removal,  void recordMemberRemoved(Record &record, Variable &member))
    DE_AUDIENCE(Deletion, void recordBeingDeleted(Record &record))

public:
    Record();

    /**
     * Constructs a copy of another record.
     *
     * @param other     Record to copy.
     * @param behavior  Which members to copy.
     */
    Record(const Record &other, Behavior behavior = AllMembers);

    Record(Record &&moved);

    virtual ~Record();

    Record &setFlags(Flags flags, FlagOpArg op = SetFlags);

    Flags flags() const;

    /**
     * Deletes all the variables in the record.
     *
     * @param behavior  Clear behavior: which members to remove.
     */
    void clear(Behavior behavior = AllMembers);

    /**
     * Adds a copy of each member of another record into this record. The
     * previous contents of this record are untouched as long as they have no
     * members with the same names as in @a other.
     *
     * @param other     Record whose members are to be copied.
     * @param behavior  Copy behavior.
     */
    void copyMembersFrom(const Record &other, Behavior behavior = AllMembers);

    /**
     * Duplicates the contents of @a from into this record. Existing variables with
     * matching names are kept, with only their values changed. New variables are
     * added, and missing variables are removed from this record.
     *
     * Recursively called on subrecords.
     *
     * @param from      Source record.
     * @param behavior  Assignment behavior.
     */
    void assignPreservingVariables(const Record &from, Behavior behavior = AllMembers);

    /**
     * Assignment operator.
     * @return This record.
     */
    Record &operator = (const Record &other);

    /**
     * Move assignment operator.
     * @return This record.
     */
    Record &operator = (Record &&moved);

    /**
     * Assignment with specific behavior. All existing members in this record
     * are cleared (unless ignored due to @a behavior).
     *
     * @param other     Record to get variables from.
     * @param behavior  Which members to assign.
     *
     * @return This record.
     */
    Record &assign(const Record &other, Behavior behavior = AllMembers);

    /**
     * Partial assignment. All members matching @a excluded are ignored both in the
     * @a other record and this record.
     *
     * @param other     Record to get variables from.
     * @param excluded  Which members to exclude.
     *
     * @return This record.
     */
    Record &assign(const Record &other, const RegExp &excluded);

    /**
     * Determines if the record contains a variable or a subrecord named @a variableName.
     */
    bool has(const String &name) const;

    /**
     * Determines if the record contains a variable named @a variableName.
     */
    bool hasMember(const String &variableName) const;

    /**
     * Determines if the record contains a subrecord named @a subrecordName.
     * Subrecords are owned by this record.
     */
    bool hasSubrecord(const String &subrecordName) const;

    /**
     * Determines if the record contains a variable @a recordName that
     * references or owns a record. Records can be descended into with the
     * member (.) notation.
     *
     * @param recordName  Variable name.
     *
     * @return @c true if the variable points to a record.
     */
    bool hasRecord(const String &recordName) const;

    /**
     * Adds a new variable to the record.
     *
     * @param variable  Variable to add. Record gets ownership.
     *      The variable must have a name.
     *
     * @return @a variable, for convenience.
     */
    Variable &add(Variable *variable);

    /**
     * Removes a variable from the record.
     *
     * @param variable  Variable owned by the record.
     *
     * @return  Caller gets ownership of the removed variable.
     */
    Variable *remove(Variable &variable);

    /**
     * Removes a variable from the record.
     *
     * @param variableName  Name of the variable.
     *
     * @return  Caller gets ownership of the removed variable.
     */
    Variable *remove(const String &variableName);

    Variable *tryRemove(const String &variableName);

    /**
     * Removes all members whose name begins with @a prefix.
     * @param prefix  Prefix string.
     */
    void removeMembersWithPrefix(const String &prefix);

    /**
     * Adds a new variable to the record with a NoneValue. If there is an existing
     * variable with the given name, the old variable is deleted first.
     *
     * @param variableName  Name of the variable.
     *
     * @return  The new variable.
     */
    Variable &add(const String &variableName, Flags variableFlag = Variable::DefaultMode);

    /**
     * Adds a number variable to the record. The variable is set up to only accept
     * number values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     * @param number  Value of the variable.
     *
     * @return  The number variable.
     */
    Variable &addNumber(const String &variableName, Value::Number number);

    /**
     * Adds a number variable to the record with a Boolean semantic hint. The variable is
     * set up to only accept number values. An existing variable with the same name is
     * deleted first.
     *
     * @param variableName  Name of the variable.
     * @param booleanValue  Value of the variable (@c true or @c false).
     *
     * @return  The number variable.
     */
    Variable &addBoolean(const String &variableName, bool booleanValue);

    /**
     * Adds a text variable to the record. The variable is set up to only accept
     * text values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     * @param text  Value of the variable.
     *
     * @return  The text variable.
     */
    Variable &addText(const String &variableName, const Value::Text &text);

    Variable &addTime(const String &variableName, const Time &time);

    /**
     * Adds an array variable to the record. The variable is set up to only accept
     * array values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     * @param array         Value for the new variable (ownership taken). If not
     *                      provided, an empty array will be created for the variable.
     *
     * @return  The array variable.
     */
    Variable &addArray(const String &variableName, ArrayValue *array = nullptr);

    /**
     * Adds a dictionary variable to the record. The variable is set up to only accept
     * dictionary values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     *
     * @return  The dictionary variable.
     */
    Variable &addDictionary(const String &variableName);

    /**
     * Adds a block variable to the record. The variable is set up to only accept
     * block values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     *
     * @return  The block variable.
     */
    Variable &addBlock(const String &variableName);

    /**
     * Adds a function variable to the record. The variable is set up to only
     * accept function values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     * @param func          Function. The variable's value will hold a
     *                      reference to the Function; the caller may release
     *                      its reference afterwards.
     *
     * @return The function variable.
     */
    Variable &addFunction(const String &variableName, Function *func);

    /**
     * Adds a new subrecord to the record. Adds a variable named @a name and gives
     * ownership of @a subrecord to it. An existing subrecord with the same name is
     * deleted first.
     *
     * @param name  Name to use for the subrecord. This must be a valid variable name.
     * @param subrecord  Record to add. This record gets ownership
     *      of the subrecord.
     *
     * @return @a subrecord, for convenience.
     */
    Record &add(const String &name, Record *subrecord);

    enum SubrecordAdditionBehavior { ReplaceExisting, KeepExisting };

    /**
     * Adds a new empty subrecord to the record. Adds a variable named @a name and
     * creates a new record owned by it.
     *
     * The default behavior is to first delete an existing subrecord with the same name.
     *
     * Note that if @a name is empty and behavior is KeepExisting, the returned record
     * is `*this`.
     *
     * @param name      Name to use for the subrecord. This must be a valid variable name.
     * @param behavior  Addition behavior (keep or replace existing subrecord).
     *
     * @return  The new subrecord.
     */
    Record &addSubrecord(const String &name, SubrecordAdditionBehavior behavior = ReplaceExisting);

    /**
     * Removes a subrecord from the record.
     *
     * @param name  Name of subrecord owned by this record.
     *
     * @return  Caller gets ownership of the removed record.
     */
    Record *removeSubrecord(const String &name);

    /**
     * Sets the value of a variable, creating the variable if needed.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Value for the variable.
     *
     * @return Variable whose value was set.
     */
    Variable &set(const String &name, bool value);

    /// @copydoc set()
    Variable &set(const String &name, const char *value);

    /// @copydoc set()
    Variable &set(const String &name, const Value::Text &value);

    /// @copydoc set()
    Variable &set(const String &name, Value::Number value);

    /// @copydoc set()
    Variable &set(const String &name, const NumberValue &value);
    
    /// @copydoc set()
    Variable &set(const String &name, dint32 value);

    /// @copydoc set()
    Variable &set(const String &name, duint32 value);

    /// @copydoc set()
    Variable &set(const String &name, dint64 value);

    /// @copydoc set()
    Variable &set(const String &name, duint64 value);

    /// @copydoc set()
    //Variable &set(const String &name, unsigned long value);

    /// @copydoc set()
    Variable &set(const String &name, const Time &value);

    /// @copydoc set()
    Variable &set(const String &name, const Block &value);

    /// @copydoc set()
    Variable &set(const CString &name, const Block &value);

    /// @copydoc set()
    Variable &set(const String &name, const Record &value);

    /**
     * Sets the value of a variable, creating the variable if it doesn't exist.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Array to use as the value of the variable. Ownership taken.
     */
    Variable &set(const String &name, ArrayValue *value);

    Variable &set(const String &name, Value *value);

    Variable &set(const String &name, const Value &value);

    /**
     * Appends a word to the value of the variable.
     *
     * @param name       Name of the variable.
     * @param word       Word to append.
     * @param separator  Separator to append before the word, if the variable is not
     *                   currently empty.
     */
    Variable &appendWord(const String &name, const String &word, const String &separator = " ");

    Variable &appendUniqueWord(const String &name, const String &word, const String &separator = " ");

    Variable &appendMultipleUniqueWords(const String &name, const String &words, const String &separator = " ");

    Variable &appendToArray(const String &name, Value *value);

    /**
     * Inserts a value to an array variable. The array is assumed to be sorted, and the
     * insertion point is determined based on the sorting function.
     *
     * @param name   Name of the variable.
     * @param value  Value to insert.
     */
    Variable &insertToSortedArray(const String &name, Value *value);

    /**
     * Looks up a variable in the record. Variables in subrecords can be accessed
     * using the member notation: <code>subrecord-name.variable-name</code>
     *
     * @param name  Variable name.
     *
     * @return  Variable.
     */
    Variable &operator[](const String &name);

    /**
     * Looks up a variable in the record. Variables in subrecords can be accessed
     * using the member notation: <code>subrecord-name.variable-name</code>
     *
     * @param name  Variable name.
     *
     * @return  Variable (non-modifiable).
     */
    const Variable &operator[](const String &name) const;

    Variable *tryFind(const String &name);

    const Variable *tryFind(const String &name) const;

    inline Variable &member(const String &name) {
        return (*this)[name];
    }

    inline const Variable &member(const String &name) const {
        return (*this)[name];
    }

    /**
     * Looks up a subrecord in the record.
     *
     * @param name  Name of the subrecord.
     *
     * @return  Subrecord.
     */
    Record &subrecord(const String &name);

    /**
     * Looks up a subrecord in the record.
     *
     * @param name  Name of the subrecord.
     *
     * @return  Subrecord (non-modifiable).
     */
    const Record &subrecord(const String &name) const;

    dsize size() const;

    inline bool isEmpty() const { return size() == 0; }

    /**
     * Returns a non-modifiable map of the members.
     */
    const Members &members() const;

    LoopResult forMembers(std::function<LoopResult (const String &, Variable &)> func);

    LoopResult forMembers(std::function<LoopResult (const String &, const Variable &)> func) const;

    /**
     * Collects a map of all the subrecords present in the record.
     */
    Subrecords subrecords() const;

    /**
     * Collects a map of all subrecords that fulfill a given predicate.
     *
     * @param filter  Inclusion predicate: returns @c true, if the subrecord is to be
     *                included in the collection.
     *
     * @return Map of subrecords.
     */
    Subrecords subrecords(std::function<bool (const Record &)> filter) const;

    LoopResult forSubrecords(std::function<LoopResult (const String &, Record &)> func);

    LoopResult forSubrecords(std::function<LoopResult (const String &, const Record &)> func) const;

    /**
     * Checks if the value of any member variables have changed. The check is done
     * recursively in subrecords.
     *
     * @return At least one member variable or variable in a subrecord has changed
     * its value.
     */
    bool anyMembersChanged() const;

    void markAllMembersUnchanged();

    /**
     * Creates a text representation of the record. Each variable name is
     * prefixed with @a prefix.
     *
     * @param prefix  Prefix for each variable name.
     * @param lines  NULL (used for recursion into subrecords).
     *
     * @return  Text representation.
     */
    String asText(const String &prefix, List<KeyValue> *lines) const;

    /**
     * Convenience template for getting the value of a variable in a
     * specific type.
     *
     * @param name  Name of variable.
     *
     * @return  Value cast to a specific value type.
     */
    template <typename ValueType>
    const ValueType &value(const String &name) const {
        return (*this)[name].value<ValueType>();
    }

    /**
     * Convenience method for getting the Function referenced by a member.
     *
     * An exception is thrown if @a name is not found (NotFoundError) or does not have a
     * FunctionValue (Variable::TypeError).
     *
     * @param name  Name of member.
     *
     * @return  The function instance.
     */
    const Function &function(const String &name) const;

    /**
     * Adds a new record to be used as a superclass of this record.
     *
     * @param superValue  Value referencing the super record to add. Ownership taken.
     */
    void addSuperRecord(Value *superValue);

    /**
     * Adds a new record to be used as a superclass of this record.
     *
     * @param superRecord  Record to use as super record. A new RecordValue is
     *                     created to refer to this record.
     */
    void addSuperRecord(const Record &superRecord);

    /**
     * Adds a new native function to the record according to the specification.
     *
     * @param spec  Native function specification.
     *
     * @return  Reference to this record.
     */
    Record &operator << (const NativeFunctionSpec &spec);

    /**
     * Looks up the record that contains the variable referred to be @a name.
     * If @a name contains no '.' characters, always returns this record.
     *
     * @param name  Variable name.
     *
     * @return Record containing the @a name.
     */
    const Record &parentRecordForMember(const String &name) const;

    String asInfo() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::StringArgument; }
    String asText() const { return asText("", nullptr); }

    /*
     * Utility template for initializing a Record with an arbitrary number of
     * members and values.
     */
    Record &setMembers() { return *this; }

    template <typename NameType, typename ValueType, typename... Args>
    Record &setMembers(const NameType &name, const ValueType &valueType, Args... args)
    {
        set(name, valueType);
        return setMembers(args...);
    }

    template <typename... Args>
    static Record withMembers(Args... args)
    {
        return Record().setMembers(args...);
    }

private:
    DE_PRIVATE(d)
};

/// Converts the record into a human-readable text representation.
DE_PUBLIC std::ostream &operator << (std::ostream &os, const Record &record);

} // namespace de

#endif /* LIBCORE_RECORD_H */
