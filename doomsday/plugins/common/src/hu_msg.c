/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Heads-up text and input code
 * Compiles for jDoom, jHeretic, jHexen
 * TODO: Merge chat input and menu-editbox widget
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdlib.h>

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_stuff.h"
#include "hu_msg.h"
#include "hu_lib.h"
#include "p_tick.h" // for P_IsPaused()
#include "g_common.h"
#include "g_controls.h"
#include "d_net.h"

// MACROS ------------------------------------------------------------------

#define HU_INPUTX   HU_MSGX
#define HU_INPUTY   (HU_MSGY + HU_MSGHEIGHT*(SHORT(hu_font[0].height) +1))

#define IN_RANGE(x)     ((x)>=MAX_MESSAGES? (x)-MAX_MESSAGES : (x)<0? (x)+MAX_MESSAGES : (x))

#define FLASHFADETICS  35
#define HU_MSGREFRESH   DDKEY_ENTER
#define HU_MSGX     0
#define HU_MSGY     0
#define HU_MSGWIDTH 64             // in characters
#define HU_MSGHEIGHT    1          // in lines

#define HU_MSGTIMEOUT   (4*TICRATE)

#define MAX_MESSAGES    8
#define MAX_LINELEN     140

// TYPES -------------------------------------------------------------------

typedef struct message_s {
    char    text[MAX_LINELEN];
    int     time;
    int     duration; // time when posted.
} message_t;

typedef struct messagebuffer_s {
    boolean     message_dontfuckwithme;
    boolean     message_noecho;
    message_t   messages[MAX_MESSAGES];
    int         message_counter;

    boolean     message_on;
    boolean     message_nottobefuckedwith;

    int         firstmsg, lastmsg, msgcount;
    float       yoffset; // Scroll-up offset.
    char       *lastmessage;
} messagebuffer_t;

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

static void     HUMsg_ClearBuffer(messagebuffer_t *msgBuff);
static void     HUMsg_CloseChat(void);
static void     HUMsg_DropLast(messagebuffer_t *msgBuff);
static void     HUMsg_Message(messagebuffer_t *msgBuff, char *msg,
                              int msgtics);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int actual_leveltime;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean shiftdown = false;
boolean chat_on;

#if __JDOOM__ || __JHERETIC__

char   *player_names[4];
int     player_names_idx[] = {
    TXT_HUSTR_PLRGREEN,
    TXT_HUSTR_PLRINDIGO,
    TXT_HUSTR_PLRBROWN,
    TXT_HUSTR_PLRRED
};

#else

char   *player_names[8];
int     player_names_idx[] = {
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

static messagebuffer_t msgBuffer[MAXPLAYERS];

static char lastmessage[HU_MAXLINELENGTH + 1];

static int chat_to = 0; // 0=all, 1=player 0, etc.

static hu_itext_t w_chat;
static boolean always_off = false;

static hu_itext_t w_inputbuffer[MAXPLAYERS];

static hu_stext_t w_message[MAXPLAYERS];

static const char english_shiftxform[] = {
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
    '!',                        // shift-backslash - OH MY GOD DOES WATCOM SUCK
    ']',                        // shift-]
    '"', '_',
    '\'',                       // shift-`
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '{', '|', '}', '~', 127
};

cvar_t msgCVars[] = {
    {"msg-show", 0, CVT_BYTE, &cfg.msgShow, 0, 1},

    // Behaviour
    {"msg-echo", 0, CVT_BYTE, &cfg.echoMsg, 0, 1},

#if !__JHEXEN__
    {"msg-secret", 0, CVT_BYTE, &cfg.secretMsg, 0, 1},
#endif

    {"msg-count", 0, CVT_INT, &cfg.msgCount, 0, 8},

    {"msg-uptime", CVF_NO_MAX, CVT_INT, &cfg.msgUptime, 35, 0},

#if __JHEXEN__
    {"msg-hub-override", 0, CVT_BYTE, &cfg.overrideHubMsg, 0, 2},
#endif

    // Display style
    {"msg-scale", CVF_NO_MAX, CVT_FLOAT, &cfg.msgScale, 0, 0},

    {"msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2},

    // Colour (default, individual message may have built-in colour settings)
    {"msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[0], 0, 1},
    {"msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[1], 0, 1},
    {"msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[2], 0, 1},

    {"msg-blink", CVF_NO_MAX, CVT_INT, &cfg.msgBlink, 0, 0},

    // Chat macros
    {"chat-macro0", 0, CVT_CHARPTR, &cfg.chat_macros[0], 0, 0},
    {"chat-macro1", 0, CVT_CHARPTR, &cfg.chat_macros[1], 0, 0},
    {"chat-macro2", 0, CVT_CHARPTR, &cfg.chat_macros[2], 0, 0},
    {"chat-macro3", 0, CVT_CHARPTR, &cfg.chat_macros[3], 0, 0},
    {"chat-macro4", 0, CVT_CHARPTR, &cfg.chat_macros[4], 0, 0},
    {"chat-macro5", 0, CVT_CHARPTR, &cfg.chat_macros[5], 0, 0},
    {"chat-macro6", 0, CVT_CHARPTR, &cfg.chat_macros[6], 0, 0},
    {"chat-macro7", 0, CVT_CHARPTR, &cfg.chat_macros[7], 0, 0},
    {"chat-macro8", 0, CVT_CHARPTR, &cfg.chat_macros[8], 0, 0},
    {"chat-macro9", 0, CVT_CHARPTR, &cfg.chat_macros[9], 0, 0},
    {NULL}
};

// Console commands for the message buffer
ccmd_t  msgCCmds[] = {
    {"chatcomplete",    CCmdMsgAction},
    {"chatdelete",      CCmdMsgAction},
    {"chatcancel",      CCmdMsgAction},
    {"chatsendmacro",   CCmdMsgAction},
    {"beginchat",       CCmdMsgAction},
    {"msgrefresh",      CCmdMsgAction},
    {"message",      CCmdLocalMessage},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up
 * Register Cvars and CCmds for the opperation/look of the
 * message buffer and chat widget.
 */
void HUMsg_Register(void)
{
    int     i;

    for(i = 0; msgCVars[i].name; ++i)
        Con_AddVariable(msgCVars + i);

    for(i = 0; msgCCmds[i].name; ++i)
        Con_AddCommand(msgCCmds + i);
}

/**
 * Called by HU_init()
 */
void HUMsg_Init(void)
{
    int     i;

    // Setup strings.
    for(i = 0; i < 10; i++)
        if(!cfg.chat_macros[i]) // Don't overwrite if already set.
            cfg.chat_macros[i] = GET_TXT(TXT_HUSTR_CHATMACRO0 + i);

#define INIT_STRINGS(x, x_idx) \
    INIT_STRINGS(player_names, player_names_idx);
}

/**
 * Called by HU_Start()
 */
void HUMsg_Start(void)
{
    int     i;

    HUMsg_CloseChat();

    // Create the chat widget
    HUlib_initIText(&w_chat, HU_INPUTX, HU_INPUTY, hu_font_a, HU_FONTSTART,
                    &chat_on);

    // Create the message and input buffers for all local players.
    // TODO: we only need buffers for active local players.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        msgBuffer[i].message_on = false;
        msgBuffer[i].message_dontfuckwithme = false;
        msgBuffer[i].message_nottobefuckedwith = false;
        msgBuffer[i].msgcount = 0;
        msgBuffer[i].yoffset = 0; // Scroll-up offset.

        // create the message widget
        HUlib_initSText(&w_message[i], HU_MSGX, HU_MSGY, HU_MSGHEIGHT, hu_font_a,
                        HU_FONTSTART, &msgBuffer[i].message_on);

        // create the inputbuffer widgets
        HUlib_initIText(&w_inputbuffer[i], 0, 0, 0, 0, &always_off);
    }
}

static void HUMsg_OpenChat(int chattarget)
{
    chat_on = true;
    chat_to = chattarget;

    HUlib_resetIText(&w_chat);

    // Enable the chat binding class
    DD_SetBindClass(GBC_CHAT, true);
}

static void HUMsg_CloseChat(void)
{
    if(chat_on)
    {
        chat_on = false;
        // Disable the chat binding class
        DD_SetBindClass(GBC_CHAT, false);
    }
}

static void HUMsg_TickBuffer(messagebuffer_t *msgBuff)
{
    int i;

    // Don't tick the message buffer if the game is paused.
    if(P_IsPaused())
        return;

    // Countdown to scroll-up.
    for(i = 0; i < MAX_MESSAGES; i++)
        msgBuff->messages[i].time--;

    if(msgBuff->msgcount)
    {
        msgBuff->yoffset = 0;
        if(msgBuff->messages[msgBuff->firstmsg].time >= 0 &&
           msgBuff->messages[msgBuff->firstmsg].time <= LINEHEIGHT_A)
        {
            msgBuff->yoffset =
                LINEHEIGHT_A - msgBuff->messages[msgBuff->firstmsg].time;
        }
        else if(msgBuff->messages[msgBuff->firstmsg].time < 0)
            HUMsg_DropLast(msgBuff);
    }

    // tick down message counter if message is up
    if(msgBuff->message_counter && !--msgBuff->message_counter)
    {
        msgBuff->message_on = false;
        msgBuff->message_nottobefuckedwith = false;
    }

    if(!msgBuff->message_counter)
    {
        // Refresh the screen when a message goes away
        GL_Update(DDUF_TOP);
    }
}

/**
 * Called by HU_ticker()
 */
void HUMsg_Ticker(void)
{
    int i;

    for(i = 0; i < MAXPLAYERS; ++i)
        HUMsg_TickBuffer(&msgBuffer[i]);
}

static void HUMsg_DrawBuffer(messagebuffer_t *msgBuff)
{
    int     i, m, num, y, lh = LINEHEIGHT_A, td, x;
    int     msgTics;
    float   col[4];

    // How many messages should we print?
    num = msgBuff->msgcount;

    switch(cfg.msgAlign)
    {
    case ALIGN_LEFT:
        x = 0;
        break;

    case ALIGN_CENTER:
        x = 160;
        break;

    case ALIGN_RIGHT:
        x = 320;
        break;

    default:
        x = 0;
        break;
    }

    Draw_BeginZoom(cfg.msgScale, x, 0);
    gl.Translatef(0, -msgBuff->yoffset, 0);

    // First 'num' messages starting from the last one.
    for(y = (num - 1) * lh, m = IN_RANGE(msgBuff->lastmsg - 1); num;
        y -= lh, num--, m = IN_RANGE(m - 1))
    {
        // Set colour.
        memcpy(col, cfg.msgColor, sizeof(cfg.msgColor));
        // Set alpha.
        col[3] = 1;

        td = cfg.msgUptime - msgBuff->messages[m].time;
        msgTics = msgBuff->messages[m].duration - msgBuff->messages[m].time;
        if(cfg.msgBlink && msgTics < cfg.msgBlink && (td & 2))
        {   // Flash color.
            col[0] = col[1] = col[2] = 1;
        }
        else if(cfg.msgBlink && msgTics < cfg.msgBlink + FLASHFADETICS &&
                msgTics >= cfg.msgBlink)
        {   // Fade color to normal.
            for(i = 0; i < 3; ++i)
                col[i] += (((1.0f - col[i]) / FLASHFADETICS) *
                            (cfg.msgBlink + FLASHFADETICS - msgTics));
        }
        else
        {   // Fade alpha out.
            if(m == msgBuff->firstmsg && msgBuff->messages[m].time <= LINEHEIGHT_A)
                col[3] = msgBuff->messages[m].time / (float) LINEHEIGHT_A *0.9f;
        }

        // draw using param text
        // messages may use the params to override
        // eg colour (Hexen's important messages)
        WI_DrawParamText(x, 1 + y, msgBuff->messages[m].text, hu_font_a,
                         col[0], col[1], col[2], col[3], false, false,
                         cfg.msgAlign);
    }

    Draw_EndZoom();
}

void HUMsg_Drawer(void)
{
    // Don't draw the messages when the level title is up.
    if(cfg.levelTitle && actual_leveltime < 6 * 35)
        return;

    HUMsg_DrawBuffer(&msgBuffer[consoleplayer]);

    HUlib_drawIText(&w_chat);
}

boolean HU_Responder(event_t *ev)
{
    boolean eatkey = false;
    unsigned char c;

    if(gamestate != GS_LEVEL || !chat_on)
        return false;

    if(ev->type == EV_KEY && ev->data1 == DDKEY_RSHIFT)
    {
        shiftdown = (ev->state == EVS_DOWN || ev->state == EVS_REPEAT);
        return false;
    }

    if(ev->type != EV_KEY && ev->state != EVS_DOWN && ev->state != EVS_REPEAT)
        return false;

    c = (unsigned char) ev->data1;

    if(shiftdown || (c >= 'a' && c <= 'z'))
        c = english_shiftxform[c];

    eatkey = HUlib_keyInIText(&w_chat, c);

    return eatkey;
}

void HUMsg_PlayerMessage(player_t *plr, char *message, int tics, boolean noHide,
                         boolean yellow)
{
    messagebuffer_t *msgBuff = &msgBuffer[plr - players];

    if(plr != &players[consoleplayer] || !message || !tics)
        return;

    if(cfg.msgShow || msgBuff->message_dontfuckwithme)
    {
        // display message if necessary.
        if((message && !msgBuff->message_nottobefuckedwith) ||
           (message && msgBuff->message_dontfuckwithme))
        {
            free(msgBuff->lastmessage);

            if(yellow)
            {
                char *format = "{r=1; g=0.7; b=0.3;}";

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

            HUMsg_Message(msgBuff, msgBuff->lastmessage, tics);

            msgBuff->message_on = true;
            msgBuff->message_counter = HU_MSGTIMEOUT;
            msgBuff->message_nottobefuckedwith = msgBuff->message_dontfuckwithme;
            msgBuff->message_dontfuckwithme = 0;
        }
    }
}

void P_ClearMessage(player_t *player)
{
    HUMsg_ClearBuffer(&msgBuffer[player - players]);
    if(player == &players[consoleplayer])
    {
        GL_Update(DDUF_TOP);
    }
}

/**
 * Add a new message.
 */
static void HUMsg_Message(messagebuffer_t *msgBuff, char *msg, int msgtics)
{
    msgBuff->messages[msgBuff->lastmsg].time =
        msgBuff->messages[msgBuff->lastmsg].duration = cfg.msgUptime + msgtics;

    strcpy(msgBuff->messages[msgBuff->lastmsg].text, msg);
    msgBuff->lastmsg = IN_RANGE(msgBuff->lastmsg + 1);

    if(msgBuff->msgcount == MAX_MESSAGES)
        msgBuff->firstmsg = msgBuff->lastmsg;
    else if(msgBuff->msgcount == cfg.msgCount)
        msgBuff->firstmsg = IN_RANGE(msgBuff->firstmsg + 1);
    else
        msgBuff->msgcount++;
}

/**
 * Removes the oldest message.
 */
static void HUMsg_DropLast(messagebuffer_t *msgBuff)
{
    if(!msgBuff->msgcount)
        return;

    msgBuff->firstmsg = IN_RANGE(msgBuff->firstmsg + 1);

    if(msgBuff->messages[msgBuff->firstmsg].time < 10)
        msgBuff->messages[msgBuff->firstmsg].time = 10;
    msgBuff->msgcount--;
}

static void HUMsg_ClearBuffer(messagebuffer_t *msgBuff)
{
    // The message display is empty.
    msgBuff->firstmsg = msgBuff->lastmsg = 0;
    msgBuff->msgcount = 0;
}

static void HUMsg_RefreshBuffer(messagebuffer_t *msgBuff)
{
    msgBuff->message_on = true;
    msgBuff->message_counter = HU_MSGTIMEOUT;
}

/**
 * Sends a string to other player(s) as a chat message.
 */
static void HUMsg_SendMessage(char *msg)
{
    char    buff[256];
    int     i;

    strcpy(lastmessage, msg);

    if(chat_to == HU_BROADCAST)
    {   // Send the message to the other players explicitly,
        strcpy(buff, "chat ");
        M_StrCatQuoted(buff, msg);
        DD_Execute(buff, false);
    }
    else
    {   // Send to all of the destination color.
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].plr->ingame && cfg.PlayerColor[i] == chat_to)
            {
                sprintf(buff, "chatNum %d ", i);
                M_StrCatQuoted(buff, msg);
                DD_Execute(buff, false);
            }
    }
#if __WOLFTC__
    if(gamemode == commercial)
        S_LocalSound(sfx_hudms1, 0);
    else
        S_LocalSound(sfx_hudms2, 0);
#elif __JDOOM__
    if(gamemode == commercial)
        S_LocalSound(sfx_radio, 0);
    else
        S_LocalSound(sfx_tink, 0);
#endif
}

/**
 * Sets the chat buffer to a chat macro string.
 */
static boolean HUMsg_SendMacro(int num)
{
    if(!chat_on)
        return false;

    if(num >= 0 && num < 9)
    {   // leave chat mode and notify that it was sent
        if(chat_on)
            HUMsg_CloseChat();

        HUMsg_SendMessage(cfg.chat_macros[num]);
        return true;
    }

    return false;
}

/**
 * Handles controls (console commands) for the message
 * buffer and chat widget
 */
DEFCC(CCmdMsgAction)
{
    int chattarget;

    if(chat_on)
    {
        if(!stricmp(argv[0], "chatcomplete"))  // send the message
        {
            HUMsg_CloseChat();
            if(w_chat.l.len)
            {
                HUMsg_SendMessage(w_chat.l.l);
            }
        }
        else if(!stricmp(argv[0], "chatcancel"))  // close chat
        {
            HUMsg_CloseChat();
        }
        else if(!stricmp(argv[0], "chatdelete"))
        {
            HUlib_delCharFromIText(&w_chat);
        }
    }

    if(!stricmp(argv[0], "chatsendmacro"))  // send a chat macro
    {
        if(argc < 2 || argc > 3)
        {
            Con_Message("Usage: %s (player) (macro number)\n", argv[0]);
            Con_Message("Send a chat macro to other player(s) in multiplayer.\n"
                        "If (player) is omitted, the message will be sent to all players.\n");
            return true;
        }

        if(!chat_on) // we need to enable chat mode first...
        {
            if(IS_NETGAME)
            {
                Con_Message("You can't chat if not in multiplayer\n");
                return false;
            }

            if(argc == 3)
            {
                chattarget = atoi(argv[1]);
                if(chattarget < 0 || chattarget > 3)
                {
                    // Bad destination.
                    Con_Message("Invalid player number \"%i\". Should be 0-3\n",
                                chattarget);
                    return false;
                }
            }
            else
                chattarget = HU_BROADCAST;

            HUMsg_OpenChat(chattarget);
        }

        if(!(HUMsg_SendMacro(atoi(((argc == 3)? argv[2] : argv[1])))))
        {
            Con_Message("invalid macro number\n");
            return false;
        }
    }
    else if(!stricmp(argv[0], "msgrefresh")) // refresh the message buffer
    {
        int i;

        if(chat_on)
            return false;

        for(i = 0; i < MAXPLAYERS; ++i)
            HUMsg_RefreshBuffer(&msgBuffer[i]);
    }
    else if(!stricmp(argv[0], "beginchat")) // begin chat mode
    {
        if(!IS_NETGAME)
        {
            Con_Message("You can't chat if not in multiplayer\n");
            return false;
        }

        if(chat_on)
            return false;

        if(argc == 2)
        {
            chattarget = atoi(argv[1]);
            if(chattarget < 0 || chattarget > 3)
            {
                // Bad destination.
                Con_Message("Invalid player number \"%i\". Should be 0-3\n",
                            chattarget);
                return false;
            }
        }
        else
            chattarget = HU_BROADCAST;

        HUMsg_OpenChat(chattarget);
    }

    return true;
}

/**
 * Display a local game message.
 */
DEFCC(CCmdLocalMessage)
{
    if(argc != 2)
    {
        Con_Printf("%s (msg)\n", argv[0]);
        return true;
    }
    D_NetMessageNoSound(argv[1]);
    return true;
}
