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
    if (!setup(App::packageLoader().select(packageId)))
    {
        document().setText(packageId);
    }
}

PackagePopupWidget::PackagePopupWidget(File const *packageFile)
{
    if (!setup(packageFile))
    {
        document().setText(tr("No package"));
    }
}

bool PackagePopupWidget::setup(File const *file)
{
    document().setMaximumLineWidth(rule("home.popup.width").valuei());
    setPreferredHeight(rule("home.popup.height"));

    Record const &names = file->objectNamespace();
    if (file && names.has(Package::VAR_PACKAGE))
    {
        Record const &meta = names.subrecord(Package::VAR_PACKAGE);

        String msg = String(_E(1) "%1" _E(.) "\n%2\n"
                            _E(l) "Version: " _E(.) "%3\n"
                            _E(l) "License: " _E(.)_E(>) "%4" _E(<)
                            _E(l) "\nFile: " _E(.)_E(>)_E(C) "%5" _E(.)_E(<))
                    .arg(meta.gets(Package::VAR_TITLE))
                    .arg(meta.gets("ID"))
                    .arg(meta.gets("version"))
                    .arg(meta.gets("license"))
                    .arg(file->description());

        if (meta.has("author"))
        {
            msg += "\n" _E(l) "Author(s): " _E(.)_E(>) + meta.gets("author") + _E(<);
        }

        if (meta.has("requires"))
        {
            msg += "\n" _E(l) "Requires:" _E(.);
            ArrayValue const &reqs = meta.geta("requires");
            for (Value const *val : reqs.elements())
            {
                msg += "\n - " _E(>) + val->asText() + _E(<);
            }
        }

        if (meta.has("dataFiles") && meta.geta("dataFiles").size() > 0)
        {
            msg += "\n" _E(l) "Data files:" _E(.);
            ArrayValue const &files = meta.geta("dataFiles");
            for (Value const *val : files.elements())
            {
                msg += "\n - " _E(>) + val->asText() + _E(<);
            }
        }

        if (meta.has("notes"))
        {
            msg += "\n\n" + meta.gets("notes");
        }

        document().setText(msg);
        return true;
    }
    return false;
}
