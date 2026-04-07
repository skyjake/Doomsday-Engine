/** @file binding.h  Base class for binding record accessors.
 *
 * @authors Copyright © 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef CLIENT_INPUTSYSTEM_BINDING_H
#define CLIENT_INPUTSYSTEM_BINDING_H

#include <de/error.h>
#include <de/compiledrecord.h>

/**
 * Base class for binding record accessors.
 *
 * @ingroup ui
 */
class Binding : public de::RecordAccessor
{
public:
    /// Base class for binding configuration errors. @ingroup errors
    DE_ERROR(ConfigureError);

    enum ConditionType
    {
        Invalid,

        GlobalState,    ///< Related to the high-level application/game state.

        AxisState,      ///< An axis control is in a specific position.
        ButtonState,    ///< A button control is in a specific state.
        HatState,       ///< A hat control is pointing in a specific direction.
        ModifierState,  ///< A control modifier is in a specific state.
    };

    enum ControlTest
    {
        None,

        AxisPositionWithin,
        AxisPositionBeyond,
        AxisPositionBeyondPositive,
        AxisPositionBeyondNegative,

        ButtonStateAny,
        ButtonStateDown,
        ButtonStateRepeat,
        ButtonStateDownOrRepeat,
        ButtonStateUp
    };

    struct CompiledCondition
    {
        ConditionType type = Invalid;
        ControlTest test = None;
        int device = -1;
        int id = -1;
        float pos = 0;
        bool negate = false;
        bool multiplayer = false;

        CompiledCondition() {}
        CompiledCondition(const de::Record &rec);
        bool operator == (const CompiledCondition &other) const;
    };

    typedef de::CompiledRecordT<CompiledCondition> CompiledConditionRecord;

public:
    Binding()                     : RecordAccessor(0) {}
    Binding(const Binding &other) : RecordAccessor(other) {}
    Binding(de::Record &d)        : RecordAccessor(d) {}
    Binding(const de::Record &d)  : RecordAccessor(d) {}

    virtual ~Binding() {}

    Binding &operator = (const de::Record *d) {
        setAccessedRecord(d);
        return *this;
    }

    de::Record &def();
    const de::Record &def() const;

    /**
     * Determines if this binding accessor points to a record.
     */
    operator bool() const;

    /**
     * Inserts the default members into the binding. All bindings are required to
     * implement this, as it is automatically called when configuring a binding.
     *
     * All bindings share some common members, so derived classes are required to
     * call this before inserting their own members.
     */
    virtual void resetToDefaults();

    /**
     * Generates a textual descriptor for the binding, including any state conditions.
     */
    virtual de::String composeDescriptor() = 0;

    CompiledConditionRecord &addCondition();
    int conditionCount() const;
    bool hasCondition(int index) const;
    CompiledConditionRecord &condition(int index);
    const CompiledConditionRecord &condition(int index) const;

    /**
     * Compare the binding conditions with @a other and return @c true if equivalent.
     */
    bool equalConditions(const Binding &other) const;

public:
    /**
     * Returns a new unique identifier. Never returns zero (not a valid Id).
     */
    static int newIdentifier();

    /**
     * Reset the unique identifier allocator, so that the next Id is @c 1.
     */
    static void resetIdentifiers();
};

#endif // CLIENT_INPUTSYSTEM_BINDING_H
