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
    return QSettings().value(STK_ONLY_MANUAL, false).toBool();
}

bool UpdaterSettings::deleteAfterUpdate() const
{
    return QSettings().value(STK_DELETE, true).toBool();
}

bool UpdaterSettings::isDefaultDownloadPath() const
{
    de::String path = downloadPath();
    return path == defaultDownloadPath();
}

de::String UpdaterSettings::downloadPath() const
{
    return QSettings().value(STK_DOWNLOAD_PATH, defaultDownloadPath()).toString();
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

void UpdaterSettings::setDownloadPath(de::String downloadPath)
{
    QSettings().setValue(STK_DOWNLOAD_PATH, downloadPath);
}

void UpdaterSettings::useDefaultDownloadPath()
{
    setDownloadPath(defaultDownloadPath());
}

de::String UpdaterSettings::defaultDownloadPath()
{
    return QDesktopServices::storageLocation(QDesktopServices::TempLocation);
}
