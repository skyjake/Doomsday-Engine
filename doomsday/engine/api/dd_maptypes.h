/** @file dd_maptypes.h Map Data Property Types.
 *
 * @author Copyright &copy; 2006-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_API_PLAY_PUBLIC_MAP_DATA_TYPES_H
#define LIBDENG_API_PLAY_PUBLIC_MAP_DATA_TYPES_H

/**
 * @ingroup map
 * @{
 */

// Vertex properties:
#define DMT_VERTEX_ORIGIN DDVT_DOUBLE

// HEdge properties:
#define DMT_HEDGE_SIDEDEF DDVT_PTR
#define DMT_HEDGE_V DDVT_PTR
#define DMT_HEDGE_LINEDEF DDVT_PTR
#define DMT_HEDGE_SECTOR DDVT_PTR
#define DMT_HEDGE_BSPLEAF DDVT_PTR
#define DMT_HEDGE_TWIN DDVT_PTR
#define DMT_HEDGE_ANGLE DDVT_ANGLE
#define DMT_HEDGE_SIDE DDVT_BYTE
#define DMT_HEDGE_LENGTH DDVT_DOUBLE
#define DMT_HEDGE_OFFSET DDVT_DOUBLE
#define DMT_HEDGE_NEXT DDVT_PTR
#define DMT_HEDGE_PREV DDVT_PTR

// BspLeaf properties:
#define DMT_BSPLEAF_HEDGECOUNT DDVT_UINT
#define DMT_BSPLEAF_HEDGE DDVT_PTR
#define DMT_BSPLEAF_POLYOBJ DDVT_PTR
#define DMT_BSPLEAF_SECTOR DDVT_PTR

// Material properties:
#define DMT_MATERIAL_FLAGS DDVT_SHORT
#define DMT_MATERIAL_WIDTH DDVT_INT
#define DMT_MATERIAL_HEIGHT DDVT_INT

// Surface properties:
#define DMT_SURFACE_BASE DDVT_PTR
#define DMT_SURFACE_FLAGS DDVT_INT
#define DMT_SURFACE_MATERIAL DDVT_PTR
#define DMT_SURFACE_BLENDMODE DDVT_BLENDMODE
#define DMT_SURFACE_BITANGENT DDVT_FLOAT
#define DMT_SURFACE_TANGENT DDVT_FLOAT
#define DMT_SURFACE_NORMAL DDVT_FLOAT
#define DMT_SURFACE_OFFSET DDVT_FLOAT
#define DMT_SURFACE_RGBA DDVT_FLOAT

// Plane properties:
#define DMT_PLANE_SECTOR DDVT_PTR
#define DMT_PLANE_HEIGHT DDVT_DOUBLE
#define DMT_PLANE_GLOW DDVT_FLOAT
#define DMT_PLANE_GLOWRGB DDVT_FLOAT
#define DMT_PLANE_TARGET DDVT_DOUBLE
#define DMT_PLANE_SPEED DDVT_DOUBLE

// Sector properties:
#define DMT_SECTOR_FLOORPLANE DDVT_PTR
#define DMT_SECTOR_CEILINGPLANE DDVT_PTR
#define DMT_SECTOR_VALIDCOUNT DDVT_INT
#define DMT_SECTOR_LIGHTLEVEL DDVT_FLOAT
#define DMT_SECTOR_RGB DDVT_FLOAT
#define DMT_SECTOR_MOBJLIST DDVT_PTR
#define DMT_SECTOR_LINEDEFCOUNT DDVT_UINT
#define DMT_SECTOR_LINEDEFS DDVT_PTR
#define DMT_SECTOR_BSPLEAFCOUNT DDVT_UINT
#define DMT_SECTOR_BSPLEAFS DDVT_PTR
#define DMT_SECTOR_BASE DDVT_PTR
#define DMT_SECTOR_PLANECOUNT DDVT_UINT
#define DMT_SECTOR_REVERB DDVT_FLOAT

// SideDef properties:
#define DMT_SIDEDEF_SECTOR DDVT_PTR
#define DMT_SIDEDEF_LINE DDVT_PTR
#define DMT_SIDEDEF_FLAGS DDVT_SHORT

// LineDef properties:
#define DMT_LINEDEF_SECTOR DDVT_PTR
#define DMT_LINEDEF_SIDEDEF DDVT_PTR
#define DMT_LINEDEF_AABOX DDVT_DOUBLE
#define DMT_LINEDEF_V DDVT_PTR
#define DMT_LINEDEF_FLAGS DDVT_INT
#define DMT_LINEDEF_SLOPETYPE DDVT_INT
#define DMT_LINEDEF_VALIDCOUNT DDVT_INT
#define DMT_LINEDEF_DX DDVT_DOUBLE
#define DMT_LINEDEF_DY DDVT_DOUBLE
#define DMT_LINEDEF_LENGTH DDVT_DOUBLE

// BspNode properties:
#define DMT_BSPNODE_AABOX DDVT_DOUBLE
#define DMT_BSPNODE_CHILDREN DDVT_PTR

///@}

#endif // LIBDENG_API_PLAY_PUBLIC_MAP_DATA_TYPES_H
