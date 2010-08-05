/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_API_INFINE_H
#define LIBDENG_API_INFINE_H

/**
 * @defgroup infine InFine System
 *
 * The "In Fine" finale sequence system.
 */

/// Finale identifier. Used throughout the public API when referencing active Finales.
typedef ident_t finaleid_t;

/**
 * @defgroup finaleFlags Finale Flags.
 */
/*@{*/
#define FF_LOCAL            0x1 /// Local scripts are executed client-side.
/*@}*/

/**
 * Execute a set of Finale commands.
 * @param commands  One or more commands to be executed.
 * @param flags  @see finaleFlags.
 */
finaleid_t FI_Execute(const char* commands, int flags);

/**
 * @return  @c true iff the specified Finale is active.
 */
boolean FI_ScriptActive(finaleid_t id);

/**
 * @return  @see finaleFlags.
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

#endif /* LIBDENG_API_INFINE_H */
