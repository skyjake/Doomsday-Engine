#include <utility>

/** @file packagesdialog.cpp
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/dialogs/packagesdialog.h"
#include "ui/widgets/packageswidget.h"
#include "ui/widgets/homeitemwidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/dialogs/packageinfodialog.h"
#include "ui/dialogs/datafilesettingsdialog.h"
#include "ui/clientwindow.h"
#include "ui/clientstyle.h"
#include "clientapp.h"

#include <doomsday/games.h>
#include <doomsday/res/lumpcatalog.h>
#include <doomsday/res/bundles.h>

#include <de/charsymbols.h>
#include <de/callbackaction.h>
#include <de/childwidgetorganizer.h>
#include <de/filesystem.h>
#include <de/documentpopupwidget.h>
#include <de/menuwidget.h>
#include <de/nativefile.h>
#include <de/packageloader.h>
#include <de/popupbuttonwidget.h>
#include <de/popupmenuwidget.h>
#include <de/sequentiallayout.h>
#include <de/textvalue.h>
#include <de/ui/subwidgetitem.h>
#include <de/ui/variantactionitem.h>

using namespace de;

DE_GUI_PIMPL(PackagesDialog)
, public ChildWidgetOrganizer::IWidgetFactory
, public PackagesWidget::IPackageStatus
, DE_OBSERVES(Widget, ChildAddition)
, DE_OBSERVES(HomeMenuWidget, Click)
{
    StringList selectedPackages;
    LabelWidget *nothingSelected;
    ui::ListData actions;
    HomeMenuWidget *menu;
    PackagesWidget *browser;
    LabelWidget *gameTitle;
    LabelWidget *gameDataFiles;
    const GameProfile *gameProfile = nullptr;
    res::LumpCatalog catalog;

    /**
     * Information about a selected package. If the package file is not found, only
     * the ID is known.
     */
    class SelectedPackageItem
        : public ui::Item
        , DE_OBSERVES(res::Bundles, Identify)
    {
    public:
        SelectedPackageItem(const String &packageId)
        {
            setData(TextValue(packageId));
            updatePackageInfo();
            DoomsdayApp::bundles().audienceForIdentify() += this;
        }

        void updatePackageInfo()
        {
            if ((_file = App::packageLoader().select(packageId())) != nullptr)
            {
                _info = &_file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            }
            else
            {
                _info = nullptr;
            }
        }

        String packageId() const
        {
            return data().asText();
        }

        const Record *info() const
        {
            return _info;
        }

        const File *packageFile() const
        {
            return _file;
        }

        void dataBundlesIdentified() override
        {
            Loop::mainCall([this]() {
                updatePackageInfo();
                notifyChange();
            });
        }

    private:
        const File *_file = nullptr;
        const Record *_info = nullptr;
    };

    /**
     * Widget showing information about a selected package, with a button for dragging
     * the item up and down.
     */
    class SelectedPackageWidget : public HomeItemWidget
    {
    public:
        SelectedPackageWidget(const SelectedPackageItem &item,
                              PackagesDialog &owner)
            : _owner(owner)
            , _item(&item)
        {
            useColorTheme(Normal, Normal);

            _removeButton = new ButtonWidget;
            _removeButton->setStyleImage("close.ringless", "small");
            _removeButton->margins().setTopBottom(RuleBank::UNIT);
            _removeButton->setActionFn([this] ()
            {
                PackagesDialog::Impl *d = _owner.d;
                d->removePackage(packageId());
                d->browser->updateItems();
            });
            addButton(_removeButton);
            setKeepButtonsVisible(true);

            // Package icon.
            icon().set(Background());
            icon().setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
            icon().setStyleImage("package.icon", "default");
            icon().margins().set("dialog.gap");
            const Rule &height = style().fonts().font("default").height();
            icon().rule().setInput(Rule::Width, height + rule("dialog.gap")*2);
        }

        void updateContents()
        {
            if (_item->info())
            {
                label().setText(_item->info()->gets("title"));
            }
            else
            {
                label().setText(Package::splitToHumanReadable(_item->packageId()) +
                                " " _E(D) DE_CHAR_MDASH " Missing");
            }
        }

        String packageId() const
        {
            return _item->packageId();
        }

        PopupWidget *makeInfoPopup() const
        {
            return new PackageInfoDialog(_item->packageFile(), PackageInfoDialog::EnableActions);
        }

    private:
        PackagesDialog &_owner;
        const SelectedPackageItem *_item;
        ButtonWidget *_removeButton;
    };

    Impl(Public *i) : Base(i)
    {
        self().leftArea().add(gameTitle = new LabelWidget);
        gameTitle->add(gameDataFiles = new LabelWidget);

        // Indicator that is only visible when no packages have been added to the profile.
        nothingSelected = new LabelWidget;

        nothingSelected->setText("No Mods Selected");
        style().emptyContentLabelStylist().applyStyle(*nothingSelected);
        nothingSelected->rule()
                .setRect(self().leftArea().rule())
                .setInput(Rule::Top, gameTitle->rule().bottom());
        self().leftArea().add(nothingSelected);

        // Currently selected packages.
        gameTitle->setSizePolicy(ui::Filled, ui::Expand);
        gameTitle->setImageFit(ui::FitToWidth | ui::OriginalAspectRatio | ui::CoverArea);
        gameTitle->margins().setZero();
        gameDataFiles->setFont("small");
        gameDataFiles->setSizePolicy(ui::Fixed, ui::Expand);
        gameDataFiles->set(Background(style().colors().colorf("background")));
        gameDataFiles->setTextLineAlignment(ui::AlignLeft);
        gameDataFiles->setAlignment(ui::AlignLeft);
        gameTitle->rule()
                .setInput(Rule::Left,  self().leftArea().contentRule().left())
                .setInput(Rule::Top,   self().leftArea().contentRule().top())
                .setInput(Rule::Width, rule("dialog.packages.left.width"));
        gameDataFiles->rule()
                .setRect(gameTitle->rule())
                .clearInput(Rule::Top);
        self().leftArea().add(menu = new HomeMenuWidget);
        menu->layout().setRowPadding(Const(0));
        menu->rule()
                .setInput(Rule::Left,  self().leftArea().contentRule().left())
                .setInput(Rule::Top,   gameTitle->rule().bottom())
                .setInput(Rule::Width, rule("dialog.packages.left.width"));
        menu->organizer().setWidgetFactory(*this);
        menu->audienceForChildAddition() += this;
        self().leftArea().enableIndicatorDraw(true);

        menu->audienceForClick() += this;

        // Package browser.
        self().rightArea().add(browser = new PackagesWidget(PackagesWidget::PopulationDisabled,
                                                          self().name() + ".filter"));
        browser->setActionsAlwaysShown(true);
        browser->setRightClickToOpenContextMenu(true);
        browser->setPackageStatus(*this);

        // Action for showing information about the package.
        actions << new ui::SubwidgetItem("...", ui::Up, [this] () -> PopupWidget *
        {
            const String id = browser->actionPackage();
            return new PackageInfoDialog(id, PackageInfoDialog::EnableActions);
        });

        // Action for (de)selecting the package.
        actions << new ui::VariantActionItem("create",
                                             "close.ringless",
                                             String(),
                                             String(),
                                             new CallbackAction([this] ()
        {
            const String packageId = browser->actionPackage();
            if (!selectedPackages.contains(packageId))
            {
                selectedPackages.append(packageId);
                menu->items() << new SelectedPackageItem(packageId);
                updateNothingIndicator();
                updateGameTitle();
            }
            else
            {
                removePackage(packageId);
            }
            browser->actionItem()->notifyChange();
        }));
        browser->setActionItems(actions);

        //browser->setColorTheme(Normal, Normal, Normal, Normal);
        browser->rule()
                .setInput(Rule::Left,  self().rightArea().contentRule().left())
                .setInput(Rule::Top,   self().rightArea().contentRule().top())
                .setInput(Rule::Width, rule("dialog.packages.right.width"));
        self().rightArea().enableIndicatorDraw(true);
        browser->setFilterEditorMinimumY(self().rightArea().rule().top());
    }

    void menuItemClicked(HomeMenuWidget &, ui::DataPos index) override
    {
        if (index != ui::Data::InvalidPos)
        {
            browser->scrollToPackage(menu->items().at(index).as<SelectedPackageItem>().packageId());
        }
    }

    void populate()
    {
        menu->items().clear();

        // Remove from the list those packages that are no longer listed.
        for (const String &packageId : selectedPackages)
        {
            menu->items() << new SelectedPackageItem(packageId);
        }

        updateNothingIndicator();
    }

    void updateNothingIndicator()
    {
        nothingSelected->setOpacity(menu->items().isEmpty()? .5f : 0.f, 0.4);
    }

    void updateGameTitle()
    {
        if (gameProfile && catalog.setPackages(gameProfile->allRequiredPackages() + selectedPackages))
        {
            gameTitle->setImage(ClientStyle::makeGameLogo(gameProfile->game(), catalog,
                                                           ClientStyle::UnmodifiedAppearance |
                                                           ClientStyle::AlwaysTryLoad));
            // List of the native required files.
            StringList dataFiles;
//            if (gameProfile->customDataFile())
//            {
//                if (const auto *file = PackageLoader::get().select(gameProfile->customDataFile()))
//                {
//                    dataFiles << file->source()->description(0);
//                }
//            }
//            else
//            {
                for (String packageId : gameProfile->allRequiredPackages())
                {
                    if (const File *file = PackageLoader::get().select(packageId))
                    {
                        // Only list here the game data files; Doomsday's PK3s are always
                        // there so listing them is not very helpful.
                    if (Package::matchTags(*file, DE_STR("\\bgamedata\\b")))
                        {
                            // Resolve indirection (symbolic links and interpretations) to
                            // describe the actual source file of the package.
                            dataFiles << file->source()->description(0);
                        }
                    }
                }
//            }
            if (!dataFiles.isEmpty())
            {
                gameDataFiles->setText(_E(l) + Stringf("Game data file%s: ", dataFiles.size() != 1? "s" : "") +
                                       _E(.) + String::join(dataFiles, _E(l) " and " _E(.)));
            }
            else
            {
                gameDataFiles->setText(_E(D) "Locate data file in Data Files settings");
            }
        }
    }

    GuiWidget *makeItemWidget(const ui::Item &item, const GuiWidget *) override
    {
        return new SelectedPackageWidget(item.as<SelectedPackageItem>(), self());
    }

    void updateItemWidget(GuiWidget &widget, const ui::Item &) override
    {
        widget.as<SelectedPackageWidget>().updateContents();
    }

    bool isPackageHighlighted(const String &packageId) const override
    {
        return selectedPackages.contains(packageId);
    }

    void removePackage(const String &packageId)
    {
        selectedPackages.removeOne(packageId);
        auto pos = menu->items().findData(TextValue(packageId));
        DE_ASSERT(pos != ui::Data::InvalidPos);
        menu->items().remove(pos);
        updateNothingIndicator();
        updateGameTitle();
    }

    void widgetChildAdded(Widget &child) override
    {
        ui::DataPos pos = menu->findItem(child.as<GuiWidget>());
        // We use a delay here because ScrollAreaWidget does scrolling based on
        // the current geometry of the widget and HomeItemWidget uses an animation
        // for its height.
        Loop::timer(0.3, [this, pos] ()
        {
            menu->setSelectedIndex(pos);
        });
    }
};

PackagesDialog::PackagesDialog(const String &titleText)
    : DialogWidget("packagesdialog", WithHeading)
    , d(new Impl(this))
{
    if (titleText.isEmpty())
    {
        heading().setText("Mods");
    }
    else
    {
        heading().setText(titleText);
    }
    heading().setStyleImage("package.icon");
    buttons()
            << new DialogButtonItem(Default | Accept, "OK")
            << new DialogButtonItem(Reject, "Cancel")
            << new DialogButtonItem(Action, style().images().image("refresh"),
                                    []() { FS::get().refreshAsync(); })
            << new DialogButtonItem(Action | Id1, style().images().image("gear"),
                                    "Data Files",
                                    new CallbackAction([this]() {
        // Open a Data Files dialog.
        auto *dfsDlg = new DataFileSettingsDialog;
        dfsDlg->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Up);
        dfsDlg->setDeleteAfterDismissed(true);
        add(dfsDlg);
        dfsDlg->open();
    }));

    // The individual menus will be scrolling independently.
    leftArea() .setContentSize(d->menu->rule().width(),
                               OperatorRule::maximum(d->menu->rule().height(),
                                                     d->nothingSelected->rule().height(),
                                                     rule("dialog.packages.left.minheight")) +
                               d->gameTitle->rule().height());
    rightArea().setContentSize(d->browser->rule());
    d->browser->progress().rule().setRect(rightArea().rule());

    setMaximumContentHeight(rule().width() * 0.9f);

    // Setup has been completed, so contents can be updated.
    d->browser->setPopulationEnabled(true);
}

void PackagesDialog::setProfile(const GameProfile &profile)
{
    d->gameProfile = &profile;
    //d->requiredPackages = d->game->requiredPackages();
    d->updateGameTitle();
}

void PackagesDialog::setSelectedPackages(StringList packages)
{
    d->selectedPackages = std::move(packages);
    d->browser->populate();
    d->updateGameTitle();
}

StringList PackagesDialog::selectedPackages() const
{
    return d->selectedPackages;
}

//void PackagesDialog::refreshPackages()
//{
//    d->browser->refreshPackages();
//}

void PackagesDialog::preparePanelForOpening()
{
    DialogWidget::preparePanelForOpening();
    d->populate();

    root().setFocus(&d->browser->searchTermsEditor());
}
