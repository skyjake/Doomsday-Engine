/** @file listdata.h  List-based UI data context.
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

#ifndef LIBAPPFW_UI_LISTDATA_H
#define LIBAPPFW_UI_LISTDATA_H

#include "../ui/Data"
#include "../ui/Item"

#include <QList>

namespace de {
namespace ui {

/**
 * List-based UI data context.
 */
class LIBAPPFW_PUBLIC ListData : public Data
{
public:
    ListData() {}
    ~ListData();

    dsize size() const;
    Item &at(Pos pos);
    Item const &at(Pos pos) const;
    Pos find(Item const &item) const;
    Pos findData(QVariant const &data) const;

    Data &clear();
    Data &insert(Pos pos, Item *item);
    void remove(Pos pos);
    Item *take(Pos pos);
    void sort(LessThanFunc lessThan);
    void stableSort(LessThanFunc lessThan);

private:
    typedef QList<Item *> Items;
    Items _items;
};

} // namespace ui
} // namespace de

#endif // LIBAPPFW_UI_LISTDATA_H
