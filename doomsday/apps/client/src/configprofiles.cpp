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
#include "ConfigProfiles"

#include <de/App>
#include <de/Config>
#include <de/Info>
#include <de/NumberValue>
#include <de/Process>
#include <de/RecordValue>
#include <de/Script>
#include <de/TextValue>
#include <de/ZipArchive>

#include <doomsday/doomsdayapp.h>
#include <doomsday/Game>
#include <de/Map>
#include <sstream>

using namespace de;

static String const CUSTOM_PROFILE = "Custom";

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

    typedef Map<String, Setting> Settings;
    Settings settings;

    struct Profile : public Profiles::AbstractProfile
    {
        typedef Map<String, std::shared_ptr<Value>> Values;
        Values values;

        ConfigProfiles &owner()
        {
            return static_cast<ConfigProfiles &>(AbstractProfile::owner());
        }

        ConfigProfiles const &owner() const
        {
            return static_cast<ConfigProfiles const &>(AbstractProfile::owner());
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
            auto const &settings = owner().d->settings;

            std::ostringstream os;
            for (const auto &val : values)
            {
                DE_ASSERT(settings.contains(val.first));

                Setting const &st = settings.find(val.first)->second;

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

        void initializeFromInfoBlock(ConfigProfiles const &profs,
                                     de::Info::BlockElement const &block)
        {
            // Use the default settings for anything not defined in the file.
            values = profs.d->defaults.values;

            // Read all the setting values from the profile block.
            for (auto const *element : block.contentsInOrder())
            {
                if (!element->isBlock()) continue;

                de::Info::BlockElement const &setBlock = element->as<de::Info::BlockElement>();

                // Only process known settings.
                if (setBlock.blockType() == "setting" &&
                   profs.d->settings.contains(setBlock.name()))
                {
                    values[setBlock.name()] =
                            profs.d->textToSettingValue(setBlock.keyValue("value").text,
                                                        setBlock.name());
                }
            }
        }
    };

    Profile defaults;
    String current;

    Impl(Public *i) : Base(i), current(CUSTOM_PROFILE)
    {
        DoomsdayApp::app().audienceForGameUnload() += this;
        DoomsdayApp::app().audienceForGameChange() += this;

        addProfile(current);
    }

    Profile *addProfile(String const &name)
    {
        Profile *prof = new Profile;
        prof->setName(name);
        self().add(prof);
        return prof;
    }

    Profile *tryFind(String const &name) const
    {
        return maybeAs<Profile>(self().tryFind(name));
    }

    Profile &currentProfile() const
    {
        return self().find(current).as<Profile>();
    }

    /// Caller gets ownership of the returned Value.
    Value *getDefaultFromConfig(String const &name)
    {
        try
        {
            // Get defaults for script values.
            Script script("record d; import Config; Config.setDefaults(d); d");
            Process proc(script);
            proc.execute();

            Record const &confDefaults = *proc.context().evaluator().result()
                    .as<RecordValue>().record();

            DE_ASSERT(confDefaults.has(name));

            return confDefaults[name].value().duplicate();
        }
        catch (Error const &er)
        {
            LOG_WARNING("Failed to find default for \"%s\": %s") << name << er.asText();
        }
        return new NoneValue;
    }

    /**
     * Gets the current values of the settings and saves them to a profile.
     */
    void fetch(String const &profileName)
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

    void setCurrent(String const &name)
    {
        current = name;

        if (!self().persistentName().isEmpty())
        {
            App::config().set(configVarName(), name);
        }
    }

    void apply(String const &profileName)
    {
        Profile &profile = self().find(profileName).as<Profile>();
        for (const auto &st : settings)
        {
            st.second.setValue(*profile.values[st.second.name]);
        }
    }

    void changeTo(String const &profileName)
    {
        LOG_AS("ConfigProfiles");
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

        DE_FOR_PUBLIC_AUDIENCE(ProfileChange, i)
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

    void resetSetting(String const &settingName) const
    {
        DE_ASSERT(settings.contains(settingName));
        DE_ASSERT(defaults.values.contains(settingName));

        settings.find[settingName].setValue(defaults.values[settingName]);
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

    QVariant textToSettingValue(String const &text, String const &settingName) const
    {
        DE_ASSERT(settings.contains(settingName));

        Setting const &st = settings[settingName];

        switch (st.type)
        {
        case IntCVar:
            return text.toInt();

        case FloatCVar:
            return text.toFloat();

        case StringCVar:
            return text;

        case ConfigVariable:
            return text.toDouble();
        }

        DE_ASSERT(false);
        return QVariant();
    }

    bool addCustomProfileIfMissing()
    {
        if (!tryFind(CUSTOM_PROFILE))
        {
            addProfile(CUSTOM_PROFILE);

            // Use whatever values are currently in effect.
            fetch(CUSTOM_PROFILE);
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
    void currentGameChanged(Game const &newGame)
    {
        if (!self().isPersistent() || newGame.isNull()) return;

        LOG_AS("ConfigProfiles");
        LOG_DEBUG("Game has been loaded, deserializing %s profiles") << self().persistentName();

        self().deserialize();

        // Settings haven't previously been created -- make sure we at least
        // have the Custom profile.
        if (addCustomProfileIfMissing())
        {
            current = CUSTOM_PROFILE;
        }

        // Update current profile.
        current = App::config().gets(configVarName(), current);

        if (!tryFind(current))
        {
            // Fall back to the one profile we know is available.
            if (tryFind(CUSTOM_PROFILE))
            {
                current = CUSTOM_PROFILE;
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
    void aboutToUnloadGame(Game const &gameBeingUnloaded)
    {
        if (!self().isPersistent() || gameBeingUnloaded.isNull()) return;

        LOG_AS("ConfigProfiles");
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

ConfigProfiles &ConfigProfiles::define(SettingType type,
                                       String const &settingName,
                                       QVariant const &defaultValue)
{
    d->settings.insert(settingName, Impl::Setting(type, settingName));

    QVariant def;
    if (type == ConfigVariable)
    {
        def = d->getDefaultFromConfig(settingName);
    }
    else
    {
        def = defaultValue;
    }
    d->defaults.values[settingName] = def;

    return *this;
}

String ConfigProfiles::currentProfile() const
{
    return d->current;
}

bool ConfigProfiles::saveAsProfile(String const &name)
{
    if (!tryFind(name) && !name.isEmpty())
    {
        d->addProfile(name);
        d->fetch(name);
        return true;
    }
    return false;
}

void ConfigProfiles::setProfile(String const &name)
{
    d->changeTo(name);
}

void ConfigProfiles::resetToDefaults()
{
    d->reset();

    DE_FOR_AUDIENCE(ProfileChange, i)
    {
        i->currentProfileChanged(d->current);
    }
}

void ConfigProfiles::resetSettingToDefaults(String const &settingName)
{
    d->resetSetting(settingName);
}

bool ConfigProfiles::rename(String const &name)
{
    if (d->currentProfile().setName(name))
    {
        d->setCurrent(name);

        DE_FOR_AUDIENCE(ProfileChange, i)
        {
            i->currentProfileChanged(name);
        }
        return true;
    }
    return false;
}

void ConfigProfiles::deleteProfile(String const &name)
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
ConfigProfiles::profileFromInfoBlock(de::Info::BlockElement const &block)
{
    std::unique_ptr<Impl::Profile> prof(new Impl::Profile);
    prof->initializeFromInfoBlock(*this, block);
    return prof.release();
}
