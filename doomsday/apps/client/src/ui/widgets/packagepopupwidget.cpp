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

#include <doomsday/DataBundle>

#include <de/App>
#include <de/LabelWidget>
#include <de/DocumentWidget>
#include <de/PackageLoader>
#include <de/SequentialLayout>
#include <de/SignalAction>

using namespace de;

DENG_GUI_PIMPL(PackagePopupWidget)
{
    LabelWidget *title;
    LabelWidget *path;
    DocumentWidget *description;
    LabelWidget *icon;
    LabelWidget *metaInfo;
    IndirectRule *targetHeight;

    Impl(Public *i) : Base(i)
    {
        targetHeight = new IndirectRule;

        self().useInfoStyle();

        // The Close button is always available. Other actions are shown depending
        // on what kind of package is being displayed.
        self().buttons()
                << new DialogButtonItem(Default | Accept, tr("Close"));

        createWidgets();
    }

    ~Impl()
    {
        releaseRef(targetHeight);
    }

    void createWidgets()
    {
        auto &area = self().area();

        // Left column.
        title = LabelWidget::newWithText("", &area);
        title->setFont("heading");
        title->setSizePolicy(ui::Filled, ui::Expand);
        title->setTextColor("inverted.text");
        title->setTextLineAlignment(ui::AlignLeft);
        title->margins().setBottom("");

        path = LabelWidget::newWithText("", &area);
        path->setFont("small");
        path->setSizePolicy(ui::Filled, ui::Expand);
        path->setTextColor("inverted.text");
        path->setTextLineAlignment(ui::AlignLeft);
        path->margins().setTop("unit");

        description = new DocumentWidget;
        description->setFont("small");
        description->setWidthPolicy(ui::Fixed);
        description->rule().setInput(Rule::Height, *targetHeight - title->rule().height()
                                     - path->rule().height());
        area.add(description);

        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top(),
                                ui::Down);
        layout.setOverrideWidth(Const(2*400));
        layout << *title
               << *path
               << *description;

        // Right column.
        icon = LabelWidget::newWithText("", &area);
        icon->setSizePolicy(ui::Filled, ui::Filled);
        icon->setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
        icon->setStyleImage("package");
        icon->setImageColor(style().colors().colorf("inverted.text"));
        icon->rule().setInput(Rule::Height, Const(2*200));

        metaInfo = LabelWidget::newWithText("", &area);
        metaInfo->setSizePolicy(ui::Filled, ui::Expand);
        metaInfo->setTextLineAlignment(ui::AlignLeft);
        metaInfo->setFont("small");
        metaInfo->setTextColor("inverted.text");

        SequentialLayout rightLayout(title->rule().right(), title->rule().top(), ui::Down);
        rightLayout.setOverrideWidth(Const(2*200));
        rightLayout << *icon
                    << *metaInfo;

        targetHeight->setSource(rightLayout.height());

        area.setContentSize(layout.width() + rightLayout.width(),
                            *targetHeight);
    }

    bool setup(File const *file)
    {
        /*enableCloseButton(true);
        document().setMaximumLineWidth(rule("home.popup.width").valuei());
        setPreferredHeight(rule("home.popup.height"));*/

        Record const &names = file->objectNamespace();
        if (!file || !names.has(Package::VAR_PACKAGE)) return false;

        Record const &meta = names.subrecord(Package::VAR_PACKAGE);

        String format;
        if (DataBundle const *bundle = file->target().maybeAs<DataBundle>())
        {
            format = bundle->formatAsText().upperFirstChar();
        }
        else
        {
            format = tr("Doomsday 2 Package");
        }

        title->setText(meta.gets(Package::VAR_TITLE));
        path->setText(String(_E(b) "%1" _E(.) "\n%2")
                      .arg(format)
                      .arg(file->source()->description().upperFirstChar()));

        String metaMsg = String(_E(Ta)_E(l) "Version: " _E(.)_E(Tb) "%1\n"
                                _E(Ta)_E(l) "Tags: "    _E(.)_E(Tb) "%2\n"
                                _E(Ta)_E(l) "License: " _E(.)_E(Tb) "%3")
                    .arg(meta.gets("version"))
                    .arg(meta.gets("tags"))
                    .arg(meta.gets("license"));
        if (meta.has("author"))
        {
            metaMsg += String("\n" _E(Ta)_E(l) "Author: "
                              _E(.)_E(Tb) "%1").arg(meta.gets("author"));

        }
        if (meta.has("contact"))
        {
            metaMsg += String("\n" _E(Ta)_E(l) "Contact: "
                              _E(.)_E(Tb) "%1").arg(meta.gets("contact"));
        }
        metaInfo->setText(metaMsg);

        // Description text.
        String msg = "Description of the package.";

        if (meta.has("notes"))
        {
            msg += "\n\n" + meta.gets("notes") + _E(r) "\n";
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

        self().buttons()
                << new DialogButtonItem(Action,
                                        style().images().image("play"),
                                        tr("Play in..."),
                                        new SignalAction(thisPublic, SLOT(playInGame())))
                << new DialogButtonItem(Action,
                                        style().images().image("create"),
                                        tr("Add to..."),
                                        new SignalAction(thisPublic, SLOT(addToProfile())))
                << new DialogButtonItem(Action,
                                        tr("Show File"),
                                        new SignalAction(thisPublic, SLOT(uninstall())))
                << new DialogButtonItem(Action,
                                        style().images().image("gear"),
                                        tr("Options"),
                                        new SignalAction(thisPublic, SLOT(configure())));

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

void PackagePopupWidget::playInGame()
{

}

void PackagePopupWidget::addToProfile()
{

}

void PackagePopupWidget::configure()
{

}

void PackagePopupWidget::uninstall()
{

}
