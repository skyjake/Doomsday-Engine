/** @file packageswidget.cpp  Widget for searching and examining packages.
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

#include "ui/widgets/packageswidget.h"
#include "ui/widgets/homeitemwidget.h"
#include "ui/widgets/homemenuwidget.h"
#include "ui/widgets/panelbuttonwidget.h"
#include "ui/dialogs/packageinfodialog.h"
#include "ui/widgets/packagecontentoptionswidget.h"
#include "ui/clientrootwidget.h"
#include "clientapp.h"

#include <de/AtlasProceduralImage>
#include <de/CallbackAction>
#include <de/ChildWidgetOrganizer>
#include <de/DocumentPopupWidget>
#include <de/FileSystem>
#include <de/LineEditWidget>
#include <de/Loop>
#include <de/MenuWidget>
#include <de/NativeFile>
#include <de/PackageIconBank>
#include <de/PackageLoader>
#include <de/PopupButtonWidget>
#include <de/ProgressWidget>
#include <de/RegExp>
#include <de/SequentialLayout>
#include <de/Timer>
#include <de/TextValue>
#include <de/ui/FilteredData>
#include <de/ui/VariantActionItem>

#include <doomsday/DoomsdayApp>
#include <doomsday/resource/bundles.h>

using namespace de;

static const char *VAR_TITLE  = "title";
static const char *VAR_TAGS   = "tags";
static const char *TAG_LOADED = "loaded";
//static const char *TAG_HIDDEN = "hidden";
//static const char *TAG_CACHED = "cached";

static constexpr TimeSpan REFILTER_DELAY = 200_ms;

struct PackageLoadStatus : public PackagesWidget::IPackageStatus {
    bool isPackageHighlighted(const String &packageId) const
    {
        return App::packageLoader().isLoaded(packageId);
    }
};
static PackageLoadStatus isPackageLoaded;

PackagesWidget::IPackageStatus::~IPackageStatus() {}

DE_GUI_PIMPL(PackagesWidget)
, DE_OBSERVES(FileSystem, Busy)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Information about an available package.
     */
    struct PackageItem
        : public ui::Item
        , DE_OBSERVES(File, Deletion) {
        SafePtr<const File> file;
        const Record *      info = nullptr;
        NativePath          nativePath;

        PackageItem(const File &packFile)
        {
            setFile(packFile);
            setData(TextValue(Package::versionedIdentifierForFile(packFile)));
            setLabel(info->gets(Package::VAR_TITLE));
        }

        void setFile(const File &packFile)
        {
            if (file)
            {
                file->audienceForDeletion() -= this;
            }
            DE_GUARD(packFile);
            packFile.audienceForDeletion() += this;
            file.reset(&packFile);
            if (const auto *nat = maybeAs<NativeFile>(packFile.source()))
            {
                nativePath = nat->nativePath();
            }
            else
            {
                nativePath.clear();
            }
            info = &file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            setData(TextValue(Package::versionedIdentifierForFile(packFile)));
            setLabel(info->gets(Package::VAR_TITLE));
            notifyChange();
        }

        void fileBeingDeleted(const File &)
        {
            info = nullptr;
            nativePath.clear();
        }

        bool isLoaded() const
        {
            DE_GUARD(file);
            if (!file) return false;
            return PackageLoader::get().isLoaded(*file);
        }
    };

    //---------------------------------------------------------------------------------------

    /**
     * Widget showing information about a package and containing buttons for manipulating
     * the package.
     */
    class PackageListItemWidget
        : public HomeItemWidget
        , DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation) // actions
        , DE_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)   // actions
        , DE_OBSERVES(Bank, Load)                           // package icons
    {
    public:
        PackageListItemWidget(const PackageItem &item, PackagesWidget &owner)
            : HomeItemWidget(NonAnimatedHeight) // virtualized, so don't make things difficult
            , _owner(owner)
            , _item(&item)
        {
            icon().setImageFit(ui::FitToWidth | ui::OriginalAspectRatio);
            icon().margins().set("gap").setRight("");
            icon().rule().setInput(Rule::Width, style().fonts().font("default").height() * 3);

            _actions = new MenuWidget;
            _actions->enablePageKeys(false);
            _actions->enableScrolling(false);
            _actions->margins().setZero();
            _actions->organizer().audienceForWidgetCreation() += this;
            _actions->organizer().audienceForWidgetUpdate() += this;
            _actions->setGridSize(0, ui::Expand, 1, ui::Expand);
            _actions->setItems(*owner.d->actionItems);
            audienceForDoubleClick() += [this](){ triggerAction(); };
            addButton(_actions);
            setKeepButtonsVisible(!_owner.d->actionOnlyForSelection);

            createTagButtons();
            fetchIcon();
        }

        void setItem(const PackageItem &item)
        {
            if (_item != &item)
            {
                iconBank().audienceForLoad() -= this;
                setSelected(false);
                _packagePath.clear();
                _item = &item;
                destroyTagButtons();
                createTagButtons();
                fetchIcon();
            }
        }

        void createTagButtons()
        {
            if (!_item->file) return;

            SequentialLayout layout(label().rule().left() + label().margins().left(),
                                    label().rule().bottom() - label().margins().bottom(),
                                    ui::Right);

            for (const String &tag : Package::tags(*_item->file))
            {
                auto *btn = new ButtonWidget;
                btn->setText(_E(l) + tag);
                btn->setActionFn([this, tag]() {
                    String terms = _owner.d->search->text();
                    if (!terms.isEmpty() && !terms.last().isSpace()) terms += " ";
                    terms += tag;
                    _owner.d->search->setText(terms);
                });
                updateTagButtonStyle(btn, "accent");
                btn->setSizePolicy(ui::Expand, ui::Expand);
                btn->margins()
                    .setTop(RuleBank::UNIT)
                    .setBottom(RuleBank::UNIT)
                    .setLeft("gap")
                    .setRight("gap");
                add(btn);

                layout << *btn;
                _tags.append(btn);
            }

            if (!_tags.isEmpty())
            {
                //rule().setInput(Rule::Height, label().rule().height());
                label().margins().setBottom(rule(RuleBank::UNIT) * 2 +
                                            style().fonts().font("small").height() + rule("gap"));
            }
            else
            {
                label().margins().setBottom(rule("gap"));
            }
        }

        void destroyTagButtons()
        {
            for (ButtonWidget *button : _tags)
            {
                GuiWidget::destroy(button);
            }
            _tags.clear();
        }

        void updateTagButtonStyle(ButtonWidget *tag, const String &color)
        {
            tag->setFont("small");
            tag->setTextColor(color);
            tag->set(Background(Background::Rounded, style().colors().colorf(color), 6));
        }

        PackageIconBank &iconBank()
        {
            return _owner.root().as<ClientRootWidget>().packageIconBank();
        }

        void fetchIcon()
        {
            _iconId = Id::None;

            if (_item && _item->file && iconBank().packageContainsIcon(*_item->file))
            {
                _packagePath = _item->file->path();
                if (Id pkgIcon = iconBank().packageIcon(*_item->file))
                {
                    setPackageIcon(pkgIcon);
                }
                else
                {
                    iconBank().audienceForLoad() += this;
                }
            }
        }

        void widgetCreatedForItem(GuiWidget &widget, const ui::Item &) override
        {
            // An action button has been created.
            LabelWidget &label = widget.as<LabelWidget>();
            label.setSizePolicy(ui::Expand, ui::Expand);
        }

        void widgetUpdatedForItem(GuiWidget &widget, const ui::Item &item) override
        {
            // An action button needs updating.
            LabelWidget &label = widget.as<LabelWidget>();

            if (const ui::VariantActionItem *varItem = maybeAs<ui::VariantActionItem>(item))
            {
                label.setText(varItem->label(_actions->variantItemsEnabled()));
                label.setStyleImage(varItem->styleImageId(_actions->variantItemsEnabled()),
                                    label.fontId());
            }
            else
            {
                label.setText(item.label());
                if (const ui::ImageItem *imgItem = maybeAs<ui::ImageItem>(item))
                {
                    if (imgItem->styleImageId().isEmpty())
                    {
                        label.setImage(imgItem->image());
                    }
                    else
                    {
                        label.setStyleImage(imgItem->styleImageId(), label.fontId());
                    }
                }
            }
        }

        void updateContents()
        {
            bool isFile   = false;
            auto pkgIdVer = Package::split(packageId());

            if (pkgIdVer.first.beginsWith("file."))
            {
                isFile = true;
                if (!_iconId)
                {
                    icon().setStyleImage("file", "default");
                    icon().setImageScale(.5f);
                }

                // Local files should not be indicated to be packages.
                if (!_item->nativePath.isEmpty())
                {
                    pkgIdVer.first = _item->nativePath.pretty();
                }
            }
            else if (!_iconId)
            {
                icon().setStyleImage("package.icon", "default");
                icon().setImageScale(.5f);
            }

            String labelText = Stringf(_E(b) "%s\n" _E(l)_E(C) _E(s) "%s",
                                              _item->label().c_str(),
                                              pkgIdVer.first.c_str());
                
            if (!isFile && pkgIdVer.second.isValid())
            {
                labelText += Stringf(_E(C) " %s" _E(.), pkgIdVer.second.compactNumber().c_str());
            }
            label().setText(labelText);

            const bool highlight = _owner.d->packageStatus->isPackageHighlighted(packageId());
            _actions->setVariantItemsEnabled(highlight);

            for (GuiWidget *w : _actions->childWidgets())
            {
                if (ButtonWidget *button = maybeAs<ButtonWidget>(w))
                {
                    button->setImageColor(
                        style().colors().colorf(highlight ? "text" : "inverted.text"));
                    button->setColorTheme(highlight ? invertColorTheme(_owner.d->selectedItemHilit)
                                                    : invertColorTheme(_owner.d->selectedItem));
                }
            }

            String auxColor = "accent";
            if (highlight)
            {
                useColorTheme(_owner.d->unselectedItemHilit, _owner.d->selectedItemHilit);
                auxColor = "background";
            }
            else
            {
                useColorTheme(_owner.d->unselectedItem, _owner.d->selectedItem);
            }

            for (ButtonWidget *b : _tags)
            {
                updateTagButtonStyle(b, auxColor);
            }

            checkIconColor();
        }

        void setSelected(bool selected) override
        {
            HomeItemWidget::setSelected(selected);
            checkIconColor();
        }

        void checkIconColor()
        {
            // Package icons should always use their original colors.
            if (_iconId)
            {
                icon().setImageColor(Vec4f(1));
            }
        }

        String packageId() const
        {
            return _item->data().asText();
        }

        PopupWidget *makeInfoPopup() const
        {
            return new PackageInfoDialog(_item->file, _owner.d->packageInfoMode);
        }

        float estimatedHeight() const override
        {
            const float fontHeight = style().fonts().font("default").height().value();
            float       estimate   = fontHeight * 2 + label().margins().height().value();
            return estimate;
        }

        void triggerAction(bool defaultAction = true)
        {
            // Click the default action button (the last one).
            if (!_actions->items().isEmpty())
            {
                auto *w = (defaultAction ? _actions->childWidgets().last()
                                         : _actions->childWidgets().first());
                if (auto *button = maybeAs<ButtonWidget>(w))
                {
                    button->trigger();
                }
            }
        }

        void itemRightClicked() override
        {
            if (_owner.d->rightClickToOpenContextMenu)
            {
                triggerAction(false);
            }
        }

        void bankLoaded(const DotPath &path) override
        {
            if (_iconId) return;

            DE_ASSERT_IN_MAIN_THREAD();

            auto &bank = iconBank();

            if (_packagePath == path)
            {
                setPackageIcon(bank.texture(path));
                bank.audienceForLoad() -= this;
            }
        }

        void setPackageIcon(const Id &id)
        {
            _iconId = id;

            if (_iconId)
            {
                auto *img = new AtlasProceduralImage(_owner);
                img->setPreallocatedImage(_iconId);
                icon().setImage(img);
                icon().setImageScale(1);
                checkIconColor();
            }
        }

    private:
        PackagesWidget &_owner;
        const PackageItem *_item;
        Path _packagePath;
        List<ButtonWidget *> _tags;
        MenuWidget *_actions = nullptr;
        Id _iconId;
    };

    //---------------------------------------------------------------------------------------

    using Strings = std::set<String>;

    Dispatch dispatch;
    Dispatch mainCallForIdentify;

    // Search filter:
    LineEditWidget *search;
    const Rule *    searchMinY = nullptr;
    ButtonWidget *  clearSearch;
    Animation       searchBackgroundOpacity{0.f, Animation::Linear};
    StringList      filterTerms;
    Strings         hiddenTagsInEffect;
    Timer           refilterTimer;

    ProgressWidget *refreshProgress;

    // Packages list:
    StringList                 hiddenTags; // Packages with these not shown unless filterTerms has the tag.
    StringList                 manualPackagePaths; // if empty, all available packages used
    HomeMenuWidget *           menu;
    ui::ListDataT<PackageItem> allPackages;
    ui::FilteredData           filteredPackages{allPackages};
    ui::ListData               defaultActionItems;
    const ui::Data *           actionItems                 = &defaultActionItems;
    bool                       populateEnabled             = true;
    bool                       showOnlyLoaded              = false;
    bool                       actionOnlyForSelection      = true;
    bool                       rightClickToOpenContextMenu = false;
    PackageInfoDialog::Mode    packageInfoMode             = PackageInfoDialog::EnableActions;

    const IPackageStatus *packageStatus = &isPackageLoaded;

    GuiWidget::ColorTheme unselectedItem      = GuiWidget::Normal;
    GuiWidget::ColorTheme selectedItem        = GuiWidget::Normal;
    GuiWidget::ColorTheme unselectedItemHilit = GuiWidget::Inverted;
    GuiWidget::ColorTheme selectedItemHilit   = GuiWidget::Inverted;

    //- PackagesWidget::Pimpl Methods -------------------------------------------------------

    /**
     * Initializes the PackagesWidget private implementation.
     * @param i  Public instance.
     */
    Impl(Public * i)
        : Base(i)
        , hiddenTags({"hidden", "core", "gamedata"})
    {
        defaultActionItems << new ui::VariantActionItem(
            "Load", "Unload", new CallbackAction([this]() {
                DE_ASSERT(menu->interactedItem());

                const String packageId = menu->interactedItem()->as<PackageItem>().data().asText();

                auto &loader = App::packageLoader();
                if (loader.isLoaded(packageId))
                {
                    loader.unload(packageId);
                }
                else
                {
                    try
                    {
                        loader.load(packageId);
                    }
                    catch (const Error &er)
                    {
                        LOG_RES_ERROR("Package \"" + packageId +
                                      "\" could not be loaded: " + er.asText());
                    }
                }
                menu->interactedItem()->notifyChange();
            }));

        self().add(menu = new HomeMenuWidget);
        self().add(search = new LineEditWidget);
        self().add(clearSearch = new ButtonWidget);

        // Search/filter terms.
        search->rule()
            .setInput(Rule::Left, self().rule().left() + self().margins().left())
            .setInput(Rule::Right, self().rule().right() - self().margins().right())
            .setInput(Rule::Top, self().rule().top() + self().margins().top());
        search->setEmptyContentHint("Enter search terms");
        search->setSignalOnEnter(true);
        search->margins().setRight(style().fonts().font("default").height() + rule("gap"));

        clearSearch->set(Background());
        clearSearch->setStyleImage("close.ring", "default");
        clearSearch->setSizePolicy(ui::Filled, ui::Filled);
        clearSearch->margins().set("");
        clearSearch->rule()
            .setInput(Rule::Right, search->rule().right() - rule("gap"))
            .setInput(Rule::Height, search->font().lineSpacing())
            .setInput(Rule::Width, search->font().lineSpacing())
            .setMidAnchorY(search->rule().midY());
        clearSearch->setActionFn([this]() {
            search->setText("");
            root().setFocus(search);
        });

        // Filtered list of packages.
        filteredPackages.setFilter([this](const ui::Item &it) {
            auto &item = it.as<PackageItem>();

            // The terms are looked in:
            // - title
            // - identifier
            // - tags

            if (!item.info)
            {
                // Don't know what this is (probably deleted), can't show it.
                return false;
            }
            if (showOnlyLoaded && !item.isLoaded())
            {
                return false;
            }
            {
                bool isHidden = false;
                for (const auto &h : hiddenTagsInEffect)
                {
                    if (Package::matchTags(*item.file, h))
                    {
                        isHidden = true;
                        break;
                    }
                }
                if (isHidden)
                {
                    return false;
                }
            }
            return filterTerms.isEmpty() || checkTerms({item.data().asText(),        // ID
                                                        item.file->source()->name(), // file name
                                                        item.info->gets(VAR_TITLE),
                                                        item.info->gets(VAR_TAGS)});
        });
        menu->setItems(filteredPackages);
        menu->setBehavior(ChildVisibilityClipping);
        if (!self().name().isEmpty())
        {
            menu->setOpacity(0); // initially
        }
        menu->layout().setRowPadding(Const(0));
        menu->rule()
            .setInput(Rule::Left, self().rule().left() + self().margins().left())
            .setInput(Rule::Right, self().rule().right() - self().margins().right())
            .setInput(Rule::Top,
                      self().rule().top() + self().margins().top() + search->rule().height());
        menu->organizer().setWidgetFactory(*this);
        menu->setVirtualizationEnabled(
            true,
            rule("gap").valuei() * 2 + rule(RuleBank::UNIT).valuei() +
                int(style().fonts().font("default").height().value() * 3));
        menu->organizer().setRecyclingEnabled(true); // homogeneous widgets

        search->audienceForContentChange() += [this](){ updateFilterTerms(); };
        search->audienceForEnter()         += [this](){ focusFirstListedPackage(); };

        refilterTimer.setSingleShot(true);
        refilterTimer.setInterval(REFILTER_DELAY);
        refilterTimer += [this] () { updateFilterTerms(true); };

        // Refresh progress indicator.
        refreshProgress = new ProgressWidget;
        refreshProgress->setMode(ProgressWidget::Indefinite);
        refreshProgress->setImageScale(.3f);
        self().add(refreshProgress);

        // By default, only the progress indicator is shown.
        showProgressIndicator(true);

        self().rule().setInput(Rule::Height,
                               search->rule().height() + menu->rule().height() +
                                   self().margins().height());
    }

    ~Impl() override
    {
        //releaseRef(maxPanelHeight);
        releaseRef(searchMinY);

        // Private instance deleted before child widgets.
        menu->useDefaultItems();
    }

    void showProgressIndicator(bool show)
    {
        if (show)
        {
            refreshProgress->setOpacity(1);
            menu->hide();
            search->hide();
            clearSearch->hide();
        }
        else
        {
            refreshProgress->setOpacity(0, 0.3);
            menu->show();
            search->show();
            clearSearch->show();
        }
    }

    void setManualPackages(StringList ids)
    {
        auto &loader = PackageLoader::get();

        manualPackagePaths.clear();
        for (const String &id : ids)
        {
            if (const File *file = loader.select(id))
            {
                manualPackagePaths << file->path();
            }
            else
            {
                throw UnavailableError("PackagesWidget::setManualPackages",
                                       "Package \"" + id + "\" not found");
            }
        }
    }

    void populate()
    {
        if (!populateEnabled) return;

        //qDebug() << "Populating" << &self;

        showProgressIndicator(false);

        StringList packages = (manualPackagePaths.isEmpty() ? App::packageLoader().findAllPackages()
                                                            : manualPackagePaths);

        // Remove from the list those packages that are no longer listed.
        for (ui::DataPos i = 0; i < allPackages.size(); ++i)
        {
            auto &pkgItem = allPackages.at(i);
            if (!pkgItem.info || !packages.contains(pkgItem.data().asText()))
            {
                allPackages.remove(i--);
            }
        }

        // Add/update the listed packages.
        for (const String &path : packages)
        {
            const File &pack = App::rootFolder().locate<File>(path);

            // Is this already in the list?
            const ui::DataPos pos = allPackages.findData(TextValue(Package::versionedIdentifierForFile(pack)));
            if (pos != ui::Data::InvalidPos)
            {
                allPackages.at(pos).setFile(pack);
            }
            else
            {
                allPackages << new PackageItem(pack);
            }
        }

        allPackages.sort();

        DE_NOTIFY_PUBLIC(ItemCount, i)
        {
            i->itemCountChanged(filteredPackages.size(), allPackages.size());
        }
    }

    void updateItems()
    {
        filteredPackages.forAll([](ui::Item &item) {
            item.as<PackageItem>().notifyChange();
            return LoopContinue;
        });
    }

    void updateFilterTerms(bool immediately = false)
    {
        GLWindow::glActivateMain();

        if (!immediately)
        {
            if (!refilterTimer.isActive())
            {
                menu->setOpacity(0.f, REFILTER_DELAY);
            }
            refilterTimer.start();
        }
        else
        {
            // Refiltering will potentially alter the widget tree, so doing it during
            // event handling is not a great idea.
            dispatch += [this]() {
                /// @todo Parse quoted terms. -jk
                setFilterTerms(search->text().strip().split(RegExp::WHITESPACE));

                menu->setOpacity(1.f, REFILTER_DELAY);
            };
        }
    }

    void setFilterTerms(const StringList& terms)
    {
        filterTerms = terms;
        hiddenTagsInEffect.clear();
        for (const auto &t : hiddenTags)
        {
            if (!terms.contains(t))
            {
                hiddenTagsInEffect.insert("\\b" + t + "\\b"); // tag regexp
            }
        }
        clearSearch->show(!terms.isEmpty());
        if ((showOnlyLoaded = filterTerms.contains(TAG_LOADED)) != false)
        {
            filterTerms.removeAll(TAG_LOADED);
        }

        filteredPackages.refilter();

        DE_NOTIFY_PUBLIC(ItemCount, i)
        {
            i->itemCountChanged(filteredPackages.size(), allPackages.size());
        }
    }

    void focusFirstListedPackage()
    {
        if (filteredPackages.size() > 0)
        {
            menu->scrollY(0);
            root().setFocus(menu->childWidgets().first());
        }
    }

    void fileSystemBusyStatusChanged(FS::BusyStatus bs) override
    {
        if (bs == FS::Busy)
        {
            showProgressIndicator(true);
        }
        else if (DoomsdayApp::bundles().isEverythingIdentified())
        {
            // After bundles have been refreshed, make sure the list items are up to date.
            if (!mainCallForIdentify)
            {
                mainCallForIdentify.enqueue([this]() {
                    root().window().glActivate();
                    populateEnabled = true;
                    self().populate();
                });
            }
        }
    }

    /**
     * Checks whether the filter terms can be found in the provided text strings.
     * All terms must be found in at least one string, but all terms need not be found in
     * every provided string.
     *
     * @param texts  Text strings.
     *
     * @return @c true, if all filter terms found.
     */
    bool checkTerms(const StringList &texts) const
    {
        for (const auto &filterTerm : filterTerms)
        {
            bool found = false;
            for (const auto &text : texts)
            {
                if (text.contains(filterTerm, CaseInsensitive))
                {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }

    //- ChildWidgetOrganizer::IWidgetFactory --------------------------------------

    GuiWidget *makeItemWidget(const ui::Item &item, const GuiWidget *) override
    {
        return new PackageListItemWidget(item.as<PackageItem>(), self());
    }

    void updateItemWidget(GuiWidget & widget, const ui::Item &item) override
    {
        auto &w = widget.as<PackageListItemWidget>();
        w.setItem(item.as<PackageItem>());
        DE_ASSERT_IN_MAIN_THREAD();
        w.updateContents();
    }

    DE_PIMPL_AUDIENCE(ItemCount)
};

DE_AUDIENCE_METHOD(PackagesWidget, ItemCount)

PackagesWidget::PackagesWidget(PopulateBehavior initBehavior, const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{
    d->populateEnabled = (initBehavior == PopulationEnabled);
    FS::get().audienceForBusy() += d;
    populate();
}

PackagesWidget::PackagesWidget(StringList manualPackageIds, const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{
    margins().set(ConstantRule::zero());
    setManualPackageIds(std::move(manualPackageIds));
}

HomeMenuWidget &PackagesWidget::menu()
{
    return *d->menu;
}

ProgressWidget &PackagesWidget::progress()
{
    return *d->refreshProgress;
}

void PackagesWidget::setManualPackageIds(StringList manualPackageIds)
{
    d->setManualPackages(std::move(manualPackageIds));
    populate();
}

void PackagesWidget::setAllowPackageInfoActions(bool allowActions)
{
    d->packageInfoMode =
        allowActions ? PackageInfoDialog::EnableActions : PackageInfoDialog::InformationOnly;
}

void PackagesWidget::setRightClickToOpenContextMenu(bool enable)
{
    d->rightClickToOpenContextMenu = enable;
}

void PackagesWidget::setHiddenTags(StringList hiddenTags)
{
    d->hiddenTags = std::move(hiddenTags);
    populate();
}

void PackagesWidget::setPopulationEnabled(bool enable)
{
    d->populateEnabled = enable;
}

void PackagesWidget::setFilterEditorMinimumY(const Rule &minY)
{
    d->search->rule().setInput(Rule::Top,
                               OperatorRule::maximum(minY, rule().top() + margins().top()));
    changeRef(d->searchMinY, minY);
}

void PackagesWidget::setPackageStatus(const IPackageStatus &packageStatus)
{
    d->packageStatus = &packageStatus;
}

void PackagesWidget::showProgressIndicator()
{
    d->showProgressIndicator(true);
}

void PackagesWidget::setActionItems(const ui::Data &actionItems)
{
    d->actionItems = &actionItems;
}

ui::Data &PackagesWidget::actionItems()
{
    return *const_cast<ui::Data *>(d->actionItems);
}

void PackagesWidget::setActionsAlwaysShown(bool showActions)
{
    d->actionOnlyForSelection = !showActions;

    // Update existing widgets.
    for (auto *w : d->menu->childWidgets())
    {
        if (HomeItemWidget *item = maybeAs<HomeItemWidget>(w))
        {
            item->setKeepButtonsVisible(showActions);
        }
    }
}

void PackagesWidget::setColorTheme(ColorTheme unselectedItem,
                                   ColorTheme selectedItem,
                                   ColorTheme unselectedItemHilit,
                                   ColorTheme selectedItemHilit,
                                   ColorTheme search)
{
    d->unselectedItem      = unselectedItem;
    d->selectedItem        = selectedItem;
    d->unselectedItemHilit = unselectedItemHilit;
    d->selectedItemHilit   = selectedItemHilit;

    d->search->setColorTheme(search);
    d->clearSearch->setImageColor(d->search->textColorf());

    d->populate();
}

void PackagesWidget::populate()
{
    d->populate();
}

void PackagesWidget::updateItems()
{
    d->updateItems();
}

dsize PackagesWidget::itemCount() const
{
    return d->allPackages.size();
}

const ui::Item *PackagesWidget::itemForPackage(const String &packageId) const
{
    ui::DataPos found = d->filteredPackages.findData(TextValue(packageId));
    if (found != ui::Data::InvalidPos)
    {
        return &d->filteredPackages.at(found);
    }
    return nullptr;
}

String PackagesWidget::actionPackage() const
{
    if (d->menu->interactedItem())
    {
        return d->menu->interactedItem()->as<Impl::PackageItem>().data().asText();
    }
    return String();
}

GuiWidget *PackagesWidget::actionWidget() const
{
    if (d->menu->interactedItem())
    {
        return d->menu->organizer().itemWidget(*d->menu->interactedItem());
    }
    return nullptr;
}

const ui::Item *PackagesWidget::actionItem() const
{
    return d->menu->interactedItem();
}

void PackagesWidget::scrollToPackage(const String &packageId) const
{
    if (const auto *item = itemForPackage(packageId))
    {
        auto &scrollArea = d->menu->findTopmostScrollable();

        // If the widget exists currently, we can just scroll to it.
        if (const auto *widget = d->menu->organizer().itemWidget(*item))
        {
            scrollArea.scrollToWidget(*widget);
        }
        else
        {
            const auto pos = d->filteredPackages.find(*item);

            // Estimate the position.
            scrollArea.scrollY(pos * d->menu->organizer().averageChildHeight() -
                                   scrollArea.rule().height().value() / 2,
                               0.3);
        }
    }
}

LineEditWidget &PackagesWidget::searchTermsEditor()
{
    return *d->search;
}

void PackagesWidget::initialize()
{
    GuiWidget::initialize();
    d->menu->organizer().setVisibleArea(root().viewTop(), root().viewBottom());
}

void PackagesWidget::update()
{
    GuiWidget::update();

    if (d->searchMinY)
    {
        const TimeSpan SPAN = 0.3;

        // Time to show or hide the background?
        if (d->searchBackgroundOpacity.target() < .5f &&
            d->search->rule().top().valuei() == d->searchMinY->valuei())
        {
            d->searchBackgroundOpacity.setValue(1.f, SPAN);
        }
        else if (d->searchBackgroundOpacity.target() > .5f &&
                 d->search->rule().top().valuei() > d->searchMinY->valuei())
        {
            d->searchBackgroundOpacity.setValue(0.f, SPAN);
        }

        // Update search field background opacity.
        d->search->setUnfocusedBackgroundOpacity(d->searchBackgroundOpacity);
    }
}

void PackagesWidget::operator>>(PersistentState &toState) const
{
    if (name().isEmpty()) return;

    Record &rec = toState.objectNamespace();
    rec.set(name().concatenateMember("search"), d->search->text());
}

void PackagesWidget::operator<<(const PersistentState &fromState)
{
    if (name().isEmpty()) return;

    const Record &rec = fromState.objectNamespace();
    d->search->setText(rec.gets(name().concatenateMember("search"), ""));
    d->updateFilterTerms(true);
}
