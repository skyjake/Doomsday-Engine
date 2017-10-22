/** @file packagescolumnwidget.cpp
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/home/packagescolumnwidget.h"
#include "ui/widgets/packageswidget.h"
#include "ui/dialogs/packageinfodialog.h"
#include "ui/dialogs/datafilesettingsdialog.h"
#include "ui/widgets/homeitemwidget.h"
#include "ui/widgets/homemenuwidget.h"

#include <de/CallbackAction>
#include <de/Config>
#include <de/DirectoryListDialog>
#include <de/FileSystem>
#include <de/Loop>
#include <de/Package>
#include <de/PopupMenuWidget>
#include <de/ui/ActionItem>
#include <de/ui/SubwidgetItem>

#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/resource/databundle.h>

using namespace de;

DENG_GUI_PIMPL(PackagesColumnWidget)
, DENG2_OBSERVES(DoomsdayApp, GameChange)
{
    PackagesWidget *packages;
    LabelWidget *countLabel;
    ButtonWidget *folderOptionsButton;
    ui::ListData actions;
    LoopCallback mainCall;
    int totalPackageCount = 0;

    Impl(Public *i) : Base(i)
    {
        DoomsdayApp::app().audienceForGameChange() += this;

        actions << new ui::SubwidgetItem(tr("..."), ui::Left, [this] () -> PopupWidget *
        {
            return new PackageInfoDialog(packages->actionPackage());
        });

        countLabel = new LabelWidget;

        ScrollAreaWidget &area = self().scrollArea();
        area.add(packages = new PackagesWidget(PackagesWidget::PopulationEnabled, "home-packages"));
        packages->setActionItems(actions);
        packages->setRightClickToOpenContextMenu(true);
        packages->margins().setLeft("").setRight("");
        packages->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Top,   self().header().rule().bottom() + rule("gap"))
                .setInput(Rule::Left,  area.contentRule().left());

        QObject::connect(packages, &PackagesWidget::itemCountChanged,
                         [this] (duint shown, duint total)
        {
            if (shown == total)
            {
                countLabel->setText(tr("%1 available").arg(total));
            }
            else
            {
                countLabel->setText(tr("%1 shown out of %2 available").arg(shown).arg(total));
            }
            totalPackageCount = total;
            emit self().availablePackageCountChanged(total);
        });

        area.add(folderOptionsButton = new ButtonWidget);
        folderOptionsButton->setStyleImage("gear", "default");
        folderOptionsButton->setText(tr("Configure Data Files"));
        folderOptionsButton->setTextAlignment(ui::AlignRight);
        folderOptionsButton->setSizePolicy(ui::Fixed, ui::Expand);
        folderOptionsButton->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Left,  area.contentRule().left())
                .setInput(Rule::Top,   packages->rule().bottom());
        folderOptionsButton->setAction(new CallbackAction([this] ()
        {
            auto *dlg = new DataFileSettingsDialog;
            dlg->setDeleteAfterDismissed(true);
            dlg->setAnchorAndOpeningDirection(folderOptionsButton->rule(), ui::Left);
            self().root().addOnTop(dlg);
            dlg->open();
        }));

        // Column actions menu.
        self().header().menuButton().setPopup([this] (PopupButtonWidget const &) -> PopupWidget * {
            auto *menu = new PopupMenuWidget;
            menu->items()
                    << new ui::ActionItem(tr("Refresh"),
                                          new CallbackAction([this] () { packages->refreshPackages(); }));
                return menu;
        }, ui::Down);
    }

    void currentGameChanged(Game const &game) override
    {
        folderOptionsButton->show(game.isNull());
    }
};

PackagesColumnWidget::PackagesColumnWidget()
    : ColumnWidget("packages-column")
    , d(new Impl(this))
{
    header().title().setText(_E(s) "\n" _E(.) + tr("Mods"));
    header().info().setText(tr("Browse available mods/add-ons and install new ones."));
    header().infoPanel().close(0);

    // Total number of packages listed.
    d->countLabel->setFont("small");
    d->countLabel->setSizePolicy(ui::Expand, ui::Fixed);
    d->countLabel->rule()
            .setInput(Rule::Left,   header().menuButton().rule().right())
            .setInput(Rule::Height, header().menuButton().rule().height())
            .setInput(Rule::Top,    header().menuButton().rule().top());
    header().add(d->countLabel);

    scrollArea().setContentSize(maximumContentWidth(),
                                header().rule().height() +
                                rule("gap") +
                                d->packages->rule().height() +
                                d->folderOptionsButton->rule().height()*2);

    // Additional layout for the packages list.
    d->packages->setFilterEditorMinimumY(scrollArea().margins().top());
    d->packages->progress().rule().setRect(scrollArea().rule());
}

int PackagesColumnWidget::availablePackageCount() const
{
    return d->totalPackageCount;
}

String PackagesColumnWidget::tabHeading() const
{
    return tr("Mods");
}

String PackagesColumnWidget::tabShortcut() const
{
    return QStringLiteral("s");
}

void PackagesColumnWidget::setHighlighted(bool highlighted)
{
    ColumnWidget::setHighlighted(highlighted);
    if (highlighted)
    {
        root().setFocus(&d->packages->searchTermsEditor());
    }
}
