/** @file directorylistdialog.cpp  Dialog for editing a list of directories.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/DirectoryListDialog"
#include "de/DirectoryArrayWidget"

#include <de/SignalAction>

namespace de {

DENG2_PIMPL(DirectoryListDialog)
{
    Variable array;
    DirectoryArrayWidget *list;

    Impl(Public *i) : Base(i)
    {
        array.set(new ArrayValue);
        list = new DirectoryArrayWidget(array);
        list->margins().setZero();
        self().add(list->detachAddButton(self().area().rule().width()));
        list->addButton().hide();
    }
};

DirectoryListDialog::DirectoryListDialog(String const &name)
    : MessageDialog(name)
    , d(new Impl(this))
{
    area().add(d->list);

    buttons() << new DialogButtonItem(Default | Accept)
              << new DialogButtonItem(Reject)
              << new DialogButtonItem(Action, style().images().image("create"),
                                      tr("Add Folder"),
                                      new SignalAction(&d->list->addButton(), SLOT(trigger())));

    updateLayout();
}

void DirectoryListDialog::setValue(Value const &elements)
{
    d->array.set(elements);
}

Value const &DirectoryListDialog::value() const
{
    return d->array.value();
}

} // namespace de
