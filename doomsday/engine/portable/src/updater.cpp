/**
 * @file updater.cpp
 * Automatic updater that works with dengine.net. @ingroup base
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

#include "updater.h"
#include "dd_version.h"
#include "dd_types.h"
#include "nativeui.h"
#include "json.h"
#include "updater/downloaddialog.h"
#include "updater/updateavailabledialog.h"
#include "updater/updatersettings.h"
#include "updater/versioninfo.h"
#include <de/App>
#include <de/Time>
#include <de/Log>
#include <QStringList>
#include <QDateTime>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTextStream>
#include <QDir>
#include <QDebug>

static Updater* updater = 0;

#ifdef MACOSX
#  define INSTALL_SCRIPT_NAME "deng-upgrade.scpt"
#endif

/// @todo The platform ID should come from the Builder.
#if defined(WIN32)
#  define PLATFORM_ID       "win-x86"

#elif defined(MACOSX)
#  if defined(__64BIT__)
#    define PLATFORM_ID     "mac10_6-x86-x86_64"
#  else
#    define PLATFORM_ID     "mac10_4-x86-ppc"
#  endif

#else
#  if defined(__64BIT__)
#    define PLATFORM_ID     "linux-x86_64"
#  else
#    define PLATFORM_ID     "linux-x86"
#  endif
#endif

struct Updater::Instance
{
    Updater* self;
    QNetworkAccessManager* network;
    DownloadDialog* download;

    VersionInfo latestVersion;
    QString latestPackageUri;
    QString latestLogUri;

    Instance(Updater* up) : self(up), network(0), download(0)
    {
        network = new QNetworkAccessManager(self);
    }

    ~Instance()
    {
    }

    QString composeCheckUri()
    {
        UpdaterSettings st;
        QString uri = QString(DOOMSDAY_HOMEURL) + "/latestbuild?";
        uri += QString("platform=") + PLATFORM_ID;
        uri += (st.channel() == UpdaterSettings::Stable? "&stable" : "&unstable");
        uri += "&graph";

        LOG_DEBUG("Check URI: ") << uri;
        return uri;
    }

    void queryLatestVersion()
    {
        UpdaterSettings().setLastCheckTime(de::Time());
        network->get(QNetworkRequest(composeCheckUri()));
    }

    void handleReply(QNetworkReply* reply)
    {
        reply->deleteLater(); // make sure it gets deleted

        QVariant result = parseJSON(QString::fromUtf8(reply->readAll()));
        if(!result.isValid()) return;

        QVariantMap map = result.toMap();
        latestPackageUri = map["direct_download_uri"].toString();
        latestLogUri = map["release_changeloguri"].toString();

        latestVersion = VersionInfo(map["version"].toString(), map["build_uniqueid"].toInt());

        LOG_VERBOSE("Received latest version information:\n"
                    " - version: %s (running %s)\n"
                    " - package: %s\n"
                    " - change log: %s")
                << latestVersion.asText()
                << VersionInfo().asText()
                << latestPackageUri << latestLogUri;

        // Is this newer than what we're running?
        // TODO: Silent check flag.
        UpdateAvailableDialog dlg(latestVersion, latestLogUri);
        if(dlg.exec())
        {
            LOG_MSG("Download and install.");
            download = new DownloadDialog(latestPackageUri);
            QObject::connect(download, SIGNAL(finished(int)), self, SLOT(downloadCompleted(int)));
            download->show();
        }
    }

    /**
     * Start the installation process using the provided distribution package.
     * The engine is shut down gracefully (game is always autosaved first).
     *
     * @param distribPackagePath  File path of the distribution package.
     */
    void startInstall(de::String distribPackagePath)
    {
#ifdef MACOSX
        // Generate a script to:
        // 1. Open the distrib package.
        // 2. Check that there is a "Doomsday Engine.app" inside.
        // 3. Move current app bundle to the Trash.
        // 4. Copy the on-disk app bundle to the old app's place.
        // 5. Close the distrib package.
        // 6. Open the new "Doomsday Engine.app".

        // This assumes the Doomsday executable is inside the Snowberry bundle.
        de::String execPath = QDir::cleanPath(QDir(DENG2_APP->executablePath().fileNamePath())
                                              .filePath("../../../.."));
        LOG_MSG(execPath);
        if(!execPath.fileName().endsWith(".app"))
        {
            Sys_MessageBox2(MBT_ERROR, "Updating", "Automatic update failed.",
                            ("Expected an application bundle, but found <b>" +
                             execPath.fileName() + "</b> instead.").toUtf8(), 0);
            return;
        }

        de::String appPath = execPath.fileNamePath();
        de::String appName = "Doomsday Engine.app";
        de::String volName = "Doomsday Engine " + latestVersion.base();

        QString scriptPath = QDir(QDesktopServices::storageLocation(QDesktopServices::TempLocation))
                             .filePath(INSTALL_SCRIPT_NAME);
        QFile file(scriptPath);
        if(file.open(QFile::WriteOnly | QFile::Truncate))
        {
            QTextStream out(&file);
            out << "tell application \"Finder\"\n"
                << "  set oldAppFile to POSIX file \"" << execPath << "\"\n"
                << "  set dmgFile to POSIX file \"" << distribPackagePath << "\"\n"
                << "  set destFolder to POSIX file \"" << appPath << "\"\n"
                << "  open document file dmgFile\n"
                << "  -- Wait for it to get mounted\n"
                << "  repeat until name of every disk contains \"" << volName << "\"\n"
                << "    delay 1\n"
                << "  end repeat\n"
                << "  -- Move the old app to the trash\n"
                << "  try\n"
                << "    delete oldAppFile\n"
                << "  end try\n"
                << "  -- Copy the new one\n"
                << "  duplicate \"" << volName << ":" << appName << "\" to folder (destFolder as string)\n"
                << "  -- Eject the disk\n"
                << "  eject \"" << volName << "\"\n"
                << "  -- Open the new app\n"
                << "  open (destFolder as string) & \":" << appName << "\"\n"
                << "end tell\n";
            file.close();
        }

        // Register a shutdown action to execute the script and quit.
#endif
    }
};

Updater::Updater(QObject *parent) : QObject(parent)
{
    d = new Instance(this);
    connect(d->network, SIGNAL(finished(QNetworkReply*)), this, SLOT(gotReply(QNetworkReply*)));

    d->queryLatestVersion();
}

Updater::~Updater()
{
    delete d;
}

void Updater::gotReply(QNetworkReply* reply)
{
    d->handleReply(reply);
}

void Updater::downloadCompleted(int result)
{
    if(result == DownloadDialog::Accepted)
    {
        d->startInstall(d->download->downloadedFilePath());
    }

    // The download dialgo can be dismissed now.
    d->download->deleteLater();
    d->download = 0;
}

void Updater_Init(void)
{
    updater = new Updater;
}

void Updater_Shutdown(void)
{
    delete updater;
}
