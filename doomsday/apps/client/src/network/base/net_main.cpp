/** @file net_main.cpp  Client/server networking.
 *
 * Player number zero is always the server. In single-player games there is only
 * the server present.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_base.h"
#include "dd_def.h"
#include "dd_loop.h"
#include "dd_main.h"
#include "api_console.h"
#include "world/p_players.h"
#include "network/net_main.h"
#include "network/net_buf.h"
#include "network/net_event.h"

#ifdef __CLIENT__
#  include "client/cl_def.h"
#  include "network/net_demo.h"
#  include "network/sys_network.h"
#  include "gl/gl_main.h"
#  include "render/rend_main.h"
#  include "render/blockmapvisual.h"
#  include "render/viewports.h"
#  include "api_fontrender.h"
#  include "ui/ui_main.h"
#  include "ui/inputdebug.h"
#  include "ui/widgets/taskbarwidget.h"
#  ifdef DE_DEBUG
#    include "ui/zonedebug.h"
#  endif
#  include <de/glinfo.h>
#endif
#ifdef __SERVER__
#  include "serversystem.h"
#  include "server/sv_def.h"
#  include "server/sv_frame.h"
#  include "server/sv_pool.h"
#endif

#include <doomsday/console/cmd.h>
#include <doomsday/console/exec.h>
#include <doomsday/console/var.h>
#include <doomsday/network/masterserver.h>
#include <de/legacy/concurrency.h>
#include <de/legacy/timer.h>
#include <de/charsymbols.h>
#include <de/value.h>
#include <de/version.h>

using namespace de;

#define ACK_THRESHOLD_MUL       1.5f  ///< The threshold is the average ack time * mul.
#define ACK_MINIMUM_THRESHOLD   50    ///< Never wait a too short time for acks.

char *playerName = (char *) "Player";

netstate_t netState = {
    true, // first update
    0, 0, 0, 0.0f, 0
};

static byte monitorMsgQueue;
static byte netDev;

// Local packets are stored into this buffer.
static dd_bool reboundPacket;
static netbuffer_t reboundStore;

#ifdef __CLIENT__
static int coordTimer;
#endif

void Net_Init()
{
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        DD_Player(i)->viewConsole = -1;
    }

    std::memset(&::netBuffer, 0, sizeof(::netBuffer));
    ::netBuffer.headerLength = ::netBuffer.msg.data - (byte *) &::netBuffer.msg;
    // The game is always started in single-player mode.
    netState.netGame = false;
}

void Net_Shutdown()
{
    netState.netGame = false;
    N_Shutdown();
}

#undef Net_GetPlayerName
DE_EXTERN_C const char *Net_GetPlayerName(int player)
{
    return DD_Player(player)->name;
}

#undef Net_GetPlayerID
DE_EXTERN_C ident_t Net_GetPlayerID(int player)
{
#ifdef __SERVER__
    auto &cl = *DD_Player(player);
    if(cl.isConnected())
        return cl.id;
#else
    DE_UNUSED(player);
#endif
    return 0;
}

/**
 * Sends the contents of the netBuffer.
 */
void Net_SendBuffer(int toPlayer, int spFlags)
{
#ifdef __CLIENT__
    // Don't send anything during demo playback.
    if(::playback) return;
#endif

    ::netBuffer.player = toPlayer;

    // A rebound packet?
    if(spFlags & SPF_REBOUND)
    {
        ::reboundStore  = ::netBuffer;
        ::reboundPacket = true;
        return;
    }

#ifdef __CLIENT__
    Demo_WritePacket(toPlayer);
#endif

    // Can we send the packet?
    if(spFlags & SPF_DONT_SEND)
        return;

    // Send the packet to the network.
    N_SendPacket(spFlags);
}

/**
 * @return @c false, if there are no packets waiting.
 */
dd_bool Net_GetPacket()
{
    if(reboundPacket)  // Local packets rebound.
    {
        ::netBuffer = ::reboundStore;
        ::netBuffer.player = ::consolePlayer;
        //::netBuffer.cursor = ::netBuffer.msg.data;
        ::reboundPacket = false;
        return true;
    }

#ifdef __CLIENT__
    if(::playback)
    {
        // We're playing a demo. This overrides all other packets.
        return Demo_ReadPacket();
    }
#endif

    if(!netState.netGame)
    {
        // Packets cannot be received.
        return false;
    }

    if(!N_GetPacket())
        return false;

#ifdef __CLIENT__
    // Are we recording a demo?
    DE_ASSERT(consolePlayer >= 0 && consolePlayer < DDMAXPLAYERS);
    if(netState.isClient && DD_Player(::consolePlayer)->recording)
    {
        Demo_WritePacket(::consolePlayer);
    }
#endif

    return true;
}

#undef Net_PlayerSmoother
DE_EXTERN_C Smoother* Net_PlayerSmoother(int player)
{
    if(player < 0 || player >= DDMAXPLAYERS)
        return 0;

    return DD_Player(player)->smoother();
}

void Net_SendPlayerInfo(int srcPlrNum, int destPlrNum)
{
    DE_ASSERT(srcPlrNum >= 0 && srcPlrNum < DDMAXPLAYERS);
    const dsize nameLen = strlen(DD_Player(srcPlrNum)->name);

    LOG_AS("Net_SendPlayerInfo");
    LOGDEV_NET_VERBOSE("src=%i dest=%i name=%s")
        << srcPlrNum << destPlrNum << DD_Player(srcPlrNum)->name;

    Msg_Begin(PKT_PLAYER_INFO);
    Writer_WriteByte(::msgWriter, srcPlrNum);
    Writer_WriteUInt16(::msgWriter, nameLen);
    Writer_Write(::msgWriter, DD_Player(srcPlrNum)->name, nameLen);
    Msg_End();
    Net_SendBuffer(destPlrNum, 0);
}

/**
 * This is the public interface of the message sender.
 */
#undef Net_SendPacket
DE_EXTERN_C void Net_SendPacket(int to_player, int type, const void *data, dsize length)
{
    duint flags = 0;

#ifndef DE_WRITER_TYPECHECK
    Msg_Begin(type);
    if(data) Writer_Write(::msgWriter, data, length);
    Msg_End();
#else
    DE_ASSERT(length <= NETBUFFER_MAXSIZE);
    ::netBuffer.msg.type = type;
    ::netBuffer.length = length;
    if(data) std::memcpy(::netBuffer.msg.data, data, length);
#endif

    if(netState.isClient)
    {
        // As a client we can only send messages to the server.
        Net_SendBuffer(0, flags);
    }
    else
    {   // The server can send packets to any player.
        // Only allow sending to the sixteen possible players.
        Net_SendBuffer((to_player & DDSP_ALL_PLAYERS) ? NSP_BROADCAST
                                                      : (to_player & 0xf),
                       flags);
    }
}

/**
 * Prints the message in the console.
 */
void Net_ShowChatMessage(int plrNum, const char *message)
{
    DE_ASSERT(plrNum >= 0 && plrNum < DDMAXPLAYERS);
    const char *fromName = (plrNum > 0 ? DD_Player(plrNum)->name : "[sysop]");
    const char *sep      = (plrNum > 0 ? ":"                    : "");
    LOG_NOTE("%s%s%s %s")
        << (!plrNum? _E(1) : _E(D))
        << fromName << sep << message;
}

/**
 * After a long period with no updates (map setup), calling this will reset the tictimer
 * so that no time seems to have passed.
 */
void Net_ResetTimer()
{
    netState.firstUpdate = true;

    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        Smoother_Clear(DD_Player(i)->smoother());
    }
}

/**
 * @return @c true if the specified player is a real, local player.
 */
dd_bool Net_IsLocalPlayer(int plrNum)
{
    DE_ASSERT(plrNum >= 0 && plrNum < DDMAXPLAYERS);
    const auto &pd = DD_Player(plrNum)->publicData();
    return pd.inGame && (pd.flags & DDPF_LOCAL);
}

/**
 * Send the local player(s) ticcmds to the server.
 */
void Net_SendCommands()
{}

static void Net_DoUpdate()
{
    static int lastTime = 0;

    // This timing is only used by the client when it determines if it is time to
    // send ticcmds or coordinates to the server.

    // Check time.
    const int nowTime = Timer_Ticks();

    // Clock reset?
    if(netState.firstUpdate)
    {
        netState.firstUpdate = false;
        lastTime = nowTime;
    }

    const int newTics = nowTime - lastTime;
    if(newTics <= 0) return;  // Nothing new to update.

    lastTime = nowTime;

    // This is as far as dedicated servers go.
#ifdef __CLIENT__

    // Clients will periodically send their coordinates to the server so any prediction
    // errors can be fixed. Client movement is almost entirely local.
    DE_ASSERT(::consolePlayer >= 0 && ::consolePlayer < DDMAXPLAYERS);

    ::coordTimer -= newTics;
    if(netState.isClient && ::coordTimer <= 0 && DD_Player(::consolePlayer)->publicData().mo)
    {
        mobj_t *mob = DD_Player(::consolePlayer)->publicData().mo;

        ::coordTimer = 1; //netCoordTime; // 35/2

        Msg_Begin(PKT_COORDS);
        Writer_WriteFloat(::msgWriter, ::gameTime);
        Writer_WriteFloat(::msgWriter, mob->origin[VX]);
        Writer_WriteFloat(::msgWriter, mob->origin[VY]);
        if(mob->origin[VZ] == mob->floorZ)
        {
            // This'll keep us on the floor even in fast moving sectors.
            Writer_WriteInt32(::msgWriter, DDMININT);
        }
        else
        {
            Writer_WriteInt32(::msgWriter, FLT2FIX(mob->origin[VZ]));
        }
        // Also include angles.
        Writer_WriteUInt16(::msgWriter, mob->angle >> 16);
        Writer_WriteInt16(::msgWriter, P_LookDirToShort(DD_Player(::consolePlayer)->publicData().lookDir));
        // Control state.
        Writer_WriteChar(::msgWriter, FLT2FIX(DD_Player(::consolePlayer)->publicData().forwardMove) >> 13);
        Writer_WriteChar(::msgWriter, FLT2FIX(DD_Player(::consolePlayer)->publicData().sideMove) >> 13);
        Msg_End();

        Net_SendBuffer(0, 0);
    }
#endif // __CLIENT__
}

/**
 * Handle incoming packets, clients send ticcmds and coordinates to the server.
 */
void Net_Update()
{
    Net_DoUpdate();

    // Check for received packets.
#ifdef __CLIENT__
    Cl_GetPackets();
#endif
}

/**
 * This is the network one-time initialization (into single-player mode).
 */
void Net_InitGame()
{
#ifdef __CLIENT__
    Cl_InitID();
#endif

    // In single-player mode there is only player number zero.
    ::consolePlayer = ::displayPlayer = 0;

    // We're in server mode if we aren't a client.
    netState.isServer = true;

    // Netgame is true when we're aware of the network (i.e. other players).
    netState.netGame = false;

    DD_Player(0)->publicData().inGame = true;
    DD_Player(0)->publicData().flags |= DDPF_LOCAL;

#ifdef __CLIENT__
    DD_Player(0)->id          = ::clientID;
#endif
    DD_Player(0)->viewConsole = 0;
}

void Net_StopGame()
{
    LOG_AS("Net_StopGame");

#ifdef __SERVER__
    if(netState.isServer)
    {
        // We are an open server.
        // This means we should inform all the connected clients that the
        // server is about to close.
        Msg_Begin(PSV_SERVER_CLOSE);
        Msg_End();
        Net_SendBuffer(NSP_BROADCAST, 0);
    }
#endif

#ifdef __CLIENT__
    LOGDEV_NET_MSG("Sending PCL_GOODBYE");

    // We are a connected client.
    Msg_Begin(PCL_GOODBYE);
    Msg_End();
    Net_SendBuffer(0, 0);

    // Must stop recording, we're disconnecting.
    Demo_StopRecording(::consolePlayer);
    Cl_CleanUp();
    netState.isClient    = false;
    ::netLoggedIn = false;
#endif

    // Netgame has ended.
    netState.netGame  = false;
    netState.isServer = true;
    allowSending      = false;

#ifdef __SERVER__
    // No more remote users.
    ::netRemoteUser = 0;
#endif

    // All remote players are forgotten.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        player_t &plr = *DD_Player(i);

#ifdef __SERVER__
        plr.ready = false;
        plr.remoteUserId = 0;
#endif
        plr.id         = 0;
        plr.viewConsole = -1;

        plr.publicData().inGame = false;
        plr.publicData().flags &= ~(DDPF_CAMERA | DDPF_CHASECAM | DDPF_LOCAL);
    }

    // We're about to become player zero, so update it's view angles to match
    // our current ones.
    if(DD_Player(0)->publicData().mo)
    {
        /* $unifiedangles */
        DE_ASSERT(::consolePlayer >= 0 && ::consolePlayer < DDMAXPLAYERS);
        DD_Player(0)->publicData().mo->angle = DD_Player(::consolePlayer)->publicData().mo->angle;
        DD_Player(0)->publicData().lookDir   = DD_Player(::consolePlayer)->publicData().lookDir;
    }

    LOGDEV_NET_NOTE("Reseting console and view players to zero");

    ::consolePlayer = ::displayPlayer = 0;

    DD_Player(0)->viewConsole = 0;

    DD_Player(0)->publicData().inGame = true;
    DD_Player(0)->publicData().flags |= DDPF_LOCAL;
}

/**
 * Returns a delta based on 'now' (- future, + past).
 */
int Net_TimeDelta(byte now, byte then)
{
    int delta;

    if(now >= then)
    {
        // Simple case.
        delta = now - then;
    }
    else
    {
        // There's a wraparound.
        delta = 256 - then + now;
    }

    // The time can be in the future. We'll allow one second.
    if(delta > 220)
        delta -= 256;

    return delta;
}

#ifdef __CLIENT__
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
#endif

#ifdef __CLIENT__

void Net_DrawDemoOverlay()
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

#endif  // __CLIENT__

void Net_Drawer()
{
#ifdef __CLIENT__
    // Draw the blockmap debug display.
    Rend_BlockmapDebug();

    // Draw the light range debug display.
    Rend_DrawLightModMatrix();

# ifdef DE_DEBUG
    // Draw the input debug display.
    I_DebugDrawer();
# endif

    // Draw the demo recording overlay.
    Net_DrawDemoOverlay();

# if defined (DE_DEBUG) && defined (DE_OPENGL)
    Z_DebugDrawer();
# endif
#endif  // __CLIENT__
}

void Net_Ticker(timespan_t time)
{
    // Network event ticker.
    N_NETicker(time);

#ifdef __SERVER__
    if(netDev)
    {
        static int printTimer = 0;

        if(printTimer++ > TICSPERSEC)
        {
            printTimer = 0;
            for(int i = 0; i < DDMAXPLAYERS; ++i)
            {
                if(Sv_IsFrameTarget(i))
                {
                    LOGDEV_NET_MSG("%i(rdy:%b): avg=%05ims thres=%05ims "
                                   "maxfs=%05ib unakd=%05i")
                        << i << DD_Player(i)->ready << 0 << 0
                        << Sv_GetMaxFrameSize(i)
                        << Sv_CountUnackedDeltas(i);
                }
            }
        }
    }
#endif // __SERVER__

    // The following stuff is only for netgames.
    if(!netState.netGame) return;

    // Check the pingers.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        auto &cl = *DD_Player(i);

        // Clients can only ping the server.
        if(!(netState.isClient && i) && i != ::consolePlayer)
        {
            if(cl.pinger().sent)
            {
                // The pinger is active.
                if(Timer_RealMilliseconds() - cl.pinger().sent > PING_TIMEOUT)  // Timed out?
                {
                    cl.pinger().times[cl.pinger().current] = -1;
                    Net_SendPing(i, 0);
                }
            }
        }
    }
}

/**
 * Composes a PKT_CHAT network message.
 */
void Net_WriteChatMessage(int from, int toMask, const char *message)
{
    const auto len = de::min<dsize>(strlen(message), 0xffff);

    Msg_Begin(PKT_CHAT);
    Writer_WriteByte(::msgWriter, from);
    Writer_WriteUInt32(::msgWriter, toMask);
    Writer_WriteUInt16(::msgWriter, len);
    Writer_Write(::msgWriter, message, len);
    Msg_End();
}

/**
 * All arguments are sent out as a chat message.
 */
D_CMD(Chat)
{
    DE_UNUSED(src);

    int mode = !stricmp(argv[0], "chat") ||
                !stricmp(argv[0], "say") ? 0 : !stricmp(argv[0], "chatNum") ||
                !stricmp(argv[0], "sayNum") ? 1 : 2;

    if(argc == 1)
    {
        LOG_SCR_NOTE("Usage: %s %s(text)") << argv[0]
                << (!mode ? "" : mode == 1 ? "(plr#) " : "(name) ");
        LOG_SCR_MSG("Chat messages are max 80 characters long. Use quotes to get around "
                    "arg processing.");
        return true;
    }

    LOG_AS("chat (Cmd)");

    // Chatting is only possible when connected.
    if(!netState.netGame) return false;

    // Too few arguments?
    if(mode && argc < 3) return false;

    // Assemble the chat message.
    std::string buffer;
    buffer = argv[!mode ? 1 : 2];;
    for(int i = (!mode ? 2 : 3); i < argc; ++i)
    {
        buffer += " ";
        buffer += argv[i];
    }

    // Send the message.
    dushort mask = 0;
    switch(mode)
    {
    case 0: // chat
        mask = ~0;
        break;

    case 1: // chatNum
        mask = 1 << String(argv[1]).toInt();
        break;

    case 2: // chatTo
        for(int i = 0; i < DDMAXPLAYERS; ++i)
        {
            if(!stricmp(DD_Player(i)->name, argv[1]))
            {
                mask = 1 << i;
                break;
            }
        }
        break;

    default:
        LOG_SCR_ERROR("Invalid value, mode = %i") << mode;
        return false;
    }

    Net_WriteChatMessage(::consolePlayer, mask, buffer.c_str());

    if(!netState.isClient)
    {
        if(mask == (dushort) ~0)
        {
            Net_SendBuffer(NSP_BROADCAST, 0);
        }
        else
        {
            for(int i = 1; i < DDMAXPLAYERS; ++i)
            {
                if(DD_Player(i)->publicData().inGame && (mask & (1 << i)))
                    Net_SendBuffer(i, 0);
            }
        }
    }
    else
    {
        Net_SendBuffer(0, 0);
    }

    // Show the message locally.
    Net_ShowChatMessage(::consolePlayer, buffer.c_str());

    // Inform the game, too.
    gx.NetPlayerEvent(::consolePlayer, DDPE_CHAT_MESSAGE, (void *) buffer.c_str());

    return true;
}

#ifdef __CLIENT__
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
#endif

D_CMD(SetTicks)
{
    DE_UNUSED(src, argc);

    netState.firstUpdate = true;
    Timer_SetTicksPerSecond(String(argv[1]).toDouble());
    return true;
}

// Create a new local player.
D_CMD(MakeCamera)
{
    DE_UNUSED(src, argc);

    LOG_AS("makecam (Cmd)");

    int cp = String(argv[1]).toInt();
    if(cp < 0 || cp >= DDMAXPLAYERS)
        return false;

    /// @todo Should make a LocalPlayer; 'connected' is server-side.
/*    if(::clients[cp].connected)
    {
        LOG_ERROR("Client %i already connected") << cp;
        return false;
    }

    ::clients[cp].connected   = true;*/
    //DD_Player(cp)->ready       = true;
    DD_Player(cp)->viewConsole = cp;

    DD_Player(cp)->publicData().flags |= DDPF_LOCAL;
    Smoother_Clear(DD_Player(cp)->smoother());

#ifdef __SERVER__
    Sv_InitPoolForClient(cp);
#endif

#ifdef __CLIENT__
    R_SetupDefaultViewWindow(cp);

    // Update the viewports.
    R_SetViewGrid(0, 0);
#endif

    return true;
}

#ifdef __CLIENT__

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

#endif // __CLIENT__

/**
 * The 'net' console command.
 */
D_CMD(Net)
{
    DE_UNUSED(src);

    bool success = true;

    if(argc == 1) // No args?
    {
        LOG_SCR_NOTE("Usage: %s (cmd/args)") << argv[0];
        LOG_SCR_MSG("Commands:");
        LOG_SCR_MSG("  init");
        LOG_SCR_MSG("  shutdown");
        LOG_SCR_MSG("  info");
        LOG_SCR_MSG("  request");
#ifdef __CLIENT__
        LOG_SCR_MSG("  setup client");
        LOG_SCR_MSG("  search (address) [port]   (local or targeted query)");
        LOG_SCR_MSG("  servers   (asks the master server)");
        LOG_SCR_MSG("  connect (idx)");
        LOG_SCR_MSG("  mconnect (m-idx)");
        LOG_SCR_MSG("  disconnect");
#endif
#ifdef __SERVER__
        LOG_SCR_MSG("  announce");
#endif
        return true;
    }

    if(argc == 2) // One argument?
    {
#ifdef __SERVER__
        if(!stricmp(argv[1], "announce"))
        {
            N_MasterAnnounceServer(serverPublic? true : false);
        }
        else
#endif
        if(!stricmp(argv[1], "request"))
        {
            N_MasterRequestList();
        }
        else if(!stricmp(argv[1], "servers"))
        {
            N_MAPost(MAC_REQUEST);
            N_MAPost(MAC_WAIT);
            N_MAPost(MAC_LIST);
        }
        else if(!stricmp(argv[1], "info"))
        {
            N_PrintNetworkStatus();
            LOG_NET_MSG("Network game: %b") << netState.netGame;
            LOG_NET_MSG("This is console %i (local player %i)")
                << ::consolePlayer << P_ConsoleToLocal(::consolePlayer);
        }
#ifdef __CLIENT__
        else if(!stricmp(argv[1], "disconnect"))
        {
            if(!netState.netGame)
            {
                LOG_NET_ERROR("This client is not connected to a server");
                return false;
            }

            if(!netState.isClient)
            {
                LOG_NET_ERROR("This is not a client");
                return false;
            }

            Net_ServerLink().disconnect();

            LOG_NET_NOTE("Disconnected");
        }
#endif
        else
        {
            LOG_SCR_ERROR("Invalid arguments");
            return false; // Bad args.
        }
    }

    if(argc == 3) // Two arguments?
    {
#ifdef __CLIENT__
        if(!stricmp(argv[1], "search"))
        {
            Net_ServerLink().discover(argv[2]);
        }
        else if(!stricmp(argv[1], "connect"))
        {
            if(netState.netGame)
            {
                LOG_NET_ERROR("Already connected");
                return false;
            }

            int index = strtoul(argv[2], 0, 10);
            //serverinfo_t info;
            ServerInfo info;
            if(Net_ServerLink().foundServerInfo(index, info))
            {
                info.printToLog(index);
                //ServerInfo_Print(&info, index);
                Net_ServerLink().connectDomain(info.address().asText(), 5.0);
            }
        }
        else if(!stricmp(argv[1], "mconnect"))
        {
            ServerInfo info;
            if(N_MasterGet(strtol(argv[2], 0, 0), &info))
            {
                // Connect using TCP/IP.
                return Con_Executef(CMDS_CONSOLE, false, "connect %s",
                                    info.address().asText().c_str());
            }
            else return false;
        }
#endif
    }

#ifdef __CLIENT__
    if(argc == 4)
    {
        if(!stricmp(argv[1], "search"))
        {
            Net_ServerLink().discover(String(argv[2]) + ":" + argv[3]);
        }
    }
#endif

    return success;
}

D_CMD(Ping);  // net_ping.cpp
#ifdef __CLIENT__
D_CMD(Login);  // cl_main.cpp
#endif

void Net_Register()
{
#ifdef DE_DEBUG
    C_VAR_FLOAT     ("net-dev-latency",         &netState.simulatedLatencySeconds, CVF_NO_MAX, 0, 0);
#endif

    C_VAR_BYTE      ("net-queue-show",          &monitorMsgQueue, 0, 0, 1);
    C_VAR_BYTE      ("net-dev",                 &netDev, 0, 0, 1);
    C_VAR_CHARPTR   ("net-name",                &playerName, 0, 0, 0);

    C_CMD_FLAGS ("chat",    nullptr,    Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS ("chatnum", nullptr,    Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS ("chatto",  nullptr,    Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS ("conlocp", "i",        MakeCamera, CMDF_NO_NULLGAME);
    C_CMD       ("net",     nullptr,    Net);
    C_CMD_FLAGS ("ping",    nullptr,    Ping, CMDF_NO_NULLGAME);
    C_CMD_FLAGS ("say",     nullptr,    Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS ("saynum",  nullptr,    Chat, CMDF_NO_NULLGAME);
    C_CMD_FLAGS ("sayto",   nullptr,    Chat, CMDF_NO_NULLGAME);
    C_CMD       ("settics", "i",        SetTicks);

#ifdef __CLIENT__
    C_CMD_FLAGS ("connect", nullptr,    Connect, CMDF_NO_NULLGAME | CMDF_NO_DEDICATED);
    C_CMD       ("setname", "s",        SetName);
    C_CMD       ("setcon",  "i",        SetConsole);

    N_Register();
#endif

#ifdef __SERVER__
    Server_Register();
#endif
}
