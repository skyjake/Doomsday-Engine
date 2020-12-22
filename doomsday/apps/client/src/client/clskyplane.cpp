/** @file clskyplane.cpp  Client-side world map sky plane.
 *
 * @authors Copyright © 2016 Daniel Swanson <danij@dengine.net>
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

#include "client/clskyplane.h"

using namespace de;

DE_PIMPL_NOREF(ClSkyPlane)
{
    bool isCeiling = false; ///< @c true if this is the ceiling; otherwise the floor.
    double height = 0;

    Impl(bool ceiling, ddouble defaultHeight)
        : isCeiling(ceiling), height(defaultHeight)
    {}

    DE_PIMPL_AUDIENCE(HeightChange)
};

DE_AUDIENCE_METHOD(ClSkyPlane, HeightChange)

ClSkyPlane::ClSkyPlane(bool isCeiling, double defaultHeight)
    : d(new Impl(isCeiling, defaultHeight))
{}

bool ClSkyPlane::isCeiling() const
{
    return d->isCeiling;
}

bool ClSkyPlane::isFloor() const
{
    return !d->isCeiling;
}

ddouble ClSkyPlane::height() const
{
    return d->height;
}

void ClSkyPlane::setHeight(double newHeight)
{
    if (!fequal(d->height, newHeight))
    {
        d->height = newHeight;
        DE_NOTIFY(HeightChange, i) i->clSkyPlaneHeightChanged(*this);
    }
}
