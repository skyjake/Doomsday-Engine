/**
 * @file bsp_intersection.h
 * BSP Builder Intersections. @ingroup map
 *
 * Based on glBSP 2.24 (in turn, based on BSP 2.3), which is hosted on
 * SourceForge: http://sourceforge.net/projects/glbsp/
 *
 * @authors Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_MAP_BSP_INTERSECTION
#define LIBDENG_MAP_BSP_INTERSECTION

#include "bspbuilder/hedges.hh"
#include "bspbuilder/bspbuilder.hh"

struct linedef_s;
struct superblock_s;

#ifdef __cplusplus
extern "C" {
#endif

struct hplaneintercept_s;
typedef struct hplaneintercept_s HPlaneIntercept;

HPlaneIntercept* HPlaneIntercept_Next(HPlaneIntercept* intersection);

HPlaneIntercept* HPlaneIntercept_Prev(HPlaneIntercept* intersection);

void* HPlaneIntercept_UserData(HPlaneIntercept* intersection);

static boolean initedOK = false;
static HPlaneIntercept* usedIntercepts;

HPlaneIntercept* HPlaneIntercept_New(void);

namespace de {

typedef struct hplanebuildinfo_s {
    struct linedef_s* lineDef; // Not NULL if partition originated from a linedef.
    struct linedef_s* sourceLineDef;

    double pSX, pSY;
    double pDX, pDY;
    double pPara, pPerp;
    double pLength;
} HPlaneBuildInfo;

class BspBuilder;

/**
 * HPlane instance.
 */
class HPlane {
public:
    HPlane(BspBuilder* builder) :
        builder(builder), headPtr(0)
    {
        partition.origin[0] = partition.origin[1] = 0;
        partition.angle[0] = partition.angle[1] = 0;
        memset(&info, 0, sizeof(info));
    }

    ~HPlane()
    {
        clear();
    }

    const double* origin() { return partition.origin; }
    double x() { return partition.origin[0]; }
    double y() { return partition.origin[1]; }

    HPlane* setOrigin(double const origin[2]);
    HPlane* setXY(double x, double y);
    HPlane* setX(double x);
    HPlane* setY(double y);

    const double* angle() { return partition.angle; }
    double dX() { return partition.angle[0]; }
    double dY() { return partition.angle[1]; }

    HPlane* setAngle(double const angle[2]);
    HPlane* setDXY(double x, double y);
    HPlane* setDX(double dx);
    HPlane* setDY(double dy);

    /// @fixme Should be immutable.
    HPlaneBuildInfo* buildInfo() { return &info; }

    /**
     * Empty all intersections from the specified HPlane.
     */
    void clear();

    /**
     * Insert a point at the given intersection into the intersection list.
     *
     * @todo Ownership of the user data should NOT be passed to this object.
     *
     * @param userData  Ownership passes to HPlane.
     */
    HPlaneIntercept* newIntercept2(double distance, void* userData);
    HPlaneIntercept* newIntercept(double distance/*, userData=NULL*/);

private:
    /// BspBuilder instance which produced this.
    /// @todo Remove me.
    BspBuilder* builder;

    struct {
        double origin[2];
        double angle[2];
    } partition;

public:
    /// The intercept list. Kept sorted by along_dist, in ascending order.
    HPlaneIntercept* headPtr;

private:
    /// Additional information used by the node builder during construction.
    HPlaneBuildInfo info;
};

} // namespace de

int HPlane_IterateIntercepts2(de::HPlane* bi, int (*callback)(HPlaneIntercept*, void*), void* parameters);
int HPlane_IterateIntercepts(de::HPlane* bi, int (*callback)(HPlaneIntercept*, void*)/*, parameters=NULL*/);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_MAP_BSP_INTERSECTION
