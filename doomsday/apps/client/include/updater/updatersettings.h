/**
 * @file updatersettings.h
 * Persistent settings for automatic updates. @ingroup updater
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_CLIENT_UPDATERSETTINGS_H
#define DE_CLIENT_UPDATERSETTINGS_H

#include <de/time.h>
#include <de/string.h>
#include <de/nativepath.h>

/**
 * Convenient interface to the Updater settings. All changes to the settings
 * are immediately saved persistently. In practice, the values are stored in
 * Config.updater.
 */
class UpdaterSettings
{
public:
    enum Frequency
    {
        Daily     = 0,
        Biweekly  = 1,   // Tuesday + Saturday
        Weekly    = 2,   // 7 days
        Monthly   = 3,   // 30 days
        AtStartup = 4
    };
    enum Channel
    {
        Stable   = 0,
        Unstable = 1,
        StableOrCandidate = 2,
    };

public:
    UpdaterSettings();

    Frequency frequency() const;
    Channel channel() const;
    de::Time lastCheckTime() const;
    bool onlyCheckManually() const;
    bool autoDownload() const;
    bool deleteAfterUpdate() const;
    bool isDefaultDownloadPath() const;
    de::NativePath downloadPath() const;
    de::NativePath pathToDeleteAtStartup() const;

    /**
     * @return Human-readable description of when the latest update
     * check was made.
     */
    de::String lastCheckAgo() const;

    void setFrequency(Frequency freq);
    void setChannel(Channel channel);
    void setLastCheckTime(const de::Time& time);
    void setOnlyCheckManually(bool onlyManually);
    void setAutoDownload(bool autoDl);
    void setDeleteAfterUpdate(bool deleteAfter);
    void setDownloadPath(de::NativePath downloadPath);
    void useDefaultDownloadPath();
    void setPathToDeleteAtStartup(de::NativePath deletePath);

    static de::NativePath defaultDownloadPath();
};

#endif // DE_CLIENT_UPDATERSETTINGS_H
