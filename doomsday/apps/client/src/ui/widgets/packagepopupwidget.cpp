/** @file packagepopupwidget.cpp  Popup showing information about a package.
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

#include "ui/widgets/packagepopupwidget.h"

#include <de/App>
#include <de/PackageLoader>

using namespace de;

PackagePopupWidget::PackagePopupWidget(String const &packageId)
{
    if(!setup(App::packageLoader().select(packageId)))
    {
        document().setText(packageId);
    }
}

PackagePopupWidget::PackagePopupWidget(File const *packageFile)
{
    if(!setup(packageFile))
    {
        document().setText(tr("No package"));
    }
}

bool PackagePopupWidget::setup(File const *file)
{
    if(file && file->objectNamespace().has(Package::VAR_PACKAGE))
    {
        Record const &info = file->objectNamespace().subrecord(Package::VAR_PACKAGE);

        document().setText(QString(_E(1) "%1" _E(.) "\n%2\n"
                                   _E(l) "Version: " _E(.) "%3\n"
                                   _E(l) "License: " _E(.)_E(>) "%4" _E(<)
                                   _E(l) "\nFile: " _E(.)_E(>)_E(C) "%5")
                           .arg(info.gets("title"))
                           .arg(info.gets("ID"))
                           .arg(info.gets("version"))
                           .arg(info.gets("license"))
                           .arg(file->description()));
        return true;
    }
    return false;
}
