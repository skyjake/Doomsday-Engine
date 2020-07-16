/** @file configprofiles.cpp  Configuration setting profiles.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "api_console.h"
#include "configprofiles.h"

#include <de/app.h>
#include <de/config.h>
#include <de/info.h>
#include <de/dscript.h>
#include <de/ziparchive.h>

#include <doomsday/doomsdayapp.h>
#include <doomsday/game.h>
#include <de/keymap.h>
#include <sstream>

using namespace de;

DE_STATIC_STRING(CUSTOM_PROFILE, "Custom");

DE_PIMPL(ConfigProfiles)
, DE_OBSERVES(DoomsdayApp, GameUnload)
, DE_OBSERVES(DoomsdayApp, GameChange)
{
    struct Setting
    {
        SettingType type;
        String name;

        Setting() : type(IntCVar) {}
        Setting(SettingType t, String n) : type(t), name(n) {}

        /**
         * Changes the current value of the setting.
         *
         * @param val  New value.
         */
        void setValue(const Value &val) const
        {
            switch (type)
            {
            case IntCVar:
                Con_SetInteger(name, val.asInt());
                break;

            case FloatCVar:
                Con_SetFloat(name, float(val.asNumber()));
                break;

            case StringCVar:
                Con_SetString(name, val.asText());
                break;

            case ConfigVariable:
                Config::get(name).set(val);
                break;
            }
        }
    };

    typedef KeyMap<String, Setting> Settings;
    Settings settings;

    struct Profile : public Profiles::AbstractProfile
    {
        typedef KeyMap<String, std::shared_ptr<Value>> Values;
        Values values;

        ConfigProfiles &owner()
        {
            return static_cast<ConfigProfiles &>(AbstractProfile::owner());
        }

        const ConfigProfiles &owner() const
        {
            return static_cast<const ConfigProfiles &>(AbstractProfile::owner());
        }

        bool resetToDefaults() override
        {
            if (!isReadOnly())
            {
                values = owner().d->defaults.values;
                return true;
            }
            return false;
        }

        String toInfoSource() const override
        {
            const auto &settings = owner().d->settings;

            std::ostringstream os;
            for (const auto &val : values)
            {
                DE_ASSERT(settings.contains(val.first));

                const Setting &st = settings.find(val.first)->second;

//                String valueText;
//                switch (st.type)
//                {
//                case IntCVar:
//                case FloatCVar:
//                case StringCVar:
//                case ConfigVariable:
//                    valueText = val.second->asText();
//                    break;
//                }

                if (!os.str().empty()) os << "\n";

                os << "setting \"" << st.name << "\" {\n"
                   << "    value: " << val.second->asText() << "\n"
                   << "}";
            }
            return os.str();
        }

        void initializeFromInfoBlock(const ConfigProfiles &        profs,
                                     const de::Info::BlockElement &block)
        {
            // Use the default settings for anything not defined in the file.
            values = profs.d->defaults.values;

            // Read all the setting values from the profile block.
            for (const auto *element : block.contentsInOrder())
            {
                if (!element->isBlock()) continue;

                const de::Info::BlockElement &setBlock = element->as<de::Info::BlockElement>();

                // Only process known settings.
                if (setBlock.blockType() == "setting" &&
                    profs.d->settings.contains(setBlock.name()))
                {
                    values[setBlock.name()].reset(
                            profs.d->textToSettingValue(setBlock.keyValue("value").text,
                                                        setBlock.name()));
                }
            }
        }
    };

    Profile defaults;
    String current;

    Impl(Public *i) : Base(i), current(CUSTOM_PROFILE())
    {
        DoomsdayApp::app().audienceForGameUnload() += this;
        DoomsdayApp::app().audienceForGameChange() += this;

        addProfile(current);
    }

    Profile *addProfile(const String &name)
    {
        Profile *prof = new Profile;
        prof->setName(name);
        self().add(prof);
        return prof;
    }

    Profile *tryFind(const String &name) const
    {
        return maybeAs<Profile>(self().tryFind(name));
    }

    Profile &currentProfile() const
    {
        return self().find(current).as<Profile>();
    }

    /// Caller gets ownership of the returned Value.
    Value *getDefaultFromConfig(const String &name)
    {
        try
        {
            // Get defaults for script values.
            Script script("record d; import Config; Config.setDefaults(d); d");
            Process proc(script);
            proc.execute();

            const Record &confDefaults = *proc.context().evaluator().result()
                    .as<RecordValue>().record();

            DE_ASSERT(confDefaults.has(name));

            return confDefaults[name].value().duplicate();
        }
        catch (const Error &er)
        {
            LOG_WARNING("Failed to find default for \"%s\": %s") << name << er.asText();
        }
        return new NoneValue;
    }

    /**
     * Gets the current values of the settings and saves them to a profile.
     */
    void fetch(const String &profileName)
    {
        Profile &prof = self().find(profileName).as<Profile>();
        if (prof.isReadOnly()) return;

        for (const auto &st : settings)
        {
            std::shared_ptr<Value> val;
            switch (st.second.type)
            {
            case IntCVar:
                val.reset(new NumberValue(Con_GetInteger(st.second.name)));
                break;

            case FloatCVar:
                val.reset(new NumberValue(Con_GetFloat(st.second.name)));
                break;

            case StringCVar:
                val.reset(new TextValue(st.second.name));
                break;

            case ConfigVariable: {
                val.reset(Config::get(st.second.name).value().duplicate());
                break; }
            }
            if (!val) val.reset(new NoneValue);

            prof.values[st.first] = val;
        }
    }

    void setCurrent(const String &name)
    {
        current = name;

        if (!self().persistentName().isEmpty())
        {
            App::config().set(configVarName(), name);
        }
    }

    void apply(const String &profileName)
    {
        Profile &profile = self().find(profileName).as<Profile>();
        for (const auto &st : settings)
        {
            st.second.setValue(*profile.values[st.second.name]);
        }
    }

    void changeTo(const String &profileName)
    {
        LOG_AS("configprofiles.h");
        DE_ASSERT(tryFind(profileName));

        if (current == profileName) return;

        if (!self().persistentName().isEmpty())
        {
            LOG_MSG("Changing %s profile to '%s'") << self().persistentName() << profileName;
        }

        // First update the old profile.
        fetch(current);

        // Then set the values from the other profile.
        setCurrent(profileName);
        apply(current);

        DE_NOTIFY_PUBLIC_VAR(ProfileChange, i)
        {
            i->currentProfileChanged(profileName);
        }
    }

    void reset()
    {
        if (currentProfile().resetToDefaults())
        {
            apply(current);
        }
    }

    void resetSetting(const String &settingName) const
    {
        DE_ASSERT(settings.contains(settingName));
        DE_ASSERT(defaults.values.contains(settingName));

        settings[settingName].setValue(*defaults.values[settingName]);
    }

    /**
     * For a persistent register, determines the name of the Config variable
     * that stores the name of the currently selected profile.
     */
    String configVarName() const
    {
        if (self().persistentName().isEmpty()) return "";
        return self().persistentName().concatenateMember("profile");
    }

    Value *textToSettingValue(const String &text, const String &settingName) const
    {
        DE_ASSERT(settings.contains(settingName));

        const Setting &st = settings[settingName];

        switch (st.type)
        {
        case IntCVar:
            return new NumberValue(text.toInt());

        case FloatCVar:
            return new NumberValue(text.toFloat());

        case StringCVar:
            return new TextValue(text);

        case ConfigVariable:
            return new NumberValue(text.toDouble());
        }

        DE_ASSERT_FAIL("Invalid setting type");
        return new NoneValue;
    }

    bool addCustomProfileIfMissing()
    {
        if (!tryFind(CUSTOM_PROFILE()))
        {
            addProfile(CUSTOM_PROFILE());

            // Use whatever values are currently in effect.
            fetch(CUSTOM_PROFILE());
            return true;
        }
        return false; // nothing added
    }

    /**
     * Deserializes all the profiles (see aboutToUnloadGame()). In addition,
     * fixed/built-in profiles are loaded from /data/profiles/(persistentName)/
     *
     * @param newGame  New current game.
     */
    void currentGameChanged(const Game &newGame)
    {
        if (!self().isPersistent() || newGame.isNull()) return;

        LOG_AS("configprofiles.h");
        LOG_DEBUG("Game has been loaded, deserializing %s profiles") << self().persistentName();

        self().deserialize();

        // Settings haven't previously been created -- make sure we at least
        // have the Custom profile.
        if (addCustomProfileIfMissing())
        {
            current = CUSTOM_PROFILE();
        }

        // Update current profile.
        current = App::config().gets(configVarName(), current);

        if (!tryFind(current))
        {
            // Fall back to the one profile we know is available.
            if (tryFind(CUSTOM_PROFILE()))
            {
                current = CUSTOM_PROFILE();
            }
            else
            {
                current = self().profiles().first();
            }
        }

        // Make sure these are the values now in use.
        apply(current);

        App::config().set(configVarName(), current);
    }

    /**
     * Serializes all the profiles:
     * - Config.(persistentName).profile stores the name of the current profile
     * - /home/configs/(persistentName).dei contains all the existing profile
     *   values, with one file per profile
     *
     * @param gameBeingUnloaded  Current game.
     */
    void aboutToUnloadGame(const Game &gameBeingUnloaded)
    {
        if (!self().isPersistent() || gameBeingUnloaded.isNull()) return;

        LOG_AS("configprofiles.h");
        LOG_DEBUG("Game being unloaded, serializing %s profiles") << self().persistentName();

        // Update the current profile.
        fetch(current);

        // Remember which profile is the current one.
        App::config().set(configVarName(), current);

        self().serialize();
    }
};

ConfigProfiles::ConfigProfiles() : d(new Impl(this))
{}

ConfigProfiles &ConfigProfiles::define(SettingType   type,
                                       const String &settingName,
                                       const Value & defaultValue)
{
    d->settings.insert(settingName, Impl::Setting(type, settingName));

    std::shared_ptr<Value> def;
    if (type == ConfigVariable)
    {
        def.reset(d->getDefaultFromConfig(settingName));
    }
    else
    {
        def.reset(defaultValue.duplicate());
    }
    d->defaults.values[settingName] = def;

    return *this;
}

String ConfigProfiles::currentProfile() const
{
    return d->current;
}

bool ConfigProfiles::saveAsProfile(const String &name)
{
    if (!tryFind(name) && !name.isEmpty())
    {
        d->addProfile(name);
        d->fetch(name);
        return true;
    }
    return false;
}

void ConfigProfiles::setProfile(const String &name)
{
    d->changeTo(name);
}

void ConfigProfiles::resetToDefaults()
{
    d->reset();

    DE_NOTIFY_VAR(ProfileChange, i)
    {
        i->currentProfileChanged(d->current);
    }
}

void ConfigProfiles::resetSettingToDefaults(const String &settingName)
{
    d->resetSetting(settingName);
}

bool ConfigProfiles::rename(const String &name)
{
    if (d->currentProfile().setName(name))
    {
        d->setCurrent(name);

        DE_NOTIFY_VAR(ProfileChange, i)
        {
            i->currentProfileChanged(name);
        }
        return true;
    }
    return false;
}

void ConfigProfiles::deleteProfile(const String &name)
{
    // Can't delete the current profile.
    if (name == d->current) return;

    if (auto *prof = tryFind(name))
    {
        remove(*prof);
        delete prof;
    }
}

Profiles::AbstractProfile *
ConfigProfiles::profileFromInfoBlock(const de::Info::BlockElement &block)
{
    std::unique_ptr<Impl::Profile> prof(new Impl::Profile);
    prof->initializeFromInfoBlock(*this, block);
    return prof.release();
}
