/** @file drawlists.h  Drawable primitive list collection/management.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef DE_CLIENT_RENDER_DRAWLISTS_H
#define DE_CLIENT_RENDER_DRAWLISTS_H

#include "render/drawlist.h"
#include <de/vector.h>

class DrawLists
{
public:
    typedef de::List<DrawList *> FoundLists;

public:
    DrawLists();

    /**
     * Locate an appropriate draw list for the given specification.
     *
     * @param spec  Draw list specification.
     *
     * @return  The chosen list.
     */
    DrawList &find(const DrawListSpec &spec);

    /**
     * Finds all draw lists which match the given specification. Note that only
     * non-empty lists are collected.
     *
     * @param group  Logical geometry group identifier.
     * @param found  Set of draw lists which match the result.
     *
     * @return  Number of draw lists found.
     */
    int findAll(GeomGroup group, FoundLists &found);

    /**
     * To be called before rendering of a new frame begins.
     */
    void reset();

    /**
     * All lists will be destroyed.
     */
    void clear();

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_RENDER_DRAWLISTS_H
