/** @file item.cpp  Context item.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/ui/Item"

namespace de {
namespace ui {

DENG2_PIMPL_NOREF(Item)
{
    Data *context;
    Semantics semantics;
    String label;
    QVariant data;

    Instance(Semantics sem, String const &text = "", QVariant var = QVariant())
        : context(0)
        , semantics(sem)
        , label(text)
        , data(var) {}

    DENG2_PIMPL_AUDIENCE(Change)
};

DENG2_AUDIENCE_METHOD(Item, Change)

Item::Item(Semantics semantics)
    : d(new Instance(semantics))
{}

Item::Item(Semantics semantics, String const &label)
    : d(new Instance(semantics, label))
{}

Item::~Item()
{}

Item::Semantics Item::semantics() const
{
    return d->semantics;
}

void Item::setLabel(String const &label)
{
    d->label = label;
    notifyChange();
}

String Item::label() const
{
    return d->label;
}

void Item::setDataContext(Data &context)
{
    d->context = &context;
}

bool Item::hasDataContext() const
{
    return d->context != 0;
}

Data &Item::dataContext() const
{
    DENG2_ASSERT(hasDataContext());
    return *d->context;
}

String Item::sortKey() const
{
    return d->label;
}

void Item::setData(QVariant const &v)
{
    d->data = v;
}

QVariant const &Item::data() const
{
    return d->data;
}

void Item::notifyChange()
{
    DENG2_FOR_AUDIENCE2(Change, i)
    {
        i->itemChanged(*this);
    }
}

} // namespace ui
} // namespace de
