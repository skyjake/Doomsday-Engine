/** @file settingsregister.h  Register of settings profiles.
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

#ifndef DENG_SETTINGSREGISTER_H
#define DENG_SETTINGSREGISTER_H

#include <de/String>
#include <de/Observers>
#include <QVariant>
#include <QList>

/**
 * Collection of settings (cvars, Config variables) of which there can be
 * several alternative profiles. When a register is created, it automatically
 * gets a profile called "User".
 *
 * The default values are stored separately, so that any profile can be reseted
 * back to the default values.
 *
 * All settings of a register should be defined before it gets used.
 *
 * The current profile is simply whatever values the identified cvars/variables
 * presently hold. These current values get stored persistently in the app's
 * Config (and via con_config) as usual. SettingsRegister is responsible for
 * storing the non-current profiles persistently. The (de)serialization occurs
 * whenever the game is (un)loaded, as all cvars are presently game-specific.
 *
 * It is possible to install new profiles via resource packs. The profiles should
 * be placed to /data/profiles/(persistentName)/.
 */
class SettingsRegister
{
public:
    enum SettingType {
        IntCVar,
        FloatCVar,
        StringCVar,
        ConfigVariable      ///< Default value gotten from Config.setDefaults().
    };

    DENG2_DEFINE_AUDIENCE(ProfileChange, void currentProfileChanged(de::String const &name))

public:
    SettingsRegister();

    /**
     * Sets the name this register will use for storing profiles persistently.
     * By default the register has no persistent name and thus will not be
     * stored persistently.
     *
     * In the Config, there will be a record called "Config.(persistentName)"
     * containing relevant information.
     *
     * @param name  Persistent name for the register. Must be file name and
     *              script variable name friendly.
     */
    void setPersistentName(de::String const &name);

    /**
     * Defines a new setting in the profile.
     *
     * @param type          Type of setting.
     * @param settingName   Name of the setting.
     * @param defaultValue  Default value of the setting (for cvars).
     *
     * @return Reference to this instance.
     */
    SettingsRegister &define(SettingType type,
                             de::String const &settingName,
                             QVariant const &defaultValue = QVariant());

    de::String currentProfile() const;

    /**
     * Determines if a profile should be considered read-only. The UI should
     * not let the user modify profiles that are read-only.
     *
     * @param name  Profile name.
     *
     * @return @c true, if the profile is read-only.
     */
    bool isReadOnlyProfile(de::String const &name) const;

    /**
     * Current values of the settings are saved as a profile. If there is a
     * profile with the same name, it will be replaced. The current profile is
     * not changed.
     *
     * @param name  Name of the profile.
     *
     * @return @c true if a new profile was created. @c false, if the operation
     * failed (e.g., name already in use).
     */
    bool saveAsProfile(de::String const &name);

    /**
     * Changes the current settings profile.
     *
     * @param name  Name of the profile to use.
     */
    void setProfile(de::String const &name);

    /**
     * Resets the current profile to default values.
     */
    void resetToDefaults();

    /**
     * Resets one setting in the current profile to its default value.
     *
     * @param settingName  Name of the setting.
     */
    void resetSettingToDefaults(de::String const &settingName);

    /**
     * Renames the current profile.
     *
     * @param name  New name of the profile.
     *
     * @return @c true, if renamed successfully.
     */
    bool rename(de::String const &name);

    /**
     * Deletes a profile. The current profile cannot be deleted.
     *
     * @param name  Name of the profile to delete.
     */
    void deleteProfile(de::String const &name);

    /**
     * Lists the names of all the existing profiles.
     */
    QList<de::String> profiles() const;

    int profileCount() const;

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_SETTINGSREGISTER_H
