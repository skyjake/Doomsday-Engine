/** @file controllerpresets.cpp  Game controller presets.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "clientapp.h"

#include <de/App>
#include <de/Process>
#include <de/RecordValue>
#include <de/Script>
#include <de/ScriptSystem>

#include <doomsday/console/var.h>

#include <QRegExp>

using namespace de;

static String VAR_CONTROLLER_PRESETS("controllerPresets");

DENG2_PIMPL_NOREF(ControllerPresets)
, DENG2_OBSERVES(DoomsdayApp, GameChange)
{
    Record &inputModule;
    char const *presetCVarPath = nullptr;
    int deviceId = 0;

    Impl()
        : inputModule(App::scriptSystem().nativeModule("Input"))
    {
        inputModule.addDictionary(VAR_CONTROLLER_PRESETS);
        DoomsdayApp::app().audienceForGameChange() += this;
    }

//    ~Impl()
//    {
//        DoomsdayApp::app().audienceForGameChange() -= this;
//    }

    DictionaryValue const &presets() const
    {
        return inputModule.get(VAR_CONTROLLER_PRESETS).as<DictionaryValue>();
    }

    QList<QString> ids() const
    {
        QSet<QString> ids;
        for (auto i : presets().elements())
        {
            if (auto const *value = i.second->maybeAs<RecordValue>())
            {
                ids.insert(value->dereference().gets("id"));
            }
        }
        return ids.toList();
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
    Record const *findMatching(String const &deviceName) const
    {
        for (auto i : presets().elements())
        {
            String const key = i.first.value->asText();
            if (!key.isEmpty() && QRegExp(key, Qt::CaseInsensitive).exactMatch(deviceName))
            {
                if (auto const *value = i.second->maybeAs<RecordValue>())
                {
                    return value->record();
                }
            }
        }
        return nullptr;
    }

    Record const *findById(String const &id) const
    {
        for (auto i : presets().elements())
        {
            if (auto const *value = i.second->maybeAs<RecordValue>())
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

    void currentGameChanged(Game const &newGame)
    {
        DENG2_ASSERT(deviceId == IDEV_JOY1); /// @todo Expand for other devices as needed. -jk

        // When loading a game, automatically apply the control scheme matching
        // the connected game controller (unless a specific scheme is already set).
        if (!newGame.isNull() && !Joystick_Name().isEmpty())
        {
            String const currentScheme = CVar_String(presetCVar());

            if (auto const *gamepad = findMatching(Joystick_Name()))
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

    void applyPreset(Record const *preset)
    {
        ClientApp::inputSystem().removeBindingsForDevice(deviceId);

        if (preset)
        {
            LOG_INPUT_MSG("Applying bindings for %s gamepad") << preset->gets("id");

            // Call the bind() function defined in the controllers.de script.
            Script script("preset.bind()");
            Process proc(script);
            proc.globals().add("preset").set(new RecordValue(*preset));
            proc.execute();
        }

        CVar_SetString(presetCVar(), preset? preset->gets("id").toUtf8() : "");
    }
};

ControllerPresets::ControllerPresets(int deviceId, char const *presetCVarPath)
    : d(new Impl)
{
    d->deviceId       = deviceId;
    d->presetCVarPath = presetCVarPath;
}

String ControllerPresets::currentPreset() const
{
    return CVar_String(d->presetCVar());
}

void ControllerPresets::applyPreset(String const &presetId)
{
    d->applyPreset(d->findById(presetId));
}

QStringList ControllerPresets::ids() const
{
    return d->ids();
}
