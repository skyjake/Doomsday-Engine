/** @file net_demo.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Handling of demo recording and playback.
 * Opening of, writing to, reading from and closing of demo files.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_filesys.h"
#include "de_network.h"
#include "de_misc.h"

#include "render/r_main.h"
#include "render/rend_main.h"
#include "map/p_players.h"

// MACROS ------------------------------------------------------------------

#define DEMOTIC SECONDS_TO_TICKS(demoTime)

// Local Camera flags.
#define LCAMF_ONGROUND      0x1
#define LCAMF_FOV           0x2 // FOV has changed (short).
#define LCAMF_CAMERA        0x4 // Camera mode.

// TYPES -------------------------------------------------------------------

#pragma pack(1)
typedef struct {
    ushort          length;
} demopacket_header_t;
#pragma pack()

typedef struct {
    boolean         first;
    int             begintime;
    boolean         canwrite; /// @c false until Handshake packet.
    int             cameratimer;
    int             pausetime;
    float           fov;
} demotimer_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(DemoLump);
D_CMD(PauseDemo);
D_CMD(PlayDemo);
D_CMD(RecordDemo);
D_CMD(StopDemo);

void Demo_WriteLocalCamera(int plnum);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float netConnectTime;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

filename_t demoPath = "demo/";

LZFILE* playdemo = 0;
int playback = false;
int viewangleDelta = 0;
float lookdirDelta = 0;
float posDelta[3];
float demoFrameZ, demoZ;
boolean demoOnGround;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static demotimer_t writeInfo[DDMAXPLAYERS];
static demotimer_t readInfo;
static float startFOV;
static int demoStartTic;

// CODE --------------------------------------------------------------------

void Demo_Register(void)
{
    C_CMD_FLAGS("demolump", "ss", DemoLump, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("pausedemo", NULL, PauseDemo, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("playdemo", "s", PlayDemo, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("recorddemo", NULL, RecordDemo, CMDF_NO_NULLGAME);
    C_CMD_FLAGS("stopdemo", NULL, StopDemo, CMDF_NO_NULLGAME);
}

void Demo_Init(void)
{
    // Make sure the demo path is there.
    F_MakePath(demoPath);
}

/**
 * Open a demo file and begin recording.
 * Returns false if the recording can't be begun.
 */
boolean Demo_BeginRecording(const char* fileName, int plrNum)
{
    DENG_UNUSED(fileName);
    DENG_UNUSED(plrNum);
    return false;

#if 0
    client_t* cl = &clients[plrNum];
    player_t* plr = &ddPlayers[plrNum];
    ddstring_t buf;

    // Is a demo already being recorded for this client?
    if(cl->recording || playback || (isDedicated && !plrNum) || !plr->shared.inGame)
        return false;

    // Compose the real file name.
    Str_InitStd(&buf);
    Str_Appendf(&buf, "%s%s", demoPath, fileName);
    F_ExpandBasePath(&buf, &buf);
    F_ToNativeSlashes(&buf, &buf);

    // Open the demo file.
    cl->demo = lzOpen(Str_Text(&buf), "wp");
    Str_Free(&buf);
    if(!cl->demo)
    {
        return false; // Couldn't open it!
    }

    cl->recording = true;
    cl->recordPaused = false;
    writeInfo[plrNum].first = true;
    writeInfo[plrNum].canwrite = false;
    writeInfo[plrNum].cameratimer = 0;
    writeInfo[plrNum].fov = -1; // Must be written in the first packet.

    if(isServer)
    {
        // Playing demos alters gametic. This'll make sure we're going to
        // get updates.
        clients[0].lastTransmit = -1;
        // Servers need to send a handshake packet.
        // It only needs to recorded in the demo file, though.
        allowSending = false;
        Sv_Handshake(plrNum, false);
        // Enable sending to network.
        allowSending = true;
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

void Demo_PauseRecording(int playerNum)
{
    client_t *cl = clients + playerNum;

    // A demo is not being recorded?
    if(!cl->recording || cl->recordPaused)
        return;
    // All packets will be written for the same tic.
    writeInfo[playerNum].pausetime = SECONDS_TO_TICKS(demoTime);
    cl->recordPaused = true;
}

/**
 * Resumes a paused recording.
 */
void Demo_ResumeRecording(int playerNum)
{
    client_t       *cl = clients + playerNum;

    // Not recording or not paused?
    if(!cl->recording || !cl->recordPaused)
        return;
    Demo_WriteLocalCamera(playerNum);
    cl->recordPaused = false;
    // When the demo is read there can't be a jump in the timings, so we
    // have to make it appear the pause never happened; begintime is
    // moved forwards.
    writeInfo[playerNum].begintime += DEMOTIC - writeInfo[playerNum].pausetime;
}

/**
 * Stop recording a demo.
 */
void Demo_StopRecording(int playerNum)
{
    client_t       *cl = clients + playerNum;

    // A demo is not being recorded?
    if(!cl->recording)
        return;

    // Close demo file.
    lzClose(cl->demo);
    cl->demo = 0;
    cl->recording = false;
}

void Demo_WritePacket(int playerNum)
{
    LZFILE         *file;
    demopacket_header_t hdr;
    demotimer_t    *inf = writeInfo + playerNum;
    byte            ptime;

    if(playerNum < 0)
    {
        Demo_BroadcastPacket();
        return;
    }

    // Is this client recording?
    if(!clients[playerNum].recording)
        return;

    if(!inf->canwrite)
    {
        if(netBuffer.msg.type != PSV_HANDSHAKE)
            return;
        // The handshake has arrived. Now we can begin writing.
        inf->canwrite = true;
    }

    if(clients[playerNum].recordPaused)
    {
        // Some types of packet are not written in record-paused mode.
        if(netBuffer.msg.type == PSV_SOUND ||
           netBuffer.msg.type == DDPT_MESSAGE)
            return;
    }

    // This counts as an update. (We know the client is alive.)
    //clients[playerNum].updateCount = UPDATECOUNT;

    file = clients[playerNum].demo;

#if _DEBUG
    if(!file)
        Con_Error("Demo_WritePacket: No demo file!\n");
#endif

    if(!inf->first)
    {
        ptime =
            (clients[playerNum].recordPaused ? inf->pausetime : DEMOTIC) -
            inf->begintime;
    }
    else
    {
        ptime = 0;
        inf->first = false;
        inf->begintime = DEMOTIC;
    }
    lzWrite(&ptime, 1, file);

    // The header.

#if _DEBUG
    if(netBuffer.length >= sizeof(hdr.length))
        Con_Error("Demo_WritePacket: Write buffer too large!\n");
#endif

    hdr.length = (ushort) 1 + netBuffer.length;
    lzWrite(&hdr, sizeof(hdr), file);

    // Write the packet itself.
    lzPutC(netBuffer.msg.type, file);
    lzWrite(netBuffer.msg.data, (long) netBuffer.length, file);
}

void Demo_BroadcastPacket(void)
{
    int                 i;

    // Write packet to all recording demo files.
    for(i = 0; i < DDMAXPLAYERS; ++i)
        Demo_WritePacket(i);
}

boolean Demo_BeginPlayback(const char* fileName)
{
    ddstring_t buf;

    if(playback)
        return false; // Already in playback.
    if(netGame || isClient)
        return false; // Can't do it.

    // Check that we aren't recording anything.
    { int i;
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(clients[i].recording)
            return false;
    }}

    // Compose the real file name.
    Str_InitStd(&buf);
    Str_Set(&buf, fileName);
    if(!F_IsAbsolute(&buf))
    {
        Str_Prepend(&buf, demoPath);
    }
    F_ExpandBasePath(&buf, &buf);
    F_ToNativeSlashes(&buf, &buf);

    // Open the demo file.
    playdemo = lzOpen(Str_Text(&buf), "rp");
    Str_Free(&buf);
    if(!playdemo)
        return false; // Failed to open the file.

    // OK, let's begin the demo.
    playback = true;
    isServer = false;
    isClient = true;
    readInfo.first = true;
    viewangleDelta = 0;
    lookdirDelta = 0;
    demoFrameZ = 1;
    demoZ = 0;
    startFOV = fieldOfView;
    demoStartTic = DEMOTIC;
    memset(posDelta, 0, sizeof(posDelta));
    // Start counting frames from here.
    /*
    if(ArgCheck("-timedemo"))
        r_framecounter = 0;
        */

    return true;
}

void Demo_StopPlayback(void)
{
    //float           diff;

    if(!playback)
        return;

    Con_Message("Demo was %.2f seconds (%i tics) long.",
                (DEMOTIC - demoStartTic) / (float) TICSPERSEC,
                DEMOTIC - demoStartTic);

    playback = false;
    lzClose(playdemo);
    playdemo = 0;
    fieldOfView = startFOV;
    Net_StopGame();

    /*
    if(ArgCheck("-timedemo"))
    {
        diff = Sys_GetSeconds() - netConnectTime;
        if(!diff)
            diff = 1;
        // Print summary and exit.
        Con_Message("Timedemo results: %i game tics in %.1f seconds", r_framecounter, diff);
        Con_Message("%f FPS", r_framecounter / diff);
        Sys_Quit();
    }
    */

    // "Play demo once" mode?
    if(CommandLine_Check("-playdemo"))
        Sys_Quit();
}

boolean Demo_ReadPacket(void)
{
    static byte     ptime;
    int             nowtime = DEMOTIC;
    demopacket_header_t hdr;

    if(!playback)
        return false;

    if(lzEOF(playdemo))
    {
        Demo_StopPlayback();
        // Any interested parties?
        DD_CallHooks(HOOK_DEMO_STOP, false, 0);
        return false;
    }

    if(readInfo.first)
    {
        readInfo.first = false;
        readInfo.begintime = nowtime;
        ptime = lzGetC(playdemo);
    }

    // Check if the packet can be read.
    if(Net_TimeDelta(nowtime - readInfo.begintime, ptime) < 0)
        return false; // Can't read yet.

    // Read the packet.
    lzRead(&hdr, sizeof(hdr), playdemo);

    // Get the packet.
    netBuffer.length = hdr.length - 1;
    netBuffer.player = 0; // From the server.
    netBuffer.msg.type = lzGetC(playdemo);
    lzRead(netBuffer.msg.data, (long) netBuffer.length, playdemo);
    //netBuffer.cursor = netBuffer.msg.data;

/*#if _DEBUG
Con_Printf("RDP: pt=%i ang=%i ld=%i len=%i type=%i\n", ptime,
           hdr.angle, hdr.lookdir, hdr.length, netBuffer.msg.type);
#endif*/

    // Read the next packet time.
    ptime = lzGetC(playdemo);

    return true;
}

/**
 * Writes a view angle and coords packet. Doesn't send the packet outside.
 */
void Demo_WriteLocalCamera(int plrNum)
{
    player_t* plr = &ddPlayers[plrNum];
    ddplayer_t* ddpl = &plr->shared;
    mobj_t* mo = ddpl->mo;
    fixed_t x, y, z;
    byte flags;
    boolean incfov = (writeInfo[plrNum].fov != fieldOfView);
    const viewdata_t* viewData = R_ViewData(plrNum);

    if(!mo)
        return;

    Msg_Begin(clients[plrNum].recordPaused ? PKT_DEMOCAM_RESUME : PKT_DEMOCAM);
    // Flags.
    flags = (mo->origin[VZ] <= mo->floorZ ? LCAMF_ONGROUND : 0)  // On ground?
        | (incfov ? LCAMF_FOV : 0);
    if(ddpl->flags & DDPF_CAMERA)
    {
        flags &= ~LCAMF_ONGROUND;
        flags |= LCAMF_CAMERA;
    }
    Writer_WriteByte(msgWriter, flags);

    // Coordinates.
    x = FLT2FIX(mo->origin[VX]);
    y = FLT2FIX(mo->origin[VY]);
    Writer_WriteInt16(msgWriter, x >> 16);
    Writer_WriteByte(msgWriter, x >> 8);
    Writer_WriteInt16(msgWriter, y >> 16);
    Writer_WriteByte(msgWriter, y >> 8);

    z = FLT2FIX(mo->origin[VZ] + viewData->current.origin[VZ]);
    Writer_WriteInt16(msgWriter, z >> 16);
    Writer_WriteByte(msgWriter, z >> 8);

    Writer_WriteInt16(msgWriter, mo->angle /*ddpl->clAngle*/ >> 16); /* $unifiedangles */
    Writer_WriteInt16(msgWriter, ddpl->lookDir / 110 * DDMAXSHORT /* $unifiedangles */);
    // Field of view is optional.
    if(incfov)
    {
        Writer_WriteInt16(msgWriter, fieldOfView / 180 * DDMAXSHORT);
        writeInfo[plrNum].fov = fieldOfView;
    }
    Msg_End();
    Net_SendBuffer(plrNum, SPF_DONT_SEND);
}

/**
 * Read a view angle and coords packet. NOTE: The Z coordinate of the
 * camera is the real eye Z coordinate, not the player mobj's Z coord.
 */
void Demo_ReadLocalCamera(void)
{
    ddplayer_t         *pl = &ddPlayers[consolePlayer].shared;
    mobj_t             *mo = pl->mo;
    int                 flags;
    float               z;
    int                 intertics = LOCALCAM_WRITE_TICS;
    int                 dang;
    float               dlook;

    if(!mo)
        return;

    if(netBuffer.msg.type == PKT_DEMOCAM_RESUME)
    {
        intertics = 1;
    }

    // Framez keeps track of the current camera Z.
    demoFrameZ += demoZ;

    flags = Reader_ReadByte(msgReader);
    demoOnGround = (flags & LCAMF_ONGROUND) != 0;
    if(flags & LCAMF_CAMERA)
        pl->flags |= DDPF_CAMERA;
    else
        pl->flags &= ~DDPF_CAMERA;

    // X and Y coordinates are easy. Calculate deltas to the new coords.
    posDelta[VX] =
        (FIX2FLT((Reader_ReadInt16(msgReader) << 16) + (Reader_ReadByte(msgReader) << 8)) - mo->origin[VX]) / intertics;
    posDelta[VY] =
        (FIX2FLT((Reader_ReadInt16(msgReader) << 16) + (Reader_ReadByte(msgReader) << 8)) - mo->origin[VY]) / intertics;

    // The Z coordinate is a bit trickier. We are tracking the *camera's*
    // Z coordinate (z+viewheight), not the player mobj's Z.
    z = FIX2FLT((Reader_ReadInt16(msgReader) << 16) + (Reader_ReadByte(msgReader) << 8));
    posDelta[VZ] = (z - demoFrameZ) / LOCALCAM_WRITE_TICS;

    // View angles.
    dang = Reader_ReadInt16(msgReader) << 16;
    dlook = Reader_ReadInt16(msgReader) * 110.0f / DDMAXSHORT;

    // FOV included?
    if(flags & LCAMF_FOV)
        fieldOfView = Reader_ReadInt16(msgReader) * 180.0f / DDMAXSHORT;

    if(intertics == 1 || demoFrameZ == 1)
    {
        // Immediate change.
        /*pl->clAngle = dang;
        pl->clLookDir = dlook;*/
        pl->mo->angle = dang;
        pl->lookDir = dlook;
        /* $unifiedangles */
        viewangleDelta = 0;
        lookdirDelta = 0;
    }
    else
    {
        viewangleDelta = (dang - pl->mo->angle) / intertics;
        lookdirDelta = (dlook - pl->lookDir) / intertics;
        /* $unifiedangles */
    }

    // The first one gets no delta.
    if(demoFrameZ == 1)
    {
        // This must be the first democam packet.
        // Initialize framez to the height we just read.
        demoFrameZ = z;
        posDelta[VZ] = 0;
    }
    // demo_z is the offset to demo_framez for the current tic.
    // It is incremented by pos_delta[VZ] every tic.
    demoZ = 0;

    if(intertics == 1)
    {
        // Instantaneous move.
        R_ResetViewer();
        demoFrameZ = z;
        ClPlayer_MoveLocal(posDelta[VX], posDelta[VY], z, demoOnGround);
        posDelta[VX] = posDelta[VY] = posDelta[VZ] = 0;
    }
}

/**
 * Called once per tic.
 */
void Demo_Ticker(timespan_t /*time*/)
{
    if(!DD_IsSharpTick())
        return;

    // Only playback is handled.
    if(playback)
    {
        player_t               *plr = &ddPlayers[consolePlayer];
        ddplayer_t             *ddpl = &plr->shared;

        ddpl->mo->angle += viewangleDelta;
        ddpl->lookDir += lookdirDelta;
        /* $unifiedangles */
        // Move player (i.e. camera).
        ClPlayer_MoveLocal(posDelta[VX], posDelta[VY], demoFrameZ + demoZ, demoOnGround);
        // Interpolate camera Z offset (to framez).
        demoZ += posDelta[VZ];
    }
    else
    {
        int                     i;

        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            player_t               *plr = &ddPlayers[i];
            ddplayer_t             *ddpl = &plr->shared;
            client_t               *cl = &clients[i];

            if(ddpl->inGame && cl->recording && !cl->recordPaused &&
               ++writeInfo[i].cameratimer >= LOCALCAM_WRITE_TICS)
            {
                // It's time to write local view angles and coords.
                writeInfo[i].cameratimer = 0;
                Demo_WriteLocalCamera(i);
            }
        }
    }
}

D_CMD(PlayDemo)
{
    DENG2_UNUSED2(src, argc);

    Con_Printf("Playing demo \"%s\"...\n", argv[1]);
    return Demo_BeginPlayback(argv[1]);
}

D_CMD(RecordDemo)
{
    DENG2_UNUSED(src);

    int                 plnum = consolePlayer;

    if(argc == 3 && isClient)
    {
        Con_Printf("Clients can only record the consolePlayer.\n");
        return true;
    }

    if(isClient && argc != 2)
    {
        Con_Printf("Usage: %s (fileName)\n", argv[0]);
        return true;
    }

    if(isServer && (argc < 2 || argc > 3))
    {
        Con_Printf("Usage: %s (fileName) (plnum)\n", argv[0]);
        Con_Printf("(plnum) is the player which will be recorded.\n");
        return true;
    }

    if(argc == 3)
        plnum = atoi(argv[2]);

    Con_Printf("Recording demo of player %i to \"%s\".\n", plnum, argv[1]);

    return Demo_BeginRecording(argv[1], plnum);
}

D_CMD(PauseDemo)
{
    DENG2_UNUSED(src);

    int         plnum = consolePlayer;

    if(argc >= 2)
        plnum = atoi(argv[1]);

    if(!clients[plnum].recording)
    {
        Con_Printf("Not recording for player %i.\n", plnum);
        return false;
    }
    if(clients[plnum].recordPaused)
    {
        Demo_ResumeRecording(plnum);
        Con_Printf("Demo recording of player %i resumed.\n", plnum);
    }
    else
    {
        Demo_PauseRecording(plnum);
        Con_Printf("Demo recording of player %i paused.\n", plnum);
    }
    return true;
}

D_CMD(StopDemo)
{
    DENG2_UNUSED(src);

    int plnum = consolePlayer;

    if(argc > 2)
    {
        Con_Printf("Usage: stopdemo (plrnum)\n");
        return true;
    }
    if(argc == 2)
        plnum = atoi(argv[1]);

    if(!playback && !clients[plnum].recording)
        return true;

    Con_Printf("Demo %s of player %i stopped.\n",
               clients[plnum].recording ? "recording" : "playback", plnum);

    if(playback)
    {   // Aborted.
        Demo_StopPlayback();
        // Any interested parties?
        DD_CallHooks(HOOK_DEMO_STOP, true, 0);
    }
    else
        Demo_StopRecording(plnum);
    return true;
}

/**
 * Make a demo lump.
 */
D_CMD(DemoLump)
{
    DENG2_UNUSED2(src, argc);

    char buf[64];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, argv[1], 64);
    return M_WriteFile(argv[2], buf, 64);
}
