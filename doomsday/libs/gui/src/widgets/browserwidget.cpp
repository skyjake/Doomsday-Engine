/** @file browserwidget.cpp  Browser for tree data.
 *
 * @authors Copyright (c) 2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/browserwidget.h"
#include "de/buttonwidget.h"
#include "de/dialogcontentstylist.h"
#include "de/flowlayout.h"
#include "de/menuwidget.h"
#include "de/progresswidget.h"
#include "de/scrollareawidget.h"
#include "de/sequentiallayout.h"

namespace de {

DE_GUI_PIMPL(BrowserWidget)
{
    struct SavedState
    {
        int scrollPosY;
    };

    ui::TreeData *data = nullptr;
    List<ui::DataPos> selectedItems;
    Path path;
    KeyMap<Path, SavedState> savedState;
    LabelWidget *noContents;
    LabelWidget *cwdLabel;
    LabelWidget *menuLabel;
    ScrollAreaWidget *scroller;
    MenuWidget *menu;
    std::unique_ptr<FlowLayout> pathFlow;
    DialogContentStylist stylist;
    Dispatch dispatch;
    const Rule *contentWidth;

    Impl(Public *i)
        : Base(i)
        , stylist(*i)
    {
        RuleRectangle &rule = self().rule();
        contentWidth = holdRef(rule.width() - self().margins().width());

        SequentialLayout layout(
            rule.left() + self().margins().left(), rule.top() + self().margins().top(), ui::Down);
        layout.setOverrideWidth(*contentWidth);

        cwdLabel = LabelWidget::appendSeparatorWithText("Path", i);
        cwdLabel->margins().setLeft(Const(0)).setTop(Const(0));
        layout << *cwdLabel;

        pathFlow.reset(new FlowLayout(cwdLabel->rule().left(), cwdLabel->rule().bottom(),
                                      *contentWidth));
        layout.append(pathFlow->height());

        menuLabel = LabelWidget::appendSeparatorWithText("Contents", i);
        menuLabel->margins().setLeft(Const(0));
        layout << *menuLabel;

        scroller = &i->addNew<ScrollAreaWidget>("scroller");
        layout << *scroller;
        scroller->rule()
            .setInput(Rule::Bottom, rule.bottom());
        scroller->margins().setZero();

        menu = &scroller->addNew<MenuWidget>("items");
        menu->setGridSize(1, ui::Filled, 0, ui::Expand);
        menu->margins().setZero().setRight("dialog.gap");
        menu->rule()
            .setLeftTop(scroller->contentRule().left(), scroller->contentRule().top())
            .setInput(Rule::Width, *contentWidth - menu->margins().width());
        menu->enableScrolling(false);
        menu->enablePageKeys(false);

        // Virtualized menu expands to virtual height, so use another scroller as a parent.
        scroller->setContentSize(menu->rule());
        scroller->enablePageKeys(true);
        scroller->enableScrolling(true);
        scroller->enableIndicatorDraw(true);

        noContents = &i->addNew<LabelWidget>();
        noContents->setText("No Contents");
        style().emptyContentLabelStylist().applyStyle(*noContents);
        noContents->rule().setRect(scroller->rule());
    }

    ~Impl()
    {
        releaseRef(contentWidth);
    }

    void changeTo(const Path &newPath, bool createButtons)
    {
        if (path == newPath && !createButtons) return;

        debug("[BrowserWidget] change to '%s'", newPath.c_str());

        DE_ASSERT(data);

        clearSelection();
        savedState[path].scrollPosY = scroller->scrollPosition().y;

        // TODO: This is an async op, need to show progress widget.

        path = newPath;
        if (createButtons)
        {
            createPathButtons();
        }
        const ui::Data &items = data->items(path);
        menu->setItems(items);
        noContents->show(items.isEmpty());

        if (savedState.contains(path))
        {
            scroller->scrollY(savedState[path].scrollPosY);
        }
        else
        {
            scroller->scrollY(0);
        }

        DE_NOTIFY_PUBLIC(Navigation, i)
        {
            i->browserNavigatedTo(self(), path);
        }
    }

    void createPathButtons()
    {
        // Get rid of the old buttons.
        for (auto *i : pathFlow->widgets())
        {
            GuiWidget::destroy(i);
        }
        pathFlow->clear();

        // Create a new button for each segment of the path.
        for (int i = 0; i < path.segmentCount(); ++i)
        {
            const auto &segment = path.segment(i);

            auto &button = self().addNew<ButtonWidget>();
            button.setSizePolicy(ui::Expand, ui::Expand);
            button.setMaximumTextWidth(*contentWidth);
            const String segStr = segment.toCString();
            button.setText(segStr.isEmpty() ? DE_STR("/") : segStr);

            if (i == path.segmentCount() - 1)
            {
                button.setTextColor("accent");                
            }

            const Path buttonPath = path.subPath({0, i + 1});
            button.audienceForPress() += [this, &button, buttonPath]() {
                resetPathButtonColors();
                button.setTextColor("accent");
                dispatch += [this, buttonPath](){
                    changeTo(buttonPath, false);
                };
            };

            pathFlow->append(button);
        }

        if (hasRoot()) root().setFocus(pathFlow->widgets().back());
    }

    void resetPathButtonColors()
    {
        for (auto *button : pathFlow->widgets())
        {
            button->setTextColor("text");
        }
    }

    void clearSelection()
    {
        if (!menu->items().isEmpty())
        {
            auto &items = data->items(path);
            for (const auto pos : selectedItems)
            {
                if (pos < items.size())
                {
                    items.at(pos).setSelected(false);
                }
            }
        }
        selectedItems.clear();
    }

    DE_PIMPL_AUDIENCES(Navigation)
};

DE_AUDIENCE_METHODS(BrowserWidget, Navigation)

BrowserWidget::BrowserWidget(const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{}

void BrowserWidget::setEmptyContentText(const String &text)
{
    d->noContents->setText(text);
}

void BrowserWidget::setData(ui::TreeData &data, int averageItemHeight)
{
    d->data = &data;
    d->menu->organizer().setRecyclingEnabled(true);
    d->menu->setVirtualizationEnabled(true, averageItemHeight);
}

const ui::TreeData &BrowserWidget::data() const
{
    return *d->data;
}

MenuWidget &BrowserWidget::menu()
{
    return *d->menu;
}

void BrowserWidget::setCurrentPath(const Path &path)
{
    d->changeTo(path, true);
}

Path BrowserWidget::currentPath() const
{
    return d->path;
}

void BrowserWidget::setSelected(const ui::Item &item)
{
    d->clearSelection();

    auto &items = d->data->items(d->path);
    const ui::DataPos pos = items.find(item);
    DE_ASSERT(pos != ui::Data::InvalidPos);
    d->selectedItems.push_back(pos);
    items.at(pos).setSelected(true);
}

List<const ui::Item *> BrowserWidget::selected() const
{
    return map<List<const ui::Item *>>(d->selectedItems, [this](ui::DataPos pos) {
        return &d->data->items(d->path).at(pos);
    });
}

} // namespace de
