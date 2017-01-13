/** @file packagessidebarwidget.cpp
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

#include "ui/widgets/packagessidebarwidget.h"
#include "ui/widgets/packageswidget.h"
#include "ui/widgets/packageinfodialog.h"
#include "ui/widgets/homeitemwidget.h"

#include <de/CallbackAction>
#include <de/ui/ActionItem>

using namespace de;

DENG_GUI_PIMPL(PackagesSidebarWidget)
{
    PackagesWidget *browser;

    Impl(Public *i) : Base(i)
    {
        GuiWidget *container = &self().containerWidget();

        container->add(browser = new PackagesWidget);
        browser->setFilterEditorMinimumY(Const(0));
        browser->rule().setInput(Rule::Width, rule("sidebar.width"));

        // Action for showing information about the package.
        browser->actionItems().insert(0, new ui::ActionItem(tr("..."), new CallbackAction([this] ()
        {
            auto *pop = new PackageInfoDialog(browser->actionPackage());
            root().addOnTop(pop);
            pop->setDeleteAfterDismissed(true);
            pop->setAnchorAndOpeningDirection(browser->actionWidget()->as<HomeItemWidget>()
                                              .buttonWidget(0).rule(), ui::Up);
            pop->open();
        })));
    }
};

PackagesSidebarWidget::PackagesSidebarWidget()
    : SidebarWidget("Packages", "packages-sidebar")
    , d(new Impl(this))
{
    // Button for refreshing the available packages.
    auto *refreshButton = new ButtonWidget;
    containerWidget().add(refreshButton);
    refreshButton->setSizePolicy(ui::Expand, ui::Fixed);
    refreshButton->rule()
            .setInput(Rule::Right,  closeButton().rule().left())
            .setInput(Rule::Top,    closeButton().rule().top())
            .setInput(Rule::Height, closeButton().rule().height());
    refreshButton->setStyleImage("refresh", "default");
    refreshButton->setActionFn([this] ()
    {
        d->browser->refreshPackages();
    });

    layout() << *d->browser;

    updateSidebarLayout(Const(0), Const(0));

    d->browser->progress().rule().setRect(rule());
}
