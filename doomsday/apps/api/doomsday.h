/** @file doomsday.h Primary header file for the Doomsday Engine Public API
 *
 * This header includes all of the public API headers of Doomsday.
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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
#include "api_client.h"
#include "api_console.h"
#include "api_def.h"
#include "api_event.h"
#include "api_filesys.h"
#include "api_fontrender.h"
#include "api_internaldata.h"
#include <doomsday/api_map.h>
#include "api_mapedit.h"
#include "api_material.h"
#include "api_render.h"
#include "api_resource.h"
#include "api_server.h"
#include "api_sound.h"
#include "api_svg.h"
#include "api_uri.h"

#include <doomsday/gameapi.h>
#include <doomsday/plugins.h>
#include <doomsday/players.h>
#include <doomsday/player.h>

#include <de/legacy/memoryzone.h>
#include <de/legacy/point.h>
#include <de/legacy/reader.h>
#include <de/legacy/rect.h>
#include <de/legacy/size.h>
#include <de/legacy/smoother.h>
#include <de/legacy/mathutil.h>
#include <de/legacy/vector1.h>
#include <de/legacy/writer.h>

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
 * @defgroup input Input Subsystem
 * @ingroup ui
 * Input devices and command/control bindings.
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
 * @ingroup world
 */

/**
 * @defgroup render Renderer
 */

#endif /* DOOMSDAY_PUBLIC_API_H */
