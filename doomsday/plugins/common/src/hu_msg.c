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

#ifdef __JDOOM__
# include "doomdef.h"
# include "d_config.h"
# include "m_menu.h"
# include "Mn_def.h"
# include "s_sound.h"
# include "doomstat.h"
# include "r_local.h"
# include "p_local.h"
# include "m_ctrl.h"
# include "dstrings.h"  // Data.
#elif __JHERETIC__
# include "doomdef.h"
# include "h_config.h"
# include "m_menu.h"
# include "mn_def.h"
# include "soundst.h"
# include "h_stat.h"
# include "r_local.h"
# include "p_local.h"
# include "m_ctrl.h"
# include "dstrings.h"  // Data.
#elif __JHEXEN__
# include "h2def.h"
# include "x_config.h"
# include "mn_def.h"
# include "sounds.h"
# include "r_local.h"
# include "p_local.h"
# include "m_ctrl.h"
# include "textdefs.h"  // Data.
#elif __JSTRIFE__
# include "h2def.h"
# include "d_config.h"
# include "mn_def.h"
# include "sounds.h"
# include "r_local.h"
# include "p_local.h"
# include "m_ctrl.h"
# include "textdefs.h"  // Data.
#endif

#include "hu_stuff.h"
#include "hu_msg.h"
#include "hu_lib.h"
#include "g_common.h"
#include "d_net.h"

// MACROS ------------------------------------------------------------------

#define HU_INPUTX   HU_MSGX
#define HU_INPUTY   (HU_MSGY + HU_MSGHEIGHT*(SHORT(hu_font[0].height) +1))

#define FLASHFADETICS  35

// TYPES -------------------------------------------------------------------

typedef struct {
    char    text[MAX_LINELEN];
    int     time;
    int     duration; // time when posted.
} message_t;

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

void    HUMsg_DropLast(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int actual_leveltime;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean shiftdown = false;
boolean chat_on;
boolean message_dontfuckwithme;
boolean message_noecho;

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

static message_t messages[MAX_MESSAGES];

static player_t *plr;

static boolean message_on;
static boolean message_nottobefuckedwith;

static int firstmsg, lastmsg, msgcount = 0;
static float yoffset = 0; // Scroll-up offset.

static int chat_to = 0; // 0=all, 1=player 0, etc.

static char lastmessage[HU_MAXLINELENGTH + 1];

static hu_itext_t w_chat;
static boolean always_off = false;

static hu_itext_t w_inputbuffer[MAXPLAYERS];

static hu_stext_t w_message;
static int message_counter;

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
    {"msg-show", 0, CVT_BYTE, &cfg.msgShow, 0, 1,
        "1=Show messages."},

    // Behaviour
    {"msg-echo", 0, CVT_BYTE, &cfg.echoMsg, 0, 1,
        "1=Echo all messages to the console."},

#if !__JHEXEN__
    {"msg-secret", 0, CVT_BYTE, &cfg.secretMsg, 0, 1,
        "1=Announce the discovery of secret areas."},
#endif

    {"msg-count", 0, CVT_INT, &cfg.msgCount, 0, 8,
        "Number of HUD messages displayed at the same time."},

    {"msg-uptime", CVF_NO_MAX, CVT_INT, &cfg.msgUptime, 35, 0,
        "Number of tics to keep HUD messages on screen."},

#if __JHEXEN__
    {"msg-hub-override", 0, CVT_BYTE, &cfg.overrideHubMsg, 0, 2,
        "Override the transition hub message."},
#endif

    // Display style
    {"msg-scale", CVF_NO_MAX, CVT_FLOAT, &cfg.msgScale, 0, 0,
        "Scaling factor for HUD messages."},

    {"msg-align", 0, CVT_INT, &cfg.msgAlign, 0, 2,
        "Alignment of HUD messages. 0 = left, 1 = center, 2 = right."},

    // Colour (default, individual message may have built-in colour settings)
    {"msg-color-r", 0, CVT_FLOAT, &cfg.msgColor[0], 0, 1,
        "Color of HUD messages red component."},
    {"msg-color-g", 0, CVT_FLOAT, &cfg.msgColor[1], 0, 1,
        "Color of HUD messages green component."},
    {"msg-color-b", 0, CVT_FLOAT, &cfg.msgColor[2], 0, 1,
        "Color of HUD messages blue component."},

    {"msg-blink", CVF_NO_MAX, CVT_INT, &cfg.msgBlink, 0, 0,
        "HUD messages blink for this number of tics when printed."},

    // Chat macros
    {"chat-macro0", 0, CVT_CHARPTR, &cfg.chat_macros[0], 0, 0, "Chat macro 1."},
    {"chat-macro1", 0, CVT_CHARPTR, &cfg.chat_macros[1], 0, 0, "Chat macro 2."},
    {"chat-macro2", 0, CVT_CHARPTR, &cfg.chat_macros[2], 0, 0, "Chat macro 3."},
    {"chat-macro3", 0, CVT_CHARPTR, &cfg.chat_macros[3], 0, 0, "Chat macro 4."},
    {"chat-macro4", 0, CVT_CHARPTR, &cfg.chat_macros[4], 0, 0, "Chat macro 5."},
    {"chat-macro5", 0, CVT_CHARPTR, &cfg.chat_macros[5], 0, 0, "Chat macro 6."},
    {"chat-macro6", 0, CVT_CHARPTR, &cfg.chat_macros[6], 0, 0, "Chat macro 7."},
    {"chat-macro7", 0, CVT_CHARPTR, &cfg.chat_macros[7], 0, 0, "Chat macro 8."},
    {"chat-macro8", 0, CVT_CHARPTR, &cfg.chat_macros[8], 0, 0, "Chat macro 9."},
    {"chat-macro9", 0, CVT_CHARPTR, &cfg.chat_macros[9], 0, 0, "Chat macro 10."},
    {NULL}
};

// Console commands for the message buffer
ccmd_t  msgCCmds[] = {
    {"chatcomplete",    CCmdMsgAction, "Send the chat message and exit chat mode.", 0 },
    {"chatdelete",      CCmdMsgAction, "Delete a character from the chat buffer.", 0},
    {"chatcancel",      CCmdMsgAction, "Exit chat mode without sending the message.", 0 },
    {"chatsendmacro",   CCmdMsgAction, "Send a chat macro.", 0 },
    {"beginchat",       CCmdMsgAction, "Begin chat mode.", 0 },
    {"msgrefresh",      CCmdMsgAction, "Show last HUD message.", 0 },
    {"message",      CCmdLocalMessage, "Show a local game message.", 0 },
    {NULL}
};

// CODE --------------------------------------------------------------------

/*
 * Called during the PreInit of each game during start up
 * Register Cvars and CCmds for the opperation/look of the
 * message buffer and chat widget.
 */
void HUMsg_Register(void)
{
    int     i;

    for(i = 0; msgCVars[i].name; i++)
        Con_AddVariable(msgCVars + i);
    for(i = 0; msgCCmds[i].name; i++)
        Con_AddCommand(msgCCmds + i);
}

/*
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
    for(i=0; i<sizeof(x_idx)/sizeof(int); i++) \
        x[i] = x_idx[i]==-1? "NEWLEVEL" : GET_TXT(x_idx[i]);

    INIT_STRINGS(player_names, player_names_idx);
}

void HUMsg_OpenChat(int chattarget)
{
    chat_on = true;
    chat_to = chattarget;

    HUlib_resetIText(&w_chat);

    // Enable the chat binding class
    DD_SetBindClass(GBC_CHAT, true);
}

void HUMsg_CloseChat(void)
{
    if(chat_on)
    {
        chat_on = false;
        // Disable the chat binding class
        DD_SetBindClass(GBC_CHAT, false);
    }
}

/*
 * Called by HU_Start()
 */
void HUMsg_Start(void)
{
    int     i;

    plr = &players[consoleplayer];
    message_on = false;
    message_dontfuckwithme = false;
    message_nottobefuckedwith = false;

    HUMsg_CloseChat();

    // create the message widget
    HUlib_initSText(&w_message, HU_MSGX, HU_MSGY, HU_MSGHEIGHT, hu_font_a,
                    HU_FONTSTART, &message_on);

    // create the chat widget
    HUlib_initIText(&w_chat, HU_INPUTX, HU_INPUTY, hu_font_a, HU_FONTSTART,
                    &chat_on);

    // create the inputbuffer widgets
    for(i = 0; i < MAXPLAYERS; i++)
        HUlib_initIText(&w_inputbuffer[i], 0, 0, 0, 0, &always_off);
}

/*
 * Called by HU_ticker()
 */
void HUMsg_Ticker(void)
{
    int     i;

    // Countdown to scroll-up.
    for(i = 0; i < MAX_MESSAGES; i++)
        messages[i].time--;

    if(msgcount)
    {
        yoffset = 0;
        if(messages[firstmsg].time >= 0 &&
           messages[firstmsg].time <= LINEHEIGHT_A)
        {
            yoffset = LINEHEIGHT_A - messages[firstmsg].time;
        }
        else if(messages[firstmsg].time < 0)
            HUMsg_DropLast();
    }

    // tick down message counter if message is up
    if(message_counter && !--message_counter)
    {
        message_on = false;
        message_nottobefuckedwith = false;
    }

    if(cfg.msgShow || message_dontfuckwithme)
    {
        // display message if necessary
        if((plr->message && !message_nottobefuckedwith) ||
           (plr->message && message_dontfuckwithme))
        {
            HUMsg_Message(plr->message, plr->messageTics);

            Z_Free(plr->message);
            plr->message = NULL;

            message_on = true;
            message_counter = HU_MSGTIMEOUT;
            message_nottobefuckedwith = message_dontfuckwithme;
            message_dontfuckwithme = 0;
        }

    }

    message_noecho = false;
}

void HUMsg_Drawer(void)
{
    int     i, m, num, y, lh = LINEHEIGHT_A, td, x;
    int     msgTics;
    float   col[4];

    // Don't draw the messages when the leveltitle is up
    if(cfg.levelTitle && actual_leveltime < 6 * 35)
        return;

    // How many messages should we print?
    num = msgcount;

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
    gl.Translatef(0, -yoffset, 0);

    // First 'num' messages starting from the last one.
    for(y = (num - 1) * lh, m = IN_RANGE(lastmsg - 1); num;
        y -= lh, num--, m = IN_RANGE(m - 1))
    {
        // Set colour
        memcpy(col, cfg.msgColor, sizeof(cfg.msgColor));
        // Set alpha
        col[3] = 1;

        td = cfg.msgUptime - messages[m].time;
        msgTics = messages[m].duration - messages[m].time;
        if(cfg.msgBlink && msgTics < cfg.msgBlink && (td & 2))
        {   // Flash color
            col[0] = col[1] = col[2] = 1;
        }
        else if(cfg.msgBlink && msgTics < cfg.msgBlink + FLASHFADETICS &&
                msgTics >= cfg.msgBlink)
        {   // Fade color to normal
            for(i = 0; i < 3; ++i)
                col[i] += (((1.0f - col[i]) / FLASHFADETICS) *
                            (cfg.msgBlink + FLASHFADETICS - msgTics));
        }
        else
        {   // Fade alpha out
            if(m == firstmsg && messages[m].time <= LINEHEIGHT_A)
                col[3] = messages[m].time / (float) LINEHEIGHT_A *0.9f;
        }

        // draw using param text
        // messages may use the params to override
        // eg colour (Hexen's important messages)
        WI_DrawParamText(x, 1 + y, messages[m].text, hu_font_a,
                         col[0], col[1], col[2], col[3], false, false,
                         cfg.msgAlign);
    }

    Draw_EndZoom();

    HUlib_drawIText(&w_chat);
}

boolean HU_Responder(event_t *ev)
{
    boolean eatkey = false;
    unsigned char c;

    if(gamestate != GS_LEVEL || !chat_on)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
    {
        shiftdown = ev->type == ev_keydown || ev->type == ev_keyrepeat;
        return false;
    }

    if(ev->type != ev_keydown && ev->type != ev_keyrepeat)
        return false;

    c = (unsigned char) ev->data1;

    if(shiftdown || (c >= 'a' && c <= 'z'))
        c = english_shiftxform[c];

    eatkey = HUlib_keyInIText(&w_chat, c);

    return eatkey;
}

/*
 * Add a new message.
 */
void HUMsg_Message(char *msg, int msgtics)
{
    messages[lastmsg].time = messages[lastmsg].duration =
        cfg.msgUptime + msgtics;

    strcpy(messages[lastmsg].text, msg);
    lastmsg = IN_RANGE(lastmsg + 1);
    if(msgcount == MAX_MESSAGES)
        firstmsg = lastmsg;
    else if(msgcount == cfg.msgCount)
        firstmsg = IN_RANGE(firstmsg + 1);
    else
        msgcount++;
}

/*
 * Removes the oldest message.
 */
void HUMsg_DropLast(void)
{
    if(!msgcount)
        return;
    firstmsg = IN_RANGE(firstmsg + 1);
    if(messages[firstmsg].time < 10)
        messages[firstmsg].time = 10;
    msgcount--;
}

/*
 * Clears the message buffer.
 */
void HUMsg_Clear(void)
{
    // The message display is empty.
    firstmsg = lastmsg = 0;
    msgcount = 0;
}

void HU_Erase(void)
{
    HUlib_eraseSText(&w_message);
    HUlib_eraseIText(&w_chat);
}

/*
 * Sends a string to other player(s) as a chat message.
 */
void HUMsg_SendMessage(char *msg)
{
    char    buff[256];
    int     i;

    strcpy(lastmessage, msg);
    // Send the message to the other players explicitly,
    // chatting is no more synchronized.
    if(chat_to == HU_BROADCAST)
    {
        strcpy(buff, "chat ");
        M_StrCatQuoted(buff, msg);
        DD_Execute(buff, false);
    }
    else
    {
        // Send to all of the destination color.
        for(i = 0; i < MAXPLAYERS; i++)
            if(players[i].plr->ingame && cfg.PlayerColor[i] == chat_to)
            {
                sprintf(buff, "chatNum %d ", i);
                M_StrCatQuoted(buff, msg);
                DD_Execute(buff, false);
            }
    }
#ifdef __JDOOM__
    if(gamemode == commercial)
        S_LocalSound(sfx_radio, 0);
    else
        S_LocalSound(sfx_tink, 0);
#endif
}

/*
 * Sets the chat buffer to a chat macro string.
 */
boolean HUMsg_SendMacro(int num)
{
    if(chat_on)
    {
        if(num >= 0 && num < 9)
        {
            // leave chat mode and notify that it was sent
            if(chat_on)
                HUMsg_CloseChat();

            HUMsg_SendMessage(cfg.chat_macros[num]);
        }
        else
            return false;
    }
    else
        return false;

    return true;
}

/*
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
            return true;;
        }
        else
        {
            if(!chat_on) // we need to enable chat mode first...
            {
                if(IS_NETGAME)
                {
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
                else
                {
                    Con_Message("You can't chat if not in multiplayer\n");
                    return false;
                }
            }

            if(!(HUMsg_SendMacro(atoi(((argc == 3)? argv[2] : argv[1])))))
            {
                Con_Message("invalid macro number\n");
                return false;
            }
        }
    }
    else if(!stricmp(argv[0], "msgrefresh")) // refresh the message buffer
    {
        if(chat_on)
            return false;

        message_on = true;
        message_counter = HU_MSGTIMEOUT;
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

/*
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
