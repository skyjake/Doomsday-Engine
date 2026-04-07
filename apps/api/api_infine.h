/** @file api_infine.h Interactive animation sequence system.
 *
 * @ingroup infine
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_INFINE_H
#define DOOMSDAY_API_INFINE_H

#include "apis.h"

/**
 * @defgroup infine Finale Animations
 * @ingroup gui
 * The "InFine" finale interactive animation sequence system.
 */
///@{

#define FIPAGE_NUM_PREDEFINED_COLORS        (10)
#define VALID_FIPAGE_PREDEFINED_COLOR(v)    ((v) < FIPAGE_NUM_PREDEFINED_COLORS)

#define FIPAGE_NUM_PREDEFINED_FONTS         (10)
#define VALID_FIPAGE_PREDEFINED_FONT(v)     ((v) < FIPAGE_NUM_PREDEFINED_FONTS)

/// Finale identifier. Used throughout the public API when referencing active Finales.
typedef ident_t finaleid_t;
typedef ident_t fi_objectid_t;

/**
 * @defgroup finaleFlags Finale Flags
 * @ingroup infine apiFlags
 */

/// @addtogroup finaleFlags
///@{
#define FF_LOCAL            0x1 ///< Local scripts are executed client-side.
///@}

DE_API_TYPEDEF(Infine)
{
    de_api_t api;

    /**
     * Execute a set of Finale commands.
     * @param script     One or more commands to be executed.
     * @param flags      @ref finaleFlags.
     * @param setupCmds  One or more commands to be executed immediately during
     *                   finale setup. Can be used to configure the default page
     *                   state.
     */
    finaleid_t (*Execute2)(const char* script, int flags, const char* setupCmds);

    /**
     * Execute a set of Finale commands.
     * @param script     One or more commands to be executed.
     * @param flags      @ref finaleFlags.
     */
    finaleid_t (*Execute)(const char* script, int flags);

    /**
     * @return  @c true iff the specified Finale is active.
     */
    dd_bool (*ScriptActive)(finaleid_t id);

    /**
     * @return  @ref finaleFlags.
     */
    int (*ScriptFlags)(finaleid_t id);

    /**
     * Immediately halt command interpretation and mark the script for termination.
     */
    void (*ScriptTerminate)(finaleid_t id);

    /**
     * Suspend command interpretation.
     */
    void (*ScriptSuspend)(finaleid_t id);

    /**
     * Resume command interpretation.
     */
    void (*ScriptResume)(finaleid_t id);

    /**
     * @return  @c true iff the specified Finale is currently suspended.
     */
    dd_bool (*ScriptSuspended)(finaleid_t id);

    /**
     * @return  @c true iff the skip request was successful.
     */
    dd_bool (*ScriptRequestSkip)(finaleid_t id);

    /**
     * @return  @c true iff command interpretation has begun.
     */
    dd_bool (*ScriptCmdExecuted)(finaleid_t id);

    /**
     * @return  @c true iff the "menu trigger" is currently active.
     */
    dd_bool (*ScriptIsMenuTrigger)(finaleid_t id);

    int (*ScriptResponder)(finaleid_t id, const void* ev);
}
DE_API_T(Infine);

#ifndef DE_NO_API_MACROS_INFINE
#define FI_Execute2             _api_Infine.Execute2
#define FI_Execute              _api_Infine.Execute
#define FI_ScriptActive         _api_Infine.ScriptActive
#define FI_ScriptFlags          _api_Infine.ScriptFlags
#define FI_ScriptTerminate      _api_Infine.ScriptTerminate
#define FI_ScriptSuspend        _api_Infine.ScriptSuspend
#define FI_ScriptResume         _api_Infine.ScriptResume
#define FI_ScriptSuspended      _api_Infine.ScriptSuspended
#define FI_ScriptRequestSkip    _api_Infine.ScriptRequestSkip
#define FI_ScriptCmdExecuted    _api_Infine.ScriptCmdExecuted
#define FI_ScriptIsMenuTrigger  _api_Infine.ScriptIsMenuTrigger
#define FI_ScriptResponder      _api_Infine.ScriptResponder
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Infine);
#endif

///@}

#endif /* DOOMSDAY_API_INFINE_H */
