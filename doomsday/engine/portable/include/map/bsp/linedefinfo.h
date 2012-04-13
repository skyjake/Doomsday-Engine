/**
 * @file linedefinfo.h
 * BSP Builder LineDef info. @ingroup bsp
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

#ifndef LIBDENG_BSP_LINEDEFINFO
#define LIBDENG_BSP_LINEDEFINFO

#include "p_mapdata.h"

#include <QGlobalStatic>

namespace de {
namespace bsp {

/**
 * Plain old data (POD) structure used to record additional information and
 * precalculated values for a LineDef in the current map.
 */
struct LineDefInfo
{
    enum Flag
    {
        TWOSIDED   =        0x1, ///< Line is marked two-sided.
        ZEROLENGTH =        0x2, ///< Zero length (line should be totally ignored).
        SELFREF    =        0x4, ///< Sector is the same on both sides.
        POLYOBJ    =        0x8  ///< Line is part of a polyobj.
    };
    Q_DECLARE_FLAGS(Flags, Flag)
    Flags flags;

    /// Normally NULL, except when this linedef directly overlaps an earlier
    /// one (a rarely-used trick to create higher mid-masked textures).
    /// No hedges should be created for these overlapping linedefs.
    LineDef* overlap;

    /// @todo Refactor me away.
    int validCount;

    LineDefInfo() : flags(0), overlap(0), validCount(0)
    {}
};

Q_DECLARE_OPERATORS_FOR_FLAGS(LineDefInfo::Flags)

} // namespace bsp
} // namespace de

#endif /// LIBDENG_BSPBUILDER_LINEDEFINFO
