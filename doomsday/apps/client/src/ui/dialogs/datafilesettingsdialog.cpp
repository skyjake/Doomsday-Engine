/** @file datafilesettings.cpp  Data file settings.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/dialogs/datafilesettingsdialog.h"
#include "ui/clientwindow.h"
#include "ui/widgets/consolewidget.h"
#include "ui/widgets/packageswidget.h"

#include <doomsday/DoomsdayApp>
#include <de/Config>

using namespace de;

DENG2_PIMPL_NOREF(DataFileSettingsDialog)
{
    Variable &iwadFolders = Config::get("resource.iwadFolder");
    Variable &pkgFolders  = Config::get("resource.packageFolder");
    Id iwadGroup;
    Id pkgGroup;
    bool modified = false;
};

DataFileSettingsDialog::DataFileSettingsDialog(String const &name)
    : DirectoryListDialog(name)
    , d(new Impl)
{
    buttons().clear()
            << new DialogButtonItem(Default | Accept, tr("Close"));

    title().setFont("heading");
    title().setText(tr("Data Files"));
    title().setStyleImage("package.icon", "heading");

    message().hide();

    //dlg->title().setFont("heading");
    //dlg->title().setStyleImage("package.icon");
    //dlg->title().setOverrideImageSize(Style::get().fonts().font("heading").ascent().value());
    //dlg->title().setTextGap("dialog.gap");
    //dlg->title().setText(QObject::tr("IWAD Folders"));
    //dlg->message().setText(QObject::tr("The following folders are searched for game data files:"));

    d->iwadGroup = addGroup(tr("IWAD Folders"),
                            tr("The following folders are searched for game IWAD files. "
                               "Only these folders are checked, not their subfolders."));
    setValue(d->iwadGroup, d->iwadFolders.value());

    d->pkgGroup = addGroup(tr("Mod / Add-on Folders"),
                           tr("The following folders and all their subfolders are searched "
                              "for mods, resource packs, and other add-ons."));
    setValue(d->pkgGroup, d->pkgFolders.value());

    connect(this, &DirectoryListDialog::arrayChanged, [this] ()
    {
        d->modified = true;
    });
}

void DataFileSettingsDialog::finish(int result)
{
    DirectoryListDialog::finish(result);

    if (d->modified)
    {
        d->iwadFolders.set(value(d->iwadGroup));
        d->pkgFolders .set(value(d->pkgGroup));

        // Reload packages and recheck for game availability.
        auto &win = ClientWindow::main();
        win.console().closeLogAndUnfocusCommandLine();
        win.root().find("home-packages")->as<PackagesWidget>().showProgressIndicator();
        DoomsdayApp::app().initWadFolders();
        DoomsdayApp::app().initPackageFolders();
    }
}
