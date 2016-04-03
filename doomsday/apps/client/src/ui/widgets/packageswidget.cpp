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
#include "clientapp.h"

#include <de/FileSystem>
#include <de/ChildWidgetOrganizer>
#include <de/SequentialLayout>
#include <de/MenuWidget>
#include <de/LineEditWidget>
#include <de/DocumentPopupWidget>
#include <de/PopupButtonWidget>
#include <de/SignalAction>
#include <de/CallbackAction>
#include <de/StyleProceduralImage>

using namespace de;

static String const VAR_TITLE("title");
static String const VAR_TAGS ("tags");
static String const TAG_HIDDEN("hidden");

struct PackageLoadStatus : public PackagesWidget::IPackageStatus
{
    bool isPackageHighlighted(String const &packageId) const
    {
        return App::packageLoader().isLoaded(packageId);
    }
};

struct LoadOrUnloadPackage : public PackagesWidget::IButtonHandler
{
    void packageButtonClicked(ButtonWidget &, de::String const &packageId) override
    {
        auto &loader = App::packageLoader();

        if(loader.isLoaded(packageId))
        {
            loader.unload(packageId);
        }
        else
        {
            try
            {
                loader.load(packageId);
            }
            catch(Error const &er)
            {
                LOG_RES_ERROR("Package \"" + packageId +
                              "\" could not be loaded: " + er.asText());
            }
        }
    }
};

static PackageLoadStatus isPackageLoaded;
static LoadOrUnloadPackage loadOrUnloadPackage;

PackagesWidget::IPackageStatus::~IPackageStatus() {}
PackagesWidget::IButtonHandler::~IButtonHandler() {}

DENG_GUI_PIMPL(PackagesWidget)
, public ChildWidgetOrganizer::IFilter
, public ChildWidgetOrganizer::IWidgetFactory
{
    LineEditWidget *search;
    ButtonWidget *clearSearch;
    HomeMenuWidget *menu;
    String buttonLabels[2];
    QStringList filterTerms;
    bool showHidden = false;

    IPackageStatus const *packageStatus = &isPackageLoaded;
    IButtonHandler       *buttonHandler = &loadOrUnloadPackage;

    GuiWidget::ColorTheme unselectedItem       = GuiWidget::Normal;
    GuiWidget::ColorTheme selectedItem         = GuiWidget::Normal;
    GuiWidget::ColorTheme loadedUnselectedItem = GuiWidget::Inverted;
    GuiWidget::ColorTheme loadedSelectedItem   = GuiWidget::Inverted;

    /**
     * Information about an available package.
     */
    struct PackageItem : public ui::Item
    {
        File const *file;
        Record const *info;

        PackageItem(File const &packFile)
            : file(&packFile)
            , info(&file->objectNamespace().subrecord(Package::VAR_PACKAGE))
        {
            setData(QString(info->gets("ID")));
        }

        void setFile(File const &packFile)
        {
            file = &packFile;
            info = &file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            notifyChange();
        }
    };

    /**
     * Widget showing information about a package and containing buttons for manipulating
     * the package.
     */
    class PackageListWidget : public HomeItemWidget
    {
    public:
        PackageListWidget(PackageItem const &item, PackagesWidget &owner)
            : _owner(owner)
            , _item(&item)
        {
            icon().set(Background());
            icon().setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
            icon().setImage(new StyleProceduralImage("package", *this));
            icon().margins().set("gap");
            Rule const &height = style().fonts().font("default").height();
            icon().setOverrideImageSize(height.value());
            icon().rule().setInput(Rule::Width, height + rule("gap")*2);

            _loadButton = new ButtonWidget;
            _loadButton->setActionFn([this] ()
            {
                _owner.d->buttonHandler->packageButtonClicked(*_loadButton, packageId());
                updateContents();
            });
            connect(this, &HomeItemWidget::doubleClicked, [this] () {
                _loadButton->trigger();
            });
            addButton(_loadButton);

            /*add(_title = new LabelWidget);
            _title->setSizePolicy(ui::Fixed, ui::Expand);
            _title->setAlignment(ui::AlignLeft);
            _title->setTextLineAlignment(ui::AlignLeft);
            _title->margins().setBottom("").setLeft("unit");

            add(_subtitle = new LabelWidget);
            _subtitle->setSizePolicy(ui::Fixed, ui::Expand);
            _subtitle->setAlignment(ui::AlignLeft);
            _subtitle->setTextLineAlignment(ui::AlignLeft);
            _subtitle->setFont("small");
            _subtitle->setTextColor("accent");
            _subtitle->margins().setTop("").setBottom("unit").setLeft("unit");

            add(_loadButton = new ButtonWidget);
            _loadButton->setSizePolicy(ui::Expand, ui::Expand);
            _loadButton->setAction(new LoadAction(*this));

            add(_infoButton = new PopupButtonWidget);
            _infoButton->setSizePolicy(ui::Expand, ui::Fixed);
            _infoButton->setText(_E(s)_E(B) + tr("..."));
            _infoButton->setPopup([this] (PopupButtonWidget const &) {
                return makeInfoPopup();
            }, ui::Left);*/

            createTagButtons();

            /*AutoRef<Rule> titleWidth(rule().width() -
                                     _loadButton->rule().width() -
                                     _infoButton->rule().width());*/

            /*_title->rule()
                    .setInput(Rule::Width, titleWidth)
                    .setInput(Rule::Left,  rule().left())
                    .setInput(Rule::Top,   rule().top());
            _subtitle->rule()
                    .setInput(Rule::Width, titleWidth)
                    .setInput(Rule::Left,  rule().left())
                    .setInput(Rule::Top,   _title->rule().bottom());

            _loadButton->rule()
                    .setInput(Rule::Right, rule().right() - _infoButton->rule().width())
                    .setMidAnchorY(rule().midY());
            _infoButton->rule()
                    .setInput(Rule::Right,  rule().right())
                    .setInput(Rule::Height, _loadButton->rule().height())
                    .setMidAnchorY(rule().midY());

            rule().setInput(Rule::Width,  rule("dialog.packages.width"))
                  .setInput(Rule::Height, _title->rule().height() +
                            _subtitle->rule().height() + _tags.at(0)->rule().height());*/
        }

        void createTagButtons()
        {
            SequentialLayout layout(label().rule().left() + label().margins().left(),
                                    label().rule().bottom() - label().margins().bottom(), ui::Right);

            for(QString tag : Package::tags(*_item->file))
            {
                auto *btn = new ButtonWidget;
                btn->setText(_E(l) + tag.toLower());
                btn->setActionFn([this, tag] ()
                {
                    String terms = _owner.d->search->text();
                    if(!terms.isEmpty() && !terms.last().isSpace()) terms += " ";
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

            if(!_tags.isEmpty())
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

        void updateContents()
        {
            label().setText(String(_E(b) "%1\n" _E(l) "%2")
                            .arg(_item->info->gets("title"))
                            .arg(packageId()));

            String auxColor = "accent";

            if(_owner.d->packageStatus->isPackageHighlighted(packageId()))
            {
                _loadButton->setText(_owner.d->buttonLabels[1]);
                _loadButton->setColorTheme(invertColorTheme(_owner.d->loadedSelectedItem));
                icon().setImageColor(style().colors().colorf("accent"));
                useColorTheme(_owner.d->loadedUnselectedItem, _owner.d->loadedSelectedItem);
                auxColor = "background";
            }
            else
            {
                _loadButton->setText(_owner.d->buttonLabels[0]);
                _loadButton->setColorTheme(invertColorTheme(_owner.d->selectedItem));
                icon().setImageColor(style().colors().colorf("text"));
                useColorTheme(_owner.d->unselectedItem, _owner.d->selectedItem);
            }

            for(ButtonWidget *b : _tags)
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
            auto *pop = new DocumentPopupWidget;
            pop->document().setText(QString(_E(1) "%1" _E(.) "\n%2\n"
                                            _E(l) "Version: " _E(.) "%3\n"
                                            _E(l) "License: " _E(.)_E(>) "%4" _E(<)
                                            _E(l) "\nFile: " _E(.)_E(>)_E(C) "%5")
                                    .arg(_item->info->gets("title"))
                                    .arg(packageId())
                                    .arg(_item->info->gets("version"))
                                    .arg(_item->info->gets("license"))
                                    .arg(_item->file->description()));
            return pop;
        }

    private:
        PackagesWidget &_owner;
        PackageItem const *_item;
        //LabelWidget *_title;
        //LabelWidget *_subtitle;
        QList<ButtonWidget *> _tags;
        ButtonWidget *_loadButton;
        //PopupButtonWidget *_infoButton;
    };

    Instance(Public *i) : Base(i)
    {
        buttonLabels[0] = tr("Load");
        buttonLabels[1] = tr("Unload");

        // Search/filter terms.
        self.add(search = new LineEditWidget);
        search->rule()
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Right, self.rule().right())
                .setInput(Rule::Top,   self.rule().top());
        search->setEmptyContentHint(tr("Search packages"));
        search->margins().setRight(style().fonts().font("default").height() +
                                   rule("gap"));

        self.add(clearSearch = new ButtonWidget);
        clearSearch->set(Background());
        clearSearch->setImage(new StyleProceduralImage("close.ring", self));
        clearSearch->setOverrideImageSize(style().fonts().font("default").height().value());
        clearSearch->setSizePolicy(ui::Expand, ui::Expand);
        clearSearch->rule()
                .setInput(Rule::Right, search->rule().right())
                .setMidAnchorY(search->rule().midY());
        clearSearch->setActionFn([this] () {
            search->setText("");
            root().setFocus(search);
        });

        // Filtered list of packages.
        self.add(menu = new HomeMenuWidget);
        menu->setBehavior(ChildVisibilityClipping);
        menu->layout().setRowPadding(Const(0));
        menu->rule()
                .setInput(Rule::Left,  self.rule().left())
                .setInput(Rule::Right, self.rule().right())
                .setInput(Rule::Top,   search->rule().bottom());
        menu->organizer().setWidgetFactory(*this);
        menu->organizer().setFilter(*this);

        QObject::connect(search, &LineEditWidget::editorContentChanged,
                         [this] () { updateFilterTerms(); });
        QObject::connect(search, &LineEditWidget::enterPressed,
                         [this] () { focusFirstListedPackge(); });
    }

    void populate()
    {
        StringList packages = App::packageLoader().findAllPackages();

        // Remove from the list those packages that are no longer listed.
        for(ui::DataPos i = 0; i < menu->items().size(); ++i)
        {
            if(!packages.contains(menu->items().at(i).data().toString()))
            {
                menu->items().remove(i--);
            }
        }

        // Add/update the listed packages.
        for(String const &path : packages)
        {
            File const &pack = App::rootFolder().locate<File>(path);

            // Core packages are mandatory and thus omitted.
            auto const tags = Package::tags(pack);
            if(tags.contains("core") || tags.contains("gamedata")) continue;

            // Is this already in the list?
            ui::DataPos pos = menu->items().findData(pack.objectNamespace().gets("package.ID"));
            if(pos != ui::Data::InvalidPos)
            {
                menu->items().at(pos).as<PackageItem>().setFile(pack);
            }
            else
            {
                menu->items() << new PackageItem(pack);
            }
        }
    }

    void updateFilterTerms()
    {
        /// @todo Parse quoted terms. -jk
        setFilterTerms(search->text().strip().split(QRegExp("\\s"), QString::SkipEmptyParts));
    }

    void setFilterTerms(QStringList const &terms)
    {
        filterTerms = terms;
        clearSearch->show(!terms.isEmpty());
        showHidden = filterTerms.contains(TAG_HIDDEN);
        if(showHidden) filterTerms.removeAll(TAG_HIDDEN);

        menu->organizer().refilter();
    }

    void focusFirstListedPackge()
    {
        //if(menu-)
    }

//- ChildWidgetOrganizer::IFilter ---------------------------------------------

    bool checkTerms(String const &text) const
    {
        for(QString const &filterTerm : filterTerms)
        {
            if(!text.contains(filterTerm, Qt::CaseInsensitive))
            {
                return false;
            }
        }
        return true;
    }

    bool isItemAccepted(ChildWidgetOrganizer const &,
                        ui::Data const &data, ui::Data::Pos pos) const
    {
        auto &item = data.at(pos).as<PackageItem>();

        // The terms are looked in:
        // - title
        // - identifier
        // - tags

        bool const hidden = Package::tags(item.info->gets(VAR_TAGS)).contains(TAG_HIDDEN);
        if(showHidden ^ hidden)
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
        return new PackageListWidget(item.as<PackageItem>(), self);
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &)
    {
        widget.as<PackageListWidget>().updateContents();
    }
};

PackagesWidget::PackagesWidget(String const &name)
    : GuiWidget(name)
    , d(new Instance(this))
{
    /*buttons()
            << new DialogButtonItem(Default | Accept, tr("Close"))
            << new DialogButtonItem(Action, style().images().image("refresh"),
                                    new CallbackAction([this] ()
            {
                App::fileSystem().refresh();
                d->populate();
            }));*/

    //area().setContentSize(d->menu->rule().width(), d->menu->rule().height());
    rule().setInput(Rule::Height,
                    d->search->rule().height() +
                    d->menu->rule().height());

    refreshPackages();
}

void PackagesWidget::setPackageStatus(IPackageStatus const &packageStatus)
{
    d->packageStatus = &packageStatus;
}

void PackagesWidget::setButtonHandler(IButtonHandler &buttonHandler)
{
    d->buttonHandler = &buttonHandler;
}

void PackagesWidget::setButtonLabels(String const &buttonLabel, String const &highlightedButtonLabel)
{
    d->buttonLabels[0] = buttonLabel;
    d->buttonLabels[1] = highlightedButtonLabel;

    d->populate();
}

void PackagesWidget::setColorTheme(ColorTheme unselectedItem, ColorTheme selectedItem,
                                   ColorTheme loadedUnselectedItem, ColorTheme loadedSelectedItem)
{
    d->unselectedItem       = unselectedItem;
    d->selectedItem         = selectedItem;
    d->loadedUnselectedItem = loadedUnselectedItem;
    d->loadedSelectedItem   = loadedSelectedItem;

    d->populate();
}

void PackagesWidget::populate()
{
    d->populate();
}

LineEditWidget &PackagesWidget::searchTermsEditor()
{
    return *d->search;
}

void PackagesWidget::operator >> (PersistentState &toState) const
{
    if(name().isEmpty()) return;

    Record &rec = toState.objectNamespace();
    rec.set(name().concatenateMember("search"), d->search->text());
}

void PackagesWidget::operator << (PersistentState const &fromState)
{
    if(name().isEmpty()) return;

    Record const &rec = fromState.objectNamespace();
    d->search->setText(rec.gets(name().concatenateMember("search"), ""));
}

void PackagesWidget::refreshPackages()
{
    App::fileSystem().refresh();
    d->populate();
}
