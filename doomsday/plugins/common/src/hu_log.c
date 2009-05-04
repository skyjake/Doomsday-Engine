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
#include <stdio.h>
#include <string.h>
#include <assert.h>

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
#include "p_tick.h" // for P_IsPaused()
#include "d_net.h"

// MACROS ------------------------------------------------------------------

#define LOG_MAX_MESSAGES    (8)

#define LOG_MSG_FLASHFADETICS (1*TICSPERSEC)
#define LOG_MSG_TIMEOUT     (4*TICRATE)

// TYPES -------------------------------------------------------------------

typedef struct logmsg_s {
    char*           text;
    int             time;
    int             duration; // Time when posted.
} logmsg_t;

typedef struct msglog_s {
    boolean         visible;

    boolean         notToBeFuckedWith;
    boolean         dontFuckWithMe;

    logmsg_t        msgs[LOG_MAX_MESSAGES];
    uint            nextMsg, msgCount;

    int             timer;
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
    {"msg-echo", 0, CVT_BYTE, &cfg.echoMsg, 0, 1},
#if __JHEXEN__
    {"msg-hub-override", 0, CVT_BYTE, &cfg.overrideHubMsg, 0, 2},
#else
    {"msg-secret", 0, CVT_BYTE, &cfg.secretMsg, 0, 1},
#endif
    {"msg-uptime", CVF_NO_MAX, CVT_INT, &cfg.msgUptime, 35, 0},

    // Display
    {"msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2},
    {"msg-blink", CVF_NO_MAX, CVT_INT, &cfg.msgBlink, 0, 0},
    {"msg-scale", CVF_NO_MAX, CVT_FLOAT, &cfg.msgScale, 0, 0},
    {"msg-show", 0, CVT_BYTE, &cfg.msgShow, 0, 1},

    // Colour defaults
    {"msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[0], 0, 1},
    {"msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[1], 0, 1},
    {"msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[2], 0, 1},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up.
 * Register Cvars and CCmds for the opperation/look of the message log.
 */
void HUMsg_Register(void)
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

    msg->text = realloc(msg->text, len + 1);
    snprintf(msg->text, len, "%s", txt);
    msg->text[len] = '\0';
    msg->time = msg->duration = tics;

    log->timer = LOG_MSG_TIMEOUT;
    if(log->nextMsg < LOG_MAX_MESSAGES - 1)
        log->nextMsg++;
    else
        log->nextMsg = 0;

    if(log->msgCount < (unsigned) cfg.msgCount)
        log->msgCount++;

    log->notToBeFuckedWith = log->dontFuckWithMe;
    log->dontFuckWithMe = 0;

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

    if(log->msgCount == 0)
        return;

    oldest = (unsigned) log->nextMsg - log->msgCount;
    if(oldest < 0)
        oldest += LOG_MAX_MESSAGES;

    msg = &log->msgs[oldest];
    msg->time = 10;

    log->msgCount--;
}

/**
 * Empties the msglog.
 *
 * @param log           Ptr to the msglog to clear.
 */
static void logEmpty(msglog_t* log)
{
    int                 i;

    for(i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        logmsg_t*           msg = &log->msgs[i];

        if(msg->text)
            free(msg->text);
        msg->text = NULL;
    }

    log->msgCount = 0;
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

    // Countdown to scroll-up.
    for(i = 0; i < LOG_MAX_MESSAGES; ++i)
    {
        if(log->msgs[i].time > 0)
            log->msgs[i].time--;
    }

    if(log->msgCount != 0)
    {
        int                 oldest;
        logmsg_t*           msg;

        oldest = (unsigned) log->nextMsg - log->msgCount;
        if(oldest < 0)
            oldest += LOG_MAX_MESSAGES;

        msg = &log->msgs[oldest];

        log->yOffset = 0;
        if(msg->time == 0)
        {
            logPop(log);
        }
        else if(msg->time <= LINEHEIGHT_A)
        {
            log->yOffset = LINEHEIGHT_A - msg->time;
        }
    }

    // Tick down message counter if a message is up.
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
    int                 n, y, lh = LINEHEIGHT_A, x;
    int                 td, msgTics, blinkSpeed;
    float               col[4];
    logmsg_t*           msg;

    // How many messages should we print?
    switch(cfg.msgAlign)
    {
    case ALIGN_LEFT:    x = 0;      break;
    case ALIGN_CENTER:  x = 160;    break;
    case ALIGN_RIGHT:   x = 320;    break;
    default:            x = 0;      break;
    }

    Draw_BeginZoom(cfg.msgScale, x, 0);
    DGL_Translatef(0, -log->yOffset, 0);

    // First 'num' messages starting from the first one.
    numVisible = MIN_OF(log->msgCount, (unsigned) cfg.msgCount);
    n = log->nextMsg - numVisible;
    if(n < 0)
        n += LOG_MAX_MESSAGES;

    y = 0;
    for(i = 0; i < numVisible; ++i, y += lh)
    {
        msg = &log->msgs[n];

        // Set colour and alpha.
        memcpy(col, cfg.msgColor, sizeof(cfg.msgColor));
        col[3] = 1;

        td = cfg.msgUptime - msg->time;
        msgTics = msg->duration - msg->time;
        blinkSpeed = cfg.msgBlink;

        if((td & 2) && blinkSpeed != 0 && msgTics < blinkSpeed)
        {
            // Flash color.
            col[0] = col[1] = col[2] = 1;
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
        else
        {
            // Fade alpha out.
            if(i == 0 && msg->time <= LINEHEIGHT_A)
                col[3] = msg->time / (float) LINEHEIGHT_A * 0.9f;
        }

        // Draw using param text.
        // Messages may use the params to override the way the message is
        // is displayed, e.g. colour (Hexen's important messages).
        WI_DrawParamText(x, 1 + y, msg->text, huFontA,
                         col[0], col[1], col[2], col[3], false, false,
                         cfg.msgAlign);

        if(n < LOG_MAX_MESSAGES - 1)
            n++;
        else
            n = 0;
    }

    Draw_EndZoom();
}

/**
 * Called by HU_Start().
 */
void HUMsg_Start(void)
{
    int                 i, j;

    // Create the message logs for all local players.
    //// \todo we only need logs for active local players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        msglog_t*           log = &msgLogs[i];

        log->visible = false;
        log->dontFuckWithMe = false;
        log->notToBeFuckedWith = false;
        log->yOffset = 0; // Scroll-up offset.
        log->timer = 0;
        log->visible = true;
        log->nextMsg = log->msgCount = 0;

        for(j = 0; j < LOG_MAX_MESSAGES; ++j)
        {
            logmsg_t*           msg = &log->msgs[j];

            msg->text = NULL;
            msg->time = 0;
            msg->duration = 0;
        }
    }
}

/**
 * Called by HU_ticker().
 */
void HUMsg_Ticker(void)
{
    int                 i;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        logTicker(&msgLogs[i]);
    }
}

void HUMsg_Drawer(int player)
{
    if(cfg.msgShow)
    {
        logDrawer(&msgLogs[player]);
    }
}

void HUMsg_PlayerMessage(int player, char* message, int tics,
                         boolean noHide, boolean yellow)
{
#define YELLOW_FORMAT   "{r=1; g=0.7; b=0.3;}"

    msglog_t*           log = &msgLogs[player];

    if(!message || !tics)
        return;

    if(!log->notToBeFuckedWith || log->dontFuckWithMe)
    {
        char*           buf;

        if(yellow)
        {
            buf = malloc((strlen(message)+strlen(YELLOW_FORMAT)+1) * sizeof(char));
            sprintf(buf, YELLOW_FORMAT "%s", message);
        }
        else
        {
            buf = malloc((strlen(message)+1) * sizeof(char));
            sprintf(buf, "%s", message);
        }

        logPush(log, buf, cfg.msgUptime + tics);

        free(buf);
    }

#undef YELLOW_FORMAT
}

void HUMsg_ClearMessages(int player)
{
    logEmpty(&msgLogs[player]);
}

/**
 * Rewind the log buffer, and make the last few messages visible again.
 *
 * @param log           Ptr to the msglog to refresh.
 */
void HUMsg_Refresh(int player)
{
    player_t*           plr;
    msglog_t*           log;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    plr = &players[player];
    if(!((plr->plr->flags & DDPF_LOCAL) && plr->plr->inGame))
        return;

    log = &msgLogs[player];
    log->visible = true;
    log->timer = LOG_MSG_TIMEOUT;
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

#define CHAT_X              (0)
#define CHAT_Y              (0 + (huFont[0].height +1))

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
static hu_itext_t chat;
static hu_itext_t chatBuffer[MAXPLAYERS];
static boolean chatAlwaysOff = false;

static const char shiftXForm[] = {
    0,
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
    31,
    ' ', '!', '"', '#', '$', '%', '&',
    '"',                        // shift-'
    '(', ')', '*', '+',
    '<',                        // shift-,
    '_',                        // shift--
    '>',                        // shift-.
    '?',                        // shift-/
    ')',                        // shift-0
    '!',                        // shift-1
    '@',                        // shift-2
    '#',                        // shift-3
    '$',                        // shift-4
    '%',                        // shift-5
    '^',                        // shift-6
    '&',                        // shift-7
    '*',                        // shift-8
    '(',                        // shift-9
    ':',
    ':',                        // shift-;
    '<',
    '+',                        // shift-=
    '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[',                        // shift-[
    '!',                        // shift-backslash
    ']',                        // shift-]
    '"', '_',
    '\'',                       // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};

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
    HUlib_initIText(&chat, CHAT_X, CHAT_Y, huFontA, HU_FONTSTART, &chatOn);

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        Chat_Open(i, false);

        // Create the input buffers.
        HUlib_initIText(&chatBuffer[i], 0, 0, 0, 0, &chatAlwaysOff);
    }
}

void Chat_Open(int player, boolean open)
{
    if(open)
    {
        chatOn = true;
        chatTo = player;

        HUlib_resetIText(&chat);

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

    if(G_GetGameState() != GS_MAP || !chatOn)
        return false;

    if(ev->type == EV_KEY && ev->data1 == DDKEY_RSHIFT)
    {
        shiftdown = (ev->state == EVS_DOWN || ev->state == EVS_REPEAT);
        return false;
    }

    if(ev->type != EV_KEY || ev->state != EVS_DOWN)
        return false;

    c = (unsigned char) ev->data1;

    if(shiftdown || (c >= 'a' && c <= 'z'))
        c = shiftXForm[c];

    eatkey = HUlib_keyInIText(&chat, c);

    return eatkey;
}

void Chat_Drawer(int player)
{
    if(player == CONSOLEPLAYER)
        HUlib_drawIText(&chat);
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
            HUlib_delCharFromIText(&chat);
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
