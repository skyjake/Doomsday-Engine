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

#ifndef LIBDENG_FINALEINTERPRETER_H
#define LIBDENG_FINALEINTERPRETER_H

/**
 * @defgroup playsimServerFinaleFlags Play-simulation Server-side Finale Flags.
 *
 * Packet: PSV_FINALE Finale flags. Used with GPT_FINALE and GPT_FINALE2
 */
/*@{*/
#define FINF_BEGIN          0x01
#define FINF_END            0x02
#define FINF_SCRIPT         0x04   // Script included.
#define FINF_AFTER          0x08   // Otherwise before.
#define FINF_SKIP           0x10
#define FINF_OVERLAY        0x20   // Otherwise before (or after).
/*@}*/

#define FINALE_SCRIPT_EXTRADATA_SIZE      gx.finaleConditionsSize

/// \todo Should be private.
typedef struct fi_handler_s {
    ddevent_t       ev; // Template.
    fi_objectname_t marker;
} fi_handler_t;

typedef struct finaleinterpreter_t {
    finale_mode_t   mode;
    struct finaleinterpreter_flags_s {
        char            stopped:1;
        char            can_skip:1;
        char            suspended:1;
        char            paused:1;
        char            eat_events:1; // Script will eat all input events.
        char            show_menu:1;
    } flags;
    char*           script; // A copy of the script.
    const char*     cp; // The command cursor.
    int             doLevel; // Level of DO-skipping.
    boolean         cmdExecuted; // Set to true after first command is executed.
    boolean         skipping, lastSkipped, gotoSkip, gotoEnd, skipNext;
    fi_objectname_t gotoTarget;
    int             wait;
    int             inTime;
    struct fi_object_s* waitingText;
    struct fi_object_s* waitingPic;

    uint            numEventHandlers;
    fi_handler_t*   eventHandlers;

    int             initialGameState; // Game state before the script began.
    int             overlayGameState; // Overlay scripts run only in one gameMode.
    void*           extraData;
} finaleinterpreter_t;

void                FinaleInterpreter_LoadScript(finaleinterpreter_t* fi, finale_mode_t mode, const char* script, int gameState, const void* extraData);
void                FinaleInterpreter_StopScript(finaleinterpreter_t* fi);
void                FinaleInterpreter_ReleaseScript(finaleinterpreter_t* fi);

void*               FinaleInterpreter_ExtraData(finaleinterpreter_t* fi);

boolean             FinaleInterpreter_IsMenuTrigger(finaleinterpreter_t* fi);
boolean             FinaleInterpreter_IsSuspended(finaleinterpreter_t* fi);
boolean             FinaleInterpreter_CommandExecuted(finaleinterpreter_t* fi);
boolean             FinaleInterpreter_CanSkip(finaleinterpreter_t* fi);
void                FinaleInterpreter_AllowSkip(finaleinterpreter_t* fi, boolean yes);

boolean             FinaleInterpreter_RunTic(finaleinterpreter_t* fi);
boolean             FinaleInterpreter_RunCommands(finaleinterpreter_t* fi);
boolean             FinaleInterpreter_SkipToMarker(finaleinterpreter_t* fi, const char* marker);
boolean             FinaleInterpreter_Skip(finaleinterpreter_t* fi);

int                 FinaleInterpreter_Responder(finaleinterpreter_t* fi, ddevent_t* ev);

#endif /* LIBDENG_FINALEINTERPRETER_H */
