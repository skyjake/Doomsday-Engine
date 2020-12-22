/** @file pause.cpp  Pausing the game.
 *
 * @authors Copyright © 1999-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "common.h"
#include "pause.h"

#include "d_net.h"
#include "d_netcl.h"
#include "d_netsv.h"
#include "g_common.h"
#include "hu_menu.h"
#include "hu_msg.h"

using namespace de;
using namespace common;

#define PAUSEF_PAUSED           0x1
#define PAUSEF_FORCED_PERIOD    0x2

int paused;

static int gamePauseWhenFocusLost; // cvar
static int gameUnpauseWhenFocusGained; // cvar

#ifdef __JDOOM__
/// How long to pause the game after a map has been loaded (cvar).
/// - -1: matches the engine's busy transition tics.
static int gamePauseAfterMapStartTics = -1; // cvar
#else
// Crossfade doesn't require a very long pause.
static int gamePauseAfterMapStartTics = 7; // cvar
#endif

static int forcedPeriodTicsRemaining;

static void beginPause(int flags)
{
    if(!paused)
    {
        paused = PAUSEF_PAUSED | flags;

        // This will stop all sounds from all origins.
        /// @todo Would be nice if the engine supported actually pausing the sounds. -jk
        S_StopSound(0, 0);

        // Servers are responsible for informing clients about
        // pauses in the game.
        NetSv_Paused(paused);
    }
}

static void endPause()
{
    if(paused)
    {
        LOG_VERBOSE("Pause ends (state:%i)") << paused;

        forcedPeriodTicsRemaining = 0;

        if(!(paused & PAUSEF_FORCED_PERIOD))
        {
            // Any impulses or accumulated relative offsets that occured
            // during the pause should be ignored.
            DD_Execute(true, "resetctlaccum");
        }

        NetSv_Paused(0);
    }
    paused = 0;
}

static void checkForcedPeriod()
{
    if((paused != 0) && (paused & PAUSEF_FORCED_PERIOD))
    {
        if(forcedPeriodTicsRemaining-- <= 0)
        {
            endPause();
        }
    }
}

dd_bool Pause_IsPaused()
{
    return (paused != 0) || (!IS_NETGAME && (Hu_MenuIsActive() || Hu_IsMessageActive()));
}

dd_bool Pause_IsUserPaused()
{
    return (paused != 0) && !(paused & PAUSEF_FORCED_PERIOD);
}

D_CMD(Pause)
{
    DE_UNUSED(src, argc, argv);

    if(G_QuitInProgress())
        return false;

    // Toggle pause.
    Pause_Set(!(paused & PAUSEF_PAUSED));
    return true;
}

void Pause_Set(dd_bool yes)
{
    // Can we start a pause?
    if(Hu_MenuIsActive() || Hu_IsMessageActive() || IS_CLIENT)
        return; // Nope.

    if(yes)
        beginPause(0);
    else
        endPause();
}

void Pause_End()
{
    endPause();
}

void Pause_SetForcedPeriod(int tics)
{
    if(tics <= 0) return;

    LOG_MSG("Forced pause for %i tics") << tics;

    forcedPeriodTicsRemaining = tics;
    beginPause(PAUSEF_FORCED_PERIOD);
}

void Pause_Ticker()
{
    checkForcedPeriod();
}

dd_bool Pause_Responder(event_t *ev)
{
    if(ev->type == EV_FOCUS)
    {
        if(gamePauseWhenFocusLost && !ev->data1)
        {
            Pause_Set(true);
            return true;
        }
        else if(gameUnpauseWhenFocusGained && ev->data1)
        {
            Pause_Set(false);
            return true;
        }
    }
    return false;
}

void Pause_MapStarted()
{
    if(!IS_CLIENT)
    {
        if(gamePauseAfterMapStartTics < 0)
        {
            // Use the engine's transition visualization duration.
            Pause_SetForcedPeriod(Con_GetInteger("con-transition-tics"));
        }
        else
        {
            // Use the configured time.
            Pause_SetForcedPeriod(gamePauseAfterMapStartTics);
        }
    }
}

void Pause_Register()
{
    forcedPeriodTicsRemaining = 0;

    // Default values (overridden by values from .cfg files).
    gamePauseWhenFocusLost     = true;
    gameUnpauseWhenFocusGained = false;

    C_CMD("pause", "", Pause);

#define READONLYCVAR    (CVF_READ_ONLY|CVF_NO_MAX|CVF_NO_MIN|CVF_NO_ARCHIVE)

    C_VAR_INT("game-paused",              &paused,                     READONLYCVAR, 0,  0);
    C_VAR_INT("game-pause-focuslost",     &gamePauseWhenFocusLost,     0,            0,  1);
    C_VAR_INT("game-unpause-focusgained", &gameUnpauseWhenFocusGained, 0,            0,  1);
    C_VAR_INT("game-pause-mapstart-tics", &gamePauseAfterMapStartTics, 0,            -1, 70);
    
#undef READONLYCVAR
}

void NetSv_Paused(int pauseState)
{
    if(!IS_SERVER || !IS_NETGAME)
        return;

    writer_s *writer = D_NetWrite();
    Writer_WriteByte(writer,
            (pauseState & PAUSEF_PAUSED?        1 : 0) |
            (pauseState & PAUSEF_FORCED_PERIOD? 2 : 0));
    Net_SendPacket(DDSP_ALL_PLAYERS, GPT_PAUSE, Writer_Data(writer), Writer_Size(writer));
}

void NetCl_Paused(reader_s *msg)
{
    byte flags = Reader_ReadByte(msg);
    paused = 0;
    if(flags & 1)
    {
        paused |= PAUSEF_PAUSED;
    }
    if(flags & 2)
    {
        paused |= PAUSEF_FORCED_PERIOD;
    }
    DD_SetInteger(DD_CLIENT_PAUSED, paused != 0);
}
