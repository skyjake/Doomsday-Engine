/** @file treedata.h  Data model for a tree of lists.
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

#pragma once

#include "de/ui/data.h"
#include <de/path.h>

namespace de {
namespace ui {

/**
 * Data model representing a tree of lists.
 *
 * @ingroup uidata
 */
class LIBGUI_PUBLIC TreeData
{
public:
    virtual ~TreeData() = default;

    virtual bool        contains(const Path &path) const = 0;
    virtual Data &      items(const Path &path)          = 0;
    virtual const Data &items(const Path &path) const    = 0;

    DE_CAST_METHODS()
};

} // namespace ui
} // namespace de
