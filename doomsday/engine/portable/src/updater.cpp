/**
 * @file updater.cpp
 * Automatic updater that works with dengine.net. @ingroup base
 *
 * When one of the updater dialogs is shown, the main window is automatically
 * switched to windowed mode. This is because the dialogs would be hidden
 * behind the main window or incorrectly located when the main window is in
 * fullscreen mode. It is also possible that the screen resolution is too low
 * to fit the shown dialogs. In the long term, the native dialogs should be
 * replaced with the engine's own (scriptable) UI widgets (once they are
 * available).
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

#include <QStringList>
#include <QDateTime>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTextStream>
#include <QDir>
#include <QDebug>

#include "de_platform.h"

#ifdef WIN32
#  undef open
#endif

#include <stdlib.h>
#include "sys_system.h"
#include "dd_version.h"
#include "dd_types.h"
#include "con_main.h"
#include "nativeui.h"
#include "window.h"
#include "json.h"
#include "updater.h"
#include "updater/downloaddialog.h"
#include "updater/updateavailabledialog.h"
#include "updater/updatersettings.h"
#include "updater/updatersettingsdialog.h"
#include "updater/versioninfo.h"
#include <de/App>
#include <de/LegacyCore>
#include <de/Time>
#include <de/Date>
#include <de/Log>

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

static de::CommandLine* installerCommand;

/**
 * Callback for atexit(). Create the installerCommand before calling this.
 */
static void runInstallerCommand(void)
{
    installerCommand->execute();
    delete installerCommand;
    installerCommand = 0;
}

static bool switchToWindowedMode()
{
    bool wasFull = Window_IsFullscreen(Window_Main());
    if(wasFull)
    {
        int attribs[] = { DDWA_FULLSCREEN, false, DDWA_END };
        Window_ChangeAttributes(Window_Main(), attribs);
    }
    return wasFull;
}

static void switchBackToFullscreen(bool wasFull)
{
    if(wasFull)
    {
        int attribs[] = { DDWA_FULLSCREEN, true, DDWA_END };
        Window_ChangeAttributes(Window_Main(), attribs);
    }
}

struct Updater::Instance
{
    Updater* self;
    QNetworkAccessManager* network;
    DownloadDialog* download;
    bool alwaysShowNotification;
    UpdateAvailableDialog* availableDlg;
    UpdaterSettingsDialog* settingsDlg;
    bool backToFullscreen;

    VersionInfo latestVersion;
    QString latestPackageUri;
    QString latestLogUri;

    Instance(Updater* up) : self(up), network(0), download(0), availableDlg(0), settingsDlg(0), backToFullscreen(false)
    {
        network = new QNetworkAccessManager(self);

        // Delete a package installed earlier?
        UpdaterSettings st;
        if(st.deleteAfterUpdate())
        {
            de::String p = st.pathToDeleteAtStartup();
            if(!p.isEmpty())
            {
                QFile file(p);
                if(file.exists())
                {
                    LOG_INFO("Deleting previously installed package: %s") << p;
                    file.remove();
                }
            }
        }
        st.setPathToDeleteAtStartup("");
    }

    ~Instance()
    {
        if(settingsDlg) delete settingsDlg;

        // Delete the ongoing download.
        if(download) delete download;
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

    bool shouldCheckForUpdate() const
    {
        UpdaterSettings st;
        if(st.onlyCheckManually()) return false;

        float dayInterval = 30;
        switch(st.frequency())
        {
        case UpdaterSettings::AtStartup:
            dayInterval = 0;
            break;

        case UpdaterSettings::Daily:
            dayInterval = 1;
            break;

        case UpdaterSettings::Biweekly:
            dayInterval = 5;
            break;

        case UpdaterSettings::Weekly:
            dayInterval = 7;
            break;

        default:
            break;
        }

        de::Time now;

        // Check always when the day interval has passed. Note that this
        // doesn't check the actual time interval since the last check, but the
        // difference in "calendar" days.
        if(st.lastCheckTime().asDate().daysTo(de::Date()) >= dayInterval)
            return true;

        if(st.frequency() == UpdaterSettings::Biweekly)
        {
            // Check on Tuesday and Saturday, as the builds are usually on
            // Monday and Friday.
            int weekday = now.asDateTime().date().dayOfWeek();
            if(weekday == 2 || weekday == 6) return true;
        }

        // No need to check right now.
        return false;
    }

    void showSettingsNonModal()
    {
        if(!settingsDlg)
        {
            settingsDlg = new UpdaterSettingsDialog(Window_Widget(Window_Main()));
            QObject::connect(settingsDlg, SIGNAL(finished(int)), self, SLOT(settingsDialogClosed(int)));
        }
        else
        {
            settingsDlg->fetch();
        }
        settingsDlg->open();
    }

    void queryLatestVersion(bool notifyAlways)
    {
        UpdaterSettings().setLastCheckTime(de::Time());
        alwaysShowNotification = notifyAlways;
        doCheckRequest();
    }

    void doCheckRequest()
    {
        network->get(QNetworkRequest(composeCheckUri()));
    }

    void handleReply(QNetworkReply* reply)
    {
        reply->deleteLater(); // make sure it gets deleted

        if(reply->error() != QNetworkReply::NoError)
        {
            Con_Message("Network request failed: %s\n", reply->url().toString().toUtf8().constData());
            return;
        }

        QVariant result = parseJSON(QString::fromUtf8(reply->readAll()));
        if(!result.isValid()) return;

        QVariantMap map = result.toMap();
        latestPackageUri = map["direct_download_uri"].toString();
        latestLogUri = map["release_changeloguri"].toString();

        latestVersion = VersionInfo(map["version"].toString(), map["build_uniqueid"].toInt());

        VersionInfo currentVersion;

        LOG_VERBOSE("Received latest version information:\n"
                    " - version: %s (running %s)\n"
                    " - package: %s\n"
                    " - change log: %s")
                << latestVersion.asText()
                << currentVersion.asText()
                << latestPackageUri << latestLogUri;

        if(availableDlg)
        {
            // This was a recheck.
            availableDlg->showResult(latestVersion, latestLogUri);
            return;
        }

        // Is this newer than what we're running?
        if(latestVersion > currentVersion || alwaysShowNotification)
        {
            Con_Message("Found an update: %s\n", latestVersion.asText().toUtf8().constData());

            // Automatically switch to windowed mode for convenience.
            bool wasFull = switchToWindowedMode();

            UpdateAvailableDialog dlg(latestVersion, latestLogUri, Window_Widget(Window_Main()));
            QObject::connect(&dlg, SIGNAL(checkAgain()), self, SLOT(recheck()));
            availableDlg = &dlg;
            if(dlg.exec())
            {
                availableDlg = 0;
                LOG_MSG("Download and install.");
                download = new DownloadDialog(latestPackageUri);
                QObject::connect(download, SIGNAL(finished(int)), self, SLOT(downloadCompleted(int)));
                download->show();
            }
            else
            {
                availableDlg = 0;
                switchBackToFullscreen(wasFull);
            }
        }
        else
        {
            Con_Message("You are running the latest available %s release.\n",
                        UpdaterSettings().channel() == UpdaterSettings::Stable? "stable" : "unstable");
        }
    }

    /**
     * Starts the installation process using the provided distribution package.
     * The engine is first shut down gracefully (game has already been autosaved).
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
        if(!execPath.fileName().endsWith(".app"))
        {
#ifdef _DEBUG
            execPath = "/Applications/Doomsday Engine.app";
#else
            Sys_MessageBox2(MBT_ERROR, "Updating", "Automatic update failed.",
                            ("Expected an application bundle, but found <b>" +
                             execPath.fileName() + "</b> instead.").toUtf8(), 0);
            return;
#endif
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
        installerCommand = new de::CommandLine;
        installerCommand->append("osascript");
        installerCommand->append(scriptPath);
        atexit(runInstallerCommand);

#elif defined(WIN32)
        // The distribution package is an installer executable, we can just run it.
        installerCommand = new de::CommandLine;
        installerCommand->append(distribPackagePath);
        atexit(runInstallerCommand);

#else
        // Open the package with the default handler.
        installerCommand = new de::CommandLine;
        installerCommand->append("xdg-open");
        installerCommand->append(distribPackagePath);
        atexit(runInstallerCommand);
#endif

        // If requested, delete the downloaded package afterwards. Currently
        // this occurs the next time when the engine is launched; on some
        // platforms it could be incorporated into the reinstall procedure.
        // (This will work better when there is no more separate frontend, as
        // the engine is restarted after the install.)
        UpdaterSettings st;
        if(st.deleteAfterUpdate())
        {
            st.setPathToDeleteAtStartup(distribPackagePath);
        }

        Sys_Quit();
    }
};

Updater::Updater(QObject *parent) : QObject(parent)
{
    d = new Instance(this);
    connect(d->network, SIGNAL(finished(QNetworkReply*)), this, SLOT(gotReply(QNetworkReply*)));

    // Do a silent auto-update check when starting.
    if(d->shouldCheckForUpdate())
    {
        d->queryLatestVersion(false);
    }
}

Updater::~Updater()
{
    delete d;
}

void Updater::setBackToFullscreen(bool yes)
{
    d->backToFullscreen = yes;
}

void Updater::gotReply(QNetworkReply* reply)
{
    d->handleReply(reply);
}

void Updater::downloadCompleted(int result)
{
    if(result == DownloadDialog::Accepted)
    {
        // Autosave the game.

        // Check the signature of the downloaded file.

        // Everything is ready to begin the installation!
        d->startInstall(d->download->downloadedFilePath());
    }

    // The download dialgo can be dismissed now.
    d->download->deleteLater();
    d->download = 0;
}

void Updater::settingsDialogClosed(int /*result*/)
{
    if(d->backToFullscreen)
    {
        d->backToFullscreen = false;
        switchBackToFullscreen(true);
    }
}

void Updater::recheck()
{
    d->doCheckRequest();
}

void Updater::showSettings()
{
    d->showSettingsNonModal();
}

void Updater::checkNow(bool notify)
{
    // Not if there is an ongoing download.
    if(d->download) return;

    d->queryLatestVersion(notify);
}

void Updater::checkNowShowingProgress()
{
    // Not if there is an ongoing download.
    if(d->download) return;

    d->availableDlg = new UpdateAvailableDialog;
    connect(d->availableDlg, SIGNAL(checkAgain()), this, SLOT(recheck()));
    d->queryLatestVersion(true);
    d->availableDlg->exec();
    delete d->availableDlg;
    d->availableDlg = 0;
}

void Updater_Init(void)
{
    updater = new Updater;
}

void Updater_Shutdown(void)
{
    delete updater;
}

Updater* Updater_Instance(void)
{
    return updater;
}

void Updater_CheckNow(boolean notify)
{
    if(novideo || !updater) return;

    updater->checkNow(notify);
}

static void showSettingsDialog(void)
{
    if(updater)
    {
        updater->showSettings();
    }
}

void Updater_ShowSettings(void)
{
    if(novideo || !updater) return;

    // Automatically switch to windowed mode for convenience.
    int delay = 0;
    if(switchToWindowedMode())
    {
        updater->setBackToFullscreen(true);

        // The mode switch takes a while and may include deferred window resizing,
        // so let's wait a while before opening the dialog to make sure everything
        // has settled.
        /// @todo Improve the mode changes so that this is not needed.
        delay = 500;
    }
    de::LegacyCore::instance().timer(delay, showSettingsDialog);
}
