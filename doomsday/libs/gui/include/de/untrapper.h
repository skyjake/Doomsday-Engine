/** @file untrapper.h  Mouse untrapping utility.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBAPPFW_UNTRAPPER_H
#define LIBAPPFW_UNTRAPPER_H

#include "libgui.h"
#include <de/glwindow.h>

namespace de {

/**
 * Utility for untrapping the mouse. The mouse is untrapped from the specified window for
 * the lifetime of the Untrapper instance. When Untrapper is destroyed, mouse is
 * automatically trapped if it originally was trapped.
 *
 * @ingroup appfw
 */
class LIBGUI_PUBLIC Untrapper
{
public:
    /**
     * Mouse is untrapped if it has been trapped in @a window.
     * @param window  Window where (un)trapping is done.
     */
    Untrapper(GLWindow &window);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBAPPFW_UNTRAPPER_H
