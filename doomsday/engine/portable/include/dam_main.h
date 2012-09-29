/**
 * @file dam_main.c
 * Doomsday Archived Map (DAM), map management. @ingroup map
 *
 * @authors Copyright &copy; 2007-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_ARCHIVED_MAP_MAIN_H
#define LIBDENG_ARCHIVED_MAP_MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct archivedmap_s {
    Uri* uri;
    ddstring_t cachedMapPath;
    boolean cachedMapFound;
    boolean lastLoadAttemptFailed;
} archivedmap_t;

extern byte mapCache;

/// To be called during init to register the cvars and ccmds for this module.
void DAM_Register(void);

/// Initialize this module.
void DAM_Init(void);

/// Shutdown this module.
void DAM_Shutdown(void);

/**
 * Compose the relative path (relative to the runtime directory) to the directory
 * within the archived map cache where maps from the specified source will reside.
 *
 * @param sourcePath  Path to the primary resource file for the original map data.
 * @return  The composed path.
 */
AutoStr* DAM_ComposeCacheDir(const char* sourcePath);

boolean DAM_AttemptMapLoad(const Uri* uri);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_ARCHIVED_MAP_MAIN_H */
