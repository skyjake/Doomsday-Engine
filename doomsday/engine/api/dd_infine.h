/**
 * @file dd_infine.h
 * Interactive animation sequence system.
 *
 * @ingroup infine
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_API_INFINE_H
#define LIBDENG_API_INFINE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup infine Finale Animations
 * @ingroup gui
 * The "In Fine" finale interactive animation sequence system.
 */
///@{

/// Finale identifier. Used throughout the public API when referencing active Finales.
typedef ident_t finaleid_t;

/**
 * @defgroup finaleFlags Finale Flags
 * @ingroup infine apiFlags
 */

/// @addtogroup finaleFlags
///@{
#define FF_LOCAL            0x1 ///< Local scripts are executed client-side.
///@}

/**
 * Execute a set of Finale commands.
 * @param script     One or more commands to be executed.
 * @param flags      @ref finaleFlags.
 * @param setupCmds  One or more commands to be executed immediately during
 *                   finale setup. Can be used to configure the default page
 *                   state.
 */
finaleid_t FI_Execute2(const char* script, int flags, const char* setupCmds);

/**
 * Execute a set of Finale commands.
 * @param script     One or more commands to be executed.
 * @param flags      @ref finaleFlags.
 */
finaleid_t FI_Execute(const char* script, int flags);

/**
 * @return  @c true iff the specified Finale is active.
 */
boolean FI_ScriptActive(finaleid_t id);

/**
 * @return  @ref finaleFlags.
 */
int FI_ScriptFlags(finaleid_t id);

/**
 * Immediately halt command interpretation and mark the script for termination.
 */
void FI_ScriptTerminate(finaleid_t id);

/**
 * Suspend command interpretation.
 */
void FI_ScriptSuspend(finaleid_t id);

/**
 * Resume command interpretation.
 */
void FI_ScriptResume(finaleid_t id);

/**
 * @return  @c true iff the specified Finale is currently suspended.
 */
boolean FI_ScriptSuspended(finaleid_t id);

/**
 * @return  @c true iff the skip request was successful.
 */
boolean FI_ScriptRequestSkip(finaleid_t id);

/**
 * @return  @c true iff command interpretation has begun.
 */
boolean FI_ScriptCmdExecuted(finaleid_t id);

/**
 * @return  @c true iff the "menu trigger" is currently active.
 */
boolean FI_ScriptIsMenuTrigger(finaleid_t id);

int FI_ScriptResponder(finaleid_t id, const void* ev);

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_API_INFINE_H */
