/**
 * @file doomsday.h
 * Primary header file for the Doomsday Engine Public API
 *
 * @todo Break this header file up into group-specific ones.
 * Including doomsday.h should include all of the public API headers.
 *
 * @authors Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
 * @authors Copyright &copy; 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

/**
 * @mainpage libdeng API
 *
 * This documentation covers all the functions and data that Doomsday makes
 * available for games and other plugins.
 *
 * @section Overview
 * The documentation has been organized into <a href="modules.html">modules</a>.
 * The primary ones are listed below:
 * - @ref base
 * - @ref console
 * - @ref input
 * - @ref network
 * - @ref resource
 * - @ref render
 */

#ifndef DOOMSDAY_PUBLIC_API_H
#define DOOMSDAY_PUBLIC_API_H

// The calling convention.
#if defined(WIN32)
#   define _DECALL  __cdecl
#elif defined(UNIX)
#   define _DECALL
#endif

#include "dd_share.h"
#include "api_base.h"
#include "api_busy.h"
#include "api_plugin.h"
#include "api_def.h"
#include "api_event.h"
#include "api_filesys.h"
#include "api_console.h"
#include "api_material.h"
#include "api_materialarchive.h"
#include "api_fontrender.h"
#include "api_svg.h"
#include "api_sound.h"
#include "api_render.h"
#include "api_resource.h"
#include "api_map.h"
#include "api_mapedit.h"
#include "api_client.h"
#include "api_server.h"

#include <de/memoryzone.h>
#include <de/point.h>
#include <de/reader.h>
#include <de/rect.h>
#include <de/size.h>
#include <de/smoother.h>
#include <de/mathutil.h>
#include <de/vector1.h>
#include <de/writer.h>

/**
 * Public definitions of the internal map data pointers.  These can be
 * accessed externally, but only as identifiers to data instances.
 * For example, a game could use Sector to identify to sector to
 * change with the Map Update API.
 *
 * Define @c DENG_INTERNAL_MAP_DATA_ACCESS if access to the internal map data
 * structures is needed.
 */
#ifndef DENG_INTERNAL_MAP_DATA_ACCESS
typedef struct bspnode_s  { int type; } BspNode;
typedef struct vertex_s   { int type; } Vertex;
typedef struct linedef_s  { int type; } LineDef;
typedef struct sidedef_s  { int type; } SideDef;
typedef struct hedge_s    { int type; } HEdge;
typedef struct bspleaf_s  { int type; } BspLeaf;
typedef struct sector_s   { int type; } Sector;
typedef struct plane_s    { int type; } Plane;
typedef struct material_s { int type; } material_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup base Base
 */

/**
 * @defgroup defs Definitions
 * @ingroup base
 */

/**
 * @defgroup fs File System
 * @ingroup base
 */

/**
 * @defgroup system System Routines
 * @ingroup base
 * Functionality provided by or related to the operating system.
 */

/**
 * @defgroup playsim Playsim
 * @ingroup game
 */

/**
 * @defgroup polyobj Polygon Objects
 * @ingroup map
 */

/**
 * @defgroup render Renderer
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* DOOMSDAY_PUBLIC_API_H */
