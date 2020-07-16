/** @file impulsebinding.h  Impulse binding record accessor.
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

#ifndef CLIENT_INPUTSYSTEM_IMPULSEBINDING_H
#define CLIENT_INPUTSYSTEM_IMPULSEBINDING_H

#include <de/string.h>
#include <de/compiledrecord.h>
#include "binding.h"
#include "ddevent.h"

enum ibcontroltype_t
{
    IBD_TOGGLE = E_TOGGLE,
    IBD_AXIS   = E_AXIS,
    IBD_ANGLE  = E_ANGLE,
    NUM_IBD_TYPES
};

#define EVTYPE_TO_IBDTYPE(evt)  ((evt) == E_AXIS? IBD_AXIS : (evt) == E_TOGGLE? IBD_TOGGLE : IBD_ANGLE)
#define IBDTYPE_TO_EVTYPE(cbt)  ((cbt) == IBD_AXIS? E_AXIS : (cbt) == IBD_TOGGLE? E_TOGGLE : E_ANGLE)

// Flags for impulse bindings.
#define IBDF_INVERSE        0x1
#define IBDF_TIME_STAGED    0x2

struct CompiledImpulseBinding
{
    int id = -1;
    int deviceId = -1;
    int controlId = -1;
    ibcontroltype_t type = IBD_TOGGLE;  ///< Type of event.
    float angle = 0;
    int flags = 0;
    int impulseId = 0;           ///< Identifier of the bound player impulse.
    int localPlayer = -1;        ///< Local player number.

    CompiledImpulseBinding() {}
    CompiledImpulseBinding(const de::Record &bind);
};

typedef de::CompiledRecordT<CompiledImpulseBinding> CompiledImpulseBindingRecord;

/**
 * Utility for handling input-device-control => impulse binding records.
 *
 * @ingroup ui
 */
class ImpulseBinding : public Binding
{
public:
    ImpulseBinding()                            : Binding() {}
    ImpulseBinding(const ImpulseBinding &other) : Binding(other) {}
    ImpulseBinding(de::Record &d)               : Binding(d) {}
    ImpulseBinding(const de::Record &d)         : Binding(d) {}

    ImpulseBinding &operator = (const de::Record *d) {
        *static_cast<Binding *>(this) = d;
        def().resetCompiled();
        return *this;
    }

    CompiledImpulseBindingRecord &def();
    const CompiledImpulseBindingRecord &def() const;

    void resetToDefaults();

    de::String composeDescriptor();

    /**
     * Parse a device-control => player impulse trigger descriptor and (re)configure the
     * binding.
     *
     * @param ctrlDesc     Descriptor for control information and any additional conditions.
     * @param impulseId    Identifier of the player impulse to execute when triggered, if any.
     * @param localPlayer  Local player number to execute the impulse for when triggered.
     * @param assignNewId  @c true= assign a new unique identifier.
     *
     * @throws ConfigureError on failure. At which point @a binding should be considered
     * to be in an undefined state. The caller may choose to clear and then reconfigure
     * it using another descriptor.
     */
    void configure(const char *ctrlDesc, int impulseId, int localPlayer, bool assignNewId = true);
};

#endif // CLIENT_INPUTSYSTEM_IMPULSEBINDING_H

