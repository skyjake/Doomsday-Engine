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
#include "IHPlane"

class HEdge;
class Surface;

/// Maximum number of intercepts in a WallEdge.
#define WALLEDGE_MAX_INTERCEPTS          64

namespace de {

class WallSpec
{
public:
    enum Flag
    {
        /// Force the geometry to be opaque, irrespective of material opacity.
        ForceOpaque           = 0x01,

        /// Do not generate geometry for dynamic lights.
        NoDynLights           = 0x02,

        /// Do not generate geometry for dynamic (mobj) shadows.
        NoDynShadows          = 0x04,

        /// Do not generate geometry for faked radiosity.
        NoFakeRadio           = 0x08,

        /// Do not apply angle based light level deltas.
        NoLightDeltas         = 0x10,

        /// Do not smooth edge normals.
        NoEdgeNormalSmoothing = 0x20,

        DefaultFlags = ForceOpaque
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    Flags flags;

    /// Wall section identifier.
    int section;

    WallSpec(int section, Flags = DefaultFlags) : flags(flags), section(section)
    {}
};

Q_DECLARE_OPERATORS_FOR_FLAGS(WallSpec::Flags)

/**
 * Helper/utility class intended to simplify the process of generating
 * sections of wall geometry from a map line segment.
 *
 * @ingroup map
 */
class WallEdge
{
public:
    /// Invalid range geometry was found during prepare() @ingroup errors
    DENG2_ERROR(InvalidError);

    class Intercept : public de::IHPlane::IIntercept
    {
    public:
        Intercept(WallEdge *owner, double distance = 0);
        Intercept(Intercept const &other);

        WallEdge &owner() const;

        de::Vector3d origin() const;

    private:
        WallEdge *_owner;
    };

    typedef QList<Intercept> Intercepts;

public:
    /**
     * @param spec  Wall section spec. A copy is made.
     */
    WallEdge(WallSpec const &spec, HEdge &hedge, int edge);

    WallEdge(WallEdge const &other);

    inline WallEdge &operator = (WallEdge other) {
        d.swap(other.d);
        return *this;
    }

    inline void swap(WallEdge &other) {
        d.swap(other.d);
    }

    inline Intercept const & operator [] (int index) const { return at(index); }

    Intercept const &at(int index) const;

    void prepare();

    WallSpec const &spec() const;

    bool isValid() const;

    de::Vector2d const &origin() const;

    coord_t mapLineOffset() const;

    Line::Side &mapSide() const;

    Surface &surface() const;

    int section() const;

    int divisionCount() const;

    Intercept const &bottom() const;

    Intercept const &top() const;

    int firstDivision() const;

    int lastDivision() const;

    de::Vector2f const &materialOrigin() const;

    de::Vector3f const &normal() const;

    Intercepts const &intercepts() const;

private:
    DENG2_PRIVATE(d)
};

} // namespace de

namespace std {
// std::swap specialization for WallEdge
template <>
inline void swap<de::WallEdge>(de::WallEdge &a, de::WallEdge &b) {
    a.swap(b);
}
}

#endif // DENG_RENDER_WALLEDGE
