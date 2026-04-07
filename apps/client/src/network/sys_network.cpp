/** @file sys_network.cpp  Low-level network socket routines (deprecated).
 *
 * @todo Remove this source file entirely once dependent code is revised.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "network/sys_network.h"

#include "api_console.h"
#include "api_fontrender.h"
#include "client/cl_def.h"
#include "clientapp.h"
#include "dd_loop.h"
#include "gl/gl_main.h"
#include "network/net_buf.h"
#include "network/net_demo.h"
#include "network/sys_network.h"
#include "render/blockmapvisual.h"
#include "render/rend_main.h"
#include "render/viewports.h"
#include "ui/inputdebug.h"
#include "ui/ui_main.h"
#include "ui/widgets/taskbarwidget.h"
#include "world/p_players.h"

#ifdef DE_DEBUG
#  include "ui/zonedebug.h"
#endif

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include <de/glinfo.h>

using namespace de;

char *playerName = (char *) "Player";

D_CMD(SetName)
{
    DE_UNUSED(src, argc);

    Con_SetString("net-name", argv[1]);

    if(!netState.netGame) return true;

    // The server does not have a name.
    if(!netState.isClient) return false;

    auto &cl = *DD_Player(::consolePlayer);
    std::memset(cl.name, 0, sizeof(cl.name));
    strncpy(cl.name, argv[1], PLAYERNAMELEN - 1);

    Net_SendPlayerInfo(::consolePlayer, 0);
    return true;
}

D_CMD(SetConsole)
{
    DE_UNUSED(src, argc);

    int cp = String(argv[1]).toInt();
    if(cp < 0 || cp >= DDMAXPLAYERS)
    {
        LOG_SCR_ERROR("Invalid player #%i") << cp;
        return false;
    }

    if(DD_Player(cp)->publicData().inGame)
    {
        ::consolePlayer = ::displayPlayer = cp;
    }

    // Update the viewports.
    R_SetViewGrid(0, 0);
    return true;
}

int Net_StartConnection(const char *address, int port)
{
    LOG_AS("Net_StartConnection");
    LOG_NET_MSG("Connecting to %s (port %i)...") << address << port;

    // Start searching at the specified location.
    Net_ServerLink().connectDomain(Stringf("%s:%i", address, port), 7.0 /*timeout*/);
    return true;
}

/**
 * Intelligently connect to a server. Just provide an IP address and the rest is automatic.
 */
D_CMD(Connect)
{
    DE_UNUSED(src);

    if(argc < 2 || argc > 3)
    {
        LOG_SCR_NOTE("Usage: %s (ip-address) [port]") << argv[0];
        LOG_SCR_MSG("A TCP/IP connection is created to the given server. If a port is not "
                    "specified port zero will be used");
        return true;
    }

    if(netState.netGame)
    {
        LOG_NET_ERROR("Already connected");
        return false;
    }

    // If there is a port specified in the address, use it.
    int port = 0;
    char *ptr;
    if((ptr = strrchr(argv[1], ':')))
    {
        port = strtol(ptr + 1, 0, 0);
        *ptr = 0;
    }
    if(argc == 3)
    {
        port = strtol(argv[2], 0, 0);
    }

    return Net_StartConnection(argv[1], port);
}

void N_Register(void)
{
    C_VAR_CHARPTR("net-name", &playerName, 0, 0, 0);

    C_CMD_FLAGS ("connect", nullptr,    Connect, CMDF_NO_NULLGAME | CMDF_NO_DEDICATED);
    C_CMD       ("setname", "s",        SetName);
    C_CMD       ("setcon",  "i",        SetConsole);
}

ServerLink &Net_ServerLink(void)
{
    return ClientApp::app().serverLink();
}

/**
 * Called from "net info" (client-side).
 */
void N_PrintNetworkStatus(void)
{
    if (netState.isClient)
    {
        LOG_NET_NOTE(_E(b) "CLIENT: " _E(.) "Connected to server at %s") << Net_ServerLink().address();
    }
    else
    {
        LOG_NET_NOTE(_E(b) "OFFLINE: " _E(.) "Single-player mode");
    }
    N_PrintBufferInfo();
}

/**
 * Returns @c true if a demo is currently being recorded.
 */
static dd_bool recordingDemo()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(DD_Player(i)->publicData().inGame && DD_Player(i)->recording)
            return true;
    }
    return false;
}

static void Net_DrawDemoOverlay()
{
    const int x = DE_GAMEVIEW_WIDTH - 10;
    const int y = 10;

    if(!recordingDemo() || !(SECONDS_TO_TICKS(::gameTime) & 8))
        return;

    char buf[160];
    strcpy(buf, "[");
    {
        int count = 0;
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            auto *plr = DD_Player(i);
            if(plr->publicData().inGame && plr->recording)
            {
                // This is a "real" player (or camera).
                if(count++)
                    strcat(buf, ",");

                char tmp[40]; sprintf(tmp, "%i:%s", i, plr->recordPaused ? "-P-" : "REC");
                strcat(buf, tmp);
            }
        }
    }
    strcat(buf, "]");

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    // Go into screen projection mode.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, DE_GAMEVIEW_WIDTH, DE_GAMEVIEW_HEIGHT, -1, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(::fontFixed);
    FR_LoadDefaultAttrib();
    FR_SetColorAndAlpha(1, 1, 1, 1);
    FR_DrawTextXY3(buf, x, y, ALIGN_TOPRIGHT, DTF_NO_EFFECTS);

    DGL_Disable(DGL_TEXTURE_2D);

    // Restore original matrix.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}

void Net_Drawer()
{
    // Draw the blockmap debug display.
    Rend_BlockmapDebug();

    // Draw the light range debug display.
    Rend_DrawLightModMatrix();

#ifdef DE_DEBUG
    // Draw the input debug display.
    I_DebugDrawer();
#endif

    // Draw the demo recording overlay.
    Net_DrawDemoOverlay();

#if defined (DE_DEBUG) && defined (DE_OPENGL)
    Z_DebugDrawer();
#endif
}
