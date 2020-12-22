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
#include "ui/dialogs/repositorybrowserdialog.h"
#include "ui/widgets/homeitemwidget.h"
#include "ui/widgets/homemenuwidget.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/games.h>
#include <doomsday/res/databundle.h>

#include <de/callbackaction.h>
#include <de/config.h>
#include <de/directorylistdialog.h>
#include <de/filesystem.h>
#include <de/loop.h>
#include <de/package.h>
#include <de/popupmenuwidget.h>
#include <de/ui/actionitem.h>
#include <de/ui/subwidgetitem.h>

using namespace de;

DE_GUI_PIMPL(PackagesColumnWidget)
, DE_OBSERVES(DoomsdayApp, GameChange)
, DE_OBSERVES(PackagesWidget, ItemCount)
{
    PackagesWidget *packages;
    LabelWidget *   countLabel;
    ButtonWidget *  folderOptionsButton;
    ui::ListData    actions;
    Dispatch    mainCall;
    int             totalPackageCount = 0;

    Impl(Public *i) : Base(i)
    {
        DoomsdayApp::app().audienceForGameChange() += this;

        actions << new ui::SubwidgetItem("...", ui::Left, [this]() -> PopupWidget * {
            return new PackageInfoDialog(packages->actionPackage(),
                                         PackageInfoDialog::EnableActions);
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

        packages->audienceForItemCount() += this;

        area.add(folderOptionsButton = new ButtonWidget);
        folderOptionsButton->setStyleImage("gear", "default");
        folderOptionsButton->setText("Configure Data Files");
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
        self().header().menuButton().setPopup([this] (const PopupButtonWidget &) -> PopupWidget * {
            auto *menu = new PopupMenuWidget;
            menu->items()
                    << new ui::SubwidgetItem(style().images().image("gear"),
                                             ui::Item::ShownAsButton | ui::Item::ClosesParentPopup,
                                             "Settings", ui::Left, makePopup<DataFileSettingsDialog>)
//                    << new ui::ActionItem("Install Mods..." _E(l)_E(s)_E(D) " BETA",
//                                          new CallbackAction([this]() { openRepositoryBrowser(); }))
                    << new ui::Item(ui::Item::Separator)
                    << new ui::ActionItem("Show Recognized IWADs",
                                          new CallbackAction([this]() { packages->searchTermsEditor().setText("gamedata"); }))
                    << new ui::ActionItem("Show Box Contents",
                                          new CallbackAction([this]() { packages->searchTermsEditor().setText("hidden"); }))
                    << new ui::ActionItem("Show Core Packages",
                                          new CallbackAction([this]() { packages->searchTermsEditor().setText("core"); }))
                    << new ui::Item(ui::Item::Separator)
                    << new ui::ActionItem("Refresh List",
                                          new CallbackAction([]() { FS::get().refreshAsync(); }));
                return menu;
        }, ui::Down);
    }

    void itemCountChanged(duint shown, duint total) override
    {
        if (shown == total)
        {
            countLabel->setText(Stringf("%u available", total));
    }
        else
        {
            countLabel->setText(Stringf("%u shown out of %u available", shown, total));
        }
        totalPackageCount = total;
        DE_NOTIFY_PUBLIC(AvailableCount, i) i->availablePackageCountChanged(total);
    }

    void currentGameChanged(const Game &game) override
    {
        folderOptionsButton->show(game.isNull());
    }

    void openRepositoryBrowser()
    {
        auto *dlg = new RepositoryBrowserDialog;
        dlg->setDeleteAfterDismissed(true);
        dlg->exec(root());
    }

    DE_PIMPL_AUDIENCE(AvailableCount)
};

DE_AUDIENCE_METHOD(PackagesColumnWidget, AvailableCount)

PackagesColumnWidget::PackagesColumnWidget()
    : ColumnWidget("packages-column")
    , d(new Impl(this))
{
    header().title().setText(_E(s) "\n" _E(.) "Mods");
    header().info().setText("Browse available mods/add-ons and install new ones.");
    header().infoPanel().close(0.0);

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
    return "Mods";
}

int PackagesColumnWidget::tabShortcut() const
{
    return 's';
}

void PackagesColumnWidget::setHighlighted(bool highlighted)
{
    ColumnWidget::setHighlighted(highlighted);
    if (highlighted)
    {
        root().setFocus(&d->packages->searchTermsEditor());
    }
}
