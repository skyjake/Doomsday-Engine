/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * net_demo.c: Demos
 *
 * Handling of demo recording and playback.
 * Opening of, writing to, reading from and closing of demo files.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_misc.h"

#include "r_main.h"
#include "gl_main.h"            // For r_framecounter.

// MACROS ------------------------------------------------------------------

#define DEMOTIC SECONDS_TO_TICKS(demoTime)

// Local Camera flags.
#define LCAMF_ONGROUND  0x1
#define LCAMF_FOV       0x2     // FOV has changed (short).
#define LCAMF_CAMERA    0x4     // Camera mode.

// TYPES -------------------------------------------------------------------

#pragma pack(1)

typedef struct {
    //unsigned short    angle;
    //short         lookdir;
    unsigned short length;
} demopacket_header_t;

#pragma pack()

typedef struct {
    boolean first;
    int     begintime;
    boolean canwrite;           // False until Handshake packet.
    int     cameratimer;
    int     pausetime;
    float   fov;
} demotimer_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    Demo_WriteLocalCamera(int plnum);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float net_connecttime;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char    demoPath[128] = "demo\\";

//int demotic = 0;
LZFILE *playdemo = 0;
int     playback = false;
int     viewangle_delta = 0;
float   lookdir_delta = 0;
int     pos_delta[3];
int     demo_framez, demo_z;
boolean demo_onground;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static demotimer_t writeInfo[MAXPLAYERS];
static demotimer_t readinfo;
static float start_fov;
static int demostarttic;

// CODE --------------------------------------------------------------------

void Demo_Init(void)
{
    // Make sure the demo path is there.
    M_CheckPath(demoPath);
}

/*
 * Open a demo file and begin recording.
 * Returns false if the recording can't be begun.
 */
boolean Demo_BeginRecording(char *fileName, int playerNum)
{
    char    buf[200];
    client_t *cl = clients + playerNum;

    // Is a demo already being recorded for this client?
    if(cl->recording || playback || (isDedicated && !playerNum) ||
       !players[playerNum].ingame)
        return false;

    // Compose the real file name.
    strcpy(buf, demoPath);
    strcat(buf, fileName);
    M_TranslatePath(buf, buf);

    // Open the demo file.
    cl->demo = lzOpen(buf, "wp");
    if(!cl->demo)
        return false;           // Couldn't open it!
    cl->recording = true;
    cl->recordPaused = false;
    writeInfo[playerNum].first = true;
    writeInfo[playerNum].canwrite = false;
    writeInfo[playerNum].cameratimer = 0;
    writeInfo[playerNum].fov = -1;  // Must be written in the first packet.

    if(isServer)
    {
        // Playing demos alters gametic. This'll make sure we're going to
        // get updates.
        clients[0].lastTransmit = -1;
        // Servers need to send a handshake packet.
        // It only needs to recorded in the demo file, though.
        allowSending = false;
        Sv_Handshake(playerNum, false);
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

/*
 * Resumes a paused recording.
 */
void Demo_ResumeRecording(int playerNum)
{
    client_t *cl = clients + playerNum;

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

/*
 * Stop recording a demo.
 */
void Demo_StopRecording(int playerNum)
{
    client_t *cl = clients + playerNum;

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
    LZFILE *file;
    demopacket_header_t hdr;
    demotimer_t *inf = writeInfo + playerNum;
    byte    ptime;

    if(playerNum == NSP_BROADCAST)
    {
        Demo_BroadcastPacket();
        return;
    }

    // Is this client recording?
    if(!clients[playerNum].recording)
        return;
    if(!inf->canwrite)
    {
        if(netBuffer.msg.type != psv_handshake)
            return;
        // The handshake has arrived. Now we can begin writing.
        inf->canwrite = true;
    }
    if(clients[playerNum].recordPaused)
    {
        // Some types of packet are not written in record-paused mode.
        if(netBuffer.msg.type == psv_sound ||
           netBuffer.msg.type == DDPT_MESSAGE)
            return;
    }

    // This counts as an update. (We know the client is alive.)
    clients[playerNum].updateCount = UPDATECOUNT;

    file = clients[playerNum].demo;

#ifdef _DEBUG
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
    hdr.length = 1 + netBuffer.length;
    lzWrite(&hdr, sizeof(hdr), file);

    // Write the packet itself.
    lzPutC(netBuffer.msg.type, file);
    lzWrite(netBuffer.msg.data, netBuffer.length, file);
}

void Demo_BroadcastPacket(void)
{
    int     i;

    // Write packet to all recording demo files.
    for(i = 0; i < MAXPLAYERS; i++)
        Demo_WritePacket(i);
}

boolean Demo_BeginPlayback(char *fileName)
{
    char    buf[256];
    int     i;

    if(playback)
        return false;           // Already in playback.
    if(netgame || isClient)
        return false;           // Can't do it.
    // Check that we aren't recording anything.
    for(i = 0; i < MAXPLAYERS; i++)
        if(clients[i].recording)
            return false;
    // Open the demo file.
    sprintf(buf, "%s%s", Dir_IsAbsolute(fileName) ? "" : demoPath, fileName);
    M_TranslatePath(buf, buf);
    playdemo = lzOpen(buf, "rp");
    if(!playdemo)
        return false;           // Failed to open the file.
    // OK, let's begin the demo.
    playback = true;
    isServer = false;
    isClient = true;
    readinfo.first = true;
    viewangle_delta = 0;
    lookdir_delta = 0;
    demo_framez = 1;
    demo_z = 0;
    start_fov = fieldOfView;
    demostarttic = DEMOTIC;
    memset(pos_delta, 0, sizeof(pos_delta));
    // Start counting frames from here.
    if(ArgCheck("-timedemo"))
        r_framecounter = 0;
    return true;
}

void Demo_StopPlayback(void)
{
    float   diff;

    if(!playback)
        return;

    Con_Message("Demo was %.2f seconds (%i tics) long.\n",
                (DEMOTIC - demostarttic) / (float) TICSPERSEC,
                DEMOTIC - demostarttic);

    playback = false;
    lzClose(playdemo);
    playdemo = 0;
    fieldOfView = start_fov;
    Net_StopGame();

    if(ArgCheck("-timedemo"))
    {
        diff = Sys_GetSeconds() - net_connecttime;
        if(!diff)
            diff = 1;
        // Print summary and exit.
        Con_Message("Timedemo results: ");
        Con_Message("%i game tics in %.1f seconds\n", r_framecounter, diff);
        Con_Message("%f FPS\n", r_framecounter / diff);
        Sys_Quit();
    }

    // "Play demo once" mode?
    if(ArgCheck("-playdemo"))
        Sys_Quit();
}

boolean Demo_ReadPacket(void)
{
    static byte ptime;
    int     nowtime = DEMOTIC;
    demopacket_header_t hdr;

    if(!playback)
        return false;
    if(lzEOF(playdemo))
    {
        Demo_StopPlayback();
        // Tell the Game the demo has ended.
        if(gx.NetWorldEvent)
            gx.NetWorldEvent(DDWE_DEMO_END, 0, 0);
        return false;
    }

    if(readinfo.first)
    {
        readinfo.first = false;
        readinfo.begintime = nowtime;
        ptime = lzGetC(playdemo);
    }

    // Check if the packet can be read.
    if(Net_TimeDelta(nowtime - readinfo.begintime, ptime) < 0)
        return false;           // Can't read yet.

    // Read the packet.
    lzRead(&hdr, sizeof(hdr), playdemo);

    // Get the packet.
    netBuffer.length = hdr.length - 1 /*netBuffer.headerLength */ ;
    netBuffer.player = 0;       // From the server.
    netBuffer.msg.id = 0;
    netBuffer.msg.type = lzGetC(playdemo);
    lzRead(netBuffer.msg.data, netBuffer.length, playdemo);
    netBuffer.cursor = netBuffer.msg.data;

    /*  Con_Printf("RDP: pt=%i ang=%i ld=%i len=%i type=%i\n", ptime,
       hdr.angle, hdr.lookdir, hdr.length, netBuffer.msg.type); */

    // Read the next packet time.
    ptime = lzGetC(playdemo);

    // Calculate view angle deltas.
    /*  viewangle_delta = (hdr.angle << 16) - players[consoleplayer].clAngle;
       lookdir_delta = (hdr.lookdir * 110.0f / DDMAXSHORT)
       - players[consoleplayer].clLookDir; */

    // How many tics to next frame?
    /*  tics = Net_TimeDelta(ptime, nowtime - readinfo.begintime);
       if(tics)
       {
       viewangle_delta /= tics;
       lookdir_delta /= tics;
       } */

    return true;
}

/*
 * Writes a view angle and coords packet. Doesn't send the packet outside.
 */
void Demo_WriteLocalCamera(int plnum)
{
    mobj_t *mo = players[plnum].mo;
    int     z;
    byte    flags;
    boolean incfov = (writeInfo[plnum].fov != fieldOfView);

    if(!mo)
        return;

    Msg_Begin(clients[plnum].recordPaused ? pkt_democam_resume : pkt_democam);
    // Flags.
    flags = (mo->pos[VZ] <= mo->floorz ? LCAMF_ONGROUND : 0)  // On ground?
        | (incfov ? LCAMF_FOV : 0);
    if(players[plnum].flags & DDPF_CAMERA)
    {
        flags &= ~LCAMF_ONGROUND;
        flags |= LCAMF_CAMERA;
    }
    Msg_WriteByte(flags);
    // Coordinates.
    Msg_WriteShort(mo->pos[VX] >> 16);
    Msg_WriteByte(mo->pos[VX] >> 8);
    Msg_WriteShort(mo->pos[VY] >> 16);
    Msg_WriteByte(mo->pos[VY] >> 8);
    //z = mo->pos[VZ] + players[plnum].viewheight;
    z = players[plnum].viewz;
    Msg_WriteShort(z >> 16);
    Msg_WriteByte(z >> 8);
    Msg_WriteShort(players[plnum].clAngle >> 16);
    Msg_WriteShort(players[plnum].clLookDir / 110 * DDMAXSHORT);
    // Field of view is optional.
    if(incfov)
    {
        Msg_WriteShort(fieldOfView / 180 * DDMAXSHORT);
        writeInfo[plnum].fov = fieldOfView;
    }
    Net_SendBuffer(plnum, SPF_DONT_SEND);
}

/*
 * Read a view angle and coords packet. NOTE: The Z coordinate of the
 * camera is the real eye Z coordinate, not the player mobj's Z coord.
 */
void Demo_ReadLocalCamera(void)
{
    ddplayer_t *pl = players + consoleplayer;
    mobj_t *mo = players[consoleplayer].mo;
    int     flags;
    int     z;
    int     intertics = LOCALCAM_WRITE_TICS;
    int     dang;
    float   dlook;

    if(!mo)
        return;

    if(netBuffer.msg.type == pkt_democam_resume)
    {
        intertics = 1;
    }

    // Framez keeps track of the current camera Z.
    demo_framez += demo_z;

    flags = Msg_ReadByte();
    demo_onground = (flags & LCAMF_ONGROUND) != 0;
    if(flags & LCAMF_CAMERA)
        pl->flags |= DDPF_CAMERA;
    else
        pl->flags &= ~DDPF_CAMERA;

    // X and Y coordinates are easy. Calculate deltas to the new coords.
    pos_delta[VX] =
        ((Msg_ReadShort() << 16) + (Msg_ReadByte() << 8) - mo->pos[VX]) / intertics;
    pos_delta[VY] =
        ((Msg_ReadShort() << 16) + (Msg_ReadByte() << 8) - mo->pos[VY]) / intertics;

    // The Z coordinate is a bit trickier. We are tracking the *camera's*
    // Z coordinate (z+viewheight), not the player mobj's Z.
    z = (Msg_ReadShort() << 16) + (Msg_ReadByte() << 8);
    pos_delta[VZ] = (z - demo_framez) / LOCALCAM_WRITE_TICS;

    // View angles.
    dang = Msg_ReadShort() << 16;
    dlook = Msg_ReadShort() * 110.0f / DDMAXSHORT;

    // FOV included?
    if(flags & LCAMF_FOV)
        fieldOfView = Msg_ReadShort() * 180.0f / DDMAXSHORT;

    if(intertics == 1 || demo_framez == 1)
    {
        // Immediate change.
        pl->clAngle = dang;
        pl->clLookDir = dlook;
        viewangle_delta = 0;
        lookdir_delta = 0;
    }
    else
    {
        viewangle_delta = (dang - pl->clAngle) / intertics;
        lookdir_delta = (dlook - pl->clLookDir) / intertics;
    }

    // The first one gets no delta.
    if(demo_framez == 1)
    {
        // This must be the first democam packet.
        // Initialize framez to the height we just read.
        demo_framez = z;
        pos_delta[VZ] = 0;
    }
    // demo_z is the offset to demo_framez for the current tic.
    // It is incremented by pos_delta[VZ] every tic.
    demo_z = 0;

    if(intertics == 1)
    {
        // Instantaneous move.
        R_ResetViewer();
        Cl_MoveLocalPlayer(pos_delta[VX], pos_delta[VY], demo_framez =
                           z, demo_onground);
        pl->viewz = z;          // Might get an unsynced frame is not set right now.
        pos_delta[VX] = pos_delta[VY] = pos_delta[VZ] = 0;
    }
}

/*
 * Called once per tic.
 */
void Demo_Ticker(timespan_t time)
{
    static trigger_t fixed = { 1 / 35.0 };
    ddplayer_t *pl = players + consoleplayer;
    int     i;

    if(!M_CheckTrigger(&fixed, time))
        return;

    // Only playback is handled.
    if(playback)
    {
        pl->clAngle += viewangle_delta;
        pl->clLookDir += lookdir_delta;
        // Move player (i.e. camera).
        Cl_MoveLocalPlayer(pos_delta[VX], pos_delta[VY], demo_framez + demo_z,
                           demo_onground);
        // Interpolate camera Z offset (to framez).
        demo_z += pos_delta[VZ];
    }
    else
    {
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].ingame && clients[i].recording &&
               !clients[i].recordPaused &&
               ++writeInfo[i].cameratimer >= LOCALCAM_WRITE_TICS)
            {
                // It's time to write local view angles and coords.
                writeInfo[i].cameratimer = 0;
                Demo_WriteLocalCamera(i);
            }
    }
}

D_CMD(PlayDemo)
{
    if(argc != 2)
    {
        Con_Printf("Usage: %s (fileName)\n", argv[0]);
        return true;
    }
    Con_Printf("Playing demo \"%s\"...\n", argv[1]);
    return Demo_BeginPlayback(argv[1]);
}

D_CMD(RecordDemo)
{
    int     plnum = consoleplayer;

    if(argc == 3 && isClient)
    {
        Con_Printf("Clients can only record the consoleplayer.\n");
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
    int     plnum = consoleplayer;

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
    int     plnum = consoleplayer;

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
    {
        Demo_StopPlayback();
        // Tell the Game DLL that the playback was aborted.
        gx.NetWorldEvent(DDWE_DEMO_END, true, 0);
    }
    else
        Demo_StopRecording(plnum);
    return true;
}

/*
 * Make a demo lump.
 */
D_CMD(DemoLump)
{
    char    buf[64];

    if(argc != 3)
    {
        Con_Printf("Usage: %s (demofile) (lumpfile)\n", argv[0]);
        Con_Printf
            ("Writes a 64-byte reference lump for the given demo file.\n");
        return true;
    }
    memset(buf, 0, sizeof(buf));
    strncpy(buf, argv[1], 64);
    return M_WriteFile(argv[2], buf, 64);
}
