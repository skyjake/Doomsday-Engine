/** @file packagesdialog.cpp
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "clientapp.h"

#include <de/FileSystem>
#include <de/MenuWidget>
#include <de/ChildWidgetOrganizer>
#include <de/SequentialLayout>
#include <de/DocumentPopupWidget>
#include <de/PopupButtonWidget>
#include <de/PopupMenuWidget>
#include <de/CallbackAction>
#include <de/SignalAction>
#include <de/ui/SubwidgetItem>

using namespace de;

DENG_GUI_PIMPL(PackagesDialog)
, public ChildWidgetOrganizer::IWidgetFactory
, public PackagesWidget::IPackageStatus
, public PackagesWidget::IButtonHandler
{
    StringList selectedPackages;
    LabelWidget *nothingSelected;
    HomeMenuWidget *menu;
    PackagesWidget *browser;

    /**
     * Information about a selected package. If the package file is not found, only
     * the ID is known.
     */
    class SelectedPackageItem : public ui::Item
    {
    public:
        SelectedPackageItem(String const &packageId)
        {
            setData(packageId);

            _file = App::packageLoader().select(packageId);
            if(_file)
            {
                _info = &_file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            }
        }

        String packageId() const
        {
            return data().toString();
        }

        Record const *info() const
        {
            return _info;
        }

        File const *packageFile() const
        {
            return _file;
        }

        /*void setFile(File const &packFile)
        {
            file = &packFile;
            info = &file->objectNamespace().subrecord(Package::VAR_PACKAGE);
            notifyChange();
        }*/

    private:
        File   const *_file = nullptr;
        Record const *_info = nullptr;
    };

    /**
     * Widget showing information about a selected package, with a button for dragging
     * the item up and down.
     */
    class SelectedPackageWidget : public HomeItemWidget
    {
    public:
        SelectedPackageWidget(SelectedPackageItem const &item)
            : _item(&item)
        {
            useColorTheme(Normal, Inverted);

            _infoButton = new PopupButtonWidget;
            _infoButton->setText(tr("..."));
            _infoButton->setFont("small");
            _infoButton->margins().setTopBottom("unit");
            _infoButton->setPopup([this] (PopupButtonWidget const &)
            {
                auto *pop = new PopupMenuWidget;
                pop->setColorTheme(Inverted);
                pop->items() << new ui::SubwidgetItem(
                                    tr("Info"), ui::Down,
                                    [this] () -> PopupWidget * { return makeInfoPopup(); });
                return pop;
            }
            , ui::Down);
            addButton(_infoButton);

            /*
            add(_title = new LabelWidget);
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
            }, ui::Left);

            createTagButtons();

            AutoRef<Rule> titleWidth(rule().width() -
                                     _loadButton->rule().width() -
                                     _infoButton->rule().width());

            _title->rule()
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
                            _subtitle->rule().height() + _tags.at(0)->rule().height());
                            */
        }
/*
        void createTagButtons()
        {
            SequentialLayout layout(_subtitle->rule().left(),
                                    _subtitle->rule().bottom(), ui::Right);

            for(QString tag : Package::tags(*_item->file))
            {
                auto *btn = new ButtonWidget;
                btn->setText(_E(l) + tag.toLower());
                updateTagButtonStyle(btn, "accent");
                btn->setSizePolicy(ui::Expand, ui::Expand);
                btn->margins()
                        .setTop("unit").setBottom("unit")
                        .setLeft("gap").setRight("gap");
                add(btn);

                layout << *btn;
                _tags.append(btn);
            }
        }

        void updateTagButtonStyle(ButtonWidget *tag, String const &color)
        {
            tag->setFont("small");
            tag->setTextColor(color);
            tag->set(Background(Background::Rounded, style().colors().colorf(color), 6));
        }*/

        void updateContents()
        {
            if(_item->info())
            {
                label().setText(_item->info()->gets("title"));
            }
            else
            {
                label().setText(_item->packageId());
            }

            /*
            _subtitle->setText(packageId());

            String auxColor = "accent";

            if(isLoaded())
            {
                _loadButton->setText(tr("Unload"));
                _loadButton->setTextColor("altaccent");
                _loadButton->setBorderColor("altaccent");
                _title->setFont("choice.selected");
                auxColor = "altaccent";
            }
            else
            {
                _loadButton->setText(tr("Load"));
                _loadButton->setTextColor("text");
                _loadButton->setBorderColor("text");
                _title->setFont("default");
            }

            _subtitle->setTextColor(auxColor);
            for(ButtonWidget *b : _tags)
            {
                updateTagButtonStyle(b, auxColor);
            }*/
        }

        String packageId() const
        {
            return _item->packageId();
        }

        PopupWidget *makeInfoPopup() const
        {
            auto *pop = new DocumentPopupWidget;
            if(auto const *info = _item->info())
            {
                pop->document().setText(QString(_E(1) "%1" _E(.) "\n%2\n"
                                                _E(l) "Version: " _E(.) "%3\n"
                                                _E(l) "License: " _E(.)_E(>) "%4" _E(<)
                                                _E(l) "\nFile: " _E(.)_E(>)_E(C) "%5")
                                        .arg(info->gets("title"))
                                        .arg(packageId())
                                        .arg(info->gets("version"))
                                        .arg(info->gets("license"))
                                        .arg(_item->packageFile()->description()));
            }
            else
            {
                pop->document().setText(packageId());
            }
            return pop;
        }

    private:
        SelectedPackageItem const *_item;
        //LabelWidget *_title;
        //LabelWidget *_subtitle;
        //QList<ButtonWidget *> _tags;
        PopupButtonWidget *_infoButton;
        //ButtonWidget *_loadButton;
        //PopupButtonWidget *_infoButton;
    };

    Instance(Public *i) : Base(i)
    {
        nothingSelected = new LabelWidget;
        nothingSelected->setText(_E(b) + tr("Nothing Selected"));
        nothingSelected->setFont("heading");
        nothingSelected->setOpacity(0.5f);
        nothingSelected->rule().setRect(self.leftArea().rule());
        self.leftArea().add(nothingSelected);

        // Currently selected packages.
        self.leftArea().add(menu = new HomeMenuWidget);
        menu->layout().setRowPadding(Const(0));
        menu->rule()
                .setInput(Rule::Left,  self.leftArea().contentRule().left())
                .setInput(Rule::Top,   self.leftArea().contentRule().top())
                .setInput(Rule::Width, rule("dialog.packages.width"));
        menu->organizer().setWidgetFactory(*this);
        self.leftArea().enableIndicatorDraw(true);

        // Package browser.
        self.rightArea().add(browser = new PackagesWidget);
        browser->setPackageStatus(*this);
        browser->setButtonHandler(*this);
        browser->setButtonLabels(tr("Add"), tr("Remove"));
        //browser->setColorTheme(Normal, Normal, Normal, Normal);
        browser->rule()
                .setInput(Rule::Left,  self.rightArea().contentRule().left())
                .setInput(Rule::Top,   self.rightArea().contentRule().top())
                .setInput(Rule::Width, menu->rule().width());
        self.rightArea().enableIndicatorDraw(true);
    }

    void populate()
    {
        menu->items().clear();

        // Remove from the list those packages that are no longer listed.
        for(String packageId : selectedPackages)
        {
            menu->items() << new SelectedPackageItem(packageId);
        }

        updateNothingIndicator();
    }

    void updateNothingIndicator()
    {
        nothingSelected->setOpacity(menu->items().isEmpty()? .5f : 0.f, 0.4);
    }

    GuiWidget *makeItemWidget(ui::Item const &item, GuiWidget const *)
    {
        return new SelectedPackageWidget(item.as<SelectedPackageItem>());
    }

    void updateItemWidget(GuiWidget &widget, ui::Item const &)
    {
        widget.as<SelectedPackageWidget>().updateContents();
    }

    bool isPackageHighlighted(String const &packageId) const override
    {
        return selectedPackages.contains(packageId);
    }

    void packageButtonClicked(ButtonWidget &, String const &packageId) override
    {
        if(!selectedPackages.contains(packageId))
        {
            selectedPackages.append(packageId);
            menu->items() << new SelectedPackageItem(packageId);
        }
        else
        {
            selectedPackages.removeOne(packageId);
            auto pos = menu->items().findData(packageId);
            DENG2_ASSERT(pos != ui::Data::InvalidPos);
            menu->items().remove(pos);
        }

        updateNothingIndicator();
    }
};

PackagesDialog::PackagesDialog(String const &titleText)
    : DialogWidget("packages", WithHeading)
    , d(new Instance(this))
{
    if(titleText.isEmpty())
    {
        heading().setText(tr("Packages"));
    }
    else
    {
        heading().setText(tr("Packages: %1").arg(titleText));
    }
    heading().setImage(style().images().image("package"));
    buttons()
            << new DialogButtonItem(Default | Accept, tr("Close"))
            << new DialogButtonItem(Action, style().images().image("refresh"),
                                    new SignalAction(this, SLOT(refreshPackages())));

    // The individual menus will be scrolling independently.
    leftArea() .setContentSize(d->menu->rule().width(),    d->menu->rule().height());
    rightArea().setContentSize(d->browser->rule().width(), d->browser->rule().height());

    refreshPackages();
}

void PackagesDialog::setSelectedPackages(StringList const &packages)
{
    d->selectedPackages = packages;
    d->browser->populate();
}

StringList PackagesDialog::selectedPackages() const
{
    return d->selectedPackages;
}

void PackagesDialog::refreshPackages()
{
    App::fileSystem().refresh();
    d->populate();
}

void PackagesDialog::preparePanelForOpening()
{
    DialogWidget::preparePanelForOpening();
    d->populate();

    root().setFocus(&d->browser->searchTermsEditor());
}
