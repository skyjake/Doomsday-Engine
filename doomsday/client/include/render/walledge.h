/** @file render/walledge.h Wall Edge Geometry.
 *
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_WALLEDGE
#define DENG_RENDER_WALLEDGE

#include <QList>

#include <de/Error>
#include <de/Vector>

#include "Line"
#include "WallSpec"

#include "TriangleStripBuilder"
#include "IHPlane"

class HEdge;
class Surface;

/// Maximum number of intercepts in a WallEdge.
#define WALLEDGE_MAX_INTERCEPTS          64

namespace de {

/**
 * Helper/utility class intended to simplify the process of generating
 * sections of wall geometry from a map Line segment.
 *
 * @ingroup map
 */
class WallEdge : public IEdge
{
    DENG2_NO_COPY  (WallEdge)
    DENG2_NO_ASSIGN(WallEdge)

public:
    /// Invalid range geometry was found during prepare() @ingroup errors
    DENG2_ERROR(InvalidError);

    class Intercept : public IEdge::IIntercept,
                      public IHPlane::IIntercept
    {
    public:
        Intercept(WallEdge &owner, double distance = 0);

        bool operator < (Intercept const &other) const;

        double distance() const;

        Vector3d origin() const;

        friend class WallEdge;

    protected:
        void setDistance(double newDistance);

    private:
        DENG2_PRIVATE(d)
    };

    typedef QList<Intercept *> Intercepts;

public:
    /**
     * @param spec  Geometry specification for the wall section. A copy is made.
     */
    WallEdge(WallSpec const &spec, HEdge &hedge, int edge);

    inline Intercept const & operator [] (int index) const { return at(index); }

    Intercept const &at(int index) const;

    WallSpec const &spec() const;

    /// Implements IEdge.
    bool isValid() const;

    /// Implements IEdge.
    Intercept const &from() const;

    /// Implements IEdge.
    Intercept const &to() const;

    /// Implements IEdge.
    Vector2f const &materialOrigin() const;

    Vector2d const &origin() const;

    coord_t mapLineOffset() const;

    Line::Side &mapSide() const;

    Surface &surface() const;

    int section() const;

    int divisionCount() const;

    inline Intercept const &bottom() const { return from(); }
    inline Intercept const &top() const { return to(); }

    int firstDivision() const;

    int lastDivision() const;

    de::Vector3f const &normal() const;

    Intercepts const &intercepts() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // DENG_RENDER_WALLEDGE
