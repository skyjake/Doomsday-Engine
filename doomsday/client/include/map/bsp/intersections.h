/** @file map/bsp/intersections.h BSP Builder half-plane intercept list.
 *
 * Originally based on glBSP 2.24 (in turn, based on BSP 2.3)
 * @see http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2000-2007 Andrew Apted <ajapted@gmail.com>
 * @authors Copyright © 1998-2000 Colin Reed <cph@moria.org.uk>
 * @authors Copyright © 1998-2000 Lee Killough <killough@rsn.hp.com>
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

#ifndef DENG_WORLD_MAP_BSP_INTERSECTIONS
#define DENG_WORLD_MAP_BSP_INTERSECTIONS

#include <list>

#include <de/Error>
#include <de/Log>

namespace de {
namespace bsp {

/**
 * @ingroup bsp
 */
template <typename UserData>
class Intersections
{
public:
    class Intercept
    {
    public:
        Intercept(double distance = 0, UserData *userData = 0)
            : _distance(distance), _userData(userData)
        {}

        /**
         * Determine the distance between two intercepts. It does not matter
         * if the intercepts are from different half-planes.
         */
        double operator - (Intercept const &other) const {
            return _distance - other.distance();
        }

        /**
         * Distance from the owning Intercepts's origin point. Negative values
         * mean this intercept is positioned to the left of the origin.
         */
        double distance() const { return _distance; }

        /**
         * Retrieve the data pointer associated with this intercept.
         */
        UserData *userData() const { return _userData; }

    private:
        /**
         * Distance along the owning Intercepts in the map coordinate space.
         */
        double _distance;

        /// User data pointer associated with this intercept.
        UserData *_userData;
    };

    typedef std::list<Intercept> Intercepts;

    typedef bool (*mergepredicate_t)(Intercept &a, Intercept &b, void *context);

public:
    Intersections() : _intercepts(0)
    {}

    ~Intersections() { clear(); }

    /**
     * Empty all intersections from the specified Intercepts.
     */
    void clear()
    {
        _intercepts.clear();
    }

    /**
     * Insert a point at the given intersection into the intersection list.
     *
     * @param distance  Distance along the partition for the new intercept,
     *                  in map units.
     * @param userData  User data object to link with the new intercept.
     *                  Ownership remains unchanged.
     */
    Intercept &insert(double distance, UserData *userData = 0)
    {
        Intercepts::reverse_iterator after;

        for(after = _intercepts.rbegin();
            after != _intercepts.rend() && distance < (*after).distance(); after++)
        {}

        return *_intercepts.insert(after.base(), Intercept(distance, userData));
    }

    void merge(mergepredicate_t predicate, void *context)
    {
        Intercepts::iterator node = _intercepts.begin();
        while(node != _intercepts.end())
        {
            Intercepts::iterator np = node; np++;
            if(np == _intercepts.end()) break;

            // Sanity check.
            double distance = *np - *node;
            if(distance < -0.1)
            {
                throw Error("Intercepts::merge", QString("Invalid intercept order - %1 > %2")
                                                     .arg(node->distance(), 0, 'f', 3)
                                                     .arg(  np->distance(), 0, 'f', 3));
            }

            // Are we merging this pair?
            if(predicate(*node, *np, context))
            {
                // Yes - Unlink this intercept.
                _intercepts.erase(np);
            }
            else
            {
                // No.
                node++;
            }
        }
    }

    Intercepts const &all() const
    {
        return _intercepts;
    }

#ifdef DENG_DEBUG
    void debugPrint() const
    {
        int index = 0;
        DENG2_FOR_EACH_CONST(Intersections::Intercepts, i, all())
        {
            LOG_DEBUG(" %i: >%f ") << index << i->distance();
            index++;
        }
    }
#endif

private:
    /// The intercept list. Kept sorted by distance, in ascending order.
    Intercepts _intercepts;
};

} // namespace bsp
} // namespace de

#endif // DENG_WORLD_MAP_BSP_INTERSECTIONS
