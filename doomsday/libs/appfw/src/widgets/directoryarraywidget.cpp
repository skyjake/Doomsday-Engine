/** @file directoryarraywidget.cpp  Widget for an array of native directories.
 *
 * @authors Copyright (c) 2016-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/DirectoryArrayWidget"
#include "de/BaseGuiApp"
#include "de/BaseWindow"

#include <de/Config>
#include <de/Garbage>
#include <de/NativePath>
#include <de/TextValue>
#include <de/ToggleWidget>
#include <QFileDialog>

namespace de {

static String const CFG_LAST_FOLDER("resource.latestDirectory");

DE_PIMPL_NOREF(DirectoryArrayWidget)
{};

DirectoryArrayWidget::DirectoryArrayWidget(Variable &variable, String const &name)
    : VariableArrayWidget(variable, name)
    , d(new Impl)
{
    addButton().setText(tr("Add Folder..."));
    addButton().setActionFn([this] ()
    {
        // Use a native dialog to select the IWAD folder.
        DE_BASE_GUI_APP->beginNativeUIMode();

        QFileDialog dlg(nullptr, tr("Select Folder"),
                        Config::get().gets(CFG_LAST_FOLDER, "."), "");
        dlg.setFileMode(QFileDialog::Directory);
        dlg.setReadOnly(true);
        //dlg.setNameFilter("*.wad");
        dlg.setLabelText(QFileDialog::Accept, tr("Select"));
        if (dlg.exec())
        {
            String dir = dlg.selectedFiles().at(0);
            Config::get().set(CFG_LAST_FOLDER, dir.fileNamePath());
            elementsMenu().items() << makeItem(TextValue(dir));
            setVariableFromWidget();
        }

        DE_BASE_GUI_APP->endNativeUIMode();
    });

    updateFromVariable();
}

String DirectoryArrayWidget::labelForElement(Value const &value) const
{
    return NativePath(value.asText()).pretty();
}

static const String RECURSE_TOGGLE_NAME("recurse-toggle");

/**
 * Controller that syncs state between Config.resource.recurseFolders and the toggles
 * in the DirectoryArrayWidget items. Destroys itself after the item widget is deleted.
 */
struct RecurseToggler
    : DENG2_OBSERVES(ToggleWidget, Toggle)
    , DENG2_OBSERVES(Widget, Deletion)
    , DENG2_OBSERVES(ui::Item, Change)
    , DENG2_OBSERVES(ChildWidgetOrganizer, WidgetUpdate)
{
    DirectoryArrayWidget *owner;
    ToggleWidget *        tog;
    const ui::Item *      item;

    RecurseToggler(DirectoryArrayWidget *owner, LabelWidget &element, const ui::Item &item)
        : owner(owner)
        , item(&item)
    {
        tog = &element.guiFind(RECURSE_TOGGLE_NAME)->as<ToggleWidget>();
        item.audienceForChange() += this;
        element.audienceForDeletion() += this;
        tog->audienceForToggle() += this;
        owner->elementsMenu().organizer().audienceForWidgetUpdate() += this;
    }

    static Variable &recursed()
    {
        return Config::get("resource.recursedFolders");
    }

    TextValue key() const
    {
        return {item->data().toString()};
    }

    void fetch()
    {
        if (recursed().value().contains(key()))
        {
            tog->setActive(recursed().value().element(key()).isTrue());
        }
    }

    void toggleStateChanged(ToggleWidget &toggle) override
    {
        recursed().value().setElement(key(), new NumberValue(toggle.isActive()));
        emit owner->arrayChanged();
    }

    void widgetBeingDeleted(Widget &) override
    {
        item->audienceForChange() -= this;
        // tog is already gone
        trash(this);
    }

    void itemChanged(const ui::Item &) override
    {
        fetch();
    }

    void widgetUpdatedForItem(GuiWidget &, const ui::Item &) override
    {
        fetch();
    }
};

void DirectoryArrayWidget::elementCreated(LabelWidget &element, const ui::Item &item)
{
    element.setSizePolicy(ui::Fixed, ui::Expand);
    element.setAlignment(ui::AlignLeft);
    element.setTextLineAlignment(ui::AlignLeft);
    element.setMaximumTextWidth(rule().width());
    element.rule().setInput(Rule::Width, rule().width() - margins().width());

    // Add a toggle for configuration recurse mode.
    auto *tog = new ToggleWidget(ToggleWidget::DefaultFlags, RECURSE_TOGGLE_NAME);
    element.add(tog);
    tog->setText("Subdirs");
    tog->setActive(true); // recurse is on by default
    tog->set(Background());
    tog->setFont("small");
    tog->margins().setLeft("unit").setRight("gap").setTop("unit").setBottom("unit");
    tog->setSizePolicy(ui::Expand, ui::Expand);
    tog->rule()
            .setInput(Rule::Right, element.rule().right() - rule("gap"))
            .setMidAnchorY(element.rule().midY());
    element.margins().setRight(tog->rule().width() + rule("gap"));

    new RecurseToggler(this, element, item); // deletes itself
}

} // namespace de
