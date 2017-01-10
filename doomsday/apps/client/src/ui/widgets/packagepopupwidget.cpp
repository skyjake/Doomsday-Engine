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
#include <de/LabelWidget>
#include <de/DocumentWidget>
#include <de/PackageLoader>
#include <de/SequentialLayout>

using namespace de;

DENG2_PIMPL(PackagePopupWidget)
{
    LabelWidget *title;
    LabelWidget *path;
    DocumentWidget *description;
    LabelWidget *icon;
    LabelWidget *metaInfo;

    Impl(Public *i) : Base(i)
    {
        self().useInfoStyle();

        // The Close button is always available. Other actions are shown depending
        // on what kind of package is being displayed.
        self().buttons()
                << new DialogButtonItem(Default | Accept, tr("Close"));

        createWidgets();
    }

    void createWidgets()
    {
        auto &area = self().area();

        title = LabelWidget::newWithText("", &area);
        title->setFont("heading");
        title->setSizePolicy(ui::Filled, ui::Expand);
        title->setTextColor("inverted.text");

        path = LabelWidget::newWithText("", &area);
        path->setFont("small");
        path->setSizePolicy(ui::Filled, ui::Expand);
        path->setTextColor("inverted.text");

        description = new DocumentWidget;
        description->setWidthPolicy(ui::Fixed);
        description->rule().setInput(Rule::Height, Const(2*150));
        area.add(description);

        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top(),
                                ui::Down);
        layout.setOverrideWidth(Const(2*300));
        layout << *title
               << *path
               << *description;

        area.setContentSize(layout.width(), layout.height());
    }

    bool setup(File const *file)
    {
        /*enableCloseButton(true);
        document().setMaximumLineWidth(rule("home.popup.width").valuei());
        setPreferredHeight(rule("home.popup.height"));*/

        Record const &names = file->objectNamespace();
        if (!file || !names.has(Package::VAR_PACKAGE)) return false;

        Record const &meta = names.subrecord(Package::VAR_PACKAGE);

        title->setText(meta.gets(Package::VAR_TITLE));
        path->setText(String(_E(b) "%1" _E(.) "\n%2")
                      .arg("WAD")
                      .arg(file->source()->description()));

        String msg;/* = String(_E(1) "%1" _E(.) "\n%2\n"
                            _E(l) "Version: " _E(.) "%3\n"
                            _E(l) "License: " _E(.)_E(>) "%4" _E(<)
                            _E(l) "\nFile: " _E(.)_E(>)_E(C) "%5" _E(.)_E(<))
                    .arg(meta.gets(Package::VAR_TITLE))
                    .arg(meta.gets("ID"))
                    .arg(meta.gets("version"))
                    .arg(meta.gets("license"))
                    .arg(file->description());*/

        msg = "Description of the package.";

        if (meta.has("author"))
        {
            msg += "\n" _E(l) "Author(s): " _E(.)_E(>) + meta.gets("author") + _E(<);
        }

        if (meta.has("notes"))
        {
            msg += "\n\n" + meta.gets("notes") + "\n";
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

        description->setText(msg);
        //document().setText(msg);

        // Show applicable package actions:
        // - play in game (WADs, PK3s); does not add in the profile
        // - add to profile
        // - configure / select contents (in a collection)
        // - uninstall (user packages)

        return true;
    }
};

PackagePopupWidget::PackagePopupWidget(String const &packageId)
    : DialogWidget("packagepopup")
    , d(new Impl(this))
{
    if (!d->setup(App::packageLoader().select(packageId)))
    {
        //document().setText(packageId);
    }
}

PackagePopupWidget::PackagePopupWidget(File const *packageFile)
    : DialogWidget("packagepopup")
    , d(new Impl(this))
{
    if (!d->setup(packageFile))
    {
        //document().setText(tr("No package"));
    }
}
