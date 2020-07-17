/** @file net_demo.cpp  Handling of demo recording and playback.
 *
 * Opening of, writing to, reading from and closing of demo files.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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
#include "network/net_demo.h"

#include <doomsday/doomsdayapp.h>
#include <doomsday/console/cmd.h>
#include <doomsday/filesys/fs_util.h>
#include <doomsday/network/net.h>
#include <doomsday/network/protocol.h>

#include "client/cl_player.h"

#include "api_filesys.h"
#include "api_player.h"

#include "network/net_main.h"
#include "network/net_buf.h"

#include "render/rend_main.h"
#include "render/viewports.h"

#include "world/p_object.h"
#include "world/p_players.h"

using namespace de;

#define DEMOTIC SECONDS_TO_TICKS(demoTime)

// Local Camera flags.
#define LCAMF_ONGROUND      0x1
#define LCAMF_FOV           0x2  ///< FOV has changed (short).
#define LCAMF_CAMERA        0x4  ///< Camera mode.

#pragma pack(1)
struct demopacket_header_t
{
    dushort length;
};
#pragma pack()

extern dfloat netConnectTime;

static const char *demoPath = "/home/demo/";

#if 0
LZFILE *playdemo;
#endif
dint playback;
dint viewangleDelta;
dfloat lookdirDelta;
dfloat posDelta[3];
dfloat demoFrameZ, demoZ;
dd_bool demoOnGround;

static DemoTimer readInfo;
static dfloat startFOV;
static dint demoStartTic;

void Demo_WriteLocalCamera(dint plrNum);

void Demo_Init()
{
    // Make sure the demo path is there.
    //F_MakePath(demoPath);
}

/**
 * Open a demo file and begin recording.
 * Returns @c false if the recording can't be begun.
 */
dd_bool Demo_BeginRecording(const char * /*fileName*/, dint /*plrNum*/)
{
    return false;

#if 0
    client_t *cl  = &::clients[plrNum];
    player_t *plr = &DD_Player(plrNum);

    // Is a demo already being recorded for this client?
    if(cl->recording || ::playback || (::isDedicated && !plrNum) || !plr->shared.inGame)
        return false;

    // Compose the real file name.
    ddstring_t buf; Str_Appendf(Str_InitStd(&buf), "%s%s", ::demoPath, fileName);
    F_ExpandBasePath(&buf, &buf);
    F_ToNativeSlashes(&buf, &buf);

    // Open the demo file.
    cl->demo = lzOpen(Str_Text(&buf), "wp");
    Str_Free(&buf);
    if(!cl->demo)
    {
        return false;  // Couldn't open it!
    }

    cl->recording    = true;
    cl->recordPaused = false;

    ::writeInfo[plrNum].first       = true;
    ::writeInfo[plrNum].canwrite    = false;
    ::writeInfo[plrNum].cameratimer = 0;
    ::writeInfo[plrNum].fov         = -1;  // Must be written in the first packet.

    if(netState.isServer)
    {
        // Playing demos alters gametic. This'll make sure we're going to get updates.
        ::clients[0].lastTransmit = -1;
        // Servers need to send a handshake packet.
        // It only needs to recorded in the demo file, though.
        ::allowSending = false;
        Sv_Handshake(plrNum, false);
        // Enable sending to network.
        ::allowSending = true;
    }
    else
    {
        // Clients need a Handshake packet.
        // Request a new one from the server.
        Cl_SendHello();
    }

    // The operation is a success.
    return true;
#endif
}

void Demo_PauseRecording(dint playerNum)
{
    DE_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    auto &cl = *DD_Player(playerNum);

    // A demo is not being recorded?
    if(!cl.recording || cl.recordPaused)
        return;

    // All packets will be written for the same tic.
    cl.demoTimer().pausetime = SECONDS_TO_TICKS(demoTime);
    cl.recordPaused = true;
}

/**
 * Resumes a paused recording.
 */
void Demo_ResumeRecording(dint playerNum)
{
    DE_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    auto &cl = *DD_Player(playerNum);

    // Not recording or not paused?
    if(!cl.recording || !cl.recordPaused)
        return;

    Demo_WriteLocalCamera(playerNum);
    cl.recordPaused = false;
    // When the demo is read there can't be a jump in the timings, so we
    // have to make it appear the pause never happened; begintime is
    // moved forwards.
    cl.demoTimer().begintime += DEMOTIC - cl.demoTimer().pausetime;
}

/**
 * Stop recording a demo.
 */
void Demo_StopRecording(dint playerNum)
{
    DE_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    auto &cl = *DD_Player(playerNum);

    // A demo is not being recorded?
    if(!cl.recording) return;

    // Close demo file.
    //lzClose(cl.demo); cl.demo = nullptr;
    cl.recording = false;
}

void Demo_WritePacket(dint playerNum)
{
    DE_UNUSED(playerNum);
#if 0
    if(playerNum < 0)
    {
        Demo_BroadcastPacket();
        return;
    }

    DE_ASSERT(playerNum >= 0 && playerNum < DDMAXPLAYERS);
    auto &cl = *DD_Player(playerNum);
    DemoTimer &inf = cl.demoTimer();

    // Is this client recording?
    if(!cl.recording)
        return;

    if(!inf.canwrite)
    {
        if(::netBuffer.msg.type != PSV_HANDSHAKE)
            return;

        // The handshake has arrived. Now we can begin writing.
        inf.canwrite = true;
    }

    if(cl.recordPaused)
    {
        // Some types of packet are not written in record-paused mode.
        if(::netBuffer.msg.type == PSV_SOUND ||
           ::netBuffer.msg.type == DDPT_MESSAGE)
            return;
    }

    // This counts as an update. (We know the client is alive.)
    //::clients[playerNum].updateCount = UPDATECOUNT;

    LZFILE *file = cl.demo;
    if(!file) App_Error("Demo_WritePacket: No demo file!\n");

    byte ptime;
    if(!inf.first)
    {
        ptime = (cl.recordPaused ? inf.pausetime : DEMOTIC)
              - inf.begintime;
    }
    else
    {
        ptime = 0;
        inf.first     = false;
        inf.begintime = DEMOTIC;
    }
    lzWrite(&ptime, 1, file);

    demopacket_header_t hdr;  // The header.
    if(::netBuffer.length >= sizeof(hdr.length))
        App_Error("Demo_WritePacket: Write buffer too large!\n");

    hdr.length = (dushort) 1 + ::netBuffer.length;
    lzWrite(&hdr, sizeof(hdr), file);

    // Write the packet itself.
    lzPutC(::netBuffer.msg.type, file);
    lzWrite(::netBuffer.msg.data, (long) netBuffer.length, file);
#endif
}

void Demo_BroadcastPacket()
{
    // Write packet to all recording demo files.
    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        Demo_WritePacket(i);
    }
}

dd_bool Demo_BeginPlayback(const char *fileName)
{
    // Already in playback?
    if(::playback) return false;
    // Playback not possible?
    if(netState.netGame || netState.isClient) return false;

    // Check that we aren't recording anything.
    for(dint i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(DD_Player(i)->recording)
            return false;
    }

    // Compose the real file name.
    ddstring_t buf; Str_Set(Str_InitStd(&buf), fileName);
    if(!F_IsAbsolute(&buf))
    {
        Str_Prepend(&buf, demoPath);
    }
    F_ExpandBasePath(&buf, &buf);
    F_ToNativeSlashes(&buf, &buf);

    // Open the demo file.
#if 0
    playdemo = lzOpen(Str_Text(&buf), "rp");
#endif
    Str_Free(&buf);

#if 0
    if(!playdemo)
#endif
        return false;

    // OK, let's begin the demo.
    ::playback       = true;
    netState.isServer       = false;
    netState.isClient       = true;
    ::readInfo.first = true;
    ::viewangleDelta = 0;
    ::lookdirDelta   = 0;
    ::demoFrameZ     = 1;
    ::demoZ          = 0;
    ::startFOV       = 95; //Rend_FieldOfView();
    ::demoStartTic   = DEMOTIC;
    std::memset(::posDelta, 0, sizeof(::posDelta));

    // Start counting frames from here.
    /*if(ArgCheck("-timedemo"))
    {
        ::r_framecounter = 0;
    }*/

    return true;
}

void Demo_StopPlayback()
{
    if(!::playback) return;

#if 0
    LOG_MSG("Demo was %.2f seconds (%i tics) long.")
        << ((DEMOTIC - ::demoStartTic) / dfloat( TICSPERSEC ))
        << (DEMOTIC - ::demoStartTic);

    ::playback = false;
    lzClose(::playdemo); playdemo = nullptr;
    //::fieldOfView = ::startFOV;
    Net_StopGame();

    /*if(ArgCheck("-timedemo"))
    {
        diff = Sys_GetSeconds() - ::netConnectTime;
        if(!diff) diff = 1;

        // Print summary and exit.
        LOG_MSG("Timedemo results: %i game tics in %.1f seconds", ::r_framecounter, diff);
        LOG_MSG("%f FPS", ::r_framecounter / diff);
        Sys_Quit();
    }*/

    // "Play demo once" mode?
    if(CommandLine_Check("-playdemo"))
        Sys_Quit();
#endif
}

dd_bool Demo_ReadPacket()
{
    return false;
#if 0
    static byte ptime;
    dint nowtime = DEMOTIC;

    if(!playback)
        return false;

    if(lzEOF(::playdemo))
    {
        Demo_StopPlayback();
        // Any interested parties?
        DoomsdayApp::plugins().callAllHooks(HOOK_DEMO_STOP);
        return false;
    }

    if(::readInfo.first)
    {
        ::readInfo.first = false;
        ::readInfo.begintime = nowtime;
        ptime = lzGetC(::playdemo);
    }

    // Check if the packet can be read.
    if(Net_TimeDelta(nowtime - ::readInfo.begintime, ptime) < 0)
        return false;  // Can't read yet.

    // Read the packet.
    demopacket_header_t hdr;
    lzRead(&hdr, sizeof(hdr), ::playdemo);

    // Get the packet.
    ::netBuffer.length = hdr.length - 1;
    ::netBuffer.player = 0; // From the server.
    ::netBuffer.msg.type = lzGetC(::playdemo);
    lzRead(::netBuffer.msg.data, (long) ::netBuffer.length, ::playdemo);
    //::netBuffer.cursor = ::netBuffer.msg.data;

    // Read the next packet time.
    ptime = lzGetC(::playdemo);

    return true;
#endif
}

/**
 * Writes a view angle and coords packet. Doesn't send the packet outside.
 */
void Demo_WriteLocalCamera(dint plrNum)
{
    DE_ASSERT(plrNum >= 0 && plrNum <= DDMAXPLAYERS);
    player_t *plr    = DD_Player(plrNum);
    ddplayer_t *ddpl = &plr->publicData();
    mobj_t *mob      = ddpl->mo;

    if(!mob) return;

    Msg_Begin(plr->recordPaused ? PKT_DEMOCAM_RESUME : PKT_DEMOCAM);

    // Flags.
    byte flags = (mob->origin[VZ] <= mob->floorZ ? LCAMF_ONGROUND : 0)  // On ground?
             /*| (::writeInfo[plrNum].fov != ::fieldOfView ? LCAMF_FOV : 0)*/;
    if(ddpl->flags & DDPF_CAMERA)
    {
        flags &= ~LCAMF_ONGROUND;
        flags |= LCAMF_CAMERA;
    }
    Writer_WriteByte(::msgWriter, flags);

    // Coordinates.
    fixed_t x = FLT2FIX(mob->origin[VX]);
    fixed_t y = FLT2FIX(mob->origin[VY]);
    Writer_WriteInt16(::msgWriter, x >> 16);
    Writer_WriteByte(::msgWriter, x >> 8);
    Writer_WriteInt16(::msgWriter, y >> 16);
    Writer_WriteByte(::msgWriter, y >> 8);

    fixed_t z = FLT2FIX(mob->origin[VZ] + plr->viewport().current.origin.z);
    Writer_WriteInt16(::msgWriter, z >> 16);
    Writer_WriteByte(::msgWriter, z >> 8);

    Writer_WriteInt16(msgWriter, mob->angle /*ddpl->clAngle*/ >> 16); /* $unifiedangles */
    Writer_WriteInt16(msgWriter, ddpl->lookDir / 110 * DDMAXSHORT /* $unifiedangles */);
    // Field of view is optional.
    /*if(incfov)
    {
        Writer_WriteInt16(::msgWriter, ::fieldOfView / 180 * DDMAXSHORT);
        ::writeInfo[plrNum].fov = ::fieldOfView;
    }*/
    Msg_End();
    Net_SendBuffer(plrNum, SPF_DONT_SEND);
}

/**
 * Read a view angle and coords packet. NOTE: The Z coordinate of the camera is the
 * real eye Z coordinate, not the player mobj's Z coord.
 */
void Demo_ReadLocalCamera()
{
    DE_ASSERT(::consolePlayer >= 0 && consolePlayer < DDMAXPLAYERS);
    ddplayer_t *pl = &DD_Player(::consolePlayer)->publicData();
    mobj_t *mob    = pl->mo;

    if(!mob) return;

    dint intertics = LOCALCAM_WRITE_TICS;
    if(::netBuffer.msg.type == PKT_DEMOCAM_RESUME)
        intertics = 1;

    // Framez keeps track of the current camera Z.
    ::demoFrameZ += demoZ;

    dint flags = Reader_ReadByte(::msgReader);
    ::demoOnGround = (flags & LCAMF_ONGROUND) != 0;
    if(flags & LCAMF_CAMERA)
        pl->flags |= DDPF_CAMERA;
    else
        pl->flags &= ~DDPF_CAMERA;

    // X and Y coordinates are easy. Calculate deltas to the new coords.
    ::posDelta[VX] =
        (FIX2FLT((Reader_ReadInt16(::msgReader) << 16) + (Reader_ReadByte(::msgReader) << 8)) - mob->origin[VX]) / intertics;
    ::posDelta[VY] =
        (FIX2FLT((Reader_ReadInt16(::msgReader) << 16) + (Reader_ReadByte(::msgReader) << 8)) - mob->origin[VY]) / intertics;

    // The Z coordinate is a bit trickier. We are tracking the *camera's*
    // Z coordinate (z+viewheight), not the player mobj's Z.
    dfloat z = FIX2FLT((Reader_ReadInt16(::msgReader) << 16) + (Reader_ReadByte(::msgReader) << 8));
    ::posDelta[VZ] = (z - ::demoFrameZ) / LOCALCAM_WRITE_TICS;

    // View angles.
    dint dang    = Reader_ReadInt16(::msgReader) << 16;
    dfloat dlook = Reader_ReadInt16(::msgReader) * 110.0f / DDMAXSHORT;

    // FOV included?
    /*
    if(flags & LCAMF_FOV)
        ::fieldOfView = Reader_ReadInt16(::msgReader) * 180.0f / DDMAXSHORT;
    */

    if(intertics == 1 || demoFrameZ == 1)
    {
        // Immediate change.
        /*pl->clAngle = dang;
        pl->clLookDir = dlook;*/
        pl->mo->angle = dang;
        pl->lookDir = dlook;
        /* $unifiedangles */
        ::viewangleDelta = 0;
        ::lookdirDelta   = 0;
    }
    else
    {
        ::viewangleDelta = (dang  - pl->mo->angle) / intertics;
        ::lookdirDelta   = (dlook - pl->lookDir)   / intertics;
        /* $unifiedangles */
    }

    // The first one gets no delta.
    if(::demoFrameZ == 1)
    {
        // This must be the first democam packet.
        // Initialize framez to the height we just read.
        ::demoFrameZ = z;
        ::posDelta[VZ] = 0;
    }
    // demo_z is the offset to demo_framez for the current tic.
    // It is incremented by pos_delta[VZ] every tic.
    ::demoZ = 0;

    if(intertics == 1)
    {
        // Instantaneous move.
        R_ResetViewer();
        ::demoFrameZ = z;
        ClPlayer_MoveLocal(::posDelta[VX], ::posDelta[VY], z, ::demoOnGround);
        de::zap(::posDelta);
    }
}

/**
 * Called once per tic.
 */
void Demo_Ticker(timespan_t /*time*/)
{
    if(!DD_IsSharpTick()) return;

    // Only playback is handled.
    if(::playback)
    {
        DE_ASSERT(::consolePlayer >= 0 && ::consolePlayer < DDMAXPLAYERS);
        player_t   *plr  = DD_Player(::consolePlayer);
        ddplayer_t *ddpl = &plr->publicData();

        ddpl->mo->angle += ::viewangleDelta;
        ddpl->lookDir += ::lookdirDelta;
        /* $unifiedangles */
        // Move player (i.e. camera).
        ClPlayer_MoveLocal(::posDelta[VX], ::posDelta[VY], ::demoFrameZ + ::demoZ, ::demoOnGround);
        // Interpolate camera Z offset (to framez).
        ::demoZ += ::posDelta[VZ];
    }
    else
    {
        for(dint i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t   &plr  = *DD_Player(i);
            ddplayer_t &ddpl = plr.publicData();

            if(ddpl.inGame && plr.recording && !plr.recordPaused &&
               ++plr.demoTimer().cameratimer >= LOCALCAM_WRITE_TICS)
            {
                // It's time to write local view angles and coords.
                plr.demoTimer().cameratimer = 0;
                Demo_WriteLocalCamera(i);
            }
        }
    }
}

D_CMD(PlayDemo)
{
    DE_UNUSED(src, argc);

    LOG_MSG("Playing demo \"%s\"...") << argv[1];
    return Demo_BeginPlayback(argv[1]);
}

D_CMD(RecordDemo)
{
    DE_UNUSED(src);

    if(argc == 3 && netState.isClient)
    {
        LOG_ERROR("Clients can only record the consolePlayer");
        return true;
    }

    if(netState.isClient && argc != 2)
    {
        LOG_SCR_NOTE("Usage: %s (fileName)") << argv[0];
        return true;
    }

    if(netState.isServer && (argc < 2 || argc > 3))
    {
        LOG_SCR_NOTE("Usage: %s (fileName) (plnum)") << argv[0];
        LOG_SCR_MSG("(plnum) is the player which will be recorded.");
        return true;
    }

    dint plnum = ::consolePlayer;
    if(argc == 3)
    {
        plnum = String(argv[2]).toInt();
    }

    LOG_MSG("Recording demo of player %i to \"%s\"") << plnum << argv[1];
    return Demo_BeginRecording(argv[1], plnum);
}

D_CMD(PauseDemo)
{
    DE_UNUSED(src);

    dint plnum = ::consolePlayer;
    if(argc >= 2)
    {
        plnum = String(argv[1]).toInt();
    }

    if(plnum < 0 || plnum >= DDMAXPLAYERS)
    {
        LOG_SCR_ERROR("Invalid player #%i") << plnum;
        return false;
    }
    if(!DD_Player(plnum)->recording)
    {
        LOG_SCR_ERROR("Not recording for player %i") << plnum;
        return false;
    }
    if(DD_Player(plnum)->recordPaused)
    {
        Demo_ResumeRecording(plnum);
        LOG_SCR_MSG("Demo recording of player %i resumed") << plnum;
    }
    else
    {
        Demo_PauseRecording(plnum);
        LOG_SCR_MSG("Demo recording of player %i paused") << plnum;
    }
    return true;
}

D_CMD(StopDemo)
{
    DE_UNUSED(src);

    if(argc > 2)
    {
        LOG_SCR_NOTE("Usage: stopdemo (plrnum)");
        return true;
    }

    dint plnum = ::consolePlayer;
    if(argc == 2)
    {
        plnum = String(argv[1]).toInt();
    }

    if(plnum < 0 || plnum >= DDMAXPLAYERS)
    {
        LOG_SCR_ERROR("Invalid player #%i") << plnum;
        return false;
    }

    if(!::playback && !DD_Player(plnum)->recording)
        return true;

    LOG_SCR_MSG("Demo %s of player %i stopped")
        << (DD_Player(plnum)->recording ? "recording" : "playback") << plnum;

    if(::playback)
    {
        // Aborted.
        Demo_StopPlayback();
        // Any interested parties?
        DoomsdayApp::plugins().callAllHooks(HOOK_DEMO_STOP, true);
    }
    else
    {
        Demo_StopRecording(plnum);
    }

    return true;
}

#if 0
/**
 * Make a demo lump.
 *
 * @todo Why does this exist? -ds
 */
D_CMD(DemoLump)
{
    DE_UNUSED(src, argc);

    char buf[64]; de::zap(buf);
    strncpy(buf, argv[1], 64);
    return M_WriteFile(argv[2], buf, 64);
}
#endif

void Demo_Register()
{
    //C_CMD_FLAGS("demolump",   "ss",       DemoLump,   CMDF_NO_NULLGAME);
    C_CMD_FLAGS("pausedemo",    nullptr,    PauseDemo,  CMDF_NO_NULLGAME);
    C_CMD_FLAGS("playdemo",     "s",        PlayDemo,   CMDF_NO_NULLGAME);
    C_CMD_FLAGS("recorddemo",   nullptr,    RecordDemo, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("stopdemo",     nullptr,    StopDemo,   CMDF_NO_NULLGAME);
}
