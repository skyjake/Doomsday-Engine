/** @file packagecompatibilitydialog.cpp
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

#include "ui/dialogs/packagecompatibilitydialog.h"
#include "ui/dialogs/packageinfodialog.h"
#include "ui/widgets/packageswidget.h"

#include <doomsday/DoomsdayApp>
#include <doomsday/DataBundle>
#include <de/CallbackAction>
#include <de/Package>
#include <de/PackageLoader>
#include <de/ui/SubwidgetItem>

#include "dd_main.h"

using namespace de;

DENG2_PIMPL(PackageCompatibilityDialog)
{
    String message;
    StringList wanted;
    bool conflicted = false;
    PackagesWidget *list = nullptr;
    ui::ListData actions;
    ProgressWidget *updating;
    bool ignoreCheck = false;

    Impl(Public *i) : Base(i)
    {
        self().add(updating = new ProgressWidget);
        updating->setSizePolicy(ui::Expand, ui::Expand);
        updating->useMiniStyle("altaccent");
        updating->setText(tr("Updating..."));
        updating->setTextAlignment(ui::AlignLeft);
        updating->setMode(ProgressWidget::Indefinite);
        updating->setOpacity(0);
        updating->rule()
                .setInput(Rule::Top,    self().buttonsMenu().rule().top())
                .setInput(Rule::Right,  self().buttonsMenu().rule().left())
                .setInput(Rule::Height, self().buttonsMenu().rule().height() - self().margins().bottom());
    }

    String defaultButtonLabel() const
    {
        if (!list) return "";
        if (ignoreCheck)
        {
            return _E(b)_E(D) + tr("Ignore and Continue");
        }
        if (list->itemCount() > 0)
        {
            return _E(b) + tr("Load Packages");
        }
        return _E(b) + tr("Unload Packages");
    }

    void enableIgnore(bool yes)
    {
        ignoreCheck = yes;
        if (auto *button = self().buttonWidget(Id1))
        {
            button->setText(defaultButtonLabel());
        }
    }

    void update()
    {
        if (list)
        {
            delete list;
            list = nullptr;
        }
        self().buttons().clear();

        try
        {
            // The only action on the packages is to view information.
            actions << new ui::SubwidgetItem(tr("..."), ui::Up, [this] () -> PopupWidget *
            {
                 return new PackageInfoDialog(list->actionPackage());
            });

            self().area().add(list = new PackagesWidget(wanted));
            list->setActionItems(actions);
            list->setActionsAlwaysShown(true);
            list->setFilterEditorMinimumY(self().area().rule().top());

            StringList const loaded = DoomsdayApp::loadedPackagesAffectingGameplay();
            //qDebug() << "Currently loaded:" << loaded;

            if (!GameProfiles::arePackageListsCompatible(loaded, wanted))
            {
                conflicted = true;
                if (list->itemCount() > 0)
                {
                    self().message().setText(message + "\n\n" + tr("The packages listed below "
                                                                 "should be loaded."));
                    self().buttons()
                            << new DialogButtonItem(Default | Accept | Id1, defaultButtonLabel(),
                                                    new CallbackAction([this] () { resolvePackages(); }));
                }
                else
                {
                    list->hide();
                    self().message().setText(message + "\n\n" + tr("All additional packages "
                                                                 "should be unloaded."));
                    self().buttons()
                               << new DialogButtonItem(Default | Accept | Id1, defaultButtonLabel(),
                                                       new CallbackAction([this] () { resolvePackages(); }));
                }
                self().buttons()
                        << new DialogButtonItem(Reject, tr("Cancel"));
            }
        }
        catch (PackagesWidget::UnavailableError const &er)
        {
            conflicted = true;
            self().message().setText(message + "\n\n" + er.asText());
            self().buttons() << new DialogButtonItem(Default | Reject);
        }

        self().updateLayout();
    }

    static bool containsIdentifier(String const &identifier, StringList const &ids)
    {
        for (String i : ids)
        {
            if (Package::equals(i, identifier)) return true;
        }
        return false;
    }

    void resolvePackages()
    {
        if (ignoreCheck)
        {
            LOG_RES_NOTE("Ignoring package compatibility check due to user request");
            self().accept();
            return;
        }

        qDebug() << "resolving...";

        auto &pkgLoader = PackageLoader::get();

        // Unload excess packages.
        StringList loaded = DoomsdayApp::loadedPackagesAffectingGameplay();

        int goodUntil = -1;

        // Check if all the loaded packages match the wanted ones.
        for (int i = 0; i < loaded.size() && i < wanted.size(); ++i)
        {
            if (Package::equals(loaded.at(i), wanted.at(i)))
            {
                goodUntil = i;
            }
            else
            {
                break;
            }
        }

        qDebug() << "Good until:" << goodUntil;

        // Unload excess.
        for (int i = loaded.size() - 1; i > goodUntil; --i)
        {
            qDebug() << "unloading excess" << loaded.at(i);

            pkgLoader.unload(loaded.at(i));
            /*if (DataBundle const *bundle = DataBundle::bundleForPackage(loaded.at(i)))
            {
                File1::tryUnload(*bundle);
            }*/
            loaded.removeAt(i);
        }

        // Load the remaining wanted packages.
        for (int i = goodUntil + 1; i < wanted.size(); ++i)
        {
            qDebug() << "loading wanted" << wanted.at(i);

            pkgLoader.load(wanted.at(i));
            /*if (DataBundle const *bundle = DataBundle::bundleForPackage(wanted.at(i)))
            {
                File1::tryLoad(*bundle);
            }*/
        }

        qDebug() << DoomsdayApp::loadedPackagesAffectingGameplay();

        self().buttonsMenu().disable();
        updating->setOpacity(1, 0.3);

        // Refresh resources.
        //DD_UpdateEngineState();

        self().accept();
    }
};

PackageCompatibilityDialog::PackageCompatibilityDialog(String const &name)
    : MessageDialog(name)
    , d(new Impl(this))
{
    title().setText(tr("Incompatible Add-ons"));
}

void PackageCompatibilityDialog::setMessage(String const &msg)
{
    d->message = msg;
}

void PackageCompatibilityDialog::setWantedPackages(StringList const &packages)
{
    DENG2_ASSERT(!packages.isEmpty());

    d->wanted = packages;
    d->update();
}

bool PackageCompatibilityDialog::isCompatible() const
{
    return !d->conflicted;
}

bool PackageCompatibilityDialog::handleEvent(Event const &event)
{
    // Hold Alt to skip the check.
    if (event.isKey())
    {
        auto const &key = event.as<KeyEvent>();
        if (key.ddKey() == DDKEY_LALT || key.ddKey() == DDKEY_RALT)
        {
            d->enableIgnore(key.type() != Event::KeyRelease);
        }
    }

    return MessageDialog::handleEvent(event);
}
