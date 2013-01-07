/**\file finaleinterpreter.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_FINALEINTERPRETER_H
#define LIBDENG_FINALEINTERPRETER_H

#include "dd_input.h"
#include "dd_ui.h"

#define FINF_BEGIN          0x01
#define FINF_END            0x02
#define FINF_SCRIPT         0x04 // Script included.
#define FINF_SKIP           0x10

/**
 * @defgroup finaleInterpreterCommandDirective Finale Interpreter Command Directive
 * @ingroup infine
 */
/*@{*/
#define FID_NORMAL          0
#define FID_ONLOAD          0x1
/*@}*/

/**
 * Interactive interpreter for Finale scripts. An instance of which is created
 * (and owned) by each active (running) script.
 *
 * @see Finale
 * @ingroup infine
 */
/// \todo Should be private.
typedef struct fi_handler_s {
    ddevent_t       ev; // Template.
    fi_objectname_t marker;
} fi_handler_t;

/// \todo Should be private.
typedef struct fi_namespace_s {
    uint            num;
    struct fi_namespace_record_s* vector;
} fi_namespace_t;

/// UIPage indices.
enum {
    PAGE_PICS = 0, /// @note also used for its background.
    PAGE_TEXT = 1 /// @note also used for its filter.
};

#define FINALEINTERPRETER_MAX_TOKEN_LENGTH (8192)

typedef struct finaleinterpreter_s {
    struct finaleinterpreter_flags_s {
        char stopped:1;
        char suspended:1;
        char paused:1;
        char can_skip:1;
        char eat_events:1; /// Script will eat all input events.
        char show_menu:1;
    } flags;

    /// Id of the Finale which owns this interpreter.
    finaleid_t _id;

    /// Copy of the script being interpreted.
    char* _script;

    /// Beginning of the script (after any directive blocks).
    char* _scriptBegin;

    /// Current position in the script.
    const char* _cp;

    /// Script token read/parse buffer.
    char _token[FINALEINTERPRETER_MAX_TOKEN_LENGTH];

    /// Event handlers defined by the loaded script.
    uint _numEventHandlers;
    fi_handler_t* _eventHandlers;

    /// Known symbols (to the loaded script).
    fi_namespace_t _namespace;

    /// Pages on which objects created by this interpeter are visible.
    struct fi_page_s* _pages[2];

    /// Set to true after first command is executed.
    boolean _cmdExecuted;
    boolean _skipping, _lastSkipped, _gotoSkip, _gotoEnd, _skipNext;

    /// Level of DO-skipping.
    int _doLevel;

    uint _timer;
    int _wait, _inTime;

    fi_objectname_t _gotoTarget;

    struct fi_object_s* _waitingText;
    struct fi_object_s* _waitingPic;
} finaleinterpreter_t;

finaleinterpreter_t* P_CreateFinaleInterpreter(void);
void P_DestroyFinaleInterpreter(finaleinterpreter_t* fi);

boolean FinaleInterpreter_RunTic(finaleinterpreter_t* fi);
int FinaleInterpreter_Responder(finaleinterpreter_t* fi, const ddevent_t* ev);

void FinaleInterpreter_LoadScript(finaleinterpreter_t* fi, const char* script);
void FinaleInterpreter_ReleaseScript(finaleinterpreter_t* fi);
void FinaleInterpreter_Suspend(finaleinterpreter_t* fi);
void FinaleInterpreter_Resume(finaleinterpreter_t* fi);

boolean FinaleInterpreter_IsMenuTrigger(finaleinterpreter_t* fi);
boolean FinaleInterpreter_IsSuspended(finaleinterpreter_t* fi);
boolean FinaleInterpreter_CommandExecuted(finaleinterpreter_t* fi);
boolean FinaleInterpreter_CanSkip(finaleinterpreter_t* fi);
void FinaleInterpreter_AllowSkip(finaleinterpreter_t* fi, boolean yes);

boolean FinaleInterpreter_SkipToMarker(finaleinterpreter_t* fi, const char* marker);
boolean FinaleInterpreter_Skip(finaleinterpreter_t* fi);

#endif /* LIBDENG_FINALEINTERPRETER_H */
