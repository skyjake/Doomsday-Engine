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

#include <doomsday/doomsdayapp.h>
#include <doomsday/res/databundle.h>
#include <de/callbackaction.h>
#include <de/package.h>
#include <de/packageloader.h>
#include <de/ui/subwidgetitem.h>
#include <de/togglewidget.h>
#include <de/loop.h>

#include "dd_main.h"

using namespace de;

DE_PIMPL(PackageCompatibilityDialog)
{
    String          message;
    StringList      wanted;
    bool            conflicted    = false;
    PackagesWidget *list          = nullptr;
    ToggleWidget *  ignoreToggle  = nullptr;
    LabelWidget *   ignoreWarning = nullptr;
    ui::ListData    actions;
    ProgressWidget *updating;
    bool            ignoreCheck = false;
    Dispatch    mainCall;

    Impl(Public *i) : Base(i)
    {
        self().add(updating = new ProgressWidget);
        self().area().enableIndicatorDraw(true);
        updating->setSizePolicy(ui::Expand, ui::Expand);
        updating->useMiniStyle("altaccent");
        updating->setText("Updating...");
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
            return _E(b)_E(D) "Ignore and Continue";
        }
        if (list->itemCount() > 0)
        {
            return _E(b) "Load Mods";
        }
        return _E(b) "Unload Mods";
    }

    void enableIgnore(bool yes)
    {
        ignoreCheck = yes;
        if (auto *button = self().buttonWidget(Id1))
        {
            button->setText(defaultButtonLabel());
        }
        if (auto *button = self().buttonWidget(Id2))
        {
            button->enable(ignoreCheck);
        }
    }

    void update()
    {
        if (list)
        {
            delete list;
            list = nullptr;
        }
        if (ignoreWarning)
        {
            delete ignoreWarning;
            ignoreWarning = nullptr;
        }
        if (ignoreToggle)
        {
            delete ignoreToggle;
            ignoreToggle = nullptr;
        }
        self().buttons().clear();

        try
        {
            // The only action on the packages is to view information.
            actions << new ui::SubwidgetItem("...", ui::Up, [this] () -> PopupWidget * {
                 return new PackageInfoDialog(list->actionPackage(),
                                             PackageInfoDialog::InformationOnly);
            });

            self().area().add(ignoreToggle = new ToggleWidget);
            ignoreToggle->setText("Ignore incompatible/missing mods");
            ignoreToggle->setAlignment(ui::AlignLeft);
            ignoreToggle->setActionFn([this]() {
                enableIgnore(ignoreToggle->isActive());
            });

            self().area().add(ignoreWarning = new LabelWidget);
            ignoreWarning->setText(_E(b) "Caution: " _E(.) "Playing without the right resources "
                                                           "may cause a crash.");
            ignoreWarning->setFont("separator.annotation");
            ignoreWarning->setTextColor("altaccent");
            ignoreWarning->setAlignment(ui::AlignLeft);
            ignoreWarning->setTextLineAlignment(ui::AlignLeft);
            ignoreWarning->margins().setTop(ConstantRule::zero());

            // Check which of the wanted packages are actually available.
            StringList wantedUnavailable;
            StringList wantedAvailable;
            List<std::pair<String, Version>> wantedDifferentVersionAvailable;
            {
                auto &pkgLoader = PackageLoader::get();

                for (const auto &id : wanted)
                {
                    if (pkgLoader.isAvailable(id))
                    {
                        wantedAvailable << id;
                    }
                    else
                    {
                        auto id_ver = Package::split(id);
                        if (const auto *p = pkgLoader.select(id_ver.first))
                        {
                            wantedDifferentVersionAvailable.push_back(
                                {Package::versionedIdentifierForFile(*p), id_ver.second});
                        }
                        else
                        {
                            wantedUnavailable << id;
                        }
                    }
                }
            }

            // Detail the problem to the user.
            String unavailNote;
            if (!wantedDifferentVersionAvailable.empty())
            {
                unavailNote +=
                    _E(b)_E(D) "Caution:" _E(.)_E(.) " There is a different version of "
                                                     "the following mods available:\n";
                for (const auto &avail_expected : wantedDifferentVersionAvailable)
                {
                    unavailNote +=
                        " - " _E(>) + Package::splitToHumanReadable(avail_expected.first) +
                        " " _E(l)_E(F) "\n(expected " +
                        avail_expected.second.fullNumber() + _E(<) ")\n" _E(w)_E(A);
                }
                unavailNote += "\n";
            }
            if (!wantedUnavailable.empty())
            {
                unavailNote += "The following mods are missing:\n";
                for (const auto &id : wantedUnavailable)
                {
                    unavailNote += " - " + Package::splitToHumanReadable(id) + "\n";
                }
                unavailNote += "\n";
            }

            StringList wantedDifferent =
                map<StringList>(wantedDifferentVersionAvailable,
                                [](const std::pair<String, Version> &sv) { return sv.first; });

            self().area().add(list = new PackagesWidget(wantedAvailable + wantedDifferent));
            list->setAllowPackageInfoActions(false);
            list->setHiddenTags({"core", "gamedata"});
            list->setActionItems(actions);
            list->setActionsAlwaysShown(true);
            list->setFilterEditorMinimumY(self().area().rule().top());

            const StringList loaded = DoomsdayApp::loadedPackagesAffectingGameplay();

            if (!GameProfiles::arePackageListsCompatible(loaded, wanted))
            {
                // Packages needs loading and/or unloading.
                conflicted = true;

                if (list->itemCount() == 0)
                {
                    list->hide();
                }
                if (!wantedUnavailable.empty())
                {
                    self().message().setText(message + "\n\n" + unavailNote +
                                             "Please locate the missing mods before continuing. "
                                             "You may need to add more folders in your Data Files "
                                             "settings so the mods can be found.");
                    if (list->itemCount() > 0)
                    {
                        self().message().setText(
                            self().message().text() +
                            "\n\nThe mods listed below are required and available, "
                            "and should be loaded now (the highlighted ones are already loaded).");
                    }
                    self().buttons() << new DialogButtonItem(Default | Accept | Id2,
                                                             _E(b) _E(D) "Ignore and Continue");
                    self().buttonWidget(Id2)->disable();
                }
                else
                {
                    if (list->itemCount() > 0)
                    {
                        self().message().setText(message + "\n\n" + unavailNote +
                                                 "All the mods listed below should be loaded.");
                        self().buttons()
                                << new DialogButtonItem(Default | Accept | Id1, defaultButtonLabel(),
                                                        new CallbackAction([this] () { resolvePackages(); }));
                    }
                    else
                    {
                        self().message().setText(message + "\n\n" + unavailNote +
                                                 "All additional mods should be unloaded.");
                        self().buttons()
                                   << new DialogButtonItem(Default | Accept | Id1, defaultButtonLabel(),
                                                           new CallbackAction([this] () { resolvePackages(); }));
                    }
                }
                self().buttons() << new DialogButtonItem(Reject, "Cancel");
            }
        }
        catch (const PackagesWidget::UnavailableError &er)
        {
            conflicted = true;
            self().message().setText(message + "\n\n" + er.asText());
            self().buttons() << new DialogButtonItem(Default | Reject);
        }

        self().updateLayout();
    }

    static bool containsIdentifier(const String &identifier, const StringList &ids)
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
            LOG_RES_NOTE("Ignoring package compatibility check due to user request!");
            self().accept();
            return;
        }

        LOG_RES_MSG("Resolving packages...");

        auto &pkgLoader = PackageLoader::get();

        // Unload excess packages.
        StringList loaded = DoomsdayApp::loadedPackagesAffectingGameplay();

        int goodUntil = -1;

        // Check if all the loaded packages match the wanted ones.
        for (int i = 0; i < loaded.sizei() && i < wanted.sizei(); ++i)
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

        LOG_RES_MSG("Good until %s") << goodUntil;

        // Unload excess.
        for (int i = loaded.sizei() - 1; i > goodUntil; --i)
        {
            LOG_RES_MSG("Unloading excess ") << loaded.at(i);

            pkgLoader.unload(loaded.at(i));
            loaded.removeAt(i);
        }

        // Load the remaining wanted packages.
        for (int i = goodUntil + 1; i < wanted.sizei(); ++i)
        {
            const auto &pkgId = wanted.at(i);

            if (pkgLoader.isAvailable(pkgId))
            {
                LOG_RES_MSG("Loading wanted ") << wanted.at(i);
                pkgLoader.load(wanted.at(i));
            }
            else
            {
                auto id_ver = Package::split(pkgId);
                LOG_RES_MSG("Loading latest version of wanted ") << id_ver.first;
                pkgLoader.load(id_ver.first);
            }
        }

        LOG_RES_MSG("Packages affecting gameplay:\n")
            << String::join(DoomsdayApp::loadedPackagesAffectingGameplay(), "\n");

        self().buttonsMenu().disable();
        updating->setOpacity(1, 0.3);

        self().accept();
    }
};

PackageCompatibilityDialog::PackageCompatibilityDialog(const String &name)
    : MessageDialog(name)
    , d(new Impl(this))
{
    title().setText("Incompatible Mods");
}

void PackageCompatibilityDialog::setMessage(const String &msg)
{
    d->message = msg;
}

void PackageCompatibilityDialog::setWantedPackages(const StringList& packages)
{
    DE_ASSERT(!packages.isEmpty());

    d->wanted = packages;
    d->update();
}

bool PackageCompatibilityDialog::isCompatible() const
{
    return !d->conflicted;
}

bool PackageCompatibilityDialog::handleEvent(const Event &event)
{
    // Hold Alt to skip the check.
    if (event.isKey())
    {
        const auto &key = event.as<KeyEvent>();
        if (key.ddKey() == DDKEY_LALT || key.ddKey() == DDKEY_RALT)
        {
            d->enableIgnore(key.type() != Event::KeyRelease);
        }
    }

    return MessageDialog::handleEvent(event);
}
