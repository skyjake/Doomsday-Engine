/** @file idtech1flatlib.h  Collection of flat images (plane textures).
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_RESOURCE_IDTECH1FLATLIB_H
#define LIBDOOMSDAY_RESOURCE_IDTECH1FLATLIB_H

#include "lumpcatalog.h"
#include "idtech1image.h"

namespace res {

using namespace de;

class LIBDOOMSDAY_PUBLIC IdTech1FlatLib
{
public:
    IdTech1FlatLib(const LumpCatalog &catalog);

    IdTech1Image flatImage(const String &name) const;

private:
    DE_PRIVATE(d)
};

} // namespace res

#endif // LIBDOOMSDAY_RESOURCE_IDTECH1FLATLIB_H
