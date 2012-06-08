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

de::String UpdaterSettings::pathToDeleteAtStartup() const
{
    de::String p = de::String::fromNativePath(QSettings().value(STK_DELETE_PATH).toString());
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
    de::String path = downloadPath();
    return path == defaultDownloadPath();
}

de::String UpdaterSettings::downloadPath() const
{
    de::String dir = QSettings().value(STK_DOWNLOAD_PATH, defaultDownloadPath()).toString();
    if(dir == "${DEFAULT}")
    {
        dir = defaultDownloadPath();
    }
    return dir;
}

void UpdaterSettings::setDownloadPath(de::String downloadPath)
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

void UpdaterSettings::setPathToDeleteAtStartup(de::String deletePath)
{
    QSettings().setValue(STK_DELETE_PATH, deletePath);
}

de::String UpdaterSettings::defaultDownloadPath()
{
    return QDesktopServices::storageLocation(QDesktopServices::TempLocation);
}
