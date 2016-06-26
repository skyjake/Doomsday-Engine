/** @file packageswidget.cpp  Widget for searching and examining packages.
 *
 * @authors Copyright (c) 2015-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/widgets/packagepopupwidget.h"
#include "clientapp.h"

#include <de/CallbackAction>
#include <de/ChildWidgetOrganizer>
#include <de/DocumentPopupWidget>
#include <de/FileSystem>
#include <de/LineEditWidget>
#include <de/Loop>
#include <de/MenuWidget>
#include <de/PackageLoader>
#include <de/PopupButtonWidget>
#include <de/ProgressWidget>
#include <de/SequentialLayout>
#include <de/SignalAction>
#include <de/TaskPool>
#include <de/ui/VariantActionItem>

#include <doomsday/DoomsdayApp>
#include <doomsday/resource/bundles.h>

#include <QTimer>

using namespace de;

static String const VAR_TITLE ("title");
static String const VAR_TAGS  ("tags");
static String const TAG_HIDDEN("hidden");

static TimeDelta const REFILTER_DELAY(0.2);

struct PackageLoadStatus : public PackagesWidget::IPackageStatus {
    bool isPackageHighlighted(String const &packageId) const {
        return App::packageLoader().isLoaded(packageId);
    }
};
static PackageLoadStatus isPackageLoaded;

PackagesWidget::IPackageStatus::~IPackageStatus() {}

DENG_GUI_PIMPL(PackagesWidget)
, DENG2_OBSERVES(res::Bundles, Identify)
, DENG2_OBSERVES(DoomsdayApp, FileRefresh)
, public ChildWidgetOrganizer::IFilter
, public ChildWidgetOrganizer::IWidgetFactory
{
    LoopCallback mainCall;

    // Search filter:
    LineEditWidget *search;
    Rule const *searchMinY = nullptr;
    ButtonWidget *clearSearch;
    Animation searchBackgroundOpacity { 0.f, Animation::Linear };
    QStringList filterTerms;
    QTimer refilterTimer;

    ProgressWidget *refreshProgress;

    // Packages list:
    HomeMenuWidget *menu;
    ui::ListData defaultActionItems;
    ui::Data const *actionItems = &defaultActionItems;
    bool showHidden = false;
    bool actionOnlyForSelection = true;

    IPackageStatus const *packageStatus = &isPackageLoaded;

    GuiWidget::ColorTheme unselectedItem      = GuiWidget::Normal;
    GuiWidget::ColorTheme selectedItem        = GuiWidget::Normal;
    GuiWidget::ColorTheme unselectedItemHilit = GuiWidget::Inverted;
    GuiWidget::ColorTheme selectedItemHilit   = GuiWidget::Inverted;

    TaskPool tasks;

    /**
     * Information about an available package.
     */
    struct PackageItem : public ui::Item, DENG2_OBSERVES(File, Deletion)
    {
        File const *file;
        Record const *info;

        PackageItem(File const &packFile)
            : file(&packFile)
            , info(&file->objectNamespace().subrecord(Package::VAR_PACKAGE))
        {
            file->audienceForDeletion() += this;
            setData(QString(info->gets("ID")));
            setLabel(info->gets(Package::VAR_TITLE));
        }

        void setFile(File const &packFile)
        {
            packFile.audienceForDeletion() += this;
            file = &packFile;
            info = &file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            setLabel(info->gets(Package::VAR_TITLE));
            notifyChange();
        }

        void fileBeingDeleted(File const &)
        {
            file = nullptr;
            info = nullptr;
        }
    };

    /**
     * Widget showing information about a package and containing buttons for manipulating
     * the package.
     */
    class PackageListItemWidget : public HomeItemWidget
                                , DENG2_OBSERVES(ChildWidgetOrganizer, WidgetCreation) // actions
                                , DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
    {
    public:
        PackageListItemWidget(PackageItem const &item, PackagesWidget &owner)
            : HomeItemWidget(NonAnimatedHeight) // virtualized, so don't make things difficult
            , _owner(owner)
            , _item(&item)
        {
            icon().setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
            icon().setStyleImage("package", "default");
            icon().margins().set("gap");
            Rule const &height = style().fonts().font("default").height();
            icon().rule().setInput(Rule::Width, height + rule("gap")*2);

            _actions = new MenuWidget;
            _actions->enablePageKeys(false);
            _actions->enableScrolling(false);
            _actions->margins().setZero();
            _actions->organizer().audienceForWidgetCreation() += this;
            _actions->organizer().audienceForWidgetUpdate()   += this;
            _actions->setGridSize(0, ui::Expand, 1, ui::Expand);
            _actions->setItems(*owner.d->actionItems);
            connect(this, &HomeItemWidget::doubleClicked, [this] ()
            {
                // Click the default action button (the last one).
                if (!_actions->items().isEmpty())
                {
                    if (auto *button = _actions->childWidgets().last()->maybeAs<ButtonWidget>())
                    {
                        button->trigger();
                    }
                }
            });
            addButton(_actions);
            setKeepButtonsVisible(!_owner.d->actionOnlyForSelection);

            createTagButtons();
        }

        void createTagButtons()
        {
            SequentialLayout layout(label().rule().left()   + label().margins().left(),
                                    label().rule().bottom() - label().margins().bottom(),
                                    ui::Right);

            for (QString tag : Package::tags(*_item->file))
            {
                auto *btn = new ButtonWidget;
                btn->setText(_E(l) + tag.toLower());
                btn->setActionFn([this, tag] ()
                {
                    String terms = _owner.d->search->text();
                    if (!terms.isEmpty() && !terms.last().isSpace()) terms += " ";
                    terms += tag.toLower();
                    _owner.d->search->setText(terms);
                });
                updateTagButtonStyle(btn, "accent");
                btn->setSizePolicy(ui::Expand, ui::Expand);
                btn->margins()
                        .setTop("unit").setBottom("unit")
                        .setLeft("gap").setRight("gap");
                add(btn);

                layout << *btn;
                _tags.append(btn);
            }

            if (!_tags.isEmpty())
            {
                //rule().setInput(Rule::Height, label().rule().height());
                label().margins().setBottom(rule("unit")*2 +
                                            style().fonts().font("small").height() +
                                            rule("gap"));
            }
        }

        void updateTagButtonStyle(ButtonWidget *tag, String const &color)
        {
            tag->setFont("small");
            tag->setTextColor(color);
            tag->set(Background(Background::Rounded, style().colors().colorf(color), 6));
        }

        void widgetCreatedForItem(GuiWidget &widget, ui::Item const &)
        {
            // An action button has been created.
            LabelWidget &label = widget.as<LabelWidget>();
            label.setSizePolicy(ui::Expand, ui::Expand);
        }

        void widgetUpdatedForItem(GuiWidget &widget, ui::Item const &item)
        {
            // An action button needs updating.
            LabelWidget &label = widget.as<LabelWidget>();

            if (ui::VariantActionItem const *varItem = item.maybeAs<ui::VariantActionItem>())
            {
                label.setText      (varItem->label       (_actions->variantItemsEnabled()));
                label.setStyleImage(varItem->styleImageId(_actions->variantItemsEnabled()),
                                    label.fontId());
            }
            else
            {
                label.setText(item.label());
                if (ui::ImageItem const *imgItem = item.maybeAs<ui::ImageItem>())
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
            label().setText(String(_E(b) "%1\n" _E(l) "%2")
                            .arg(_item->label())
                            .arg(packageId()));

            String auxColor = "accent";

            bool const highlight = _owner.d->packageStatus->isPackageHighlighted(packageId());
            _actions->setVariantItemsEnabled(highlight);

            for (Widget *w : _actions->childWidgets())
            {
                if (ButtonWidget *button = w->maybeAs<ButtonWidget>())
                {
                    button->setImageColor(style().colors().colorf(highlight? "text" : "inverted.text"));
                    button->setColorTheme(highlight? invertColorTheme(_owner.d->selectedItemHilit)
                                                   : invertColorTheme(_owner.d->selectedItem));
                }
            }

            if (highlight)
            {
                icon().setImageColor(style().colors().colorf("accent"));
                useColorTheme(_owner.d->unselectedItemHilit, _owner.d->selectedItemHilit);
                auxColor = "background";
            }
            else
            {
                icon().setImageColor(style().colors().colorf("text"));
                useColorTheme(_owner.d->unselectedItem, _owner.d->selectedItem);
            }

            for (ButtonWidget *b : _tags)
            {
                updateTagButtonStyle(b, auxColor);
            }
        }

        String packageId() const
        {
            return _item->info->gets("ID");
        }

        PopupWidget *makeInfoPopup() const
        {
            return new PackagePopupWidget(_item->file);
        }

    private:
        PackagesWidget &_owner;
        PackageItem const *_item;
        QList<ButtonWidget *> _tags;
        MenuWidget *_actions;
    };

    Instance(Public *i) : Base(i)
    {
        defaultActionItems << new ui::VariantActionItem(tr("Load"), tr("Unload"), new CallbackAction([this] ()
        {
            DENG2_ASSERT(menu->interactedItem());

            String const packageId = menu->interactedItem()->as<PackageItem>().data().toString();

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
                catch (Error const &er)
                {
                    LOG_RES_ERROR("Package \"" + packageId + "\" could not be loaded: " +
                                  er.asText());
                }
            }
            menu->interactedItem()->notifyChange();
        }));

        self.add(menu = new HomeMenuWidget);
        self.add(search = new LineEditWidget);
        self.add(clearSearch = new ButtonWidget);

        // Search/filter terms.
        search->rule()
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Right, self.rule().right())
                .setInput(Rule::Top,   self.rule().top());
        search->setEmptyContentHint(tr("Search packages"));
        search->setSignalOnEnter(true);
        search->margins().setRight(style().fonts().font("default").height() + rule("gap"));

        clearSearch->set(Background());
        clearSearch->setStyleImage("close.ring", "default");
        clearSearch->setSizePolicy(ui::Expand, ui::Expand);
        clearSearch->rule()
                .setInput(Rule::Right, search->rule().right())
                .setMidAnchorY(search->rule().midY());
        clearSearch->setActionFn([this] () {
            search->setText("");
            root().setFocus(search);
        });

        // Filtered list of packages.
        menu->setBehavior(ChildVisibilityClipping);
        if (!self.name().isEmpty())
        {
            menu->setOpacity(0); // initially
        }
        menu->layout().setRowPadding(Const(0));
        menu->rule()
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Right, self.rule().right())
                .setInput(Rule::Top,   self.rule().top() + search->rule().height());
        menu->organizer().setWidgetFactory(*this);
        menu->organizer().setFilter(*this);
        menu->setVirtualizationEnabled(true, rule("gap").valuei()*2 + rule("unit").valuei() +
                                       int(style().fonts().font("default").height().value()*3));

        QObject::connect(search, &LineEditWidget::editorContentChanged, [this] () { updateFilterTerms(); });
        QObject::connect(search, &LineEditWidget::enterPressed,         [this] () { focusFirstListedPackage(); });

        refilterTimer.setSingleShot(true);
        refilterTimer.setInterval(int(REFILTER_DELAY.asMilliSeconds()));
        QObject::connect(&refilterTimer, &QTimer::timeout, [this] () { updateFilterTerms(true); });

        // Refresh progress indicator.
        refreshProgress = new ProgressWidget;
        refreshProgress->setMode(ProgressWidget::Indefinite);
        refreshProgress->setImageScale(.3f);
        self.add(refreshProgress);

        // By default, only the progress indicator is shown.
        showProgressIndicator(true);
    }

    ~Instance()
    {
        releaseRef(searchMinY);

        // Private instance deleted before child widgets.
        menu->organizer().unsetFilter();
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

    void populate()
    {
        StringList packages = App::packageLoader().findAllPackages();

        // Remove from the list those packages that are no longer listed.
        for (ui::DataPos i = 0; i < menu->items().size(); ++i)
        {
            auto &pkgItem = menu->items().at(i).as<PackageItem>();
            if (!pkgItem.info || !packages.contains(pkgItem.data().toString()))
            {
                menu->items().remove(i--);
            }
        }

        // Add/update the listed packages.
        for (String const &path : packages)
        {
            File const &pack = App::rootFolder().locate<File>(path);

            // Core packages are mandatory and thus omitted.
            auto const tags = Package::tags(pack);
            if (tags.contains(QStringLiteral("core")) ||
                tags.contains(QStringLiteral("gamedata"))) continue;

            // Is this already in the list?
            ui::DataPos pos = menu->items().findData(pack.objectNamespace().gets(Package::VAR_PACKAGE_ID));
            if (pos != ui::Data::InvalidPos)
            {
                menu->items().at(pos).as<PackageItem>().setFile(pack);
            }
            else
            {
                menu->items() << new PackageItem(pack);
            }
        }

        menu->items().sort();
        menu->organizer().refilter();
        showProgressIndicator(false);

        emit self.itemCountChanged(menu->organizer().itemCount(), menu->items().size());
    }

    void updateItems()
    {
        menu->items().forAll([this] (ui::Item &item)
        {
            item.as<PackageItem>().notifyChange();
            return LoopContinue;
        });
    }

    void updateFilterTerms(bool immediately = false)
    {
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
            mainCall.enqueue([this] ()
            {
                /// @todo Parse quoted terms. -jk
                setFilterTerms(search->text().strip().split(QRegExp("\\s"), QString::SkipEmptyParts));

                menu->setOpacity(1.f, REFILTER_DELAY);
            });
        }
    }

    void setFilterTerms(QStringList const &terms)
    {
        filterTerms = terms;
        clearSearch->show(!terms.isEmpty());
        showHidden = filterTerms.contains(TAG_HIDDEN);
        if (showHidden) filterTerms.removeAll(TAG_HIDDEN);

        menu->organizer().refilter();

        emit self.itemCountChanged(menu->organizer().itemCount(), menu->items().size());
    }

    void focusFirstListedPackage()
    {
        if (menu->organizer().itemCount() > 0)
        {
            menu->scrollY(0);
            root().setFocus(menu->childWidgets().first());
        }
    }

    void dataBundlesIdentified(bool) override
    {
        // After bundles have been refreshed, make sure the list items are up to date.
        if (!mainCall)
        {
            mainCall.enqueue([this] ()
            {
                //qDebug() << "Bundles identified, re-populating" << &self;
                self.populate();
            });
        }
    }

    void aboutToRefreshFiles()
    {
        showProgressIndicator(true);
    }

//- ChildWidgetOrganizer::IFilter ---------------------------------------------

    bool checkTerms(String const &text) const
    {
        for (QString const &filterTerm : filterTerms)
        {
            if (!text.contains(filterTerm, Qt::CaseInsensitive))
            {
                return false;
            }
        }
        return true;
    }

    bool isItemAccepted(ChildWidgetOrganizer const &, ui::Data const &,
                        ui::Item const &it) const
    {
        auto &item = it.as<PackageItem>();

        // The terms are looked in:
        // - title
        // - identifier
        // - tags

        if (!item.info) return false;

        bool const hidden = Package::tags(item.info->gets(VAR_TAGS)).contains(TAG_HIDDEN);
        if (showHidden ^ hidden)
        {
            return false;
        }

        return filterTerms.isEmpty() ||
               checkTerms(item.data().toString()) || // ID
               checkTerms(item.info->gets(VAR_TITLE)) ||
               checkTerms(item.info->gets(VAR_TAGS));
    }

//- ChildWidgetOrganizer::IWidgetFactory --------------------------------------

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        return new PackageListItemWidget(item.as<PackageItem>(), self);
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &)
    {
        widget.as<PackageListItemWidget>().updateContents();
    }
};

PackagesWidget::PackagesWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    rule().setInput(Rule::Height, d->search->rule().height() + d->menu->rule().height());

    auto &bundles = DoomsdayApp::bundles();
    bundles.audienceForIdentify() += d;

    if (bundles.isEverythingIdentified())
    {
        populate();
    }
}

ProgressWidget &PackagesWidget::progress()
{
    return *d->refreshProgress;
}

void PackagesWidget::setFilterEditorMinimumY(Rule const &minY)
{
    d->search->rule().setInput(Rule::Top, OperatorRule::maximum(minY, rule().top()));
    changeRef(d->searchMinY, minY);
}

void PackagesWidget::setPackageStatus(IPackageStatus const &packageStatus)
{
    d->packageStatus = &packageStatus;
}

void PackagesWidget::setActionItems(ui::Data const &actionItems)
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
        if (HomeItemWidget *item = w->maybeAs<HomeItemWidget>())
        {
            item->setKeepButtonsVisible(showActions);
        }
    }
}

void PackagesWidget::setColorTheme(ColorTheme unselectedItem, ColorTheme selectedItem,
                                   ColorTheme unselectedItemHilit, ColorTheme selectedItemHilit)
{
    d->unselectedItem      = unselectedItem;
    d->selectedItem        = selectedItem;
    d->unselectedItemHilit = unselectedItemHilit;
    d->selectedItemHilit   = selectedItemHilit;

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

ui::Item const *PackagesWidget::itemForPackage(String const &packageId) const
{
    ui::DataPos found = d->menu->items().findData(packageId);
    if (found != ui::Data::InvalidPos)
    {
        return &d->menu->items().at(found);
    }
    return nullptr;
}

String PackagesWidget::actionPackage() const
{
    if (d->menu->interactedItem())
    {
        return d->menu->interactedItem()->as<Instance::PackageItem>().data().toString();
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

ui::Item const *PackagesWidget::actionItem() const
{
    return d->menu->interactedItem();
}

void PackagesWidget::scrollToPackage(String const &packageId) const
{
    if (auto const *item = itemForPackage(packageId))
    {
        if (auto const *widget = d->menu->organizer().itemWidget(*item))
        {
            d->menu->findTopmostScrollable().scrollToWidget(*widget);
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
        TimeDelta const SPAN = 0.3;

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

void PackagesWidget::operator >> (PersistentState &toState) const
{
    if (name().isEmpty()) return;

    Record &rec = toState.objectNamespace();
    rec.set(name().concatenateMember("search"), d->search->text());
}

void PackagesWidget::operator << (PersistentState const &fromState)
{
    if (name().isEmpty()) return;

    Record const &rec = fromState.objectNamespace();
    d->search->setText(rec.gets(name().concatenateMember("search"), ""));
    d->updateFilterTerms(true);
}

void PackagesWidget::refreshPackages()
{
    d->showProgressIndicator(true);
    App::fileSystem().refresh();
}
