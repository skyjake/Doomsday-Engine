/**
 * @file common.h
 *
 * @authors Copyright &copy; 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_GAME_INCLUDES
#define LIBCOMMON_GAME_INCLUDES

#include <de/mathutil.h>
#include <de/timer.h>

#ifdef UNIX
#  include <strings.h>
#endif

#define WEAPONBOTTOM            (128)   // from p_pspr.c

#define IS_NETWORK_SERVER       (DD_GetInteger(DD_SERVER) && DD_GetInteger(DD_NETGAME))
#define IS_NETWORK_CLIENT       (DD_GetInteger(DD_CLIENT) && DD_GetInteger(DD_NETGAME))

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "gamerules.h"
#include "pause.h"
#ifdef __cplusplus
#  include <doomsday/filesys/lumpindex.h>
#endif

DENG_EXTERN_C dd_bool sc_FileScripts;
DENG_EXTERN_C char const *sc_ScriptsDir;

#ifdef __cplusplus
extern "C" {
#endif

int Common_GetInteger(int id);

#ifdef __cplusplus
/**
 * Returns the central LumpIndex from the engine. For use with old subsystems
 * which still depend on this old fashioned mechanism for file access.
 *
 * @deprecated  Implement file access without depending on this specialized behavior.
 */
inline de::LumpIndex const &CentralLumpIndex() { return *reinterpret_cast<de::LumpIndex const *>(F_LumpIndex()); }

} // extern "C"
#endif

#endif // LIBCOMMON_GAME_INCLUDES
