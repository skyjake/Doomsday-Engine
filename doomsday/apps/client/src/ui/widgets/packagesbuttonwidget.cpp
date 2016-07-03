/** @file packagesbuttonwidget.cpp
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

#include "ui/widgets/packagesbuttonwidget.h"
#include "ui/dialogs/packagesdialog.h"

#include <de/CallbackAction>

using namespace de;

DENG_GUI_PIMPL(PackagesButtonWidget)
{
    StringList packages;
    String dialogTitle;
    String noneLabel;
    GameProfile const *profile = nullptr;
    std::function<void (PackagesDialog &)> setupFunc;

    Impl(Public *i) : Base(i)
    {}

    void updateLabel()
    {
        self.setStyleImage("package");

        if (packages.isEmpty())
        {
            self.setText(noneLabel);
            self.setTextColor("text");
            self.setImageColor(style().colors().colorf("text"));

            if (!noneLabel.isEmpty()) self.setImage(nullptr);
        }
        else
        {
            self.setText(String::format("%i", packages.count()));
            self.setTextColor("accent");
            self.setImageColor(style().colors().colorf("accent"));
        }
    }

    void pressed()
    {
        // The Packages dialog allows selecting which packages are loaded, and in
        // which order. One can also browse the available packages.
        auto *dlg = new PackagesDialog(dialogTitle);
        if (profile)
        {
            dlg->setGame(profile->game());
        }
        dlg->setDeleteAfterDismissed(true);
        dlg->setSelectedPackages(packages);
        dlg->setAcceptanceAction(new CallbackAction([this, dlg] ()
        {
            packages = dlg->selectedPackages();
            updateLabel();

            // Notify.
            QStringList ids;
            for (auto const &p : packages) ids << p;
            emit self.packageSelectionChanged(ids);
        }));
        root().addOnTop(dlg);
        if (setupFunc) setupFunc(*dlg);
        dlg->open();
    }
};

PackagesButtonWidget::PackagesButtonWidget()
    : d(new Impl(this))
{
    setOverrideImageSize(style().fonts().font("default").height().value());
    setSizePolicy(ui::Expand, ui::Expand);
    setTextAlignment(ui::AlignLeft);
    connect(this, &ButtonWidget::pressed, [this] () { d->pressed(); });

    d->updateLabel();
}

void PackagesButtonWidget::setGameProfile(GameProfile const &profile)
{
    d->profile = &profile;
}

void PackagesButtonWidget::setSetupCallback(std::function<void (PackagesDialog &)> func)
{
    d->setupFunc = func;
}

void PackagesButtonWidget::setNoneLabel(String const &noneLabel)
{
    d->noneLabel = noneLabel;
    d->updateLabel();
}

void PackagesButtonWidget::setDialogTitle(String const &title)
{
    d->dialogTitle = title;
}

void PackagesButtonWidget::setPackages(StringList const &packageIds)
{
    d->packages = packageIds;
    d->updateLabel();
}

StringList PackagesButtonWidget::packages() const
{
    return d->packages;
}
