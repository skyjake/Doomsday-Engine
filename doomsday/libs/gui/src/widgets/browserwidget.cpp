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

#include "de/BrowserWidget"
#include "de/ButtonWidget"
#include "de/DialogContentStylist"
#include "de/FlowLayout"
#include "de/MenuWidget"
#include "de/ProgressWidget"
#include "de/ScrollAreaWidget"
#include "de/SequentialLayout"

namespace de {

DE_GUI_PIMPL(BrowserWidget)
{
    struct SavedState
    {
        int scrollPosY;
    };

    const ui::TreeData *data = nullptr;
    Path path;
    Map<Path, SavedState> savedState;
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
        layout << *cwdLabel;

        pathFlow.reset(new FlowLayout(cwdLabel->rule().left(), cwdLabel->rule().bottom(),
                                      *contentWidth));
        layout.append(pathFlow->height());

        menuLabel = LabelWidget::appendSeparatorWithText("Contents", i);
        layout << *menuLabel;

        scroller = &i->addNew<ScrollAreaWidget>("scroller");
        layout << *scroller;
        scroller->rule()
            .setInput(Rule::Bottom, rule.bottom());
        scroller->margins().setZero();

        menu = &scroller->addNew<MenuWidget>("items");
        menu->setGridSize(1, ui::Filled, 0, ui::Expand);
        menu->rule()
            .setLeftTop(scroller->contentRule().left(), scroller->contentRule().top())
            .setInput(Rule::Width, *contentWidth - this->rule("scrollarea.bar"));
        menu->enableScrolling(false);
        menu->enablePageKeys(false);

        // Virtualized menu expands to virtual height, so use another scroller as a parent.
        scroller->setContentSize(menu->rule());
        scroller->enablePageKeys(true);
        scroller->enableScrolling(true);
        scroller->enableIndicatorDraw(true);
    }

    ~Impl()
    {
        releaseRef(contentWidth);
    }

    void changeTo(const Path &newPath)
    {
        debug("[BrowserWidget] change to '%s'", newPath.c_str());

        DE_ASSERT(data);

        savedState[path].scrollPosY = scroller->scrollPosition().y;

        // TODO: This is an async op, need to show progress widget.

        path = newPath;
        createPathButtons();
        const ui::Data &items = data->items(path);
        menu->setItems(items);

        if (savedState.contains(path))
        {
            scroller->scrollY(savedState[path].scrollPosY);
        }
        else
        {
            scroller->scrollY(0);
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
            button.setText(Stringf("%s/", String(segment).c_str()));

            if (i == path.segmentCount() - 1)
            {
                button.setTextColor("accent");
            }
            else
            {
                const Path buttonPath = path.subPath({0, i + 1});
                button.audienceForPress() += [this, buttonPath]() {
                    dispatch += [this, buttonPath](){
                        changeTo(buttonPath);
                    };
                };
            }

            pathFlow->append(button);
        }
    }

    DE_PIMPL_AUDIENCE(Trigger)
};

DE_AUDIENCE_METHOD(BrowserWidget, Trigger)

BrowserWidget::BrowserWidget(const String &name)
    : GuiWidget(name)
    , d(new Impl(this))
{}

void BrowserWidget::setData(const ui::TreeData &data, int averageItemHeight)
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
    d->changeTo(path);
}

} // namespace de
