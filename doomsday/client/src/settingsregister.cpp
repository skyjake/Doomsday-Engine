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

#include "settingsregister.h"
#include <QMap>
#include <QList>

#include <de/App>
#include <de/Script>
#include <de/Process>
#include <de/RecordValue>
#include <de/NumberValue>
#include "con_main.h"

using namespace de;

DENG2_PIMPL_NOREF(SettingsRegister)
{
    struct Setting
    {
        SettingType type;
        String name;

        Setting(SettingType t, String n) : type(t), name(n) {}
    };

    typedef QList<Setting> Settings;
    Settings settings;

    struct Profile
    {
        typedef QMap<String, QVariant> Values;
        Values values;
    };

    typedef QMap<String, Profile *> Profiles;
    Profiles profiles;
    Profile defaults;

    String current;

    Instance() : current("User")
    {
        addProfile(current);
    }

    ~Instance()
    {
        qDeleteAll(profiles.values());
    }

    void addProfile(String const &name)
    {
        if(profiles.contains(name))
        {
            delete profiles[name];
        }

        profiles.insert(name, new Profile);
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
     * Gets the current values of the settings and saves them to the current
     * profile.
     */
    void fetch(String const &profileName)
    {
        DENG2_ASSERT(profiles.contains(profileName));

        Profile &prof = *profiles[profileName];

        foreach(Setting const &st, settings)
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

        foreach(Setting const &st, settings)
        {
            QVariant const &val = prof.values[st.name];

            switch(st.type)
            {
            case IntCVar:
                Con_SetInteger(st.name.toAscii(), val.toInt());
                break;

            case FloatCVar:
                Con_SetFloat(st.name.toAscii(), val.toFloat());
                break;

            case StringCVar:
                Con_SetString(st.name.toAscii(), val.toString().toUtf8());
                break;

            case ConfigVariable:
                App::config()[st.name].set(NumberValue(val.toDouble()));
                break;
            }
        }
    }

    void changeTo(String const &profileName)
    {
        DENG2_ASSERT(profiles.contains(profileName));

        if(current == profileName) return;

        // First update the old profile.
        fetch(current);

        // Then set the values from the other profile.
        apply(current = profileName);
    }

    void reset()
    {
        currentProfile().values = defaults.values;
        apply(current);
    }
};

SettingsRegister::SettingsRegister() : d(new Instance)
{}

SettingsRegister &SettingsRegister::define(SettingType type,
                                           String const &settingName,
                                           QVariant const &defaultValue)
{
    d->settings << Instance::Setting(type, settingName);

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

void SettingsRegister::saveAsProfile(String const &name)
{
    d->addProfile(name);
    d->fetch(name);
}

void SettingsRegister::setProfile(String const &name)
{
    d->changeTo(name);
}

void SettingsRegister::resetToDefaults()
{
    d->reset();
}

void SettingsRegister::rename(String const &name)
{
    Instance::Profile *p = d->profiles.take(d->current);
    d->profiles.insert(name, p);
    d->current = name;
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
