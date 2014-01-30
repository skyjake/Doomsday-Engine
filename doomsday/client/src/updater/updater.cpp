/** @file updater.cpp Automatic updater that works with dengine.net.
 * @ingroup updater
 *
 * When one of the updater dialogs is shown, the main window is automatically
 * switched to windowed mode. This is because the dialogs would be hidden
 * behind the main window or incorrectly located when the main window is in
 * fullscreen mode. It is also possible that the screen resolution is too low
 * to fit the shown dialogs. In the long term, the native dialogs should be
 * replaced with the engine's own (scriptable) UI widgets (once they are
 * available).
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

#include <QDateTime>
#include <QStringList>
#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QTextStream>
#include <QDir>

#include "de_platform.h"

#ifdef WIN32
#  undef open
#endif

#include <stdlib.h>
#include "sys_system.h"
#include "dd_version.h"
#include "dd_def.h"
#include "dd_types.h"
#include "dd_main.h"
#include "con_main.h"
#include "clientapp.h"
#include "ui/nativeui.h"
#include "ui/windowsystem.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "updater.h"
#include "updater/downloaddialog.h"
#include "updater/processcheckdialog.h"
#include "updater/updateavailabledialog.h"
#include "updater/updatersettings.h"
#include "updater/updatersettingsdialog.h"
#include "versioninfo.h"

#include <de/App>
#include <de/Time>
#include <de/Date>
#include <de/Log>
#include <de/SignalAction>
#include <de/NotificationWidget>
#include <de/data/json.h>

using namespace de;

#ifdef MACOSX
#  define INSTALL_SCRIPT_NAME "deng-upgrade.scpt"
#endif

/// @todo The platform ID should come from the Builder.
#if defined(WIN32)
#  define PLATFORM_ID       "win-x86"

#elif defined(MACOSX)
#  if defined(MACOS_10_7) || defined(MACOSX_NATIVESDK)
#    define PLATFORM_ID     "mac10_8-x86_64"
#  elif defined(__64BIT__)
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

static CommandLine* installerCommand;

/**
 * Callback for atexit(). Create the installerCommand before calling this.
 */
static void runInstallerCommand(void)
{
    DENG_ASSERT(installerCommand != 0);

    installerCommand->execute();
    delete installerCommand;
    installerCommand = 0;
}

/**
 * Notification widget about the status of the Updater.
 */
class UpdaterStatusWidget : public ProgressWidget
{
public:
    UpdaterStatusWidget()
    {
        useMiniStyle();
        setColor("text");
        setShadowColor(""); // no shadow, please
        setRotationSpeed(0);
        setSizePolicy(ui::Expand, ui::Expand);

        // The notification has a hidden button that can be clicked.
        _clickable = new ButtonWidget;
        _clickable->setOpacity(0); // not drawn
        _clickable->rule().setRect(rule());
        _clickable->setAction(new SignalAction(&ClientApp::updater(),
                                               SLOT(showCurrentDownload())));
        add(_clickable);
    }

private:
    ButtonWidget *_clickable;
};

DENG2_PIMPL(Updater),
DENG2_OBSERVES(App, StartupComplete)
{
    QNetworkAccessManager *network;
    DownloadDialog *download;
    UpdaterStatusWidget *status;
    bool alwaysShowNotification;
    UpdateAvailableDialog *availableDlg; ///< If currently open (not owned).
    bool savingSuggested;

    VersionInfo latestVersion;
    QString latestPackageUri;
    QString latestPackageUri2; // fallback location
    QString latestLogUri;

    Instance(Public *up)
        : Base(up),
          network(0),
          download(0),
          status(0),
          availableDlg(0),
          savingSuggested(false)
    {
        network = new QNetworkAccessManager(thisPublic);

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
                    LOG_NOTE("Deleting previously installed package: %s") << p;
                    file.remove();
                }
            }
        }
        st.setPathToDeleteAtStartup("");
    }

    ~Instance()
    {
        //if(settingsDlg) delete settingsDlg;

        // Delete the ongoing download.
        if(download) delete download;
    }

    void setupUI()
    {
        status = new UpdaterStatusWidget;        
    }

    QString composeCheckUri()
    {
        UpdaterSettings st;
        QString uri = QString(DOOMSDAY_HOMEURL) + "/latestbuild?";
        uri += QString("platform=") + PLATFORM_ID;
        uri += (st.channel() == UpdaterSettings::Stable? "&stable" : "&unstable");
        uri += "&graph";

        LOG_XVERBOSE("Check URI: ") << uri;
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

    void appStartupCompleted()
    {
        LOG_AS("Updater")
        LOG_DEBUG("App startup was completed");

        if(shouldCheckForUpdate())
        {
            queryLatestVersion(false);
        }
    }

    void showNotification(bool show)
    {
        ClientWindow::main().notifications().showOrHide(status, show);
    }

    void showCheckingNotification()
    {
        status->setRange(Rangei(0, 1));
        status->setProgress(0, 0);
        status->setImage(ClientApp::windowSystem().style().images().image("updater"));
        status->setImageColor(ClientApp::windowSystem().style().colors().colorf("text"));
        showNotification(true);
    }

    void showUpdateAvailableNotification()
    {
        showCheckingNotification();
        status->setImageColor(ClientApp::windowSystem().style().colors().colorf("accent"));
    }

    void showDownloadNotification()
    {
        status->setMode(ProgressWidget::Indefinite);
        status->setImage(Image());
        showNotification(true);
    }

    void queryLatestVersion(bool notifyAlways)
    {
        showCheckingNotification();

        UpdaterSettings().setLastCheckTime(de::Time());
        alwaysShowNotification = notifyAlways;
        network->get(QNetworkRequest(composeCheckUri()));
    }

    void handleReply(QNetworkReply *reply)
    {
        reply->deleteLater(); // make sure it gets deleted

        DENG2_ASSERT_IN_MAIN_THREAD();
        showNotification(false);

        if(reply->error() != QNetworkReply::NoError)
        {
            LOG_WARNING("Network request failed: %s") << reply->url().toString();
            return;
        }

        QVariant result = de::parseJSON(QString::fromUtf8(reply->readAll()));
        if(!result.isValid()) return;

        QVariantMap map  = result.toMap();
        latestPackageUri = map["direct_download_uri"].toString();
        latestLogUri     = map["release_changeloguri"].toString();

        // Check if a fallback location is specified for the download.
        if(map.contains("direct_download_fallback_uri"))
        {
            latestPackageUri2 = map["direct_download_fallback_uri"].toString();
        }
        else
        {
            latestPackageUri2 = "";
        }

        latestVersion = VersionInfo(map["version"].toString(), map["build_uniqueid"].toInt());

        VersionInfo currentVersion;

        LOG_MSG(_E(b) "Received version information:\n" _E(.)
                " - installed version: " _E(>) "%s ") << currentVersion.asText();
        LOG_MSG(" - latest version: " _E(>) "%s") << latestVersion.asText();
        LOG_MSG(" - package: " _E(>) _E(i) "%s") << latestPackageUri;
        LOG_MSG(" - change log: " _E(>) _E(i) "%s") << latestLogUri;

        if(availableDlg)
        {
            // This was a recheck.
            availableDlg->showResult(latestVersion, latestLogUri);
            return;
        }

        bool const gotUpdate = latestVersion > currentVersion;

        // Is this newer than what we're running?
        if(gotUpdate)
        {
            LOG_NOTE("Found an update: " _E(b)) << latestVersion.asText();

            if(!alwaysShowNotification)
            {
                if(UpdaterSettings().autoDownload())
                {
                    startDownload();
                    return;
                }

                // Show the notification so the user knows an update is
                // available.
                showUpdateAvailableNotification();
            }
        }
        else
        {
            LOG_NOTE("You are running the latest available " _E(b) "%s" _E(.) " release")
                    << (UpdaterSettings().channel() == UpdaterSettings::Stable? "stable" : "unstable");
        }

        if(alwaysShowNotification)
        {
            showAvailableDialogAndPause();
        }
    }

    void showAvailableDialogAndPause()
    {
        if(availableDlg) return; // Just one at a time.

        // Modal dialogs will interrupt gameplay.
        ClientWindow::main().taskBar().openAndPauseGame();

        availableDlg = new UpdateAvailableDialog(latestVersion, latestLogUri);
        execAvailableDialog();
    }

    void execAvailableDialog()
    {
        DENG2_ASSERT(availableDlg != 0);

        availableDlg->setDeleteAfterDismissed(true);
        QObject::connect(availableDlg, SIGNAL(checkAgain()), thisPublic, SLOT(recheck()));

        if(availableDlg->exec(ClientWindow::main().root()))
        {
            startDownload();
            download->open();
        }
        availableDlg = 0;
    }

    void startDownload()
    {
        DENG2_ASSERT(download == 0);

        // The notification provides access to the download dialog.
        showDownloadNotification();

        LOG_MSG("Download and install update");

        download = new DownloadDialog(latestPackageUri, latestPackageUri2);
        download->setAnchorAndOpeningDirection(status->rule(), ui::Down);
        QObject::connect(download, SIGNAL(closed()), thisPublic, SLOT(downloadDialogClosed()));
        QObject::connect(download, SIGNAL(downloadProgress(int)),thisPublic, SLOT(downloadProgressed(int)));
        QObject::connect(download, SIGNAL(downloadFailed(QString)), thisPublic, SLOT(downloadFailed(QString)));
        QObject::connect(download, SIGNAL(accepted(int)), thisPublic, SLOT(downloadCompleted(int)));

        ClientWindow::main().root().addOnTop(download);
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
        de::String volName = "Doomsday Engine " + latestVersion.base();

#ifdef DENG2_QT_5_0_OR_NEWER
        QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#else
        QString scriptPath = QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
#endif
        QDir::current().mkpath(scriptPath); // may not exist
        scriptPath = QDir(scriptPath).filePath(INSTALL_SCRIPT_NAME);
        QFile file(scriptPath);
        if(file.open(QFile::WriteOnly | QFile::Truncate))
        {
            QTextStream out(&file);
            out << "tell application \"System Events\" to set visible of process \"Finder\" to false\n"
                   "tell application \"Finder\"\n"
                   "  open POSIX file \"" << distribPackagePath << "\"\n"
                   "  -- Wait for it to get mounted\n"
                   "  repeat until name of every disk contains \"" << volName << "\"\n"
                   "    delay 1\n"
                   "  end repeat\n"
                   "  -- Start the installer\n"
                   "  open file \"" << volName << ":Doomsday.pkg\"\n"
                   "  -- Activate the Installer\n"
                   "  repeat until name of every process contains \"Installer\"\n"
                   "    delay 2\n"
                   "  end repeat\n"
                   "end tell\n"
                   "delay 1\n"
                   "tell application \"Installer\" to activate\n"
                   "tell application \"Finder\"\n"
                   "  -- Wait for it to finish\n"
                   "  repeat until name of every process does not contain \"Installer\"\n"
                   "    delay 1\n"
                   "  end repeat\n"
                   "  -- Unmount\n"
                   "  eject disk \"" << volName << "\"\n"
                   "end tell\n";
            file.close();
        }
        else
        {
            qWarning() << "Could not write" << scriptPath;
        }

        // Register a shutdown action to execute the script and quit.
        installerCommand = new de::CommandLine;
        installerCommand->append("osascript");
        installerCommand->append(scriptPath);
        atexit(runInstallerCommand);

#elif defined(WIN32)
        /**
         * @todo It would be slightly neater to check all these processes at
         * the same time.
         */
        Updater_AskToStopProcess("snowberry.exe", "Please quit the Doomsday Engine Frontend "
                                 "before starting the update. Windows cannot update "
                                 "files that are currently in use.");

        Updater_AskToStopProcess("doomsday-shell.exe", "Please quit all Doomsday Shell instances "
                                 "before starting the update. Windows cannot update "
                                 "files that are currently in use.");

        Updater_AskToStopProcess("doomsday-server.exe", "Please stop all Doomsday servers "
                                 "before starting the update. Windows cannot update "
                                 "files that are currently in use.");

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

Updater::Updater() : d(new Instance(this))
{
    connect(d->network, SIGNAL(finished(QNetworkReply *)), this, SLOT(gotReply(QNetworkReply *)));

    // Do a silent auto-update check when starting.
    App::app().audienceForStartupComplete += d;
}

void Updater::setupUI()
{
    d->setupUI();
}

ProgressWidget &Updater::progress()
{
    return *d->status;
}

void Updater::gotReply(QNetworkReply *reply)
{
    d->handleReply(reply);
}

void Updater::downloadProgressed(int percentage)
{
    d->status->setRange(Rangei(0, 100));
    d->status->setProgress(percentage);
}

void Updater::downloadCompleted(int)
{
    // Autosave the game.
    // Well, we can't do that yet so just remind the user about saving.
    if(App_GameLoaded() && !d->savingSuggested && gx.GetInteger(DD_GAME_RECOMMENDS_SAVING))
    {
        d->savingSuggested = true;

        MessageDialog *msg = new MessageDialog;
        msg->setDeleteAfterDismissed(true);
        msg->title().setText(tr("Save Game?"));
        msg->message().setText(tr(_E(b) "Installing the update will discard unsaved progress in the game.\n\n"
                                  _E(.) "Doomsday will be shut down before the installation can start. "
                                  "The game is not saved automatically, so you will have to "
                                  "save the game before installing the update."));
        msg->buttons()
                << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("I'll Save First"))
                << new DialogButtonItem(DialogWidget::Reject, tr("Discard Progress & Install"));

        if(msg->exec(ClientWindow::main().root()))
        {
            Con_Execute(CMDS_DDAY, "savegame", false, false);
            return;
        }
    }

    /// @todo Check the signature of the downloaded file.

    // Everything is ready to begin the installation!
    d->startInstall(d->download->downloadedFilePath());

    // The download dialog can be dismissed now.
    d->download->guiDeleteLater();
    d->download = 0;
    d->savingSuggested = false;
}

void Updater::downloadFailed(QString message)
{
    LOG_NOTE("Update cancelled: ") << message;
}

void Updater::recheck()
{
    d->queryLatestVersion(d->alwaysShowNotification);
}

void Updater::showSettings()
{
    //d->showSettingsNonModal();

    ClientWindow::main().taskBar().showUpdaterSettings();
}

void Updater::showCurrentDownload()
{
    if(d->download)
    {
        d->download->open();
    }
    else
    {
        d->showNotification(false);
        d->showAvailableDialogAndPause();
    }
}

void Updater::checkNow(CheckMode mode)
{
    // Not if there is an ongoing download.
    if(d->download)
    {
        d->download->open();
        return;
    }

    d->queryLatestVersion(mode == AlwaysShowResult);
}

void Updater::checkNowShowingProgress()
{
    // Not if there is an ongoing download.
    if(d->download) return;

    d->availableDlg = new UpdateAvailableDialog;
    d->queryLatestVersion(true);
    d->execAvailableDialog();
}

void Updater::printLastUpdated(void)
{
    String ago = UpdaterSettings().lastCheckAgo();
    if(ago.isEmpty())
    {
        LOG_MSG("Never checked for updates");
    }
    else
    {
        LOG_MSG("Latest update check was made %s") << ago;
    }
}

void Updater::downloadDialogClosed()
{
    if(!d->download || d->download->isFailed())
    {
        if(d->download)
        {
            d->download->setDeleteAfterDismissed(true);
            d->download = 0;
        }
        d->showNotification(false);
    }
}
