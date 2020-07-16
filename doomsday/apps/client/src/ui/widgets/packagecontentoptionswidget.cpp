/** @file packagecontentoptionswidget.cpp  Widget for package content options.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/packagecontentoptionswidget.h"

#include <de/buttonwidget.h>
#include <de/config.h>
#include <de/dictionaryvalue.h>
#include <de/labelwidget.h>
#include <de/menuwidget.h>
#include <de/packageloader.h>
#include <de/popupwidget.h>
#include <de/sequentiallayout.h>
#include <de/togglewidget.h>
#include <de/textvalue.h>

using namespace de;

DE_GUI_PIMPL(PackageContentOptionsWidget)
, public ChildWidgetOrganizer::IWidgetFactory
{
    /**
     * Item representing a contained package.
     */
    struct Item : public ui::Item
    {
        bool selectedByDefault;
        String containerPackageId;
        String category;

        Item(const String &packageId, bool selectedByDefault, const String &containerPackageId)
            : selectedByDefault(selectedByDefault)
            , containerPackageId(containerPackageId)
        {
            setData(TextValue(packageId));

            if (const File *file = PackageLoader::get().select(packageId))
            {
                const Record &meta = file->objectNamespace();
                setLabel(meta.gets(Package::VAR_PACKAGE_TITLE));
                category = meta.gets(DE_STR("package.category"), "");
            }
            else
            {
                setLabel(packageId);
            }

            if (!selectedByDefault)
            {
                setLabel(label() + " " _E(s)_E(b)_E(D) "ALT");
            }
        }

        String packageId() const
        {
            return data().asText();
        }

        DictionaryValue &conf()
        {
            return Config::get("fs.selectedPackages").value<DictionaryValue>();
        }

        const DictionaryValue &conf() const
        {
            return Config::get("fs.selectedPackages").value<DictionaryValue>();
        }

        bool isSelected() const
        {
            if (conf().contains(TextValue(containerPackageId)))
            {
                const DictionaryValue &sel = conf().element(TextValue(containerPackageId))
                                             .as<DictionaryValue>();
                if (sel.contains(TextValue(packageId())))
                {
                    return sel.element(TextValue(packageId())).isTrue();
                }
            }
            return selectedByDefault;
        }

        void setSelected(bool selected)
        {
            if (!conf().contains(TextValue(containerPackageId)))
            {
                conf().add(new TextValue(containerPackageId),
                           new DictionaryValue);
            }
            DictionaryValue &sel = conf().element(TextValue(containerPackageId))
                                   .as<DictionaryValue>();
            sel.add(new TextValue(packageId()), new NumberValue(selected));
            notifyChange();
        }

        void reset()
        {
            conf().remove(TextValue(containerPackageId));
            notifyChange();
        }
    };

    String packageId;
    LabelWidget *summary;
    MenuWidget *contents;

    Impl(Public *i, const String &packageId, const Rule &maxHeight)
        : Base(i)
        , packageId(packageId)
    {
        self().add(summary = new LabelWidget);
        summary->setSizePolicy(ui::Fixed, ui::Expand);
        summary->setFont("small");
        summary->setTextColor("altaccent");
        summary->setAlignment(ui::AlignLeft);

        auto *label = LabelWidget::newWithText("Select:", thisPublic);
        label->setSizePolicy(ui::Expand, ui::Expand);
        label->setFont("small");
        label->setAlignment(ui::AlignLeft);

        auto *allButton = new ButtonWidget;
        allButton->setSizePolicy(ui::Expand, ui::Expand);
        allButton->setText("All");
        allButton->setFont("small");
        self().add(allButton);

        auto *noneButton = new ButtonWidget;
        noneButton->setSizePolicy(ui::Expand, ui::Expand);
        noneButton->setText("None");
        noneButton->setFont("small");
        self().add(noneButton);

        auto *defaultsButton = new ButtonWidget;
        defaultsButton->setSizePolicy(ui::Expand, ui::Expand);
        defaultsButton->setText("Defaults");
        defaultsButton->setFont("small");
        self().add(defaultsButton);

        contents = new MenuWidget;
        contents->enableIndicatorDraw(true);
        contents->setBehavior(ChildVisibilityClipping);
        contents->setVirtualizationEnabled(true, rule(RuleBank::UNIT).value()*2 +
                                           style().fonts().font("default").height().value());
        contents->layout().setRowPadding(Const(0));
        contents->organizer().setWidgetFactory(*this);
        contents->setGridSize(1, ui::Filled, 0, ui::Fixed);
        self().add(contents);

        // Layout.
        auto &rect = self().rule();
        SequentialLayout layout(rect.left() + self().margins().left(),
                                rect.top()  + self().margins().top());
        layout << *label << *summary << *contents;
        summary->rule()
                .setInput(Rule::Width,  rect.width() - self().margins().width());
        contents->rule()
                .setInput(Rule::Left,   rect.left())
                .setInput(Rule::Width,  summary->rule().width() + self().margins().left())
                .setInput(Rule::Height, OperatorRule::minimum(contents->contentHeight(),
                                                              maxHeight - self().margins().height() -
                                                              label->rule().height() -
                                                              summary->rule().height()));

        SequentialLayout(label->rule().right(), label->rule().top(), ui::Right)
               << *defaultsButton << *noneButton << *allButton;
        self().rule().setInput(Rule::Height, layout.height() + self().margins().bottom());

        // Configure margins.
        for (GuiWidget *w : self().childWidgets())
        {
            w->margins().set("dialog.gap");
        }
        contents->margins().setLeftRight("gap");

        // Actions.
        defaultsButton->setActionFn([this] ()
        {
            contents->items().forAll([] (ui::Item &item) {
                if (auto *i = maybeAs<Item>(item)) i->reset();
                return LoopContinue;
            });
        });
        noneButton->setActionFn([this] ()
        {
            contents->items().forAll([] (ui::Item &item) {
                if (auto *i = maybeAs<Item>(item)) i->setSelected(false);
                return LoopContinue;
            });
        });
        allButton->setActionFn([this] ()
        {
            contents->items().forAll([] (ui::Item &item) {
                if (auto *i = maybeAs<Item>(item)) i->setSelected(true);
                return LoopContinue;
            });
        });
    }

    void populate()
    {
        contents->items().clear();

        const File *file = PackageLoader::get().select(packageId);
        if (!file) return;

        const Record &meta = file->objectNamespace().subrecord(Package::VAR_PACKAGE);

        const ArrayValue &requires   = meta.geta("requires");
        const ArrayValue &recommends = meta.geta("recommends");
        const ArrayValue &extras     = meta.geta("extras");

        const auto totalCount    = requires.size() + recommends.size() + extras.size();
        const auto optionalCount = recommends.size() + extras.size();

        summary->setText(Stringf(
            "%u package%s (%u optional)", totalCount, DE_PLURAL_S(totalCount), optionalCount));

        makeItems(recommends, true);
        makeItems(extras,     false);

        // Create category headings.
        Set<String> categories;
        contents->items().forAll([&categories](const ui::Item &i)
        {
            const String cat = i.as<Item>().category;
            if (!cat.isEmpty()) categories.insert(cat);
            return LoopContinue;
        });
        for (const auto &cat : categories)
        {
            contents->items() << new ui::Item(ui::Item::Separator, cat);
        }

        // Sort all the items by category.
        contents->items().sort([] (const ui::Item &s, const ui::Item &t)
        {
            if (!s.isSeparator() && !t.isSeparator())
            {
                const Item &a = s.as<Item>();
                const Item &b = t.as<Item>();

                const int catComp = a.category.compareWithoutCase(b.category);
                if (!catComp)
                {
                    const int nameComp = a.label().compareWithoutCase(b.label());
                    return nameComp < 0;
                }
                return catComp < 0;
            }

            if (s.isSeparator() && !t.isSeparator())
            {
                return s.label().compareWithoutCase(t.as<Item>().category) <= 0;
            }

            if (!s.isSeparator() && t.isSeparator())
            {
                return s.as<Item>().category.compareWithoutCase(t.label()) < 0;
            }

            return s.label().compareWithoutCase(t.label()) < 0;
        });
    }

    void makeItems(const ArrayValue &ids, bool recommended)
    {
        for (const Value *value : ids.elements())
        {
            contents->items() << new Item(value->asText(), recommended, packageId);
        }
    }

//- ChildWidgetOrganizer::IWidgetFactory ------------------------------------------------

    GuiWidget *makeItemWidget(const ui::Item &item, const GuiWidget *) override
    {
        if (item.semantics() & ui::Item::Separator)
        {
            auto *label = new LabelWidget;
            label->setFont("separator.label");
            label->setTextColor("accent");
            label->margins().setBottom(RuleBank::UNIT);
            label->setSizePolicy(ui::Fixed, ui::Expand);
            label->setAlignment(ui::AlignLeft);
            return label;
        }

        auto *toggle = new ToggleWidget;
        toggle->setSizePolicy(ui::Fixed, ui::Expand);
        toggle->setAlignment(ui::AlignLeft);
        toggle->set(Background());
        toggle->margins().setTopBottom(RuleBank::UNIT);
        toggle->audienceForUserToggle() += [&item, toggle]() {
            const_cast<ui::Item &>(item).as<Item>().setSelected(toggle->isActive());
        };
        return toggle;
    }

    void updateItemWidget(GuiWidget &widget, const ui::Item &item) override
    {
        LabelWidget &label = widget.as<LabelWidget>();
        label.setText(item.label());

        if (!(item.semantics() & ui::Item::Separator))
        {
            widget.as<ToggleWidget>().setActive(item.as<Item>().isSelected());
        }
    }
};

PackageContentOptionsWidget::PackageContentOptionsWidget(const String &packageId,
                                                         const Rule &maxHeight,
                                                         const String &name)
    : GuiWidget(name)
    , d(new Impl(this, packageId, maxHeight))
{
    d->populate();
}

PopupWidget *PackageContentOptionsWidget::makePopup(const String &packageId,
                                                    const Rule &width,
                                                    const Rule &maxHeight)
{
    PopupWidget *pop = new PopupWidget;
    pop->setOutlineColor("popup.outline");
    pop->setDeleteAfterDismissed(true);
    //pop->setAnchorAndOpeningDirection(rule(), ui::Left);
    /*pop->closeButton().setActionFn([this] ()
    {
        root().setFocus(this);
        _optionsPopup->close();
    });*/

    auto *opts = new PackageContentOptionsWidget(packageId, maxHeight);
    opts->rule().setInput(Rule::Width, width);
    pop->setContent(opts);
    //add(_optionsPopup);
    //_optionsPopup->open();
    return pop;
}
