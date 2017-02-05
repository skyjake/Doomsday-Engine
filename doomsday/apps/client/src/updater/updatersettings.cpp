/** @file updatersettings.cpp Persistent settings for automatic updates.
 * @ingroup updater
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "updater/updatersettings.h"
#include <QDateTime>
#include <QDesktopServices>
#include <de/Record>
#include <de/Config>
#include <de/TextValue>
#include <de/NumberValue>
#include <de/TimeValue>

using namespace de;

static String const VAR_FREQUENCY     ("updater.frequency");
static String const VAR_CHANNEL       ("updater.channel");
static String const VAR_LAST_CHECKED  ("updater.lastChecked");
static String const VAR_ONLY_MANUAL   ("updater.onlyManually");
static String const VAR_DELETE        ("updater.delete");
static String const VAR_DOWNLOAD_PATH ("updater.downloadPath");
static String const VAR_DELETE_PATH   ("updater.deleteAtStartup");
static String const VAR_AUTO_DOWNLOAD ("updater.autoDownload");

static String const SYMBOL_DEFAULT_DOWNLOAD("${DEFAULT}");

UpdaterSettings::UpdaterSettings()
{}

UpdaterSettings::Frequency UpdaterSettings::frequency() const
{
    return Frequency(Config::get().geti(VAR_FREQUENCY));
}

UpdaterSettings::Channel UpdaterSettings::channel() const
{
    return Channel(Config::get().geti(VAR_CHANNEL));
}

de::Time UpdaterSettings::lastCheckTime() const
{
    // Note that the variable has only AllowTime as the mode.
    return Config::get().getAs<TimeValue>(VAR_LAST_CHECKED).time();
}

bool UpdaterSettings::onlyCheckManually() const
{
    return Config::get().getb(VAR_ONLY_MANUAL);
}

bool UpdaterSettings::autoDownload() const
{
    return Config::get().getb(VAR_AUTO_DOWNLOAD);
}

bool UpdaterSettings::deleteAfterUpdate() const
{
    return Config::get().getb(VAR_DELETE);
}

de::NativePath UpdaterSettings::pathToDeleteAtStartup() const
{
    de::NativePath p = Config::get().gets(VAR_DELETE_PATH);
    de::String ext = p.toString().fileNameExtension();
    if (p.fileName().startsWith("doomsday") && (ext == ".exe" || ext == ".deb" || ext == ".dmg"))
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
    de::NativePath dir = Config::get().gets(VAR_DOWNLOAD_PATH);
    if (dir.toString() == SYMBOL_DEFAULT_DOWNLOAD)
    {
        dir = defaultDownloadPath();
    }
    return dir;
}

void UpdaterSettings::setDownloadPath(de::NativePath downloadPath)
{
    if (downloadPath == defaultDownloadPath())
    {
        downloadPath = SYMBOL_DEFAULT_DOWNLOAD;
    }
    Config::get().set(VAR_DOWNLOAD_PATH, downloadPath.toString());
}

void UpdaterSettings::setFrequency(UpdaterSettings::Frequency freq)
{
    Config::get().set(VAR_FREQUENCY, dint(freq));
}

void UpdaterSettings::setChannel(UpdaterSettings::Channel channel)
{
    Config::get().set(VAR_CHANNEL, dint(channel));
}

void UpdaterSettings::setLastCheckTime(de::Time const &time)
{
    Config::get(VAR_LAST_CHECKED) = new TimeValue(time);
}

void UpdaterSettings::setOnlyCheckManually(bool onlyManually)
{
    Config::get().set(VAR_ONLY_MANUAL, onlyManually);
}

void UpdaterSettings::setAutoDownload(bool autoDl)
{
    Config::get().set(VAR_AUTO_DOWNLOAD, autoDl);
}

void UpdaterSettings::setDeleteAfterUpdate(bool deleteAfter)
{
    Config::get().set(VAR_DELETE, deleteAfter);
}

void UpdaterSettings::useDefaultDownloadPath()
{
    setDownloadPath(defaultDownloadPath());
}

void UpdaterSettings::setPathToDeleteAtStartup(de::NativePath deletePath)
{
    Config::get().set(VAR_DELETE_PATH, deletePath.toString());
}

de::NativePath UpdaterSettings::defaultDownloadPath()
{
#ifdef DENG2_QT_5_0_OR_NEWER
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#else
    return QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
#endif
}

de::String UpdaterSettings::lastCheckAgo() const
{
    de::Time when = lastCheckTime();
    if (!when.isValid()) return ""; // Never checked.

    de::TimeDelta delta = when.since();
    if (delta < 0.0) return "";

    int t;
    if (delta < 60.0)
    {
        t = delta.asMilliSeconds() / 1000;
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                          arg(t != 1? QObject::tr("seconds") : QObject::tr("second"));
    }

    t = delta.asMinutes();
    if (t <= 60)
    {
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                arg(t != 1? QObject::tr("minutes") : QObject::tr("minute"));
    }

    t = delta.asHours();
    if (t <= 24)
    {
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                arg(t != 1? QObject::tr("hours") : QObject::tr("hour"));
    }

    t = delta.asDays();
    if (t <= 7)
    {
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                arg(t != 1? QObject::tr("days") : QObject::tr("day"));
    }

    return de::String("on " + when.asText(de::Time::FriendlyFormat));
}
