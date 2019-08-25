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
#include "de/MenuWidget"
#include "de/ProgressWidget"
#include "de/ScrollAreaWidget"
#include "de/SequentialLayout"

namespace de {

DE_PIMPL(BrowserWidget)
//, DE_OBSERVES(ChildWidgetOrganizer, WidgetCreation)
{
    const ui::TreeData *data = nullptr;
    Path path;
    LabelWidget *cwdLabel;
    LabelWidget *currentPath;
    LabelWidget *menuLabel;
    ScrollAreaWidget *scroller;
    MenuWidget *menu;

    Impl(Public *i)
        : Base(i)
    {
        RuleRectangle &rule = self().rule();

        SequentialLayout layout(rule.left(), rule.top(), ui::Down);
        layout.setOverrideWidth(rule.width());

        cwdLabel = LabelWidget::appendSeparatorWithText("Path", i);
        layout << *cwdLabel;

        currentPath = new LabelWidget("cwd");
        currentPath->setSizePolicy(ui::Fixed, ui::Expand);
        currentPath->setAlignment(ui::AlignLeft);
        layout << *currentPath;

        menuLabel = LabelWidget::appendSeparatorWithText("Contents", i);
        layout << *menuLabel;

        scroller = new ScrollAreaWidget("scroller");
        layout << *scroller;
        scroller->rule()
            .setInput(Rule::Bottom, rule.bottom());

        menu = new MenuWidget("items");
        menu->setGridSize(1, ui::Filled, 0, ui::Expand);
        menu->rule()
            .setLeftTop(scroller->contentRule().left(), scroller->contentRule().top())
            .setInput(Rule::Width, rule.width());
        menu->enableScrolling(false);
        menu->enablePageKeys(false);

        // Virtualized menu expands to virtual height, so use another scroller as a parent.
        scroller->setContentSize(menu->rule());
        scroller->enablePageKeys(true);
        scroller->enableScrolling(true);
        scroller->enableIndicatorDraw(true);

        i->add(currentPath);
        scroller->add(menu);
        i->add(scroller);

//        menu->organizer().audienceForWidgetCreation() += this;
    }

    void changeTo(const Path &newPath)
    {
        DE_ASSERT(data);

        scroller->scrollY(0);

        // TODO: This is an async op, need to show progress widget.
        path = newPath;
        currentPath->setText(path);
        const ui::Data &items = data->items(path);
        menu->setItems(items);

        // TODO: Recreate the path segment buttons.
    }

//    void widgetCreatedForItem(GuiWidget &widget, const ui::Item &item) override
//    {
////        widget.as<ButtonWidget>().audienceForPress() += [this, &item]() {

////        };
//    }
};

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
