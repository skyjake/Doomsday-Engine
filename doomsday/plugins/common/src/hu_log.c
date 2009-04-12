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

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_stuff.h"
#include "hu_log.h"
#include "hu_lib.h"
#include "p_tick.h" // for P_IsPaused()
#include "g_common.h"
#include "g_controls.h"
#include "d_net.h"

// MACROS ------------------------------------------------------------------

#define HU_INPUTX           HU_MSGX
#define HU_INPUTY           (HU_MSGY + HU_MSGHEIGHT*(huFont[0].height +1))

#define IN_RANGE(x) \
    ((x)>=MAX_MESSAGES? (x)-MAX_MESSAGES : (x)<0? (x)+MAX_MESSAGES : (x))

#define FLASHFADETICS       35
#define HU_MSGX             0
#define HU_MSGY             0
#define HU_MSGHEIGHT        1          // in lines

#define HU_MSGTIMEOUT       (4*TICRATE)
#define HU_BROADCAST        (5)

#define MAX_MESSAGES        8
#define MAX_LINELEN         140

// TYPES -------------------------------------------------------------------

typedef struct message_s {
    char       *text;
    int         time;
    int         duration; // Time when posted.
} message_t;

typedef struct msgbuffer_s {
    boolean     dontfuckwithme;
    boolean     noecho;
    message_t   messages[MAX_MESSAGES];
    int         timer;

    boolean     visible;
    boolean     nottobefuckedwith;

    int         firstmsg, lastmsg, msgcount;
    float       yoffset; // Scroll-up offset.
    char       *lastmessage;
} msgbuffer_t;

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

static void     HU_MsgBufAddMessage(msgbuffer_t* msgBuff, char* msg,
                                    int msgtics);
static void     HU_MsgBufDropLast(msgbuffer_t* msgBuff);
static void     HU_MsgBufClear(msgbuffer_t* msgBuff);
static void     HU_MsgBufDraw(msgbuffer_t* msgBuff);
static void     HU_MsgBufTick(msgbuffer_t* msgBuff);

static void     closeChat(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int actualMapTime;

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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static msgbuffer_t msgBuffer[MAXPLAYERS];

static int chatTo = 0; // 0=all, 1=player 0, etc.
static hu_itext_t w_chat;
static hu_itext_t w_chatbuffer[MAXPLAYERS];
static boolean w_chat_always_off = false;

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

cvar_t logCVars[] = {
    // Behaviour
    {"msg-count", 0, CVT_INT, &PLRPROFILE.msgLog.count, 0, 8},
    {"msg-echo", 0, CVT_BYTE, &gs.cfg.echoMsg, 0, 1},
#if __JDOOM__ || __JDOOM64__ || __JHERETIC__
    {"msg-secret", 0, CVT_BYTE, &GAMERULES.announceSecrets, 0, 1},
#endif
    {"msg-uptime", CVF_NO_MAX, CVT_INT, &PLRPROFILE.msgLog.upTime, 35, 0},

    // Display
    {"msg-align", 0, CVT_INT, &PLRPROFILE.msgLog.align, 0, 2},
    {"msg-blink", CVF_NO_MAX, CVT_INT, &PLRPROFILE.msgLog.blink, 0, 0},
    {"msg-scale", CVF_NO_MAX, CVT_FLOAT, &PLRPROFILE.msgLog.scale, 0, 0},
    {"msg-show", 0, CVT_BYTE, &PLRPROFILE.msgLog.show, 0, 1},

    // Colour defaults
    {"msg-color-r", 0, CVT_FLOAT, &PLRPROFILE.msgLog.color[0], 0, 1},
    {"msg-color-g", 0, CVT_FLOAT, &PLRPROFILE.msgLog.color[1], 0, 1},
    {"msg-color-b", 0, CVT_FLOAT, &PLRPROFILE.msgLog.color[2], 0, 1},

    // Chat macros
    {"chat-macro0", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[0], 0, 0},
    {"chat-macro1", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[1], 0, 0},
    {"chat-macro2", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[2], 0, 0},
    {"chat-macro3", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[3], 0, 0},
    {"chat-macro4", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[4], 0, 0},
    {"chat-macro5", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[5], 0, 0},
    {"chat-macro6", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[6], 0, 0},
    {"chat-macro7", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[7], 0, 0},
    {"chat-macro8", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[8], 0, 0},
    {"chat-macro9", 0, CVT_CHARPTR, &PLRPROFILE.chat.macros[9], 0, 0},
    {"chat-beep", 0, CVT_BYTE, &PLRPROFILE.chat.playBeep, 0, 1},
    {NULL}
};

// Console commands for the chat widget and message log.
ccmd_t logCCmds[] = {
    {"chatcancel",      "",     CCmdMsgAction},
    {"chatcomplete",    "",     CCmdMsgAction},
    {"chatdelete",      "",     CCmdMsgAction},
    {"chatsendmacro",   NULL,   CCmdMsgAction},
    {"beginchat",       NULL,   CCmdMsgAction},
    {"message",         "s",    CCmdLocalMessage},
    {"msgrefresh",      "",     CCmdMsgAction},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up.
 * Register Cvars and CCmds for the opperation/look of the (message) log.
 */
void HUMsg_Register(void)
{
    int                 i;

    for(i = 0; logCVars[i].name; ++i)
        Con_AddVariable(logCVars + i);

    for(i = 0; logCCmds[i].name; ++i)
        Con_AddCommand(logCCmds + i);
}

/**
 * Called by HU_init().
 */
void HUMsg_Init(void)
{
    int                 i;

    // Setup strings.
    for(i = 0; i < 10; ++i)
        if(!PLRPROFILE.chat.macros[i]) // Don't overwrite if already set.
            PLRPROFILE.chat.macros[i] = GET_TXT(TXT_HUSTR_CHATMACRO0 + i);

#define INIT_STRINGS(x, x_idx) \
    INIT_STRINGS(player_names, player_names_idx);
}

/**
 * Called by HU_Start().
 */
void HUMsg_Start(void)
{
    int                 i, j;

    closeChat();

    // Create the chat widget
    HUlib_initIText(&w_chat, HU_INPUTX, HU_INPUTY, huFontA, HU_FONTSTART,
                    &chatOn);

    // Create the message and input buffers for all local players.
    //// \todo we only need buffers for active local players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        msgbuffer_t *buf = &msgBuffer[i];

        buf->visible = false;
        buf->dontfuckwithme = false;
        buf->nottobefuckedwith = false;
        buf->msgcount = 0;
        buf->yoffset = 0; // Scroll-up offset.
        buf->noecho = false;
        buf->timer = 0;
        buf->visible = true;
        buf->firstmsg = 0;
        buf->lastmsg = 0;
        buf->lastmessage = NULL;

        for(j = 0; j < MAX_MESSAGES; ++j)
        {
            message_t *msg = &buf->messages[j];

            msg->text = NULL;
            msg->time = 0;
            msg->duration = 0;
        }

        // Create the inputbuffer widgets.
        HUlib_initIText(&w_chatbuffer[i], 0, 0, 0, 0, &w_chat_always_off);
    }
}

/**
 * Called by HU_ticker().
 */
void HUMsg_Ticker(void)
{
    int                 i;

    // Don't tick the message buffer if the game is paused.
    if(!P_IsPaused())
    {
        for(i = 0; i < MAXPLAYERS; ++i)
            HU_MsgBufTick(&msgBuffer[i]);
    }
}

void HUMsg_Drawer(int player)
{
    // Don't draw the messages when the map title is up.
    if(gs.cfg.mapTitle && actualMapTime < 6 * 35)
        return;

    if(PLRPROFILE.msgLog.show)
        HU_MsgBufDraw(&msgBuffer[player]);

    if(player == CONSOLEPLAYER)
        HUlib_drawIText(&w_chat);
}

boolean HUMsg_Responder(event_t* ev)
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

    eatkey = HUlib_keyInIText(&w_chat, c);

    return eatkey;
}

void HUMsg_PlayerMessage(int player, char* message, int tics,
                         boolean noHide, boolean yellow)
{
    msgbuffer_t*        msgBuff = &msgBuffer[player];

    if(!message || !tics)
        return;

    if(!msgBuff->nottobefuckedwith || msgBuff->dontfuckwithme)
    {
        if(msgBuff->lastmessage)
        {
            free(msgBuff->lastmessage);
            msgBuff->lastmessage = NULL;
        }

        if(yellow)
        {
            char*           format = "{r=1; g=0.7; b=0.3;}";

            // Alloc a new buffer.
            msgBuff->lastmessage =
                malloc((strlen(message)+strlen(format)+1) * sizeof(char));
            // Copy the format string.
            sprintf(msgBuff->lastmessage, "%s%s", format, message);
        }
        else
        {
            // Alloc a new buffer.
            msgBuff->lastmessage = malloc((strlen(message)+1) * sizeof(char));

            // Copy the message
            sprintf(msgBuff->lastmessage, "%s", message);
        }

        HU_MsgBufAddMessage(msgBuff, msgBuff->lastmessage, tics);

        msgBuff->visible = true;
        msgBuff->timer = HU_MSGTIMEOUT;
        msgBuff->nottobefuckedwith = msgBuff->dontfuckwithme;
        msgBuff->dontfuckwithme = 0;
    }
}

void HUMsg_ClearMessages(int player)
{
    HU_MsgBufClear(&msgBuffer[player]);
}

/**
 * Adds the given message to the buffer.
 *
 * @param buf           Ptr to the message buffer to add the message to.
 * @param txt           The message to be added.
 * @param tics          The length of time the message should be visible.
 */
static void HU_MsgBufAddMessage(msgbuffer_t* buf, char* txt, int tics)
{
    int                 len;
    message_t*          msg;

    if(!buf)
        return;

    msg = &buf->messages[buf->lastmsg];

    len = strlen(txt);
    msg->text = realloc(msg->text, len + 1);
    strcpy(msg->text, txt);
    msg->text[len] = 0;
    msg->time = msg->duration = PLRPROFILE.msgLog.upTime + tics;

    buf->lastmsg = IN_RANGE(buf->lastmsg + 1);

    if(buf->msgcount == MAX_MESSAGES)
        buf->firstmsg = buf->lastmsg;
    else if(buf->msgcount == PLRPROFILE.msgLog.count)
        buf->firstmsg = IN_RANGE(buf->firstmsg + 1);
    else
        buf->msgcount++;
}

/**
 * Remove the oldest message from the message buffer.
 *
 * @param buf           Ptr to the message buffer.
 */
static void HU_MsgBufDropLast(msgbuffer_t* buf)
{
    message_t*          msg;

    if(!buf || buf->msgcount == 0)
        return;

    buf->firstmsg = IN_RANGE(buf->firstmsg + 1);

    msg = &buf->messages[buf->firstmsg];
    if(msg->time < 10)
        msg->time = 10;

    buf->msgcount--;
}

/**
 * Empties the message buffer.
 *
 * @param buf           Ptr to the message buffer to clear.
 */
static void HU_MsgBufClear(msgbuffer_t* buf)
{
    int                 i;

    if(!buf)
        return;

    for(i = 0; i < MAX_MESSAGES; ++i)
    {
        if(buf->messages[i].text)
        {
            free(buf->messages[i].text);
            buf->messages[i].text = NULL;
        }
    }

    buf->firstmsg = buf->lastmsg = 0;
    buf->msgcount = 0;
}

/**
 * \fixme This doesn't seem to work as intended.
 *
 * @param buf           Ptr to the message buffer to refresh.
 */
static void HU_MsgBufRefresh(msgbuffer_t* buf)
{
    if(!buf)
        return;

    buf->visible = true;
    buf->timer = HU_MSGTIMEOUT;
}


/**
 * Tick the given message buffer. Jobs include ticking messages and
 * adjusting values used when drawing the buffer for animation.
 *
 * @param buf           Ptr to the message buffer to tick.
 */
static void HU_MsgBufTick(msgbuffer_t* buf)
{
    int                 i;

    if(!buf)
        return;

    // Countdown to scroll-up.
    for(i = 0; i < MAX_MESSAGES; ++i)
        if(buf->messages[i].time > 0)
            buf->messages[i].time--;

    if(buf->msgcount != 0)
    {
        message_t *msg = &buf->messages[buf->firstmsg];

        buf->yoffset = 0;
        if(msg->time == 0)
        {
            HU_MsgBufDropLast(buf);
        }
        else if(msg->time <= LINEHEIGHT_A)
        {
            buf->yoffset = LINEHEIGHT_A - msg->time;
        }
    }

    // Tick down message counter if a message is up.
    if(buf->timer > 0)
        buf->timer--;

    if(buf->timer == 0)
    {
        buf->visible = false;
        buf->nottobefuckedwith = false;
    }
}

/**
 * Draws the contents of the given message buffer to the screen.
 *
 * @param buf           Ptr to the message buffer to draw.
 */
static void HU_MsgBufDraw(msgbuffer_t* buf)
{
    int                 i, num, y, lh = LINEHEIGHT_A, x;
    int                 td, m, msgTics, blinkSpeed;
    float               col[4];
    message_t*          msg;

    if(!buf)
        return;

    // How many messages should we print?
    num = buf->msgcount;

    switch(PLRPROFILE.msgLog.align)
    {
    case ALIGN_LEFT:    x = 0;      break;
    case ALIGN_CENTER:  x = 160;    break;
    case ALIGN_RIGHT:   x = 320;    break;
    default:            x = 0;      break;
    }

    Draw_BeginZoom(PLRPROFILE.msgLog.scale, x, 0);
    DGL_Translatef(0, -buf->yoffset, 0);

    // First 'num' messages starting from the last one.
    for(y = (num - 1) * lh, m = IN_RANGE(buf->lastmsg - 1); num;
        y -= lh, num--, m = IN_RANGE(m - 1))
    {
        msg = &buf->messages[m];

        // Set colour and alpha.
        memcpy(col, PLRPROFILE.msgLog.color, sizeof(PLRPROFILE.msgLog.color));
        col[3] = 1;

        td = PLRPROFILE.msgLog.upTime - msg->time;
        msgTics = msg->duration - msg->time;
        blinkSpeed = PLRPROFILE.msgLog.blink;

        if((td & 2) && blinkSpeed != 0 && msgTics < blinkSpeed)
        {
            // Flash color.
            col[0] = col[1] = col[2] = 1;
        }
        else if(blinkSpeed != 0 &&
                msgTics < blinkSpeed + FLASHFADETICS &&
                msgTics >= blinkSpeed)
        {
            // Fade color to normal.
            for(i = 0; i < 3; ++i)
                col[i] += ((1.0f - col[i]) / FLASHFADETICS) *
                            (blinkSpeed + FLASHFADETICS - msgTics);
        }
        else
        {
            // Fade alpha out.
            if(m == buf->firstmsg && msg->time <= LINEHEIGHT_A)
                col[3] = msg->time / (float) LINEHEIGHT_A * 0.9f;
        }

        // Draw using param text.
        // Messages may use the params to override the way the message is
        // is displayed, e.g. colour (Hexen's important messages).
        WI_DrawParamText(x, 1 + y, msg->text, huFontA,
                         col[0], col[1], col[2], col[3], false, false,
                         PLRPROFILE.msgLog.align);
    }

    Draw_EndZoom();
}

static void openChat(int plynum)
{
    chatOn = true;
    chatTo = plynum;

    HUlib_resetIText(&w_chat);

    // Enable the chat binding class
    //DD_SetBindClass(GBC_CHAT, true);
    DD_Execute(true, "activatebcontext chat");
}

static void closeChat(void)
{
    if(chatOn)
    {
        chatOn = false;
        // Disable the chat binding class
        //DD_SetBindClass(GBC_CHAT, false);
        DD_Execute(true, "deactivatebcontext chat");
    }
}

/**
 * Sends a string to other player(s) as a chat message.
 */
static void sendMessage(const char* msg)
{
    char                buff[256];
    int                 i;

    if(chatTo == HU_BROADCAST)
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
        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame && gs.players[i].color == chatTo)
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
            closeChat();

        sendMessage(PLRPROFILE.chat.macros[num]);
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
 * Handles controls (console commands) for the message buffer and chat
 * widget
 */
DEFCC(CCmdMsgAction)
{
    int                 plynum;

    if(chatOn)
    {
        if(!stricmp(argv[0], "chatcomplete"))  // send the message
        {
            closeChat();
            if(w_chat.l.len)
            {
                sendMessage(w_chat.l.l);
            }
        }
        else if(!stricmp(argv[0], "chatcancel"))  // close chat
        {
            closeChat();
        }
        else if(!stricmp(argv[0], "chatdelete"))
        {
            HUlib_delCharFromIText(&w_chat);
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
        }
        else
            plynum = HU_BROADCAST;

        if(!chatOn) // we need to enable chat mode first...
            openChat(plynum);

        macroNum = atoi(((argc == 3)? argv[2] : argv[1]));
        if(!sendMacro(macroNum))
        {
            Con_Message("Invalid macro number\n");
            return false;
        }
    }
    else if(!stricmp(argv[0], "msgrefresh")) // refresh the message buffer
    {
        if(chatOn)
            return false;

        HU_MsgBufRefresh(&msgBuffer[CONSOLEPLAYER]);
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
        }
        else
            plynum = HU_BROADCAST;

        openChat(plynum);
    }

    return true;
}
