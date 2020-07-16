/** @file ihplane.h  Interface for a geometric half-plane.
 *
 * @authors Copyright © 2013-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_MATH_IHPLANE
#define DE_MATH_IHPLANE

#include <de/error.h>
#include <de/partition.h>
#include <de/vector.h>

namespace de {

/**
 * Interface for an interceptable geometric half-plane, which provides direct
 * access to the data/class instance used to model an intersection point.
 */
class IHPlane
{
public:
    /// An invalid reference to an intercept was specified. @ingroup errors
    DE_ERROR(UnknownInterceptError);

    /**
     * Interface for an intercept in the implementing half-plane.
     */
    class IIntercept
    {
    public:
        IIntercept(ddouble distance) : _distance(distance) {}

        virtual ~IIntercept() = default;

        /**
         * Determines the distance between "this" and the @a other intercept
         * along the half-plane. The default implementation simply subtracts
         * the other distance from that of "this".
         */
        virtual double operator - (const IIntercept &other) const {
            return distance() - other.distance();
        }

        /**
         * Determines whether the distance relative to the half-plane origin
         * for "this" intercept is logically less than that of @a other. The
         * default implementation simply compares the distance values.
         */
        virtual bool operator < (const IIntercept &other) const {
            return distance() < other.distance();
        }

        /**
         * Returns distance along the half-plane relative to the origin.
         * Implementors may override this for special functionality.
         */
        virtual ddouble distance() const { return _distance; }

    protected:
        ddouble _distance;
    };

public:
    virtual ~IHPlane() = default;

    /**
     * Reconfigure the half-plane according to the given Partition line.
     *
     * @param newPartition  The "new" partition line to configure using.
     */
    virtual void configure(const Partition &newPartition) = 0;

    /**
     * Returns the Partition (immutable) used to model the partitioning line
     * of the half-plane.
     */
    virtual const Partition &partition() const = 0;

    /**
     * Clear the list of intercept "points" for the half-plane.
     */
    virtual void clearIntercepts() = 0;

    /**
     * Attempt interception of the half-plane at @a distance from the origin.
     *
     * @param distance  Distance along the half-plane to intersect.
     *
     * @return  Resultant intercept if intersection occurs. Otherwise @c 0.
     */
    virtual const IIntercept *intercept(ddouble distance) = 0;

    /**
     * Returns the total number of half-plane intercept points.
     */
    virtual int interceptCount() const = 0;

    /**
     * Prepare the list of intercepts for search queries.
     */
    virtual void sortAndMergeIntercepts() {}

    /**
     * @note Implementors are obligated to throw UnknownInterceptError if the
     * specified @a index is not valid.
     */
    virtual const IIntercept &at(int index) const = 0;
};

} // namespace de

#endif // DE_MATH_IHPLANE
