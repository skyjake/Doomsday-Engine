/** @file updatersettingsdialog.cpp Dialog for configuring automatic updates.
 * @ingroup updater
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "updater.h"
#include "clientapp.h"
#include <de/labelwidget.h>
#include <de/choicewidget.h>
#include <de/variabletogglewidget.h>
#include <de/gridlayout.h>
//#include <de/signalaction.h>
#include <de/log.h>
//#include <QDesktopServices>

using namespace de;

#if 0
static QString defaultLocationName()
{
#ifdef DE_QT_5_0_OR_NEWER
    QString name = QStandardPaths::displayName(QStandardPaths::CacheLocation);
#else
    QString name = QDesktopServices::displayName(QDesktopServices::CacheLocation);
#endif
    if (name.isEmpty())
    {
        name = "Temporary Files";
    }
    return name;
}
#endif

DE_PIMPL(UpdaterSettingsDialog)
, DE_OBSERVES(ToggleWidget, Toggle)
{
    ToggleWidget *autoCheck;
    ChoiceWidget *freqs;
    LabelWidget * lastChecked;
    ChoiceWidget *channels;
    //ChoiceWidget *paths;
    ToggleWidget *autoDown;
    ToggleWidget *deleteAfter;
    bool          didApply = false;

    Impl(Public *i, Mode mode) : Base(i)
    {
        ScrollAreaWidget &area = self().area();

        // Create the widgets.
        area.add(autoCheck   = new ToggleWidget);
        area.add(freqs       = new ChoiceWidget);
        area.add(lastChecked = new LabelWidget);
        area.add(channels    = new ChoiceWidget);
        area.add(autoDown    = new ToggleWidget);
        //area.add(paths       = new ChoiceWidget);
        area.add(deleteAfter = new ToggleWidget);

        // The updater Config is changed when the widget state is modified.
        for (ToggleWidget *toggle : {autoCheck, autoDown, deleteAfter})
        {
            toggle->audienceForUserToggle() += [this]() { self().apply(); };
        }
        for (ChoiceWidget *choice : {freqs, channels})
        {
            choice->audienceForUserSelection() += [this]() { self().apply(); };
        }
//        QObject::connect(autoCheck,   SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), thisPublic, SLOT(apply()));
//        QObject::connect(freqs,       SIGNAL(selectionChangedByUser(uint)),                  thisPublic, SLOT(apply()));
//        QObject::connect(channels,    SIGNAL(selectionChangedByUser(uint)),                  thisPublic, SLOT(apply()));
//        QObject::connect(autoDown,    SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), thisPublic, SLOT(apply()));
//        QObject::connect(deleteAfter, SIGNAL(stateChangedByUser(ToggleWidget::ToggleState)), thisPublic, SLOT(apply()));

        LabelWidget *releaseLabel = new LabelWidget;
        area.add(releaseLabel);

        /*LabelWidget *pathLabel = new LabelWidget;
        area.add(pathLabel);*/

        autoCheck->setText("Check for Updates:");

        freqs->items()
                << new ChoiceItem("At startup", UpdaterSettings::AtStartup)
                << new ChoiceItem("Daily",      UpdaterSettings::Daily)
                << new ChoiceItem("Biweekly",   UpdaterSettings::Biweekly)
                << new ChoiceItem("Weekly",     UpdaterSettings::Weekly)
                << new ChoiceItem("Monthly",    UpdaterSettings::Monthly);

        lastChecked->margins().setTop("");
        lastChecked->setFont("separator.annotation");
        lastChecked->setTextColor("altaccent");

        releaseLabel->setText("Release Type:");

        channels->items()
                << new ChoiceItem("Stable only",      UpdaterSettings::Stable)
                << new ChoiceItem("RC or stable",     UpdaterSettings::StableOrCandidate)
                << new ChoiceItem("Unstable/nightly", UpdaterSettings::Unstable);

        /*pathLabel->setText("Download location:");
        paths->items()
                << new ChoiceItem(defaultLocationName(),
                                  UpdaterSettings::defaultDownloadPath().toString());*/

        autoDown->setText("Download Automatically");
        deleteAfter->setText("Delete File After Install");

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
               << Const(0)      << *deleteAfter;
               //<< *pathLabel    << *paths;

        area.setContentSize(layout);

        self().buttons()
                << new DialogButtonItem(DialogWidget::Default | DialogWidget::Accept, "Close");

        if (mode == WithApplyAndCheckButton)
        {
            self().buttons()
                    << new DialogButtonItem(DialogWidget::Action, "Check Now",
                                            [this]() { self().applyAndCheckNow(); });
        }
    }

    void fetch()
    {
        UpdaterSettings st;

        String ago = st.lastCheckAgo();
        if (!ago.isEmpty())
        {
            lastChecked->setText(Stringf("Last checked %s.", st.lastCheckAgo().c_str()));
        }
        else
        {
            lastChecked->setText("Never checked.");
        }

        autoCheck->setActive(!st.onlyCheckManually());
        freqs->enable(!st.onlyCheckManually());
        freqs->setSelected(freqs->items().findData(NumberValue(st.frequency())));
        channels->setSelected(channels->items().findData(NumberValue(st.channel())));
        //setDownloadPath(st.downloadPath());
        autoDown->setActive(st.autoDownload());
        deleteAfter->setActive(st.deleteAfterUpdate());
    }

    void toggleStateChanged(ToggleWidget &toggle)
    {
        if (&toggle == autoCheck)
        {
            freqs->enable(autoCheck->isActive());
        }
    }

    void apply()
    {
        UpdaterSettings st;

        st.setOnlyCheckManually(autoCheck->isInactive());
        ui::Data::Pos sel = freqs->selected();
        if (sel != ui::Data::InvalidPos)
        {
            st.setFrequency(UpdaterSettings::Frequency(freqs->items().at(sel).data().asInt()));
        }
        sel = channels->selected();
        if (sel != ui::Data::InvalidPos)
        {
            st.setChannel(UpdaterSettings::Channel(channels->items().at(sel).data().asInt()));
        }
        //st.setDownloadPath(pathList->itemData(pathList->currentIndex()).toString());
        st.setAutoDownload(autoDown->isActive());
        st.setDeleteAfterUpdate(deleteAfter->isActive());
    }

#if 0
    void setDownloadPath(const QString &/*dir*/)
    {
        paths->setSelected(0);
        /*
        if (dir != UpdaterSettings::defaultDownloadPath())
        {
            // Remove extra items.
            while (pathList->count() > 2)
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
#endif
};

UpdaterSettingsDialog::UpdaterSettingsDialog(Mode mode, const String &name)
    : DialogWidget(name, WithHeading), d(new Impl(this, mode))
{
    heading().setText("Updater Settings");
    heading().setImage(style().images().image("updater"));
}

bool UpdaterSettingsDialog::settingsHaveChanged() const
{
    return d->didApply;
}

void UpdaterSettingsDialog::apply()
{
    d->apply();
    d->didApply = true;
}

void UpdaterSettingsDialog::applyAndCheckNow()
{
    accept();
    ClientApp::updater().checkNow();
}

void UpdaterSettingsDialog::finish(int result)
{
    if (result)
    {
        d->apply();
    }

    DialogWidget::finish(result);
}

/*
void UpdaterSettingsDialog::pathActivated(int index)
{
    QString path = d->pathList->itemData(index).toString();
    if (path.isEmpty())
    {
        QString dir = QFileDialog::getExistingDirectory(this, "Download Folder", QDir::homePath());
        if (!dir.isEmpty())
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
