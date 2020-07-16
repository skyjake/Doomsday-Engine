/** @file impulsebinding.cpp  Impulse binding record accessor.
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

#include "dd_share.h" // DDMAXPLAYERS
#include "ui/impulsebinding.h"

#include <de/legacy/str.hh>
#include <de/log.h>
#include <de/recordvalue.h>
#include "ui/b_util.h"

using namespace de;

CompiledImpulseBinding::CompiledImpulseBinding(const Record &bind)
    : id            (bind.geti("id"))
    , deviceId      (bind.geti("deviceId"))
    , controlId     (bind.geti("controlId"))
    , type          (ibcontroltype_t(bind.geti("type")))
    , angle         (bind.getf("angle"))
    , flags         (bind.geti("flags"))
    , impulseId     (bind.geti("impulseId"))
    , localPlayer   (bind.geti("localPlayer"))
{}

CompiledImpulseBindingRecord &ImpulseBinding::def()
{
    return static_cast<CompiledImpulseBindingRecord &>(Binding::def());
}

const CompiledImpulseBindingRecord &ImpulseBinding::def() const
{
    return static_cast<const CompiledImpulseBindingRecord &>(Binding::def());
}

void ImpulseBinding::resetToDefaults()
{
    Binding::resetToDefaults();

    def().resetCompiled();

    def().addNumber("deviceId", -1);
    def().addNumber("controlId", -1);
    def().addNumber("type", int(IBD_TOGGLE));  ///< Type of event.
    def().addNumber("angle", 0);
    def().addNumber("flags", 0);

    def().addNumber("impulseId", 0);           ///< Identifier of the bound player impulse.
    def().addNumber("localPlayer", -1);        ///< Local player number.
}

String ImpulseBinding::composeDescriptor()
{
    LOG_AS("ui/impulsebinding.h");
    if (!*this) return "";

    String str = B_ControlDescToString(geti("deviceId"), IBDTYPE_TO_EVTYPE(geti("type")), geti("controlId"));
    if (geti("type") == IBD_ANGLE)
    {
        str += B_HatAngleToString(getf("angle"));
    }

    // Additional flags.
    if (geti("flags") & IBDF_TIME_STAGED)
    {
        str += "-staged";
    }
    if (geti("flags") & IBDF_INVERSE)
    {
        str += "-inverse";
    }

    // Append any state conditions.
    const ArrayValue &conds = def().geta("condition");
    DE_FOR_EACH_CONST(ArrayValue::Elements, i, conds.elements())
    {
        str += " + " + B_ConditionToString(*(*i)->as<RecordValue>().record());
    }

    return str;
}

static bool doConfigure(ImpulseBinding &bind, const char *ctrlDesc, int impulseId, int localPlayer)
{
    DE_ASSERT(ctrlDesc);

    bind.resetToDefaults();
    bind.def().set("impulseId", impulseId);
    bind.def().set("localPlayer", localPlayer);

    // Parse the control descriptor.
    AutoStr *str = AutoStr_NewStd();

    // First, the device name.
    ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
    if (!Str_CompareIgnoreCase(str, "key"))
    {
        bind.def().set("deviceId", IDEV_KEYBOARD);
        bind.def().set("type", int(IBD_TOGGLE));

        // Next part defined button.
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');

        int keyId;
        bool ok = B_ParseKeyId(keyId, Str_Text(str));
        if (!ok) return false;

        bind.def().set("controlId", keyId);
    }
    else if (!Str_CompareIgnoreCase(str, "mouse"))
    {
        bind.def().set("deviceId", IDEV_MOUSE);

        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');

        ddeventtype_t type;
        int controlId = 0;
        bool ok = B_ParseMouseTypeAndId(type, controlId, Str_Text(str));
        if (!ok) return false;

        bind.def().set("controlId", controlId);
        bind.def().set("type", int(EVTYPE_TO_IBDTYPE(type)));
    }
    else if (!Str_CompareIgnoreCase(str, "joy") ||
             !Str_CompareIgnoreCase(str, "head"))
    {
        bind.def().set("deviceId", (!Str_CompareIgnoreCase(str, "joy")? IDEV_JOY1 : IDEV_HEAD_TRACKER));

        // Next part defined button, axis, or hat.
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');

        ddeventtype_t type;
        int controlId = 0;
        bool ok = B_ParseJoystickTypeAndId(type, controlId, bind.geti("deviceId"), Str_Text(str));
        if (!ok) return false;

        bind.def().set("controlId", controlId);
        bind.def().set("type", int(EVTYPE_TO_IBDTYPE(type)));

        // Hats include the angle.
        if (type == E_ANGLE)
        {
            ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');

            float angle;
            ok = B_ParseHatAngle(angle, Str_Text(str));
            if (!ok) return false;

            bind.def().set("angle", angle);
        }
    }

    // Finally, there may be some flags at the end.
    while (ctrlDesc)
    {
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '-');
        if (!Str_CompareIgnoreCase(str, "inverse"))
        {
            bind.def().set("flags", bind.geti("flags") | IBDF_INVERSE);
        }
        else if (!Str_CompareIgnoreCase(str, "staged"))
        {
            bind.def().set("flags", bind.geti("flags") | IBDF_TIME_STAGED);
        }
        else
        {
            LOG_INPUT_WARNING("Unrecognized \"%s\"") << ctrlDesc;
            return false;
        }
    }

    return true;
}

void ImpulseBinding::configure(const char *ctrlDesc, int impulseId, int localPlayer, bool assignNewId)
{
    DE_ASSERT(ctrlDesc);
    DE_ASSERT(localPlayer >= 0 && localPlayer < DDMAXPLAYERS);
    LOG_AS("ui/impulsebinding.h");

    // The first part specifies the device-control condition.
    AutoStr *str = AutoStr_NewStd();
    ctrlDesc = Str_CopyDelim(str, ctrlDesc, '+');

    if (!doConfigure(*this, Str_Text(str), impulseId, localPlayer))
    {
        throw ConfigureError("ImpulseBinding::configure", "Descriptor parse error");
    }

    // Any conditions?
    def()["condition"].array().clear();
    while (ctrlDesc)
    {
        // A new condition.
        ctrlDesc = Str_CopyDelim(str, ctrlDesc, '+');

        Record &cond = addCondition();
        if (!B_ParseBindingCondition(cond, Str_Text(str)))
        {
            throw ConfigureError("ImpulseBinding::configure", "Descriptor parse error");
        }
    }

    if (assignNewId)
    {
        def().set("id", newIdentifier());
    }
}
