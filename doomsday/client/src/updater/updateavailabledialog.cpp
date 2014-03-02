/** @file updateavailabledialog.cpp Dialog for notifying the user about available updates. 
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

#include "de_platform.h"
#include "updater/updateavailabledialog.h"
#include "updater/updatersettings.h"
#include "updater/updatersettingsdialog.h"
#include "clientapp.h"
#include "versioninfo.h"
#include <de/GuiApp>
#include <de/Log>
#include <de/ProgressWidget>
#include <de/ToggleWidget>
#include <de/SignalAction>
#include <QUrl>
#include <QDesktopServices>

using namespace de;

static TimeDelta const SHOW_ANIM_SPAN = 0.3;

DENG_GUI_PIMPL(UpdateAvailableDialog),
DENG2_OBSERVES(ToggleWidget, Toggle)
{
    ProgressWidget *checking;
    ToggleWidget *autoCheck;
    //ButtonWidget *settings;
    VersionInfo latestVersion;
    String changeLog;

    Instance(Public *d) : Base(*d)
    {
        initForChecking();
    }

    Instance(Public *d, VersionInfo const &latest) : Base(*d)
    {
        initForResult(latest);
    }

    void initForChecking(void)
    {
        init();
        showProgress(true, 0);
    }

    void initForResult(VersionInfo const &latest)
    {
        init();
        updateResult(latest, 0);
    }

    void showProgress(bool show, TimeDelta span)
    {
        checking->setOpacity(show? 1 : 0, span);
        self.area().setOpacity(show? 0 : 1, span);

        if(show)
        {
            // Set up a cancel button.
            self.buttons().clear()
                    << new DialogButtonItem(DialogWidget::Reject);
        }
    }

    void init()
    {
        checking = new ProgressWidget;
        checking->setText(tr("Checking for Updates..."));

        // The checking indicator is overlaid on the normal content.
        checking->rule().setRect(self.rule());
        self.add(checking);

        autoCheck = new ToggleWidget;
        self.area().add(autoCheck);
        autoCheck->setAlignment(ui::AlignLeft);
        autoCheck->setText(tr("Check for updates automatically"));
        autoCheck->audienceForToggle += this;

        // Include the toggle in the layout.
        self.updateLayout();
    }

    void updateResult(VersionInfo const &latest, TimeDelta showSpan)
    {
        showProgress(false, showSpan);

        latestVersion = latest;

        VersionInfo currentVersion;

        String channel     = (UpdaterSettings().channel() == UpdaterSettings::Stable? "stable" : "unstable");
        String builtInType = (String(DOOMSDAY_RELEASE_TYPE) == "Stable"? "stable" : "unstable");
        bool askUpgrade    = false;
        bool askDowngrade  = false;

        if(latestVersion > currentVersion)
        {
            askUpgrade = true;

            self.title().setText(tr("Update Available"));
            self.message().setText(tr("The latest %1 release is %2, while you are running %3.")
                                   .arg(channel)
                                   .arg(_E(b) + latestVersion.asText() + _E(.))
                                   .arg(currentVersion.asText()));
        }
        else if(channel == builtInType) // same release type
        {
            self.title().setText(tr("Up to Date"));
            self.message().setText(tr("The installed %1 is the latest available %2 build.")
                                   .arg(currentVersion.asText())
                                   .arg(_E(b) + channel + _E(.)));
        }
        else if(latestVersion < currentVersion)
        {
            askDowngrade = true;

            self.title().setText(tr("Up to Date"));
            self.message().setText(tr("The installed %1 is newer than the latest available %2 build.")
                                   .arg(currentVersion.asText())
                                   .arg(_E(b) + channel + _E(.)));
        }

        autoCheck->setInactive(UpdaterSettings().onlyCheckManually());

        self.buttons().clear();

        if(askDowngrade)
        {
            self.buttons()
                    << new DialogButtonItem(DialogWidget::Accept, tr("Downgrade to Older"))
                    << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Default, tr("Close"));
        }
        else if(askUpgrade)
        {
            self.buttons()
                    << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Upgrade"))
                    << new DialogButtonItem(DialogWidget::Reject, tr("Not Now"));
        }
        else
        {
            self.buttons()
                    << new DialogButtonItem(DialogWidget::Accept, tr("Reinstall"))
                    << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Default, tr("Close"));
        }

        self.buttons()
                << new DialogButtonItem(DialogWidget::Action | DialogWidget::Id1,
                                        style().images().image("gear"),
                                        new SignalAction(thisPublic, SLOT(editSettings())));

        if(askUpgrade)
        {
            self.buttons()
                    << new DialogButtonItem(DialogWidget::Action, tr("What's New?"),
                                            new SignalAction(thisPublic, SLOT(showWhatsNew())));
        }
    }

    void toggleStateChanged(ToggleWidget &)
    {
        bool set = autoCheck->isInactive();
        UpdaterSettings().setOnlyCheckManually(set);

        LOG_DEBUG("Never check for updates: %b") << set;
    }
};

UpdateAvailableDialog::UpdateAvailableDialog()
    : MessageDialog("updateavailable"), d(new Instance(this))
{}

UpdateAvailableDialog::UpdateAvailableDialog(VersionInfo const &latestVersion, String changeLogUri)
    : MessageDialog("updateavailable"), d(new Instance(this, latestVersion))
{
    d->changeLog = changeLogUri;
}

void UpdateAvailableDialog::showResult(VersionInfo const &latestVersion, String changeLogUri)
{
    d->changeLog = changeLogUri;
    d->updateResult(latestVersion, SHOW_ANIM_SPAN);
}

void UpdateAvailableDialog::showWhatsNew()
{
    ClientApp::app().openInBrowser(QUrl(d->changeLog));
}

void UpdateAvailableDialog::editSettings()
{
    UpdaterSettingsDialog *st = new UpdaterSettingsDialog;
    st->setAnchorAndOpeningDirection(buttonWidget(DialogWidget::Id1)->rule(), ui::Up);
    st->setDeleteAfterDismissed(true);
    if(st->exec(root()))
    {        
        // The Gear button will soon be deleted, so we'll need to detach from it.
        st->detachAnchor();

        d->autoCheck->setInactive(UpdaterSettings().onlyCheckManually());
        d->showProgress(true, SHOW_ANIM_SPAN);
        emit checkAgain();
    }
}
