/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/**
 * hu_log.c: Player's game message log.
 *
 * \todo Chat widget is here and should be moved.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_log.h"
#include "hu_stuff.h"
#include "p_tick.h" // for P_IsPaused()
#include "d_net.h"

// MACROS ------------------------------------------------------------------

#define LOG_MAX_MESSAGES    (8)

#define LOG_MSG_FLASHFADETICS (1*TICSPERSEC)
#define LOG_MSG_TIMEOUT     (4*TICRATE)

// Local Message flags:
#define MF_JUSTADDED        (0x1)

// TYPES -------------------------------------------------------------------

typedef struct logmsg_s {
    char*           text;
    size_t          maxLen;
    uint            ticsRemain, tics;
    byte            flags;
} logmsg_t;

typedef struct msglog_s {
    boolean         visible;

    boolean         notToBeFuckedWith;
    boolean         dontFuckWithMe;

    logmsg_t        msgs[LOG_MAX_MESSAGES];
    uint            msgCount; // Number of used msg slots.
    uint            nextMsg; // Index of the next slot to be used in msgs.
    uint            numVisibleMsgs; // Number of visible messages.

    int             timer; // Auto-hide timer.
    float           yOffset; // Scroll-up offset.
} msglog_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static msglog_t msgLogs[MAXPLAYERS];

cvar_t msgLogCVars[] = {
    // Behaviour
    {"msg-count", 0, CVT_INT, &cfg.msgCount, 0, 8},
    {"msg-uptime", CVF_NO_MAX, CVT_INT, &cfg.msgUptime, 35, 0},

    // Display
    {"msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2},
    {"msg-blink", CVF_NO_MAX, CVT_INT, &cfg.msgBlink, 0, 0},
    {"msg-scale", CVF_NO_MAX, CVT_FLOAT, &cfg.msgScale, 0, 0},
    {"msg-show", 0, CVT_BYTE, &cfg.msgShow, 0, 1},

    // Colour defaults
    {"msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[CR], 0, 1},
    {"msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[CG], 0, 1},
    {"msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[CB], 0, 1},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up.
 * Register Cvars and CCmds for the opperation/look of the message log.
 */
void Hu_LogRegister(void)
{
    int                 i;

    for(i = 0; msgLogCVars[i].name; ++i)
        Con_AddVariable(msgLogCVars + i);
}

/**
 * Adds the given message to the buffer.
 *
 * @param log           Ptr to the msglog to add the message to.
 * @param txt           The message to be added.
 * @param tics          The length of time the message should be visible.
 */
static void logPush(msglog_t* log, const char* txt, int tics)
{
    size_t              len;
    logmsg_t*           msg;

    if(!txt || !txt[0])
        return;

    len = strlen(txt);
    msg = &log->msgs[log->nextMsg];

    if(len > msg->maxLen)
    {
        msg->text = realloc(msg->text, len + 1);
        msg->maxLen = len;
    }
    else
    {
        memset(msg->text, 0, msg->maxLen);
    }

    snprintf(msg->text, len, "%s", txt);
    msg->text[len] = '\0';
    msg->ticsRemain = msg->tics = tics;
    msg->flags = MF_JUSTADDED;

    if(log->nextMsg < LOG_MAX_MESSAGES - 1)
        log->nextMsg++;
    else
        log->nextMsg = 0;

    if(log->msgCount < LOG_MAX_MESSAGES)
        log->msgCount++;

    if(log->numVisibleMsgs < (unsigned) cfg.msgCount)
        log->numVisibleMsgs++;

    log->notToBeFuckedWith = log->dontFuckWithMe;
    log->dontFuckWithMe = 0;

    // Reset the auto-hide timer.
    log->timer = LOG_MSG_TIMEOUT;

    log->visible = true;
}

/**
 * Remove the oldest message from the msglog.
 *
 * @param log           Ptr to the msglog.
 */
static void logPop(msglog_t* log)
{
    int                 oldest;
    logmsg_t*           msg;

    if(log->numVisibleMsgs == 0)
        return;

    oldest = (unsigned) log->nextMsg - log->numVisibleMsgs;
    if(oldest < 0)
        oldest += LOG_MAX_MESSAGES;

    msg = &log->msgs[oldest];
    msg->ticsRemain = 10;
    msg->flags &= ~MF_JUSTADDED;

    log->numVisibleMsgs--;
}

/**
 * Tick the given msglog. Jobs include ticking messages and adjusting values
 * used when drawing the buffer for animation.
 *
 * @param log           Ptr to the msglog to tick.
 */
static void logTicker(msglog_t* log)
{
    int                 i;

    // Don't tick if the game is paused.
    if(P_IsPaused())
        return;

    // All messags tic away. When lower than lineheight, offset the y origin
    // of the message log. When zero, the earliest is pop'd.
    for(i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        logmsg_t*           msg = &log->msgs[i];

        if(msg->ticsRemain > 0)
            msg->ticsRemain--;
    }

    if(log->numVisibleMsgs)
    {
        int                 oldest;
        logmsg_t*           msg;

        oldest = (unsigned) log->nextMsg - log->numVisibleMsgs;
        if(oldest < 0)
            oldest += LOG_MAX_MESSAGES;

        msg = &log->msgs[oldest];

        log->yOffset = 0;
        if(msg->ticsRemain == 0)
        {
            logPop(log);
        }
        else
        {
            if(msg->ticsRemain <= LINEHEIGHT_A)
                log->yOffset = LINEHEIGHT_A - msg->ticsRemain;
        }
    }

    // Tic the auto-hide timer.
    if(log->timer > 0)
        log->timer--;
    if(log->timer == 0)
    {
        log->visible = false;
        log->notToBeFuckedWith = false;
    }
}

/**
 * Draws the contents of the given msglog to the screen.
 *
 * @param log           Ptr to the msglog to draw.
 */
static void logDrawer(msglog_t* log)
{
    uint                i, numVisible;
    int                 n, x, y;

    // How many messages should we print?
    switch(cfg.msgAlign)
    {
    default:
    case ALIGN_LEFT:    x = 0;              break;
    case ALIGN_CENTER:  x = SCREENWIDTH/2;  break;
    case ALIGN_RIGHT:   x = SCREENWIDTH;    break;
    }

    // First 'num' messages starting from the first one.
    numVisible = MIN_OF(log->numVisibleMsgs, (unsigned) cfg.msgCount);
    n = log->nextMsg - numVisible;
    if(n < 0)
        n += LOG_MAX_MESSAGES;

    y = 0;

    Draw_BeginZoom(cfg.msgScale, x, 0);
    DGL_Translatef(0, -log->yOffset, 0);

    for(i = 0; i < numVisible; ++i, y += LINEHEIGHT_A)
    {
        logmsg_t*           msg = &log->msgs[n];
        float               col[4];

        // Default colour and alpha.
        col[CR] = cfg.msgColor[CR];
        col[CG] = cfg.msgColor[CG];
        col[CB] = cfg.msgColor[CB];
        col[CA] = 1;

        if(msg->flags & MF_JUSTADDED)
        {
            uint                msgTics, td, blinkSpeed = cfg.msgBlink;

            msgTics = msg->tics - msg->ticsRemain;
            td = cfg.msgUptime - msg->ticsRemain;

            if((td & 2) && blinkSpeed != 0 && msgTics < blinkSpeed)
            {
                // Flash color.
                col[CR] = col[CG] = col[CB] = 1;
            }
            else if(blinkSpeed != 0 &&
                    msgTics < blinkSpeed + LOG_MSG_FLASHFADETICS &&
                    msgTics >= blinkSpeed)
            {
                int                 c;

                // Fade color to normal.
                for(c = 0; c < 3; ++c)
                    col[c] += ((1.0f - col[c]) / LOG_MSG_FLASHFADETICS) *
                                (blinkSpeed + LOG_MSG_FLASHFADETICS - msgTics);
            }
        }
        else
        {
            // Fade alpha out.
            if(i == 0 && msg->ticsRemain <= LINEHEIGHT_A)
                col[CA] = msg->ticsRemain / (float) LINEHEIGHT_A * .9f;
        }

        // Draw using param text.
        // Messages may use the params to override the way the message is
        // is displayed, e.g. colour (Hexen's important messages).
        WI_DrawParamText(x, 1 + y, msg->text, GF_FONTA,
                         col[CR], col[CG], col[CB], col[CA], false, false,
                         cfg.msgAlign);

        n = (n < LOG_MAX_MESSAGES - 1)? n + 1 : 0;
    }

    Draw_EndZoom();
}

/**
 * Initialize the message log of the specified player. Typically called after
 * map load or when said player enters the world.
 *
 * @param player        Player (local) number whose message log to init.
 */
void Hu_LogStart(int player)
{
    player_t*           plr;
    msglog_t*           log;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    log = &msgLogs[player];
    memset(log, 0, sizeof(msglog_t));
}

/**
 * Called during final shutdown.
 */
void Hu_LogShutdown(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        msglog_t*           log = &msgLogs[i];
        int                 j;

        for(j = 0; j < LOG_MAX_MESSAGES; ++j)
        {
            logmsg_t*           msg = &log->msgs[j];

            if(msg->text)
                free(msg->text);
            msg->text = NULL;
            msg->maxLen = 0;
        }

        log->msgCount = log->numVisibleMsgs = 0;
    }
}

/**
 * Called TICSPERSEC times a second.
 */
void Hu_LogTicker(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        logTicker(&msgLogs[i]);
    }
}

/**
 * Draw the message log of the specified player.
 *
 * @param player        Player (local) number whose message log to draw.
 */
void Hu_LogDrawer(int player)
{
    if(cfg.msgShow)
    {
        logDrawer(&msgLogs[player]);
    }
}

/**
 * Post a message to the specified player's log.
 *
 * @param player        Player (local) number whose log to post to.
 * @param flags         LMF_* flags
 *                      LMF_NOHIDE:
 *                      Always display this message regardless whether the
 *                      player's message log has been hidden.
 *                      LMF_YELLOW:
 *                      Prepend the YELLOW param string to msg.
 * @param msg           Message text to be posted.
 * @param tics          Minimum number of tics (from *now*) the message
 *                      should be visible for.
 */
void Hu_LogPost(int player, byte flags, const char* msg, int tics)
{
#define YELLOW_FORMAT   "{r=1; g=0.7; b=0.3;}"

    player_t*           plr;
    msglog_t*           log;

    if(!msg || !msg[0] || !(tics > 0))
        return;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    log = &msgLogs[player];

    if(!log->notToBeFuckedWith || log->dontFuckWithMe)
    {
        char*           buf;

        if(flags & LMF_YELLOW)
        {
            buf = malloc((strlen(msg)+strlen(YELLOW_FORMAT)+1) * sizeof(char));
            sprintf(buf, YELLOW_FORMAT "%s", msg);
        }
        else
        {
            buf = malloc((strlen(msg)+1) * sizeof(char));
            sprintf(buf, "%s", msg);
        }

        logPush(log, buf, cfg.msgUptime + tics);

        free(buf);
    }

#undef YELLOW_FORMAT
}

/**
 * Rewind the message log of the specified player, making the last few
 * messages visible again.
 *
 * @param player        Player (local) number whose message log to refresh.
 */
void Hu_LogRefresh(int player)
{
    uint                i;
    int                 n;
    player_t*           plr;
    msglog_t*           log;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    log = &msgLogs[player];
    log->visible = true;
    log->numVisibleMsgs = MIN_OF((unsigned) cfg.msgCount,
        MIN_OF(log->msgCount, (unsigned) LOG_MAX_MESSAGES));

    // Reset the auto-hide timer.
    log->timer = LOG_MSG_TIMEOUT;

    // Refresh the messages.
    n = log->nextMsg - log->numVisibleMsgs;
    if(n < 0)
        n += LOG_MAX_MESSAGES;

    for(i = 0; i < log->numVisibleMsgs; ++i)
    {
        logmsg_t*           msg = &log->msgs[n];

        // Change the tics remaining to that at post time plus a small bonus
        // so that they don't all disappear at once.
        msg->ticsRemain = msg->tics + i * (TICSPERSEC >> 2);
        msg->flags &= ~MF_JUSTADDED;

        n = (n < LOG_MAX_MESSAGES - 1)? n + 1 : 0;
    }
}

/**
 * Empty the message log of the specified player.
 *
 * @param player        Player (local) number whose message log to empty.
 */
void Hu_LogEmpty(int player)
{
    player_t*           plr;
    msglog_t*           log;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    log = &msgLogs[player];

    while(log->numVisibleMsgs)
        logPop(log);
}

/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/**
 * hu_chat.c: HUD chat widget.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_stuff.h"
#include "hu_log.h"
#include "hu_lib.h"
#include "p_tick.h" // for P_IsPaused()
#include "g_common.h"
#include "g_controls.h"
#include "d_net.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

#if __JHEXEN__
enum {
    CT_PLR_BLUE = 1,
    CT_PLR_RED,
    CT_PLR_YELLOW,
    CT_PLR_GREEN,
    CT_PLR_PLAYER5,
    CT_PLR_PLAYER6,
    CT_PLR_PLAYER7,
    CT_PLR_PLAYER8
};
#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

DEFCC(CCmdMsgAction);
DEFCC(CCmdLocalMessage);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     closeChat(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean shiftdown = false;
boolean chatOn;

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

char* player_names[4];
int player_names_idx[] = {
    TXT_HUSTR_PLRGREEN,
    TXT_HUSTR_PLRINDIGO,
    TXT_HUSTR_PLRBROWN,
    TXT_HUSTR_PLRRED
};

#else

char* player_names[8];
int player_names_idx[] = {
    CT_PLR_BLUE,
    CT_PLR_RED,
    CT_PLR_YELLOW,
    CT_PLR_GREEN,
    CT_PLR_PLAYER5,
    CT_PLR_PLAYER6,
    CT_PLR_PLAYER7,
    CT_PLR_PLAYER8
};
#endif

cvar_t chatCVars[] = {
    // Chat macros
    {"chat-macro0", 0, CVT_CHARPTR, &cfg.chatMacros[0], 0, 0},
    {"chat-macro1", 0, CVT_CHARPTR, &cfg.chatMacros[1], 0, 0},
    {"chat-macro2", 0, CVT_CHARPTR, &cfg.chatMacros[2], 0, 0},
    {"chat-macro3", 0, CVT_CHARPTR, &cfg.chatMacros[3], 0, 0},
    {"chat-macro4", 0, CVT_CHARPTR, &cfg.chatMacros[4], 0, 0},
    {"chat-macro5", 0, CVT_CHARPTR, &cfg.chatMacros[5], 0, 0},
    {"chat-macro6", 0, CVT_CHARPTR, &cfg.chatMacros[6], 0, 0},
    {"chat-macro7", 0, CVT_CHARPTR, &cfg.chatMacros[7], 0, 0},
    {"chat-macro8", 0, CVT_CHARPTR, &cfg.chatMacros[8], 0, 0},
    {"chat-macro9", 0, CVT_CHARPTR, &cfg.chatMacros[9], 0, 0},
    {"chat-beep", 0, CVT_BYTE, &cfg.chatBeep, 0, 1},
    {NULL}
};

// Console commands for the chat widget and message log.
ccmd_t chatCCmds[] = {
    {"chatcancel",      "",     CCmdMsgAction},
    {"chatcomplete",    "",     CCmdMsgAction},
    {"chatdelete",      "",     CCmdMsgAction},
    {"chatsendmacro",   NULL,   CCmdMsgAction},
    {"beginchat",       NULL,   CCmdMsgAction},
    {"message",         "s",    CCmdLocalMessage},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int chatTo = 0; // 0=all, 1=player 0, etc.
static hu_text_t chat;
static hu_text_t chatBuffer[MAXPLAYERS];
static boolean chatAlwaysOff = false;

// CODE --------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up.
 * Register Cvars and CCmds for the opperation/look of the (message) log.
 */
void Chat_Register(void)
{
    int                 i;

    for(i = 0; chatCVars[i].name; ++i)
        Con_AddVariable(chatCVars + i);

    for(i = 0; chatCCmds[i].name; ++i)
        Con_AddCommand(chatCCmds + i);
}

/**
 * Called by HU_init().
 */
void Chat_Init(void)
{
    int                 i;

    // Setup strings.
    for(i = 0; i < 10; ++i)
    {
        if(!cfg.chatMacros[i]) // Don't overwrite if already set.
            cfg.chatMacros[i] = GET_TXT(TXT_HUSTR_CHATMACRO0 + i);
    }
}

/**
 * Called by HU_Start().
 */
void Chat_Start(void)
{
    int                 i;

    // Create the chat widget
    HUlib_initText(&chat, 0, M_CharHeight('A', GF_FONTA) + 1, &chatOn);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        Chat_Open(i, false);

        // Create the input buffers.
        HUlib_initText(&chatBuffer[i], 0, 0, &chatAlwaysOff);
    }
}

void Chat_Open(int player, boolean open)
{
    if(open)
    {
        chatOn = true;
        chatTo = player;

        HUlib_resetText(&chat);

        // Enable the chat binding class
        DD_Execute(true, "activatebcontext chat");
    }
    else
    {
        if(chatOn)
        {
            chatOn = false;

            // Disable the chat binding class
            DD_Execute(true, "deactivatebcontext chat");
        }
    }
}

boolean Chat_Responder(event_t* ev)
{
    boolean             eatkey = false;
    unsigned char       c;

    if(!chatOn || G_GetGameState() != GS_MAP)
        return false;

    if(ev->type != EV_KEY)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
    {
        shiftdown = (ev->state == EVS_DOWN || ev->state == EVS_REPEAT);
        return false;
    }

    if(ev->state != EVS_DOWN)
        return false;

    c = (unsigned char) ev->data1;

    if(shiftdown)
        c = shiftXForm[c];

    eatkey = HUlib_keyInText(&chat, c);

    return eatkey;
}

void Chat_Drawer(int player)
{
    if(player == CONSOLEPLAYER)
        HUlib_drawText(&chat, GF_FONTA);
}

/**
 * Sends a string to other player(s) as a chat message.
 */
static void sendMessage(const char* msg)
{
    char                buff[256];
    int                 i;

    if(chatTo == 0)
    {   // Send the message to the other players explicitly,
        if(!IS_NETGAME)
        {   // Send it locally.
            for(i = 0; i < MAXPLAYERS; ++i)
            {
                D_NetMessageNoSound(i, msg);
            }
        }
        else
        {
            strcpy(buff, "chat ");
            M_StrCatQuoted(buff, msg);
            DD_Execute(false, buff);
        }
    }
    else
    {   // Send to all of the destination color.
        chatTo -= 1;

        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame && cfg.playerColor[i] == chatTo)
            {
                if(!IS_NETGAME)
                {   // Send it locally.
                    D_NetMessageNoSound(i, msg);
                }
                else
                {
                    sprintf(buff, "chatNum %d ", i);
                    M_StrCatQuoted(buff, msg);
                    DD_Execute(false, buff);
                }
            }
    }

#if __JDOOM__
    if(gameMode == commercial)
        S_LocalSound(SFX_RADIO, 0);
    else
        S_LocalSound(SFX_TINK, 0);
#elif __JDOOM64__
    S_LocalSound(SFX_RADIO, 0);
#endif
}

/**
 * Sets the chat buffer to a chat macro string.
 */
static boolean sendMacro(int num)
{
    if(!chatOn)
        return false;

    if(num >= 0 && num < 9)
    {   // Leave chat mode and notify that it was sent.
        if(chatOn)
            Chat_Open(CONSOLEPLAYER, false);

        sendMessage(cfg.chatMacros[num]);
        return true;
    }

    return false;
}

/**
 * Display a local game message.
 */
DEFCC(CCmdLocalMessage)
{
    D_NetMessageNoSound(CONSOLEPLAYER, argv[1]);
    return true;
}

/**
 * Handles controls (console commands) for the msglog and chat
 * widget
 */
DEFCC(CCmdMsgAction)
{
    int                 plynum;

    if(chatOn)
    {
        if(!stricmp(argv[0], "chatcomplete"))  // send the message
        {
            Chat_Open(CONSOLEPLAYER, false);
            if(chat.l.len)
            {
                sendMessage(chat.l.l);
            }
        }
        else if(!stricmp(argv[0], "chatcancel"))  // close chat
        {
            Chat_Open(CONSOLEPLAYER, false);
        }
        else if(!stricmp(argv[0], "chatdelete"))
        {
            HUlib_delCharFromText(&chat);
        }
    }

    if(!stricmp(argv[0], "chatsendmacro"))  // send a chat macro
    {
        int                 macroNum;

        if(argc < 2 || argc > 3)
        {
            Con_Message("Usage: %s (player) (macro number)\n", argv[0]);
            Con_Message("Send a chat macro to other player(s) in multiplayer.\n"
                        "If (player) is omitted, the message will be sent to all players.\n");
            return true;
        }

        if(argc == 3)
        {
            plynum = atoi(argv[1]);
            if(plynum < 0 || plynum > 3)
            {
                // Bad destination.
                Con_Message("Invalid player number \"%i\". Should be 0-3\n",
                            plynum);
                return false;
            }

            plynum += 1;
        }
        else
            plynum = 0;

        if(!chatOn) // we need to enable chat mode first...
            Chat_Open(plynum, true);

        macroNum = atoi(((argc == 3)? argv[2] : argv[1]));
        if(!sendMacro(macroNum))
        {
            Con_Message("Invalid macro number\n");
            return false;
        }
    }
    else if(!stricmp(argv[0], "beginchat")) // begin chat mode
    {
        if(chatOn)
            return false;

        if(argc == 2)
        {
            plynum = atoi(argv[1]);
            if(plynum < 0 || plynum > 3)
            {
                // Bad destination.
                Con_Message("Invalid player number \"%i\". Should be 0-3\n",
                            plynum);
                return false;
            }

            plynum += 1;
        }
        else
            plynum = 0;

        Chat_Open(plynum, true);
    }

    return true;
}
