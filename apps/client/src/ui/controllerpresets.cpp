/** @file controllerpresets.cpp  Game controller presets.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/controllerpresets.h"
#include "ui/joystick.h"
#include "ui/inputsystem.h"
#include "clientapp.h"

#include <de/app.h>
#include <de/dscript.h>
#include <de/regexp.h>

#include <doomsday/console/var.h>

using namespace de;

static const char *VAR_CONTROLLER_PRESETS = "controllerPresets";

DE_PIMPL_NOREF(ControllerPresets)
, DE_OBSERVES(DoomsdayApp, GameChange)
{
    Record &    inputModule;
    const char *presetCVarPath = nullptr;
    int         deviceId       = 0;

    Impl()
        : inputModule(App::scriptSystem()["Input"])
    {
        inputModule.addDictionary(VAR_CONTROLLER_PRESETS);
        DoomsdayApp::app().audienceForGameChange() += this;
    }

//    ~Impl()
//    {
//        DoomsdayApp::app().audienceForGameChange() -= this;
//    }

    const DictionaryValue &presets() const
    {
        return inputModule.get(VAR_CONTROLLER_PRESETS).as<DictionaryValue>();
    }

    List<String> ids() const
    {
        Set<String> ids;
        for (auto i : presets().elements())
        {
            if (const auto *value = maybeAs<RecordValue>(i.second))
            {
                ids.insert(value->dereference().gets("id"));
            }
        }
        return compose<List<String>>(ids.begin(), ids.end());
    }

    /**
     * Checks the presets dictionary if any the regular expressions match
     * the given device name. Returns a Doomsday Script object whose bind() method
     * can be called to set the bindings for the gamepad scheme.
     *
     * @param deviceName  Device name.
     *
     * @return Object defining the gamepad default bindings, or @c nullptr if not found.
     */
    const Record *findMatching(const String &deviceName) const
    {
        for (auto i : presets().elements())
        {
            const String key = i.first.value->asText();
            if (!key.isEmpty() && RegExp(key, CaseInsensitive).exactMatch(deviceName))
            {
                if (const auto *value = maybeAs<RecordValue>(i.second))
                {
                    return value->record();
                }
            }
        }
        return nullptr;
    }

    const Record *findById(const String &id) const
    {
        for (auto i : presets().elements())
        {
            if (const auto *value = maybeAs<RecordValue>(i.second))
            {
                if (value->dereference().gets("id") == id)
                {
                    return value->record();
                }
            }
        }
        return nullptr;
    }

    cvar_t *presetCVar() const
    {
        return Con_FindVariable(presetCVarPath);
    }

    void currentGameChanged(const Game &newGame)
    {
        DE_ASSERT(deviceId == IDEV_JOY1); /// @todo Expand for other devices as needed. -jk

        // When loading a game, automatically apply the control scheme matching
        // the connected game controller (unless a specific scheme is already set).
        if (!newGame.isNull() && !Joystick_Name().isEmpty())
        {
            const String currentScheme = CVar_String(presetCVar());

            if (const auto *gamepad = findMatching(Joystick_Name()))
            {
                if (currentScheme.isEmpty())
                {
                    LOG_INPUT_NOTE("Detected a known gamepad: %s") << Joystick_Name();

                    // We can now automatically take this scheme into use.
                    applyPreset(gamepad);
                }
            }
        }
    }

    void applyPreset(const Record *preset)
    {
        ClientApp::input().removeBindingsForDevice(deviceId);

        if (preset)
        {
            LOG_INPUT_MSG("Applying bindings for %s gamepad") << preset->gets("id");

            // Call the bind() function defined in the controllers.de script.
            Script script("preset.bind()");
            Process proc(script);
            proc.globals().add("preset").set(new RecordValue(*preset));
            proc.execute();
        }

        CVar_SetString(presetCVar(), preset? preset->gets("id").c_str() : "");
    }
};

ControllerPresets::ControllerPresets(int deviceId, const char *presetCVarPath)
    : d(new Impl)
{
    d->deviceId       = deviceId;
    d->presetCVarPath = presetCVarPath;
}

String ControllerPresets::currentPreset() const
{
    return CVar_String(d->presetCVar());
}

void ControllerPresets::applyPreset(const String &presetId)
{
    d->applyPreset(d->findById(presetId));
}

StringList ControllerPresets::ids() const
{
    return d->ids();
}
