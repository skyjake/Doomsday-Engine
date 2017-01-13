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
#include "ui/widgets/packagecontentoptionswidget.h"

#include <doomsday/DataBundle>
#include <doomsday/DoomsdayApp>
#include <doomsday/Games>

#include <de/App>
#include <de/ArchiveEntryFile>
#include <de/CallbackAction>
#include <de/DirectoryFeed>
#include <de/DocumentWidget>
#include <de/Folder>
#include <de/ImageFile>
#include <de/LabelWidget>
#include <de/NativeFile>
#include <de/PackageLoader>
#include <de/PopupMenuWidget>
#include <de/SequentialLayout>
#include <de/SignalAction>

#include <QDesktopServices>

using namespace de;

DENG_GUI_PIMPL(PackageInfoDialog)
{
    LabelWidget *title;
    LabelWidget *path;
    DocumentWidget *description;
    LabelWidget *icon;
    LabelWidget *metaInfo;
    IndirectRule *targetHeight;
    String packageId;
    NativePath nativePath;
    SafeWidgetPtr<PopupWidget> configurePopup;
    SafeWidgetPtr<PopupMenuWidget> profileMenu;
    enum MenuMode { AddToProfile, PlayInProfile };
    MenuMode menuMode;

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
        title->setFont("title");
        title->setSizePolicy(ui::Filled, ui::Expand);
        title->setTextColor("inverted.accent");
        title->setTextLineAlignment(ui::AlignLeft);
        title->margins().setBottom("");

        path = LabelWidget::newWithText("", &area);
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
        //icon->setSizePolicy(ui::Filled, ui::Filled);
        //icon->setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
        //icon->setStyleImage("package.large");
        //icon->setImageColor(style().colors().colorf("inverted.accent"));
        icon->rule().setInput(Rule::Height, Const(2*170));

        metaInfo = LabelWidget::newWithText("", &area);
        metaInfo->setSizePolicy(ui::Filled, ui::Expand);
        metaInfo->setTextLineAlignment(ui::AlignLeft);
        metaInfo->setFont("small");
        metaInfo->setTextColor("inverted.accent");

        SequentialLayout rightLayout(title->rule().right(), title->rule().top(), ui::Down);
        rightLayout.setOverrideWidth(Const(2*200));
        rightLayout << *icon
                    << *metaInfo;

        targetHeight->setSource(rightLayout.height());

        area.setContentSize(layout.width() + rightLayout.width(),
                            *targetHeight);
    }

    void useDefaultIcon()
    {
        icon->setStyleImage("package.large");
        icon->setImageColor(style().colors().colorf("inverted.accent"));
        icon->setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
        icon->setImageScale(.75f);
        icon->setOpacity(.5f);
        icon->setBehavior(ContentClipping, false);
    }

    void useIconFile(String const &packagePath)
    {
        try
        {
            foreach (String ext, StringList({ ".jpg", ".jpeg", ".png" }))
            {
                String const imgPath = packagePath / "icon" + ext;
                if (ImageFile const *img = FS::get().root().tryLocate<ImageFile const>(imgPath))
                {
                    Image iconImage = img->image();
                    if (iconImage.width() > 512 || iconImage.height() > 512)
                    {
                        throw Error("PackageInfoDialog::useIconFile",
                                    "Icon file " + img->description() + " is too large (max 512x512)");
                    }
                    icon->setImage(iconImage);
                    icon->setImageColor(Vector4f(1, 1, 1, 1));
                    icon->setImageFit(ui::FitToHeight | ui::OriginalAspectRatio);
                    icon->setImageScale(1);
                    icon->setBehavior(ContentClipping, true);
                    return;
                }
            }
        }
        catch (Error const &er)
        {
            LOG_RES_WARNING("Failed to use package icon image: %s") << er.asText();
        }
        useDefaultIcon();
    }

    bool setup(File const *file)
    {
        if (!file) return false; // Not a package?

        // Look up the package metadata.
        Record const &names = file->objectNamespace();
        if (!names.has(Package::VAR_PACKAGE))
        {
            return false;
        }
        Record const &meta = names.subrecord(Package::VAR_PACKAGE);

        packageId = meta.gets(Package::VAR_ID);
        nativePath = file->correspondingNativePath();
        String fileDesc = file->source()->description();

        String format;
        if (DataBundle const *bundle = file->target().maybeAs<DataBundle>())
        {
            format = bundle->formatAsText().upperFirstChar();
            useDefaultIcon();

            if (bundle->format() == DataBundle::Collection)
            {
                fileDesc = file->target().description();
            }
        }
        else
        {
            format = tr("Doomsday 2 Package");
            useIconFile(file->path());
        }

        if (file->source()->is<ArchiveEntryFile>())
        {
            // The file itself makes for a better description.
            fileDesc = file->description();
        }

        title->setText(meta.gets(Package::VAR_TITLE));
        path->setText(String(_E(b) "%1" _E(.) "\n%2")
                      .arg(format)
                      .arg(fileDesc.upperFirstChar()));

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
                << new DialogButtonItem(Action | Id2,
                                        style().images().image("play"),
                                        tr("Play in..."),
                                        new SignalAction(thisPublic, SLOT(playInGame())))
                << new DialogButtonItem(Action | Id3,
                                        style().images().image("create"),
                                        tr("Add to..."),
                                        new SignalAction(thisPublic, SLOT(addToProfile())));

        if (!nativePath.isEmpty())
        {
            self().buttons()
                    << new DialogButtonItem(Action,
                                            tr("Show File"),
                                            new SignalAction(thisPublic, SLOT(showFile())));
        }
        if (Package::hasOptionalContent(*file))
        {
            self().buttons()
                    << new DialogButtonItem(Action | Id1,
                                            style().images().image("gear"),
                                            tr("Options"),
                                            new SignalAction(thisPublic, SLOT(configure())));
        }
        return true;
    }

    static String visibleFamily(String const &family)
    {
        if (family.isEmpty()) return tr("Other");
        return family.upperFirstChar();
    }

    void openProfileMenu(RuleRectangle const &anchor, bool playableOnly)
    {
        if (profileMenu) return;

        profileMenu.reset(new PopupMenuWidget);
        profileMenu->setDeleteAfterDismissed(true);
        profileMenu->setAnchorAndOpeningDirection(anchor, ui::Left);

        QList<GameProfile *> profs = DoomsdayApp::gameProfiles().profilesSortedByFamily();

        String lastFamily;
        for (GameProfile *prof : profs)
        {
            if (playableOnly && !prof->isPlayable()) continue;

            if (lastFamily != prof->game().family())
            {
                if (!profileMenu->items().isEmpty())
                {
                    profileMenu->items() << new ui::Item(ui::Item::Separator);
                }
                profileMenu->items()
                        << new ui::Item(ui::Item::ShownAsLabel | ui::Item::Separator,
                                        visibleFamily(prof->game().family()));
                lastFamily = prof->game().family();
            }

            profileMenu->items()
                    << new ui::ActionItem(prof->name(), new CallbackAction([this, prof] ()
            {
                profileSelectedFromMenu(*prof);
            }));
        }

        self().add(profileMenu);
        profileMenu->open();
    }

    void profileSelectedFromMenu(GameProfile &profile)
    {
        switch (menuMode)
        {
        case AddToProfile:
            StringList pkgs = profile.packages();
            if (!pkgs.contains(packageId))
            {
                pkgs << packageId;
                profile.setPackages(pkgs);
            }
            break;
        }
    }
};

PackageInfoDialog::PackageInfoDialog(String const &packageId)
    : DialogWidget("packagepopup")
    , d(new Impl(this))
{
    if (!d->setup(App::packageLoader().select(packageId)))
    {
        //document().setText(packageId);
    }
}

PackageInfoDialog::PackageInfoDialog(File const *packageFile)
    : DialogWidget("packagepopup")
    , d(new Impl(this))
{
    if (!d->setup(packageFile))
    {
        //document().setText(tr("No package"));
    }
}

void PackageInfoDialog::playInGame()
{
    d->menuMode = Impl::PlayInProfile;
    d->openProfileMenu(buttonWidget(Id2)->rule(), true);
}

void PackageInfoDialog::addToProfile()
{
    d->menuMode = Impl::AddToProfile;
    d->openProfileMenu(buttonWidget(Id3)->rule(), false);
}

void PackageInfoDialog::configure()
{
    if (d->configurePopup) return; // Let it close itself.

    PopupWidget *pop = PackageContentOptionsWidget::makePopup(
                d->packageId, rule("dialog.packages.width"), root().viewHeight());
    d->configurePopup.reset(pop);
    pop->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Left);
    pop->closeButton().setActionFn([pop] ()
    {
        pop->close();
    });

    add(pop);
    pop->open();
}

void PackageInfoDialog::showFile()
{
    if (!d->nativePath.isEmpty())
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
                d->nativePath.isDirectory()? d->nativePath : d->nativePath.fileNamePath()));
    }
}
