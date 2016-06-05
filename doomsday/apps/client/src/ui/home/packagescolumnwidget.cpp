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

#include <de/PopupMenuWidget>
#include <de/ui/SubwidgetItem>

using namespace de;

DENG_GUI_PIMPL(PackagesColumnWidget)
, public PackagesWidget::IButtonHandler
{
    PackagesWidget *packages;
    LabelWidget *countLabel;

    Instance(Public *i) : Base(i)
    {
        countLabel = new LabelWidget;

        ScrollAreaWidget &area = self.scrollArea();
        area.add(packages = new PackagesWidget("home-packages"));
        packages->setButtonHandler(*this);
        packages->setButtonLabels(tr("..."), tr("..."));
        packages->rule()
                .setInput(Rule::Width, area.contentRule().width())
                .setInput(Rule::Top,   self.header().rule().bottom() +
                                       rule("gap"))
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
    }

    void packageButtonClicked(ButtonWidget &button, de::String const &packageId) override
    {
        auto *popMenu = new PopupMenuWidget;
        popMenu->setDeleteAfterDismissed(true);
        popMenu->setColorTheme(Inverted);
        popMenu->setAnchorAndOpeningDirection(button.rule(), ui::Down);
        popMenu->items()
                << new ui::SubwidgetItem(tr("Info"), ui::Down,
                                         [this, packageId] () -> PopupWidget * {
                                             return new PackagePopupWidget(packageId);
                                         })
                << new ui::Item(ui::Item::Separator)
                << new ui::ActionItem(style().images().image("close.ring"), tr("Uninstall..."));
        root().addOnTop(popMenu);
        popMenu->open();
    }
};

PackagesColumnWidget::PackagesColumnWidget()
    : ColumnWidget("packages-column")
    , d(new Instance(this))
{
    header().title().setText(_E(s) "\n" _E(.) + tr("Packages"));
    header().info().setText(tr("Browse available packages and install new ones."));
    header().infoPanel().close(0);

    d->packages->setFilterEditorMinimumY(scrollArea().margins().top());

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
}

String PackagesColumnWidget::tabHeading() const
{
    return tr("Packages");
}
