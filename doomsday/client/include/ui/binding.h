/** @file binding.h  Base class for binding record accessors.
 *
 * @authors Copyright © 2009-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QVector>
#include <de/RecordAccessor>

/**
 * Base class for binding record accessors.
 *
 * @ingroup ui
 */
class Binding : public de::RecordAccessor
{
public:
    /**
     * Describes a single trigger condition.
     */
    struct Condition
    {
        enum Type
        {
            Invalid,

            GlobalState,    ///< Related to the high-level application/game state.

            AxisState,      ///< An axis control is in a specific position.
            ButtonState,    ///< A button control is in a specific state.
            HatState,       ///< A hat control is pointing in a specific direction.
            ModifierState,  ///< A control modifier is in a specific state.
        };
        Type type { Invalid };

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
        ControlTest test { None };

        int device       = -1;     ///< The relevant input device; otherwise @c -1
        int id           = -1;     ///< device-control / impulse ID; otherwise @c -1.
        float pos        = 0;      ///< Axis-position / hat-angle; otherwise @c 0.
        bool negate      = false;  ///< Test the inverse (e.g., not in a specific state).
        bool multiplayer = false;  ///< Only for multiplayer.
    };
    typedef QVector<Condition> Conditions;
    Conditions conditions;         ///< Additional conditions.

public:
    Binding()                     : RecordAccessor(0) {}
    Binding(Binding const &other) : RecordAccessor(other) {}
    Binding(de::Record &d)        : RecordAccessor(d) {}
    Binding(de::Record const &d)  : RecordAccessor(d) {}

    virtual ~Binding() {}

    Binding &operator = (de::Record const *d) {
        setAccessedRecord(d);
        return *this;
    }

    de::Record &def();
    de::Record const &def() const;

    /**
     * Determines if this binding accessor points to a record.
     */
    operator bool() const;

    /**
     * Inserts the default members into the binding. All bindings are required to
     * implement this, as it is automatically called when configuring a binding.
     */
    virtual void resetToDefaults() = 0;

    /**
     * Generates a textual descriptor for the binding, including any state conditions.
     */
    virtual de::String composeDescriptor() = 0;

    /**
     * Compare the binding conditions with @a other and return @c true if equivalent.
     */
    bool equalConditions(Binding const &other) const;
};

typedef Binding::Condition BindingCondition;
typedef Binding::Conditions BindingConditions;

#endif // CLIENT_INPUTSYSTEM_BINDING_H
