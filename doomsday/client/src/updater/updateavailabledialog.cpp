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
#include "ui/widgets/progresswidget.h"
#include "ui/widgets/togglewidget.h"
#include "ui/signalaction.h"
#include "clientapp.h"
#include "versioninfo.h"
#include <de/GuiApp>
#include <de/Log>
#include <QUrl>
#include <QDesktopServices>
/*
#include <QDesktopWidget>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QPushButton>
#include <QStackedLayout>
#include <QFont>
#include <QLabel>
*/

using namespace de;

DENG2_PIMPL(UpdateAvailableDialog),
DENG2_OBSERVES(ToggleWidget, Toggle)
{
    ProgressWidget *checking;
    ToggleWidget *autoCheck;
    ButtonWidget *settings;
    //QStackedLayout* stack;
    //QWidget* checkPage;
    //QWidget* resultPage;
    //bool emptyResultPage;
    //QVBoxLayout* resultLayout;
    //QLabel* checking;
    //QLabel* info;
    //QCheckBox* neverCheck;
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

        checking->setOpacity(1);
        self.area().setOpacity(0);

        //updateResult(VersionInfo());

        //stack->addWidget(checkPage);
        //stack->addWidget(resultPage);
    }

    void initForResult(VersionInfo const &latest)
    {
        init();
        updateResult(latest);

        //stack->addWidget(resultPage);
        //stack->addWidget(checkPage);
    }

    void init()
    {
        ScrollAreaWidget &area = self.area();

        checking = new ProgressWidget;
        checking->setText(tr("Checking for Updates..."));
        checking->setAlignment(ui::AlignCenter, LabelWidget::AlignByCombination);
        checking->setOpacity(0);

        // The checking indicator is overlaid on the normal content.
        checking->rule().setRect(area.rule());
        self.add(checking);

        /*
#ifndef MACOSX
        self.setWindowTitle(tr(DOOMSDAY_NICENAME" %1").arg(VersionInfo().asText()));
#endif

        emptyResultPage = true;
        stack = new QStackedLayout(&self);
        checkPage = new QWidget(&self);
        resultPage = new QWidget(&self);

#ifdef MACOSX
        // Adjust spacing around all stacked widgets.
        self.setContentsMargins(9, 9, 9, 9);
#endif

        // Create the Check page.
        QVBoxLayout* checkLayout = new QVBoxLayout;
        checkPage->setLayout(checkLayout);

        checking = new QLabel(tr("Checking for available updates..."));
        checkLayout->addWidget(checking, 1, Qt::AlignCenter);

        QPushButton* stop = new QPushButton(tr("Cancel"));
        QObject::connect(stop, SIGNAL(clicked()), &self, SLOT(reject()));
        checkLayout->addWidget(stop, 0, Qt::AlignHCenter);
        stop->setAutoDefault(false);
        stop->setDefault(false);

        self.setLayout(stack);*/
    }

    void updateResult(VersionInfo const &latest)
    {
        latestVersion = latest;

        /*
        // Get rid of the existing page.
        if(!emptyResultPage)
        {
            stack->removeWidget(resultPage);
            delete resultPage;
            resultPage = new QWidget(&self);
            stack->addWidget(resultPage);
        }
        emptyResultPage = false;

        resultLayout = new QVBoxLayout;
        resultPage->setLayout(resultLayout);

        info = new QLabel;
        info->setTextFormat(Qt::RichText);
*/
        VersionInfo currentVersion;

        String channel     = (UpdaterSettings().channel() == UpdaterSettings::Stable? "stable" : "unstable");
        String builtInType = (String(DOOMSDAY_RELEASE_TYPE) == "Stable"? "stable" : "unstable");
        bool askUpgrade        = false;
        bool askDowngrade      = false;

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

        autoCheck = new ToggleWidget;
        autoCheck->setAlignment(ui::AlignLeft);
        autoCheck->setText(tr("Check for updates automatically"));
        autoCheck->setInactive(UpdaterSettings().onlyCheckManually());
        autoCheck->audienceForToggle += this;
        self.area().add(autoCheck);
        /*
        neverCheck = new QCheckBox(tr("N&ever check for updates automatically"));
        neverCheck->setChecked(UpdaterSettings().onlyCheckManually());
        QObject::connect(neverCheck, SIGNAL(toggled(bool)), &self, SLOT(neverCheckToggled(bool)));

        QDialogButtonBox* bbox = new QDialogButtonBox;
*/
        if(askDowngrade)
        {
            self.buttons().items()
                    << new DialogButtonItem(DialogWidget::Accept, tr("Downgrade to Older"))
                    << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Default, tr("Close"));
            /*
            QPushButton* yes = bbox->addButton(tr("Downgrade to &Older"), QDialogButtonBox::YesRole);
            QPushButton* no = bbox->addButton(tr("&Close"), QDialogButtonBox::RejectRole);
            QObject::connect(yes, SIGNAL(clicked()), &self, SLOT(accept()));
            QObject::connect(no, SIGNAL(clicked()), &self, SLOT(reject()));
            no->setDefault(true);
            */
        }
        else if(askUpgrade)
        {
            self.buttons().items()
                    << new DialogButtonItem(DialogWidget::Accept | DialogWidget::Default, tr("Upgrade"))
                    << new DialogButtonItem(DialogWidget::Reject, tr("Not Now"));
            /*
            QPushButton* yes = bbox->addButton(tr("&Upgrade"), QDialogButtonBox::YesRole);
            QPushButton* no = bbox->addButton(tr("&Not Now"), QDialogButtonBox::NoRole);
            QObject::connect(yes, SIGNAL(clicked()), &self, SLOT(accept()));
            QObject::connect(no, SIGNAL(clicked()), &self, SLOT(reject()));
            yes->setDefault(true);*/
        }
        else
        {
            self.buttons().items()
                    << new DialogButtonItem(DialogWidget::Accept, tr("Reinstall"))
                    << new DialogButtonItem(DialogWidget::Reject | DialogWidget::Default, tr("Close"));
            /*
            QPushButton* reinst = bbox->addButton(tr("&Reinstall"), QDialogButtonBox::YesRole);
            QPushButton* ok = bbox->addButton(tr("&Close"), QDialogButtonBox::RejectRole);
            QObject::connect(reinst, SIGNAL(clicked()), &self, SLOT(accept()));
            QObject::connect(ok, SIGNAL(clicked()), &self, SLOT(reject()));
            reinst->setAutoDefault(false);
            ok->setDefault(true);*/
        }

        /// @todo The dialog buttons should support a opposite-aligned button.
        settings = new ButtonWidget;
        settings->setSizePolicy(ui::Filled, ui::Filled);
        settings->setImage(self.style().images().image("gear"));
        settings->rule()
                .setInput(Rule::Left,   self.area().contentRule().left())
                .setInput(Rule::Bottom, self.buttons().contentRule().bottom())
                .setInput(Rule::Height, self.buttons().contentRule().height())
                .setInput(Rule::Width,  settings->rule().height());
        self.add(settings);
        settings->setAction(new SignalAction(thisPublic, SLOT(editSettings())));

        /*
        QPushButton* cfg = bbox->addButton(tr("&Settings..."), QDialogButtonBox::ActionRole);
        QObject::connect(cfg, SIGNAL(clicked()), &self, SLOT(editSettings()));
        cfg->setAutoDefault(false);*/

        if(askUpgrade)
        {
            /*
            QPushButton* whatsNew = bbox->addButton(tr("&What's New?"), QDialogButtonBox::HelpRole);
            QObject::connect(whatsNew, SIGNAL(clicked()), &self, SLOT(showWhatsNew()));
            whatsNew->setAutoDefault(false);
            */
        }

        //resultLayout->addWidget(info);
        //resultLayout->addWidget(neverCheck);
        //resultLayout->addWidget(bbox);

        // Include the toggle in the layout.
        self.updateLayout();
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

    //connect(DENG2_GUI_APP, SIGNAL(displayModeChanged()), this, SLOT(recenterDialog()));
}

void UpdateAvailableDialog::showResult(VersionInfo const &latestVersion, String changeLogUri)
{
    d->changeLog = changeLogUri;
    d->updateResult(latestVersion);
}

void UpdateAvailableDialog::showWhatsNew()
{
    ClientApp::app().openInBrowser(QUrl(d->changeLog));
}

void UpdateAvailableDialog::editSettings()
{
    UpdaterSettingsDialog *st = new UpdaterSettingsDialog;
    st->setAnchorAndOpeningDirection(d->settings->rule(), ui::Up);
    st->setDeleteAfterDismissed(true);
    if(st->exec(root()))
    {
        d->autoCheck->setInactive(UpdaterSettings().onlyCheckManually());
        emit checkAgain();
    }
    /*
    UpdaterSettingsDialog st(this);
    if(st.exec())
    {
        d->neverCheck->setChecked(UpdaterSettings().onlyCheckManually());

        d->stack->setCurrentWidget(d->checkPage);

        // Rerun the check.
        emit checkAgain();
    }*/
}
