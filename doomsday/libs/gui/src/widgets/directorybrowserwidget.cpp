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
    class ItemWidget : public ButtonWidget
    {
//        const DirectoryItem *_item{};
        LabelWidget *_status;

    public:
        ItemWidget(const Rule &height)
        {
            setSizePolicy(ui::Fixed, ui::Fixed);
            setAlignment(ui::AlignLeft);
            rule().setInput(Rule::Height, height);
            margins().setTopBottom(rule("unit"));

            _status = &addNew<LabelWidget>();
            _status->setSizePolicy(ui::Expand, ui::Fixed);
            _status->rule()
                .setInput(Rule::Right, rule().right())
                .setInput(Rule::Top, rule().top())
                .setInput(Rule::Height, height);
            _status->setTextColor("altaccent");

            margins().setRight(_status->rule().width());
        }

        void updateItem(const DirectoryItem &dirItem)
        {
            if (dirItem.isDirectory())
            {
                setText(_E(F) + dirItem.name() + _E(l) "/");
                _status->setText("(dir)");
            }
            else
            {
                setText(dirItem.name());

                const auto size = dirItem.status().size;
                String sizeText;
                if (size < 1000)
                {
                    sizeText = Stringf("%zu" _E(l) " B", size);
                }
                else if (size < 1000000)
                {
                    sizeText = Stringf("%.1f" _E(l) " KB", double(size) / 1.0e3);
                }
                else if (size < 1000000000)
                {
                    sizeText = Stringf("%.1f" _E(l) " MB", double(size) / 1.0e6);
                }
                else
                {
                    sizeText = Stringf("%.1f" _E(l) " GB", double(size) / 1.0e9);
                }
                _status->setText(sizeText);
            }
        }
    };

    DirectoryTreeData dirTree;
    IndirectRule *itemHeight = new IndirectRule;
    Dispatch dispatch;

    Impl(Public *i) : Base(i)
    {
        itemHeight->setSource(font("default").height() + rule("unit") * 2);

        self().menu().organizer().setWidgetFactory(*this);
        self().setData(dirTree, itemHeight->valuei());
    }

    ~Impl()
    {
        releaseRef(itemHeight);
    }

    GuiWidget *makeItemWidget(const ui::Item &, const GuiWidget *)
    {
        auto *widget = new ItemWidget(*itemHeight);

        widget->audienceForPress() += [this, widget]() {
            // Changing the directory causes this widget to be deleted, so
            // we need to postpone the change until the press has been
            // handled.
            const ui::DataPos pos     = self().menu().findItem(*widget);
            const auto &      dirItem = self().menu().items().at(pos).as<DirectoryItem>();
            if (dirItem.isDirectory())
            {
                const NativePath toDir = dirItem.path();
                dispatch += [this, toDir]() { self().setCurrentPath(toDir); };
            }
            else
            {
                DE_FOR_PUBLIC_AUDIENCE(Selection, i)
                {
                    i->pathSelected(self(), dirItem.path());
                }
            }
        };

        return widget;
    }

    void updateItemWidget(GuiWidget &widget, const ui::Item &item)
    {
        widget.as<ItemWidget>().updateItem(item.as<DirectoryItem>());
    }

    DE_PIMPL_AUDIENCES(Selection)
};

DE_AUDIENCE_METHODS(DirectoryBrowserWidget, Selection)

DirectoryBrowserWidget::DirectoryBrowserWidget(const String &name)
    : BrowserWidget(name)
    , d(new Impl(this))
{}

} // namespace de
