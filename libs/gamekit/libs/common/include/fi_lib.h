/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Helper routines and LIFO stack (wrapper interface) functionality for
 * use in conjunction with the Doomsday Engine's InFine API.
 */

#ifndef LIBCOMMON_INFINE_LIB
#define LIBCOMMON_INFINE_LIB

#include "d_net.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup finaleMode Finale Mode.
 */
/*@{*/
typedef enum {
    FIMODE_NORMAL = 0,
    FIMODE_OVERLAY,
    FIMODE_BEFORE,
    FIMODE_AFTER
} finale_mode_t;
/*@}*/

/**
 * Called during engine initialization to register any ccmds and cvars
 * used by this library.
 */
void FI_StackRegister(void);

/// Initialize the LIFO stack functionality.
void FI_StackInit(void);

/// Shutdown the LIFO stack functionality.
void FI_StackShutdown(void);

/**
 * Push a new set of Finale commands onto the LIFO stack, suspending any
 * existing Finale on the stack until command interpretation completes.
 * The script will have no definition ID on the stack; you can start an
 * unlimited number of instances of the script.
 *
 * @param commands One or more Finale (script) commands to be executed.
 * @param flags  @ref finaleFlags
 * @param mode  @ref finaleMode
 */
void FI_StackExecute(const char* commands, int flags, finale_mode_t mode);

/**
 * Push a new set of Finale commands onto the LIFO stack, suspending any
 * existing Finale on the stack until command interpretation completes.
 * If a script with the same definition ID is already on the stack, the
 * script is not started.
 *
 * @param commands One or more Finale (script) commands to be executed.
 * @param flags  @ref finaleFlags
 * @param mode  @ref finaleMode
 * @param defId  Script's definition ID.
 */
void FI_StackExecuteWithId(const char* scriptSrc, int flags, finale_mode_t mode, const char* defId);

/**
 * Clear the LIFO Finale stack of any active scripts.
 */
void FI_StackClear(void);

/**
 * Completely clear the LIFO Finale stack including any suspended scripts.
 */
void FI_StackClearAll(void);

/// @return  @c true if there is one or more active Finale on the stack.
dd_bool FI_StackActive(void);

/// @return  @c true iff the skip was done.
dd_bool FI_RequestSkip(void);

/// @return  @c true iff the event should open the menu.
dd_bool FI_IsMenuTrigger(void);

int FI_PrivilegedResponder(const void* ev);

void NetCl_UpdateFinaleState(Reader1 *msg);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBCOMMON_INFINE_LIB */
