/** @file updatersettingsdialog.cpp Dialog for configuring automatic updates.
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

#include "updater/updatersettingsdialog.h"
#include "updater/updatersettings.h"
#include "clientapp.h"
#include <de/LabelWidget>
#include <de/ChoiceWidget>
#include <de/VariableToggleWidget>
#include <de/GridLayout>
#include <de/SignalAction>
#include <de/Log>
#include <QDesktopServices>

using namespace de;

static QString defaultLocationName()
{
#ifdef DENG2_QT_5_0_OR_NEWER
    QString name = QStandardPaths::displayName(QStandardPaths::CacheLocation);
#else
    QString name = QDesktopServices::displayName(QDesktopServices::CacheLocation);
#endif
    if(name.isEmpty())
    {
        name = "Temporary Files";
    }
    return name;
}

DENG2_PIMPL(UpdaterSettingsDialog),
DENG2_OBSERVES(ToggleWidget, Toggle)
{
    ToggleWidget *autoCheck;
    ChoiceWidget *freqs;
    LabelWidget *lastChecked;
    ChoiceWidget *channels;
    ChoiceWidget *paths;
    ToggleWidget *autoDown;
    ToggleWidget *deleteAfter;

    Instance(Public *i, Mode mode) : Base(i)
    {
        ScrollAreaWidget &area = self.area();

        // Create the widgets.
        area.add(autoCheck   = new ToggleWidget);
        area.add(freqs       = new ChoiceWidget);
        area.add(lastChecked = new LabelWidget);
        area.add(channels    = new ChoiceWidget);
        area.add(autoDown    = new ToggleWidget);
        area.add(paths       = new ChoiceWidget);
        area.add(deleteAfter = new ToggleWidget);

        // The updater Config is changed when the widget state is modified.
        QObject::connect(autoCheck,   SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), thisPublic, SLOT(apply()));
        QObject::connect(freqs,       SIGNAL(selectionChangedByUser(uint)),                  thisPublic, SLOT(apply()));
        QObject::connect(channels,    SIGNAL(selectionChangedByUser(uint)),                  thisPublic, SLOT(apply()));
        QObject::connect(autoDown,    SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), thisPublic, SLOT(apply()));
        QObject::connect(paths,       SIGNAL(selectionChangedByUser(uint)),                  thisPublic, SLOT(apply()));
        QObject::connect(deleteAfter, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), thisPublic, SLOT(apply()));

        LabelWidget *releaseLabel = new LabelWidget;
        area.add(releaseLabel);

        LabelWidget *pathLabel = new LabelWidget;
        area.add(pathLabel);

        autoCheck->setText(tr("Check for updates:"));

        freqs->items()
                << new ChoiceItem(tr("At startup"), UpdaterSettings::AtStartup)
                << new ChoiceItem(tr("Daily"),      UpdaterSettings::Daily)
                << new ChoiceItem(tr("Biweekly"),   UpdaterSettings::Biweekly)
                << new ChoiceItem(tr("Weekly"),     UpdaterSettings::Weekly)
                << new ChoiceItem(tr("Monthly"),    UpdaterSettings::Monthly);

        lastChecked->margins().setTop("");

        releaseLabel->setText("Release type:");

        channels->items()
                << new ChoiceItem(tr("Stable"),             UpdaterSettings::Stable)
                << new ChoiceItem(tr("Unstable/Candidate"), UpdaterSettings::Unstable);

        pathLabel->setText(tr("Download location:"));

        paths->items()
                << new ChoiceItem(defaultLocationName(),
                                  UpdaterSettings::defaultDownloadPath().toString());

        autoDown->setText(tr("Download automatically"));
        deleteAfter->setText(tr("Delete file after install"));

        fetch();

        autoCheck->audienceForToggle() += this;

        // Place the widgets into a grid.
        GridLayout layout(area.contentRule().left(), area.contentRule().top());
        layout.setGridSize(2, 0);
        layout.setColumnAlignment(0, ui::AlignRight); // Labels aligned to the right.

        layout << *autoCheck    << *freqs
               << Const(0)      << *lastChecked
               << *releaseLabel << *channels
               << Const(0)      << *autoDown
               << Const(0)      << *deleteAfter
               << *pathLabel    << *paths;

        area.setContentSize(layout.width(), layout.height());

        self.buttons()
                << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, tr("Close"));

        if(mode == WithApplyAndCheckButton)
        {
            self.buttons()
                    << new DialogButtonItem(DialogWidget::Action, tr("Check Now"),
                                            new SignalAction(thisPublic, SLOT(applyAndCheckNow())));
        }
    }

    void fetch()
    {
        UpdaterSettings st;

        String ago = st.lastCheckAgo();
        if(!ago.isEmpty())
        {
            lastChecked->setText(_E(F)_E(t) + tr("Last checked %1.").arg(st.lastCheckAgo()));
        }
        else
        {
            lastChecked->setText(_E(F)_E(t) + tr("Never checked."));
        }

        autoCheck->setActive(!st.onlyCheckManually());
        freqs->enable(!st.onlyCheckManually());
        freqs->setSelected(freqs->items().findData(st.frequency()));
        channels->setSelected(channels->items().findData(st.channel()));
        setDownloadPath(st.downloadPath());
        autoDown->setActive(st.autoDownload());
        deleteAfter->setActive(st.deleteAfterUpdate());
    }

    void toggleStateChanged(ToggleWidget &toggle)
    {
        if(&toggle == autoCheck)
        {
            freqs->enable(autoCheck->isActive());
        }
    }

    void apply()
    {
        UpdaterSettings st;

        st.setOnlyCheckManually(autoCheck->isInactive());
        ui::Data::Pos sel = freqs->selected();
        if(sel != ui::Data::InvalidPos)
        {
            st.setFrequency(UpdaterSettings::Frequency(freqs->items().at(sel).data().toInt()));
        }
        sel = channels->selected();
        if(sel != ui::Data::InvalidPos)
        {
            st.setChannel(UpdaterSettings::Channel(channels->items().at(sel).data().toInt()));
        }
        //st.setDownloadPath(pathList->itemData(pathList->currentIndex()).toString());
        st.setAutoDownload(autoDown->isActive());
        st.setDeleteAfterUpdate(deleteAfter->isActive());
    }

    void setDownloadPath(QString const &/*dir*/)
    {
        paths->setSelected(0);
        /*
        if(dir != UpdaterSettings::defaultDownloadPath())
        {
            // Remove extra items.
            while(pathList->count() > 2)
            {
                pathList->removeItem(0);
            }
            pathList->insertItem(0, QDir(dir).dirName(), dir);
            pathList->setCurrentIndex(0);
        }
        else
        {
            pathList->setCurrentIndex(pathList->findData(dir));
        }*/
    }
};

UpdaterSettingsDialog::UpdaterSettingsDialog(Mode mode, String const &name)
    : DialogWidget(name, WithHeading), d(new Instance(this, mode))
{
    heading().setText(tr("Updater Settings"));
}

void UpdaterSettingsDialog::apply()
{
    d->apply();
}

void UpdaterSettingsDialog::applyAndCheckNow()
{
    accept();
    ClientApp::updater().checkNow();
}

void UpdaterSettingsDialog::finish(int result)
{
    if(result)
    {
        d->apply();
    }

    DialogWidget::finish(result);
}

/*
void UpdaterSettingsDialog::pathActivated(int index)
{
    QString path = d->pathList->itemData(index).toString();
    if(path.isEmpty())
    {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Download Folder"), QDir::homePath());
        if(!dir.isEmpty())
        {
            d->setDownloadPath(dir);
        }
        else
        {
            d->setDownloadPath(UpdaterSettings::defaultDownloadPath());
        }
    }
}
*/
