/** @file packagescolumnwidget.cpp
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/widgets/packagepopupwidget.h"
#include "ui/widgets/homeitemwidget.h"

#include <de/CallbackAction>
#include <de/Config>
#include <de/DirectoryListDialog>
#include <de/Loop>
#include <de/Package>
#include <de/PopupMenuWidget>
#include <de/ui/ActionItem>
#include <de/ui/SubwidgetItem>

#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/resource/databundle.h>

using namespace de;

static PopupWidget *makePackageFoldersDialog()
{
    Variable &pkgFolders = Config::get()["resource.packageFolder"];

    auto *dlg = new DirectoryListDialog;
    dlg->title().setFont("heading");
    dlg->title().setText(QObject::tr("Add-on and Package Folders"));
    dlg->message().setText(QObject::tr("The following folders are searched for resource packs and other add-ons:"));
    dlg->setValue(pkgFolders.value());
    dlg->setAcceptanceAction(new CallbackAction([dlg, &pkgFolders] ()
    {
        pkgFolders.set(dlg->value());

        // Reload packages and recheck for game availability.
        DoomsdayApp::app().initPackageFolders();
    }));
    return dlg;
}

DENG_GUI_PIMPL(PackagesColumnWidget)
{
    PackagesWidget *packages;
    LabelWidget *countLabel;
    ui::ListData actions;
    LoopCallback mainCall;

    Impl(Public *i) : Base(i)
    {
        actions << new ui::SubwidgetItem(tr("..."), ui::Down, [this] () -> PopupWidget *
        {
            String const packageId = packages->actionPackage();

            auto *popMenu = new PopupMenuWidget;
            popMenu->setColorTheme(Inverted);
            popMenu->items() << new ui::SubwidgetItem(tr("Info"), ui::Down,
                [this, packageId] () -> PopupWidget * {
                    return new PackagePopupWidget(packageId);
                });

            if (Package::hasOptionalContent(packageId))
            {
                auto openOpts = [this] () {
                    packages->openContentOptions(*packages->actionItem());
                };
                popMenu->items() << new ui::ActionItem(style().images().image("gear"),
                                                       tr("Select Packages"), new CallbackAction(openOpts));
            }

            popMenu->items()
                    << new ui::Item(ui::Item::Separator)
                    << new ui::ActionItem(style().images().image("close.ring"), tr("Uninstall..."));
            return popMenu;
        });

        countLabel = new LabelWidget;

        ScrollAreaWidget &area = self.scrollArea();
        area.add(packages = new PackagesWidget("home-packages"));
        //packages->setMaximumPanelHeight(self.rule().height() - self.margins().height() - rule("gap")*3);
        packages->setActionItems(actions);
        packages->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Top,   self.header().rule().bottom() + rule("gap"))
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
        });

        // Column menu.
        self.header().menuButton().setPopup([] (PopupButtonWidget const &) -> PopupWidget * {
            auto *menu = new PopupMenuWidget;
            menu->items() << new ui::SubwidgetItem(tr("Folders"), ui::Left, makePackageFoldersDialog);
            return menu;
        }, ui::Down);
    }

    /*void gameReadinessUpdated() override
    {
        if (!mainCall)
        {
            mainCall.enqueue([this] () { packages->populate(); });
        }
    }*/
};

PackagesColumnWidget::PackagesColumnWidget()
    : ColumnWidget("packages-column")
    , d(new Impl(this))
{
    header().title().setText(_E(s) "\n" _E(.) + tr("Packages"));
    header().info().setText(tr("Browse available packages and install new ones."));
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
                                d->packages->rule().height());

    // Additional layout for the packages list.
    d->packages->setFilterEditorMinimumY(scrollArea().margins().top());
    d->packages->progress().rule().setRect(scrollArea().rule());
}

String PackagesColumnWidget::tabHeading() const
{
    return tr("Packages");
}
