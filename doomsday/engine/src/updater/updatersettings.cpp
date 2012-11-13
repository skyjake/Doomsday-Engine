/**
 * @file updatersettings.cpp
 * Persistent settings for automatic updates. @ingroup updater
 *
 * @todo  There should be a unified persistent configuration subsystem that
 * combines the characteristics of QSettings and console variables.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include "updatersettings.h"
#include "versioninfo.h"
#include <QDateTime>
#include <QSettings>
#include <QDesktopServices>

#define STK_FREQUENCY       "updater/frequency"
#define STK_CHANNEL         "updater/channel"
#define STK_LAST_CHECKED    "updater/lastChecked"
#define STK_ONLY_MANUAL     "updater/onlyManually"
#define STK_DELETE          "updater/delete"
#define STK_DOWNLOAD_PATH   "updater/downloadPath"
#define STK_DELETE_PATH     "updater/deleteAtStartup"

UpdaterSettings::UpdaterSettings()
{}

UpdaterSettings::Frequency UpdaterSettings::frequency() const
{
    return Frequency(QSettings().value(STK_FREQUENCY, Weekly).toInt());
}

UpdaterSettings::Channel UpdaterSettings::channel() const
{
    return Channel(QSettings().value(STK_CHANNEL,
            QString(DOOMSDAY_RELEASE_TYPE) == "Stable"? Stable : Unstable).toInt());
}

de::Time UpdaterSettings::lastCheckTime() const
{
    return QSettings().value(STK_LAST_CHECKED).toDateTime();
}

bool UpdaterSettings::onlyCheckManually() const
{
    bool byDefault = false;
#if defined(UNIX) && !defined(MACOSX)
    // On Unix platforms don't do automatic checks by default.
    byDefault = true;
#endif
    return QSettings().value(STK_ONLY_MANUAL, byDefault).toBool();
}

bool UpdaterSettings::deleteAfterUpdate() const
{
    return QSettings().value(STK_DELETE, true).toBool();
}

de::NativePath UpdaterSettings::pathToDeleteAtStartup() const
{
    de::NativePath p(QSettings().value(STK_DELETE_PATH).toString());
    de::String ext = p.fileNameExtension();
    if(p.fileName().startsWith("doomsday") && (ext == ".exe" || ext == ".deb" || ext == ".dmg"))
    {
        return p;
    }
    // Doesn't look valid.
    return "";
}

bool UpdaterSettings::isDefaultDownloadPath() const
{
    return downloadPath() == defaultDownloadPath();
}

de::NativePath UpdaterSettings::downloadPath() const
{
    de::NativePath dir = QSettings().value(STK_DOWNLOAD_PATH, defaultDownloadPath()).toString();
    if(dir == "${DEFAULT}")
    {
        dir = defaultDownloadPath();
    }
    return dir;
}

void UpdaterSettings::setDownloadPath(de::NativePath downloadPath)
{
    if(downloadPath == defaultDownloadPath())
    {
        downloadPath = "${DEFAULT}";
    }
    QSettings().setValue(STK_DOWNLOAD_PATH, downloadPath);
}

void UpdaterSettings::setFrequency(UpdaterSettings::Frequency freq)
{
    QSettings().setValue(STK_FREQUENCY, int(freq));
}

void UpdaterSettings::setChannel(UpdaterSettings::Channel channel)
{
    QSettings().setValue(STK_CHANNEL, int(channel));
}

void UpdaterSettings::setLastCheckTime(const de::Time &time)
{
    QSettings().setValue(STK_LAST_CHECKED, time.asDateTime());
}

void UpdaterSettings::setOnlyCheckManually(bool onlyManually)
{
    QSettings().setValue(STK_ONLY_MANUAL, onlyManually);
}

void UpdaterSettings::setDeleteAfterUpdate(bool deleteAfter)
{
    QSettings().setValue(STK_DELETE, deleteAfter);
}

void UpdaterSettings::useDefaultDownloadPath()
{
    setDownloadPath(defaultDownloadPath());
}

void UpdaterSettings::setPathToDeleteAtStartup(de::NativePath deletePath)
{
    QSettings().setValue(STK_DELETE_PATH, deletePath);
}

de::NativePath UpdaterSettings::defaultDownloadPath()
{
    return QDesktopServices::storageLocation(QDesktopServices::TempLocation);
}

de::String UpdaterSettings::lastCheckAgo() const
{
    de::Time when = lastCheckTime();
    de::Time::Delta delta = when.since();

    int t;
    if(delta < 60.0)
    {
        t = delta.asMilliSeconds() / 1000;
        return de::String("%1 second%2 ago").arg(t).arg(t != 1? "s" : "");
    }

    t = delta.asMinutes();
    if(t <= 60)
    {
        return de::String("%1 minute%2 ago").arg(t).arg(t != 1? "s" : "");
    }

    t = delta.asHours();
    if(t <= 24)
    {
        return de::String("%1 hour%2 ago").arg(t).arg(t != 1? "s" : "");
    }

    return de::String("on " + when.asText(de::Time::FriendlyFormat));
}
