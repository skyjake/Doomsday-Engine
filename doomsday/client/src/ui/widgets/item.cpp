/** @file item.cpp  Context item.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "ui/widgets/item.h"

using namespace de;
using namespace ui;

Item::Item(Semantics semantics)
    : _semantics(semantics), _context(0)
{}

Item::Item(Semantics semantics, String const &label)
    : _semantics(semantics), _context(0), _label(label)
{}

Item::~Item()
{}

Item::Semantics Item::semantics() const
{
    return _semantics;
}

void Item::setLabel(String const &label)
{
    _label = label;
    notifyChange();
}

String Item::label() const
{
    return _label;
}

String Item::sortKey() const
{
    return _label;
}

void Item::notifyChange()
{
    DENG2_FOR_AUDIENCE(Change, i)
    {
        i->itemChanged(*this);
    }
}
