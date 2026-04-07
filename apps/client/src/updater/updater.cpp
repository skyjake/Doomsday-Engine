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
 * @authors Copyright © 2012-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "network/net_main.h"
#include "clientapp.h"
#include "ui/nativeui.h"
#include "ui/clientwindow.h"
#include "ui/widgets/taskbarwidget.h"
#include "updater.h"
#include "updater/processcheckdialog.h"
#include "updater/updateavailabledialog.h"
#include "updater/updatedownloaddialog.h"
#include "updater/updatersettings.h"
#include "updater/updatersettingsdialog.h"

#include <de/app.h>
#include <de/commandline.h>
#include <de/date.h>
#include <de/logbuffer.h>
#include <de/notificationareawidget.h>
#include <de/time.h>
#include <de/webrequest.h>
#include <de/json.h>
#include <doomsday/console/exec.h>

using namespace de;

#ifdef MACOSX
#  define INSTALL_SCRIPT_NAME "deng-upgrade.scpt"
#endif

#define PLATFORM_ID     DE_PLATFORM_ID

static CommandLine* installerCommand;

/**
 * Callback for atexit(). Create the installerCommand before calling this.
 */
static void runInstallerCommand(void)
{
    DE_ASSERT(installerCommand != nullptr);

    installerCommand->execute();
    delete installerCommand;
    installerCommand = nullptr;
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
        setSizePolicy(ui::Expand, ui::Expand);

        _icon = new LabelWidget;
        _icon->setImage(style().images().image("updater"));
        _icon->setOverrideImageSize(overrideImageSize());
        _icon->rule().setRect(rule());
        add(_icon);
        hideIcon();

        // The notification has a hidden button that can be clicked.
        _clickable = new PopupButtonWidget;
        _clickable->setOpacity(0); // not drawn
        _clickable->rule().setRect(rule());
        _clickable->setOpener([] (PopupWidget *) {
            ClientApp::updater().showCurrentDownload();
        });
        add(_clickable);
    }

    virtual ~UpdaterStatusWidget() = default;

    void showIcon(const DotPath &path)
    {
        _icon->setImageColor(style().colors().colorf(path));
    }

    void hideIcon()
    {
        _icon->setImageColor(Vec4f());
    }

    PopupButtonWidget &popupButton()
    {
        return *_clickable;
    }

private:
    LabelWidget *_icon;
    PopupButtonWidget *_clickable;
};

DE_PIMPL(Updater)
, DE_OBSERVES(App, StartupComplete)
, DE_OBSERVES(DialogWidget, Accept)
, DE_OBSERVES(UpdateDownloadDialog, Failure)
, DE_OBSERVES(UpdateDownloadDialog, Progress)
, DE_OBSERVES(WebRequest, Finished)
{
    WebRequest web;
    UpdateDownloadDialog *download = nullptr; // not owned (in the widget tree, if exists)
    UniqueWidgetPtr<UpdaterStatusWidget> status;
    UpdateAvailableDialog *availableDlg = nullptr; ///< If currently open (not owned).
    bool alwaysShowNotification;
    bool savingSuggested = false;

    Version latestVersion;
    String latestPackageUri;
    String latestPackageUri2; // fallback location
    String latestLogUri;

    Impl(Public *i) : Base(i)
    {
        web.setUserAgent(Version::currentBuild().userAgent());
        web.audienceForFinished() += this;

        // Delete a package installed earlier?
        UpdaterSettings st;
        if (st.deleteAfterUpdate())
        {
            de::String p = st.pathToDeleteAtStartup();
            if (!p.isEmpty())
            {
                NativePath file(p);
                if (file.exists())
                {
                    LOG_NOTE("Deleting previously installed package: %s") << p;
                    file.remove();
                }
            }
        }
        st.setPathToDeleteAtStartup("");
    }

    void webRequestFinished(WebRequest &) override
    {
        LOG_AS("Updater")
        try
        {
            handleReply();
        }
        catch (const Error &er)
        {
            LOG_WARNING("Error when reading update check reply: %s") << er.asText();
        }
    }

    void setupUI()
    {
        status.reset(new UpdaterStatusWidget);
    }

    String composeCheckUri()
    {
        UpdaterSettings st;
        String uri = Stringf("%sbuilds?latest_for=%s&type=%s",
                App::apiUrl().c_str(),
                DE_PLATFORM_ID,
                st.channel() == UpdaterSettings::Stable   ? "stable" :
                st.channel() == UpdaterSettings::Unstable ? "unstable"
                                                          : "candidate");
        LOG_XVERBOSE("URI: ", uri);
        return uri;
    }

    bool shouldCheckForUpdate() const
    {
        UpdaterSettings st;
        if (st.onlyCheckManually()) return false;

        float dayInterval = 30;
        switch (st.frequency())
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
        if (st.lastCheckTime().asDate().daysTo(de::Date()) >= dayInterval)
            return true;

        if (st.frequency() == UpdaterSettings::Biweekly)
        {
            int weekday = Date(now).dayOfWeek();
            if (weekday == 2 || weekday == 6) return true;
        }

        // No need to check right now.
        return false;
    }

    void appStartupCompleted() override
    {
        LOG_AS("Updater")
        LOG_DEBUG("App startup was completed");

        if (shouldCheckForUpdate())
        {
            queryLatestVersion(false);
        }
    }

    void showNotification(bool show)
    {
        ClientWindow::main().notifications().showOrHide(*status, show);
    }

    void showCheckingNotification()
    {
        status->setRange(Rangei(0, 1));
        status->setProgress(0, 0.0);
        status->showIcon("text");
        showNotification(true);
    }

    void showUpdateAvailableNotification()
    {
        showCheckingNotification();
        status->showIcon("accent");
    }

    void showDownloadNotification()
    {
        status->setMode(ProgressWidget::Indefinite);
        status->hideIcon();
        showNotification(true);
    }

    void queryLatestVersion(bool notifyAlways)
    {
        if (!web.isPending())
        {
            showCheckingNotification();

            UpdaterSettings().setLastCheckTime(de::Time());
            alwaysShowNotification = notifyAlways;
            web.get(composeCheckUri());
        }
    }

    void handleReply()
    {
        DE_ASSERT_IN_MAIN_THREAD();
        DE_ASSERT(web.isFinished());

        showNotification(false);

        if (web.isFailed())
        {
            LOG_WARNING("Network request failed: %s") << web.errorMessage();
            return;
        }

        const Record result = de::parseJSON(String::fromUtf8(web.result()));
        if (!result.has("direct_download_uri"))
        {
            return;
        }
        latestPackageUri = result["direct_download_uri"];
        latestLogUri     = result["release_changeloguri"];

        // Check if a fallback location is specified for the download.
        if (result.has("direct_download_fallback_uri"))
        {
            latestPackageUri2 = result["direct_download_fallback_uri"];
        }
        else
        {
            latestPackageUri2 = "";
        }

        latestVersion = Version(result["version"], result.geti("build_uniqueid"));

        const Version currentVersion = Version::currentBuild();

        LOG_MSG(_E(b) "Received version information:\n" _E(.)
                " - installed version: " _E(>) "%s ") << currentVersion.asHumanReadableText();
        LOG_MSG(" - latest version: " _E(>) "%s") << latestVersion.asHumanReadableText();
        LOG_MSG(" - package: " _E(>) _E(i) "%s") << latestPackageUri;
        LOG_MSG(" - change log: " _E(>) _E(i) "%s") << latestLogUri;

        if (availableDlg)
        {
            // This was a recheck.
            availableDlg->showResult(latestVersion, latestLogUri);
            return;
        }

        const bool gotUpdate = (latestVersion > currentVersion);

        // Is this newer than what we're running?
        if (gotUpdate)
        {
            LOG_NOTE("Found an update: " _E(b)) << latestVersion.asHumanReadableText();

            if (!alwaysShowNotification)
            {
                if (UpdaterSettings().autoDownload())
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

        if (alwaysShowNotification)
        {
            showAvailableDialogAndPause();
        }
    }

    void showAvailableDialogAndPause()
    {
        if (availableDlg) return; // Just one at a time.

        // Modal dialogs will interrupt gameplay.
        ClientWindow::main().taskBar().openAndPauseGame();

        availableDlg = new UpdateAvailableDialog(latestVersion, latestLogUri);
        execAvailableDialog();
    }

    void execAvailableDialog()
    {
        DE_ASSERT(availableDlg != nullptr);

        availableDlg->setDeleteAfterDismissed(true);
        availableDlg->audienceForRecheck() += [this]() { self().recheck(); };

        if (availableDlg->exec(ClientWindow::main().root()))
        {
            startDownload();
            download->open();
        }
        availableDlg = nullptr;
    }

    void startDownload()
    {
        DE_ASSERT(!download);

        // The notification provides access to the download dialog.
        showDownloadNotification();

        LOG_MSG("Download and install update");

        download = new UpdateDownloadDialog(latestPackageUri, latestPackageUri2);
        download->audienceForClose() += [this]() {
            if (!download || download->isFailed())
            {
                if (download)
                {
                    download->setDeleteAfterDismissed(true);
                    download = nullptr;
                }
                showNotification(false);
            }
        };
        download->audienceForAccept() += this;
        download->audienceForFailure() += this;
        download->audienceForProgress() += this;
        status->popupButton().setPopup(*download, ui::Down);

        ClientWindow::main().root().addOnTop(download);
    }

    void downloadProgress(int progress) override
    {
        DE_ASSERT(status);
        status->setRange(Rangei(0, 100));
        status->setProgress(progress);
    }

    void downloadFailed(const String &message) override
    {
        LOG_NOTE("Update cancelled: ") << message;
    }

    void dialogAccepted(DialogWidget &, int) override
    {
        // Autosave the game.
        // Well, we can't do that yet so just remind the user about saving.
        if (App_GameLoaded() && !savingSuggested && gx.GetInteger(DD_GAME_RECOMMENDS_SAVING))
        {
            savingSuggested = true;

            MessageDialog *msg = new MessageDialog;
            msg->setDeleteAfterDismissed(true);
            msg->title().setText("Save Game?");
            msg->message().setText(_E(b) "Installing the update will discard unsaved progress in the game.\n\n"
                                   _E(.) "Doomsday will be shut down before the installation can start. "
                                   "The game is not saved automatically, so you will have to "
                                   "save the game before installing the update.");
            msg->buttons()
                    << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, "I'll Save First")
                    << new DialogButtonItem(DialogWidget::Reject, "Discard Progress & Install");

            if (msg->exec(ClientWindow::main().root()))
            {
                Con_Execute(CMDS_DDAY, "savegame", false, false);
                return;
            }
        }

        /// @todo Check the signature of the downloaded file.

        // Everything is ready to begin the installation!
        startInstall(download->downloadedFilePath());

        // The download dialog can be dismissed now.
        download->guiDeleteLater();
        download = nullptr;
        savingSuggested = false;
    }

    /**
     * Starts the installation process using the provided distribution package.
     * The engine is first shut down gracefully (game has already been autosaved).
     *
     * @param distribPackagePath  File path of the distribution package.
     */
    void startInstall(const String &distribPackagePath)
    {
#ifdef MACOSX
        String volName = "Doomsday Engine " + latestVersion.compactNumber();

//#ifdef DE_QT_5_0_OR_NEWER
//        String scriptPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
//#else
//        String scriptPath = QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
//#endif
#if 0
        String scriptPath = App::cachePath();
        QDir::current().mkpath(scriptPath); // may not exist
        scriptPath = QDir(scriptPath).filePath(INSTALL_SCRIPT_NAME);
        QFile file(scriptPath);
        if (file.open(QFile::WriteOnly | QFile::Truncate))
        {
            QTextStream out(&file);
            out << "tell application \"System Events\" to set visible of process \"Finder\" to true\n"
                   "tell application \"Finder\"\n"
                   "  open POSIX file \"" << distribPackagePath << "\"\n"
                   "  -- Wait for it to get mounted\n"
                   "  repeat until name of every disk contains \"" << volName << "\"\n"
                   "    delay 1\n"
                   "  end repeat\n"
                   /*"  -- Start the installer\n"
                   "  open file \"" << volName << ":Doomsday.pkg\"\n"
                   "  -- Activate the Installer\n"
                   "  repeat until name of every process contains \"Installer\"\n"
                   "    delay 2\n"
                   "  end repeat\n"*/
                   "end tell\n"
                   /*"delay 1\n"
                   "tell application \"Installer\" to activate\n"
                   "tell application \"Finder\"\n"
                   "  -- Wait for it to finish\n"
                   "  repeat until name of every process does not contain \"Installer\"\n"
                   "    delay 1\n"
                   "  end repeat\n"
                   "  -- Unmount\n"
                   "  eject disk \"" << volName << "\"\n"
                   "end tell\n"*/;
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
#endif

#elif defined(WIN32)
        /**
         * @todo It would be slightly neater to check all these processes at
         * the same time.
         */
        Updater_AskToStopProcess("doomsday-shell.exe", "Please quit all Doomsday Shell instances "
                                 "before starting the update. Windows cannot update "
                                 "files that are currently in use.");

        Updater_AskToStopProcess("doomsday-server.exe", "Please stop all Doomsday servers "
                                 "before starting the update. Windows cannot update "
                                 "files that are currently in use.");

        // The distribution package is in .msi format.
        installerCommand = new de::CommandLine;
        installerCommand->append("msiexec");
        installerCommand->append("/i");
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
        if (st.deleteAfterUpdate())
        {
            st.setPathToDeleteAtStartup(distribPackagePath);
        }

        Sys_Quit();
    }
};

Updater::Updater() : d(new Impl(this))
{
    d->web.audienceForFinished() += d;

    // Do a silent auto-update check when starting.
    App::app().audienceForStartupComplete() += d;
}

void Updater::setupUI()
{
    d->setupUI();
}

ProgressWidget &Updater::progress()
{
    return *d->status;
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
    if (d->download)
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
    if (d->download)
    {
        d->download->open();
        return;
    }

    d->queryLatestVersion(mode == AlwaysShowResult);
}

void Updater::checkNowShowingProgress()
{
    // Not if there is an ongoing download.
    if (d->download) return;

    ClientWindow::main().glActivate();

    d->availableDlg = new UpdateAvailableDialog;
    d->queryLatestVersion(true);
    d->execAvailableDialog();
}

void Updater::printLastUpdated(void)
{
    String ago = UpdaterSettings().lastCheckAgo();
    if (ago.isEmpty())
    {
        LOG_MSG("Never checked for updates");
    }
    else
    {
        LOG_MSG("Latest update check was made %s") << ago;
    }
}
