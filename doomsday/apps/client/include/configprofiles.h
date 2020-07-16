/** @file configprofiles.h  Configuration setting profiles.
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

#ifndef DE_CONFIGPROFILES_H
#define DE_CONFIGPROFILES_H

#include <de/string.h>
#include <de/observers.h>
#include <de/profiles.h>
#include <de/list.h>
#include <de/nonevalue.h>
#include <de/numbervalue.h>
#include <de/textvalue.h>

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
 * Config (and via con_config) as usual. ConfigProfiles is responsible for
 * storing the non-current profiles persistently. The (de)serialization occurs
 * whenever the game is (un)loaded, as all cvars are presently game-specific.
 *
 * It is possible to install new profiles via resource packs. The profiles should
 * be placed to /data/profiles/(persistentName)/.
 */
class ConfigProfiles : public de::Profiles
{
public:
    enum SettingType {
        IntCVar,
        FloatCVar,
        StringCVar,
        ConfigVariable      ///< Default value gotten from Config.setDefaults().
    };

    /// Notified when the current profile has changed.
    DE_DEFINE_AUDIENCE(ProfileChange, void currentProfileChanged(const de::String &name))

public:
    ConfigProfiles();

    /**
     * Defines a new setting in the profile.
     *
     * The default values of Config variables are taken from Config (so it must already
     * be initialized).
     *
     * @param type          Type of setting.
     * @param settingName   Name of the setting.
     * @param defaultValue  Default value of the setting (for cvars).
     *
     * @return Reference to this instance.
     */
    ConfigProfiles &define(SettingType       type,
                           const de::String &settingName,
                           const de::Value & value = de::NoneValue());

    inline ConfigProfiles &define(SettingType type, const de::String &settingName, int value)
    {
        return define(type, settingName, de::NumberValue(value));
    }
    inline ConfigProfiles &define(SettingType type, const de::String &settingName, float value)
    {
        return define(type, settingName, de::NumberValue(value));
    }

    de::String currentProfile() const;

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
    bool saveAsProfile(const de::String &name);

    /**
     * Changes the current settings profile.
     *
     * @param name  Name of the profile to use.
     */
    void setProfile(const de::String &name);

    /**
     * Resets the current profile to default values.
     */
    void resetToDefaults();

    /**
     * Resets one setting in the current profile to its default value.
     *
     * @param settingName  Name of the setting.
     */
    void resetSettingToDefaults(const de::String &settingName);

    /**
     * Renames the current profile.
     *
     * @param name  New name of the profile.
     *
     * @return @c true, if renamed successfully.
     */
    bool rename(const de::String &name);

    /**
     * Deletes a profile. The current profile cannot be deleted.
     *
     * @param name  Name of the profile to delete.
     */
    void deleteProfile(const de::String &name);

protected:
    AbstractProfile *profileFromInfoBlock(
            const de::Info::BlockElement &block) override;

private:
    DE_PRIVATE(d)
};

#endif // DE_CONFIGPROFILES_H
