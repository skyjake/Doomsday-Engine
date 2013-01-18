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

#include "updatersettings.h"
#include "versioninfo.h"
#include <QDateTime>
#include <QDesktopServices>
#include <de/Record>
#include <de/App>
#include <de/Config>
#include <de/TextValue>
#include <de/NumberValue>
#include <de/TimeValue>

using namespace de;

#define VAR_FREQUENCY       "frequency"
#define VAR_CHANNEL         "channel"
#define VAR_LAST_CHECKED    "lastChecked"
#define VAR_ONLY_MANUAL     "onlyManually"
#define VAR_DELETE          "delete"
#define VAR_DOWNLOAD_PATH   "downloadPath"
#define VAR_DELETE_PATH     "deleteAtStartup"

#define SUBREC_NAME         "updater"
#define CONF(name)          SUBREC_NAME "." name

#define SYMBOL_DEFAULT_DOWNLOAD "${DEFAULT}"

void UpdaterSettings::initialize()
{
    Config &config = App::config();

    if(!config.names().has(SUBREC_NAME))
    {
        /**
         * @todo These defaults can be moved to Config.de when libdeng2 has
         * knowledge of the release type.
         */

        // Looks like we don't have existing values stored. Let's set up a
        // Record with the defaults.
        Record *s = new Record;

        s->addNumber(VAR_FREQUENCY, Weekly);
        s->addNumber(VAR_CHANNEL, QString(DOOMSDAY_RELEASE_TYPE) == "Stable"? Stable : Unstable);
        s->addTime(VAR_LAST_CHECKED, Time::invalidTime());

        bool checkByDefault = false;
#if defined(UNIX) && !defined(MACOSX)
        // On Unix platforms don't do automatic checks by default.
        checkByDefault = true;
#endif
        s->addBoolean(VAR_ONLY_MANUAL, checkByDefault);

        s->addBoolean(VAR_DELETE, true);
        s->addText(VAR_DELETE_PATH, "");
        s->addText(VAR_DOWNLOAD_PATH, SYMBOL_DEFAULT_DOWNLOAD);

        config.names().add(SUBREC_NAME, s);
    }
}

UpdaterSettings::UpdaterSettings()
{}

UpdaterSettings::Frequency UpdaterSettings::frequency() const
{
    return Frequency(App::config().geti(CONF(VAR_FREQUENCY)));
}

UpdaterSettings::Channel UpdaterSettings::channel() const
{
    return Channel(App::config().geti(CONF(VAR_CHANNEL)));
}

de::Time UpdaterSettings::lastCheckTime() const
{
    // Note that the variable has only AllowTime as the mode.
    return App::config().getAs<TimeValue>(CONF(VAR_LAST_CHECKED)).time();
}

bool UpdaterSettings::onlyCheckManually() const
{
    return App::config().getb(CONF(VAR_ONLY_MANUAL));
}

bool UpdaterSettings::deleteAfterUpdate() const
{
    return App::config().getb(CONF(VAR_DELETE));
}

de::NativePath UpdaterSettings::pathToDeleteAtStartup() const
{
    de::NativePath p = App::config().gets(CONF(VAR_DELETE_PATH));
    de::String ext = p.toString().fileNameExtension();
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
    de::NativePath dir = App::config().gets(CONF(VAR_DOWNLOAD_PATH));
    if(dir.toString() == SYMBOL_DEFAULT_DOWNLOAD)
    {
        dir = defaultDownloadPath();
    }
    return dir;
}

void UpdaterSettings::setDownloadPath(de::NativePath downloadPath)
{
    if(downloadPath == defaultDownloadPath())
    {
        downloadPath = SYMBOL_DEFAULT_DOWNLOAD;
    }
    App::config().names()[CONF(VAR_DOWNLOAD_PATH)] = new TextValue(downloadPath);
}

void UpdaterSettings::setFrequency(UpdaterSettings::Frequency freq)
{
    App::config().names()[CONF(VAR_FREQUENCY)] = new NumberValue(dint(freq));
}

void UpdaterSettings::setChannel(UpdaterSettings::Channel channel)
{
    App::config().names()[CONF(VAR_CHANNEL)] = new NumberValue(dint(channel));
}

void UpdaterSettings::setLastCheckTime(const de::Time &time)
{
    App::config().names()[CONF(VAR_LAST_CHECKED)] = new TimeValue(time);
}

void UpdaterSettings::setOnlyCheckManually(bool onlyManually)
{
    App::config().names()[CONF(VAR_ONLY_MANUAL)] = new NumberValue(onlyManually);
}

void UpdaterSettings::setDeleteAfterUpdate(bool deleteAfter)
{
    App::config().names()[CONF(VAR_DELETE)] = new NumberValue(deleteAfter);
}

void UpdaterSettings::useDefaultDownloadPath()
{
    setDownloadPath(defaultDownloadPath());
}

void UpdaterSettings::setPathToDeleteAtStartup(de::NativePath deletePath)
{
    App::config().names()[CONF(VAR_DELETE_PATH)] = new TextValue(deletePath);
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
