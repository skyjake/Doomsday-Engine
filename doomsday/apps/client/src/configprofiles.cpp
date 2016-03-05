/** @file configprofiles.cpp  Configuration setting profiles.
 *
 * @authors Copyright (c) 2013-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/Script>
#include <de/Process>
#include <de/RecordValue>
#include <de/NumberValue>
#include <de/ZipArchive>
#include <de/Info>

#include <doomsday/doomsdayapp.h>
#include <doomsday/Game>

#include <QMap>
#include <QList>
#include <QTextStream>

using namespace de;

static String const CUSTOM_PROFILE = "Custom";

DENG2_PIMPL(ConfigProfiles)
, DENG2_OBSERVES(DoomsdayApp, GameUnload)
, DENG2_OBSERVES(DoomsdayApp, GameChange)
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
        void setValue(QVariant const &val) const
        {
            switch(type)
            {
            case IntCVar:
                Con_SetInteger(name.toLatin1(), val.toInt());
                break;

            case FloatCVar:
                Con_SetFloat(name.toLatin1(), val.toFloat());
                break;

            case StringCVar:
                Con_SetString(name.toLatin1(), val.toString().toUtf8());
                break;

            case ConfigVariable:
                if(!qstrcmp(val.typeName(), "QString"))
                {
                    App::config()[name].set(TextValue(val.toString()));
                }
                else
                {
                    App::config()[name].set(NumberValue(val.toDouble()));
                }
                break;
            }
        }
    };

    typedef QMap<String, Setting> Settings;
    Settings settings;

    struct Profile : public Profiles::AbstractProfile
    {
        typedef QMap<String, QVariant> Values;
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
            if(!isReadOnly())
            {
                values = owner().d->defaults.values;
                return true;
            }
            return false;
        }

        String toInfoSource() const
        {
            auto const &settings = owner().d->settings;

            String info;
            QTextStream os(&info);
            os.setCodec("UTF-8");

            DENG2_FOR_EACH_CONST(Values, val, values)
            {
                DENG2_ASSERT(settings.contains(val.key()));

                Setting const &st = settings[val.key()];

                String valueText;
                switch(st.type)
                {
                case IntCVar:
                case FloatCVar:
                case StringCVar:
                case ConfigVariable:
                    // QVariant can handle this.
                    valueText = val.value().toString();
                    break;
                }

                os << "setting \"" << st.name << "\" {\n"
                   << "    value: " << valueText << "\n"
                   << "}";
            }

            return info;
        }

        void initializeFromInfoBlock(ConfigProfiles const &profs,
                                     de::Info::BlockElement const &block)
        {
            // Use the default settings for anything not defined in the file.
            values = profs.d->defaults.values;

            // Read all the setting values from the profile block.
            foreach(auto const *element, block.contentsInOrder())
            {
                if(!element->isBlock()) continue;

                de::Info::BlockElement const &setBlock = element->as<de::Info::BlockElement>();

                // Only process known settings.
                if(setBlock.blockType() == "setting" &&
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

    Instance(Public *i) : Base(i), current(CUSTOM_PROFILE)
    {
        DoomsdayApp::app().audienceForGameUnload() += this;
        DoomsdayApp::app().audienceForGameChange() += this;

        addProfile(current);
    }

    ~Instance()
    {
        DoomsdayApp::app().audienceForGameUnload() -= this;
        DoomsdayApp::app().audienceForGameChange() -= this;
    }

    Profile *addProfile(String const &name)
    {
        Profile *prof = new Profile;
        prof->setName(name);
        self.add(prof);
        return prof;
    }

    Profile *tryFind(String const &name) const
    {
        return self.tryFind(name)->maybeAs<Profile>();
    }

    Profile &currentProfile() const
    {
        return self.find(current).as<Profile>();
    }

    QVariant getDefaultFromConfig(String const &name)
    {
        try
        {
            // Get defaults for script values.
            Script script("record d; import Config; Config.setDefaults(d); d");
            Process proc(script);
            proc.execute();

            Record const &confDefaults = *proc.context().evaluator().result()
                    .as<RecordValue>().record();

            DENG2_ASSERT(confDefaults.has(name));

            Variable const &var = confDefaults[name];
            if(var.value().is<NumberValue>())
            {
                return var.value().asNumber();
            }
            else if(var.value().is<TextValue>())
            {
                return var.value().asText();
            }
            else
            {
                // Oops, we don't support this yet.
                DENG2_ASSERT(false);
            }
        }
        catch(Error const &er)
        {
            LOG_WARNING("Failed to find default for \"%s\": %s") << name << er.asText();
        }
        return QVariant();
    }

    /**
     * Gets the current values of the settings and saves them to a profile.
     */
    void fetch(String const &profileName)
    {
        Profile &prof = self.find(profileName).as<Profile>();
        if(prof.isReadOnly()) return;

        foreach(Setting const &st, settings.values())
        {
            QVariant val;

            switch(st.type)
            {
            case IntCVar:
                val = Con_GetInteger(st.name.toLatin1());
                break;

            case FloatCVar:
                val = Con_GetFloat(st.name.toLatin1());
                break;

            case StringCVar:
                val = QString(Con_GetString(st.name.toLatin1()));
                break;

            case ConfigVariable: {
                Value const &cfgValue = App::config()[st.name].value();
                if(cfgValue.is<NumberValue>())
                {
                    val = cfgValue.asNumber();
                }
                else
                {
                    val = cfgValue.asText();
                }
                break; }
            }

            prof.values[st.name] = val;
        }
    }

    void setCurrent(String const &name)
    {
        current = name;

        if(!self.persistentName().isEmpty())
        {
            App::config().set(confName(), name);
        }
    }

    void apply(String const &profileName)
    {
        Profile &profile = self.find(profileName).as<Profile>();

        foreach(Setting const &st, settings.values())
        {
            QVariant const &val = profile.values[st.name];
            st.setValue(val);
        }
    }

    void changeTo(String const &profileName)
    {
        LOG_AS("ConfigProfiles");
        DENG2_ASSERT(tryFind(profileName));

        if(current == profileName) return;

        if(!self.persistentName().isEmpty())
        {
            LOG_MSG("Changing %s profile to '%s'") << self.persistentName() << profileName;
        }

        // First update the old profile.
        fetch(current);

        // Then set the values from the other profile.
        setCurrent(profileName);
        apply(current);

        DENG2_FOR_PUBLIC_AUDIENCE(ProfileChange, i)
        {
            i->currentProfileChanged(profileName);
        }
    }

    void reset()
    {
        if(currentProfile().resetToDefaults())
        {
            apply(current);
        }
    }

    void resetSetting(String const &settingName) const
    {
        DENG2_ASSERT(settings.contains(settingName));
        DENG2_ASSERT(defaults.values.contains(settingName));

        settings[settingName].setValue(defaults.values[settingName]);
    }

    /**
     * For a persistent register, determines the name of the Config variable
     * that stores the name of the currently selected profile.
     */
    String confName() const
    {
        if(self.persistentName().isEmpty()) return "";
        return self.persistentName().concatenateMember("profile");
    }

    QVariant textToSettingValue(String const &text, String const &settingName) const
    {
        DENG2_ASSERT(settings.contains(settingName));

        Setting const &st = settings[settingName];

        switch(st.type)
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

        DENG2_ASSERT(false);
        return QVariant();
    }

    bool addCustomProfileIfMissing()
    {
        if(!tryFind(CUSTOM_PROFILE))
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
        if(!self.isPersistent() || newGame.isNull()) return;

        LOG_AS("ConfigProfiles");
        LOG_DEBUG("Game has been loaded, deserializing %s profiles") << self.persistentName();

        self.deserialize();

        // Settings haven't previously been created -- make sure we at least
        // have the Custom profile.
        if(addCustomProfileIfMissing())
        {
            current = CUSTOM_PROFILE;
        }

        // Still nothing?
        addCustomProfileIfMissing();

        if(App::config().objectNamespace().has(confName()))
        {
            // Update current profile.
            current = App::config()[confName()].value().asText();
        }

        if(!tryFind(current))
        {
            // Fall back to the one profile we know is available.
            if(tryFind(CUSTOM_PROFILE))
            {
                current = CUSTOM_PROFILE;
            }
            else
            {
                current = self.profiles().first();
            }
        }

        // Make sure these are the values now in use.
        apply(current);

        App::config().set(confName(), current);
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
        if(!self.isPersistent() || gameBeingUnloaded.isNull()) return;

        LOG_AS("ConfigProfiles");
        LOG_DEBUG("Game being unloaded, serializing %s profiles") << self.persistentName();

        // Update the current profile.
        fetch(current);

        // Remember which profile is the current one.
        App::config().set(confName(), current);

        self.serialize();
    }
};

ConfigProfiles::ConfigProfiles() : d(new Instance(this))
{}

ConfigProfiles &ConfigProfiles::define(SettingType type,
                                       String const &settingName,
                                       QVariant const &defaultValue)
{
    d->settings.insert(settingName, Instance::Setting(type, settingName));

    QVariant def;
    if(type == ConfigVariable)
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
    if(!tryFind(name) && !name.isEmpty())
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

    DENG2_FOR_AUDIENCE(ProfileChange, i)
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
    if(d->currentProfile().setName(name))
    {
        d->setCurrent(name);

        DENG2_FOR_AUDIENCE(ProfileChange, i)
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
    if(name == d->current) return;

    if(auto *prof = tryFind(name))
    {
        remove(*prof);
        delete prof;
    }
}

Profiles::AbstractProfile *
ConfigProfiles::profileFromInfoBlock(de::Info::BlockElement const &block)
{
    std::unique_ptr<Instance::Profile> prof(new Instance::Profile);
    prof->initializeFromInfoBlock(*this, block);
    return prof.release();
}
