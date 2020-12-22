/** @file directorylistdialog.cpp  Dialog for editing a list of directories.
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

#include "de/directorylistdialog.h"
#include "de/directoryarraywidget.h"

namespace de {

DE_PIMPL(DirectoryListDialog)
{
    struct Group
    {
        LabelWidget *title;
        LabelWidget *description;
        Variable array;
        DirectoryArrayWidget *list;
    };
    Hash<Id::Type, Group *> groups;

    Impl(Public *i) : Base(i)
    {}

    ~Impl()
    {
        groups.deleteAll();
    }

    Id addGroup(const String &title, const String &description)
    {
        Id groupId;
        std::unique_ptr<Group> group(new Group);

        self().area().add(group->title = new LabelWidget("group-title"));
        group->title->setText(title);
        group->title->setMaximumTextWidth(self().area().rule().width() -
                                          self().margins().width());
        group->title->setTextLineAlignment(ui::AlignLeft);
        group->title->setAlignment(ui::AlignLeft);
        group->title->setFont("separator.label");
        group->title->setTextColor("accent");
        group->title->margins().setTop("gap");

        self().area().add(group->description = new LabelWidget("group-desc"));
        group->description->setText(description);
        group->description->setFont("small");
        group->description->setTextColor("altaccent");
        group->description->margins().setTop("").setBottom("");
        group->description->setMaximumTextWidth(self().area().rule().width() -
                                                self().margins().width());
        group->description->setTextLineAlignment(ui::AlignLeft);
        group->description->setAlignment(ui::AlignLeft);
        group->description->margins().setBottom(ConstantRule::zero());

        group->array.set(new ArrayValue);
        group->list = new DirectoryArrayWidget(group->array, "group-direc-array");
        group->list->margins().setZero();
        self().add(group->list->detachAddButton(self().area().rule().width()));
        group->list->addButton().hide();
        self().area().add(group->list);

        group->list->audienceForChange() += [this](){
            DE_NOTIFY_PUBLIC(Change, i) { i->directoryListChanged(); }
        };

        groups.insert(groupId, group.release());
        return groupId;
    }

    DE_PIMPL_AUDIENCE(Change)
};

DE_AUDIENCE_METHOD(DirectoryListDialog, Change)

DirectoryListDialog::DirectoryListDialog(const String &name)
    : MessageDialog(name)
    , d(new Impl(this))
{
    area().enableIndicatorDraw(true);
    buttons() << new DialogButtonItem(Default | Accept)
              << new DialogButtonItem(Reject)
              << new DialogButtonItem(Action, style().images().image("create"),
                                      "Add Folder",
                                      new CallbackAction([this]() {
        d->groups.begin()->second->list->addButton().trigger();
    }));
}

Id DirectoryListDialog::addGroup(const String &title, const String &description)
{
    return d->addGroup(title, description);
}

void DirectoryListDialog::prepare()
{
    MessageDialog::prepare();
    updateLayout();
}

void DirectoryListDialog::setValue(const Id &id, const Value &elements)
{
    DE_ASSERT(d->groups.contains(id));
    d->groups[id]->array.set(elements);
}

const Value &DirectoryListDialog::value(const Id &id) const
{
    DE_ASSERT(d->groups.contains(id));
    return d->groups[id]->array.value();
}

} // namespace de
