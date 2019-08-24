/** @file directorybrowserwidget.cpp
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

#include "de/DirectoryBrowserWidget"
#include "de/DirectoryTreeData"
#include "de/ChildWidgetOrganizer"

namespace de {

DE_GUI_PIMPL(DirectoryBrowserWidget)
, public ChildWidgetOrganizer::IWidgetFactory
{
    DirectoryTreeData dirTree;
    IndirectRule *itemHeight = new IndirectRule;

    Impl(Public *i) : Base(i)
    {
        itemHeight->setSource(font("default").height());

        self().menu().organizer().setWidgetFactory(*this);
        self().setData(dirTree, itemHeight->valuei());
    }

    ~Impl()
    {
        releaseRef(itemHeight);
    }

    GuiWidget *makeItemWidget(const ui::Item &, const GuiWidget *)
    {
        auto *lab = new LabelWidget;
        lab->setSizePolicy(ui::Fixed, ui::Fixed);
        lab->rule().setInput(Rule::Height, itemHeight);
        return lab;
    }

    void updateItemWidget(GuiWidget &widget, const ui::Item &item)
    {
        const auto &dirItem = item.as<DirectoryItem>();

        String text;
        if (dirItem.isDirectory())
        {
            text = Stringf("%s/", dirItem.name().c_str());
        }
        else
        {
            text = Stringf("%s (%zu) %s",
                           dirItem.name().c_str(),
                           dirItem.status().size,
                           dirItem.status().modifiedAt.asText().c_str());
        }
        widget.as<LabelWidget>().setText(text);
    }
};

DirectoryBrowserWidget::DirectoryBrowserWidget(const String &name)
    : BrowserWidget(name)
    , d(new Impl(this))
{}

} // namespace de
