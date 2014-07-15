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

#ifndef LIBDENG2_RECORD_H
#define LIBDENG2_RECORD_H

#include "../ISerializable"
#include "../String"
#include "../Variable"
#include "../Value"
#include "../Audience"
#include "../Log"
#include "../RecordAccessor"

#include <QMap>
#include <QList>

namespace de {

class ArrayValue;
class Function;
class NativeFunctionSpec;

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
 * @ingroup data
 */
class DENG2_PUBLIC Record
        : public RecordAccessor
        , public ISerializable
        , public LogEntry::Arg::Base
        , DENG2_OBSERVES(Variable, Deletion)
{
public:
    /// Unknown variable name was given. @ingroup errors
    DENG2_ERROR(NotFoundError);

    /// All variables and subrecords in the record must have a name. @ingroup errors
    DENG2_ERROR(UnnamedError);

    /// Name of the special variable that specifies super records.
    static String const SUPER_NAME;

    typedef QMap<String, Variable *> Members;
    typedef QMap<String, Record *> Subrecords;
    typedef std::pair<String, String> KeyValue;
    typedef QList<KeyValue> List;

    enum Behavior {
        AllMembers,
        IgnoreDoubleUnderscoreMembers
    };

    DENG2_DEFINE_AUDIENCE2(Addition, void recordMemberAdded(Record &record, Variable &member))

    DENG2_DEFINE_AUDIENCE2(Removal, void recordMemberRemoved(Record &record, Variable &member))

    DENG2_DEFINE_AUDIENCE2(Deletion, void recordBeingDeleted(Record &record))

public:
    Record();

    /**
     * Constructs a copy of another record.
     *
     * @param other     Record to copy.
     * @param behavior  Which members to copy.
     */
    Record(Record const &other, Behavior behavior = AllMembers);

    virtual ~Record();

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
    void copyMembersFrom(Record const &other, Behavior behavior = AllMembers);

    /**
     * Assignment operator.
     * @return This record.
     */
    Record &operator = (Record const &other);

    /**
     * Assignment with specific behavior. All existing members in this record
     * are cleared (unless ignored due to @a behavior).
     *
     * @param behavior  Which members to assign.
     *
     * @return This record.
     */
    Record &assign(Record const &other, Behavior behavior = AllMembers);

    /**
     * Determines if the record contains a variable or a subrecord named @a variableName.
     */
    bool has(String const &name) const;

    /**
     * Determines if the record contains a variable named @a variableName.
     */
    bool hasMember(String const &variableName) const;

    /**
     * Determines if the record contains a subrecord named @a subrecordName.
     */
    bool hasSubrecord(String const &subrecordName) const;

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
    Variable *remove(String const &variableName);

    /**
     * Adds a new variable to the record with a NoneValue. If there is an existing
     * variable with the given name, the old variable is deleted first.
     *
     * @param variableName  Name of the variable.
     *
     * @return  The new variable.
     */
    Variable &add(String const &variableName);

    /**
     * Adds a number variable to the record. The variable is set up to only accept
     * number values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     * @param number  Value of the variable.
     *
     * @return  The number variable.
     */
    Variable &addNumber(String const &variableName, Value::Number const &number);

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
    Variable &addBoolean(String const &variableName, bool booleanValue);

    /**
     * Adds a text variable to the record. The variable is set up to only accept
     * text values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     * @param text  Value of the variable.
     *
     * @return  The text variable.
     */
    Variable &addText(String const &variableName, Value::Text const &text);

    Variable &addTime(String const &variableName, Time const &time);

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
    Variable &addArray(String const &variableName, ArrayValue *array = 0);

    /**
     * Adds a dictionary variable to the record. The variable is set up to only accept
     * dictionary values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     *
     * @return  The dictionary variable.
     */
    Variable &addDictionary(String const &variableName);

    /**
     * Adds a block variable to the record. The variable is set up to only accept
     * block values. An existing variable with the same name is deleted first.
     *
     * @param variableName  Name of the variable.
     *
     * @return  The block variable.
     */
    Variable &addBlock(String const &variableName);

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
    Variable &addFunction(String const &variableName, Function *func);

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
    Record &add(String const &name, Record *subrecord);

    /**
     * Adds a new empty subrecord to the record. Adds a variable named @a name and
     * creates a new record owned by it. An existing subrecord with the same name is
     * deleted first.
     *
     * @param name  Name to use for the subrecord. This must be a valid variable name.
     *
     * @return  The new subrecord.
     */
    Record &addRecord(String const &name);

    /**
     * Removes a subrecord from the record.
     *
     * @param name  Name of subrecord owned by this record.
     *
     * @return  Caller gets ownership of the removed record.
     */
    Record *removeSubrecord(String const &name);

    /**
     * Sets the value of a variable, creating the variable if needed.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Value for the variable.
     *
     * @return Variable whose value was set.
     */
    Variable &set(String const &name, bool value);

    /// @copydoc set()
    Variable &set(String const &name, char const *value);

    /// @copydoc set()
    Variable &set(String const &name, Value::Text const &value);

    /// @copydoc set()
    Variable &set(String const &name, Value::Number const &value);

    /// @copydoc set()
    Variable &set(String const &name, dint32 value);

    /// @copydoc set()
    Variable &set(String const &name, duint32 value);

    /**
     * Sets the value of a variable, creating the variable if it doesn't exist.
     *
     * @param name   Name of the variable. May contain subrecords using the dot notation.
     * @param value  Array to use as the value of the variable. Ownership taken.
     */
    Variable &set(String const &name, ArrayValue *value);

    /**
     * Looks up a variable in the record. Variables in subrecords can be accessed
     * using the member notation: <code>subrecord-name.variable-name</code>
     *
     * @param name  Variable name.
     *
     * @return  Variable.
     */
    Variable &operator [] (String const &name);

    /**
     * Looks up a variable in the record. Variables in subrecords can be accessed
     * using the member notation: <code>subrecord-name.variable-name</code>
     *
     * @param name  Variable name.
     *
     * @return  Variable (non-modifiable).
     */
    Variable const &operator [] (String const &name) const;

    /**
     * Looks up a subrecord in the record.
     *
     * @param name  Name of the subrecord.
     *
     * @return  Subrecord.
     */
    Record &subrecord(String const &name);

    /**
     * Looks up a subrecord in the record.
     *
     * @param name  Name of the subrecord.
     *
     * @return  Subrecord (non-modifiable).
     */
    Record const &subrecord(String const &name) const;

    /**
     * Returns a non-modifiable map of the members.
     */
    Members const &members() const;

    /**
     * Collects a map of all the subrecords present in the record.
     */
    Subrecords subrecords() const;

    /**
     * Creates a text representation of the record. Each variable name is
     * prefixed with @a prefix.
     *
     * @param prefix  Prefix for each variable name.
     * @param lines  NULL (used for recursion into subrecords).
     *
     * @return  Text representation.
     */
    String asText(String const &prefix, List *lines) const;

    /**
     * Convenience template for getting the value of a variable in a
     * specific type.
     *
     * @param name  Name of variable.
     *
     * @return  Value cast to a specific value type.
     */
    template <typename ValueType>
    ValueType const &value(String const &name) const {
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
    Function const &function(String const &name) const;

    /**
     * Adds a new record to be used as a superclass of this record.
     *
     * @param superValue  Value referencing the super record to add. Ownership taken.
     */
    void addSuperRecord(Value *superValue);

    /**
     * Adds a new native function to the record according to the specification.
     *
     * @param spec  Native function specification.
     *
     * @return  Reference to this record.
     */
    Record &operator << (NativeFunctionSpec const &spec);

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const { return LogEntry::Arg::StringArgument; }
    String asText() const { return asText("", 0); }

    // Observes Variable deletion.
    void variableBeingDeleted(Variable &variable);

private:
    DENG2_PRIVATE(d)
};

/// Converts the record into a human-readable text representation.
DENG2_PUBLIC QTextStream &operator << (QTextStream &os, Record const &record);

} // namespace de

#endif /* LIBDENG2_RECORD_H */
