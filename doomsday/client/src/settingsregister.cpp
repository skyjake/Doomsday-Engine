/** @file settingsregister.cpp  Register of settings profiles.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "con_main.h"
#include "settingsregister.h"

#include <QMap>
#include <QList>

#include <de/App>
#include <de/Script>
#include <de/Process>
#include <de/RecordValue>
#include <de/NumberValue>
#include <de/ZipArchive>
#include <de/Info>

#include <QTextStream>

using namespace de;

DENG2_PIMPL(SettingsRegister),
DENG2_OBSERVES(App, GameUnload),
DENG2_OBSERVES(App, GameChange)
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
                Con_SetInteger(name.toAscii(), val.toInt());
                break;

            case FloatCVar:
                Con_SetFloat(name.toAscii(), val.toFloat());
                break;

            case StringCVar:
                Con_SetString(name.toAscii(), val.toString().toUtf8());
                break;

            case ConfigVariable:
                App::config()[name].set(NumberValue(val.toDouble()));
                break;
            }
        }
    };

    typedef QMap<String, Setting> Settings;
    Settings settings;

    struct Profile
    {
        bool readOnly; ///< Profile has been loaded from a read-only file, won't be serialized.

        typedef QMap<String, QVariant> Values;
        Values values;

        Profile() : readOnly(false) {}
    };

    typedef QMap<String, Profile *> Profiles;
    Profiles profiles;
    Profile defaults;

    String persistentName;
    String current;

    Instance(Public *i) : Base(i), current("Custom")
    {
        App::app().audienceForGameUnload += this;
        App::app().audienceForGameChange += this;

        addProfile(current);
    }

    ~Instance()
    {
        App::app().audienceForGameUnload -= this;
        App::app().audienceForGameChange -= this;

        clearProfiles();
    }

    Profile *addProfile(String const &name)
    {
        if(profiles.contains(name))
        {
            delete profiles[name];
        }

        Profile *p = new Profile;
        profiles.insert(name, p);
        return p;
    }

    void clearProfiles()
    {
        qDeleteAll(profiles.values());
        profiles.clear();
    }

    Profile &currentProfile() const
    {
        DENG2_ASSERT(profiles.contains(current));
        return *profiles[current];
    }

    QVariant getDefaultFromConfig(String const &name)
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
        else
        {
            // Oops, we don't support this yet.
            DENG2_ASSERT(false);
        }
        return QVariant();
    }

    /**
     * Gets the current values of the settings and saves them to a profile.
     */
    void fetch(String const &profileName)
    {
        DENG2_ASSERT(profiles.contains(profileName));

        Profile &prof = *profiles[profileName];
        if(prof.readOnly) return;

        foreach(Setting const &st, settings.values())
        {
            QVariant val;

            switch(st.type)
            {
            case IntCVar:
                val = Con_GetInteger(st.name.toAscii());
                break;

            case FloatCVar:
                val = Con_GetFloat(st.name.toAscii());
                break;

            case StringCVar:
                val = QString(Con_GetString(st.name.toAscii()));
                break;

            case ConfigVariable:
                val = App::config()[st.name].value().asNumber();
                break;
            }

            prof.values[st.name] = val;
        }
    }

    void apply(String const &profileName)
    {
        DENG2_ASSERT(profiles.contains(profileName));

        Profile const &prof = *profiles[profileName];
        foreach(Setting const &st, settings.values())
        {
            QVariant const &val = prof.values[st.name];
            st.setValue(val);
        }
    }

    void changeTo(String const &profileName)
    {
        LOG_AS("SettingsRegister");
        DENG2_ASSERT(profiles.contains(profileName));

        if(current == profileName) return;

        if(!persistentName.isEmpty())
        {
            LOG_MSG("Changing %s profile to '%s'") << persistentName << profileName;
        }

        // First update the old profile.
        fetch(current);

        // Then set the values from the other profile.
        apply(current = profileName);

        DENG2_FOR_PUBLIC_AUDIENCE(ProfileChange, i)
        {
            i->currentProfileChanged(profileName);
        }
    }

    void reset()
    {
        if(!currentProfile().readOnly)
        {
            currentProfile().values = defaults.values;
            apply(current);
        }
    }

    void resetSetting(String const &settingName) const
    {
        DENG2_ASSERT(settings.contains(settingName));
        DENG2_ASSERT(defaults.values.contains(settingName));

        settings[settingName].setValue(defaults.values[settingName]);
    }

    String confName() const
    {
        if(persistentName.isEmpty()) return "";
        return persistentName + ".profile";
    }

    String fileName() const
    {
        if(persistentName.isEmpty()) return "";
        return String("/home/configs/%1.dei").arg(persistentName);
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

    void loadProfilesFromInfo(File const &file)
    {
        try
        {
            LOG_DEBUG("Reading setting profiles from %s") << file.description();

            Block raw;
            file >> raw;

            de::Info info;
            info.setAllowDuplicateBlocksOfType(QStringList() << "profile");
            info.parse(String::fromUtf8(raw));

            foreach(de::Info::Element const *elem, info.root().contentsInOrder())
            {
                if(!elem->isBlock()) continue;

                // There may be multiple profiles in the file.
                de::Info::BlockElement const &profBlock = elem->as<de::Info::BlockElement>();
                if(profBlock.blockType() == "profile")
                {
                    String profileName = profBlock.keyValue("name").text;
                    if(profileName.isEmpty()) continue; // Skip this one...

                    LOG_DEBUG("Reading profile '%s'") << profileName;

                    Profile *prof = addProfile(profileName);

                    // Read all the setting values from the profile block.
                    foreach(de::Info::Element const *e, profBlock.contentsInOrder())
                    {
                        if(!e->isBlock()) continue;

                        de::Info::BlockElement const &setBlock = e->as<de::Info::BlockElement>();
                        prof->values[setBlock.name()] = textToSettingValue(setBlock.keyValue("value").text,
                                                                           setBlock.name());
                    }
                }
            }
        }
        catch(Error const &er)
        {
            LOG_WARNING("Failed to load setting profiles from %s:\n%s") << file.description() << er.asText();
        }
    }

    /**
     * Deserializes all the profiles (see aboutToUnloadGame()). In addition,
     * fixed/built-in profiles are loaded from /data/profiles/(persistentName)/
     *
     * @param newGame  New current game.
     */
    void currentGameChanged(game::Game const &newGame)
    {
        if(persistentName.isEmpty() || newGame.isNull()) return;

        LOG_AS("SettingsRegister");
        LOG_DEBUG("Game has been loaded, deserializing %s profiles") << persistentName;

        clearProfiles();

        // Read all fixed profiles from /data/profiles/(persistentName)/
        FS::FoundFiles folders;
        App::fileSystem().findAll(String("profiles") / persistentName, folders);
        DENG2_FOR_EACH(FS::FoundFiles, i, folders)
        {
            if(Folder const *folder = (*i)->maybeAs<Folder>())
            {
                // Let's see if it contains any .dei files.
                DENG2_FOR_EACH_CONST(Folder::Contents, k, folder->contents())
                {
                    if(k->first.fileNameExtension() == ".dei")
                    {
                        // Load this profile.
                        loadProfilesFromInfo(*k->second);
                    }
                }
            }
        }

        // Read /home/configs/(persistentName).dei
        if(File const *file = App::rootFolder().tryLocate<File const>(fileName()))
        {
            loadProfilesFromInfo(*file);
        }

        if(App::config().names().has(confName()))
        {
            // Update current profile.
            current = App::config()[confName()].value().asText();

            // Make sure these are the values now in use.
            apply(current);
        }
    }

    /**
     * Serializes all the profiles:
     * - Config.(persistentName).profile stores the name of the current profile
     * - /home/configs/(persistentName).pack contains all the existing profile
     *   values, with one file per profile
     *
     * @param gameBeingUnloaded  Current game.
     */
    void aboutToUnloadGame(game::Game const &gameBeingUnloaded)
    {
        if(persistentName.isEmpty() || gameBeingUnloaded.isNull()) return;

        LOG_AS("SettingsRegister");
        LOG_DEBUG("Game being unloaded, serializing %s profiles") << persistentName;

        // Update the current profile.
        fetch(current);

        // Remember which profile is the current one.
        App::config().set(confName(), current);

        // We will write one Info file with all the profiles.
        String info;
        QTextStream os(&info);
        os.setCodec("UTF-8");

        os << "# Autogenerated Info file based on " << persistentName << " settings\n";

        // Write /home/configs/(persistentName).dei with all non-readonly profiles.
        int count = 0;
        DENG2_FOR_EACH_CONST(Profiles, i, profiles)
        {
            if(i.value()->readOnly) continue;

            ++count;

            os << "\nprofile {\n    name: " << i.key() << "\n";

            DENG2_FOR_EACH_CONST(Profile::Values, val, i.value()->values)
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

                os << "    setting \"" << st.name << "\" {\n"
                   << "        value: " << valueText << "\n"
                   << "    }\n";
            }

            os << "}\n";
        }

        // Create the pack and update the file system.
        App::rootFolder().replaceFile(fileName()) << info.toUtf8();

        LOG_VERBOSE("Wrote \"%s\" with %i profile%s") << fileName() << count << (count != 1? "s" : "");
    }
};

SettingsRegister::SettingsRegister() : d(new Instance(this))
{}

void SettingsRegister::setPersistentName(String const &name)
{
    d->persistentName = name;
}

SettingsRegister &SettingsRegister::define(SettingType type,
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

String SettingsRegister::currentProfile() const
{
    return d->current;
}

bool SettingsRegister::isReadOnlyProfile(String const &name) const
{
    if(d->profiles.contains(name))
    {
        return d->profiles[name]->readOnly;
    }
    DENG2_ASSERT(false);
    return false;
}

bool SettingsRegister::saveAsProfile(String const &name)
{
    if(!d->profiles.contains(name) && !name.isEmpty())
    {
        d->addProfile(name);
        d->fetch(name);
        return true;
    }
    return false;
}

void SettingsRegister::setProfile(String const &name)
{
    d->changeTo(name);
}

void SettingsRegister::resetToDefaults()
{
    d->reset();
}

void SettingsRegister::resetSettingToDefaults(String const &settingName)
{
    d->resetSetting(settingName);
}

bool SettingsRegister::rename(String const &name)
{
    if(!d->profiles.contains(name) && !name.isEmpty())
    {
        Instance::Profile *p = d->profiles.take(d->current);
        d->profiles.insert(name, p);
        d->current = name;
        return true;
    }
    return false;
}

void SettingsRegister::deleteProfile(String const &name)
{
    // Can't delete the current profile.
    if(name == d->current) return;

    delete d->profiles.take(name);
}

QList<String> SettingsRegister::profiles() const
{
    return d->profiles.keys();
}

int SettingsRegister::profileCount() const
{
    return d->profiles.size();
}
