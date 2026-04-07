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
#include "ui/widgets/taskbarwidget.h"

#include <doomsday/doomsdayapp.h>
#include <de/config.h>

using namespace de;

DE_PIMPL_NOREF(DataFileSettingsDialog)
{
    Variable &pkgFolders  = Config::get("resource.packageFolder");
    Id searchGroup;
    bool modified = false;
};

DataFileSettingsDialog::DataFileSettingsDialog(const String &name)
    : DirectoryListDialog(name)
    , d(new Impl)
{
    buttons().remove(1); // remove the Cancel
    buttons().at(0).setLabel("Apply");

    title().setFont("heading");
    title().setText("Data Files");
    title().setStyleImage("package.icon", "heading");

    message().hide();

    d->searchGroup = addGroup(
        "Search Folders",
        "The following folders are searched for game IWAD files and mods like PWADs, PK3s, and "
        "Doomsday packages. Toggle the " _E(b) "Subdirs" _E(.)
        " option to include all subfolders as well.");
    setValue(d->searchGroup, d->pkgFolders.value());

    updateLayout();
    audienceForChange() += [this]() { d->modified = true; };
}

void DataFileSettingsDialog::finish(int result)
{
    DirectoryListDialog::finish(result);

    if (d->modified)
    {
        d->pkgFolders.set(value(d->searchGroup));

        // Reload packages and recheck for game availability.
        ClientWindow::main().taskBar().close();
        DoomsdayApp::app().initPackageFolders();
    }
}
