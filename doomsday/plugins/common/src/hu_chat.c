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

#include <assert.h>
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

#include "hu_chat.h"

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

typedef struct {
    boolean active;
    boolean shiftDown;
    int to; // 0=all, 1=player 0, etc.
    hu_text_t buffer;
} uiwidget_chat_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(MsgAction);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void     closeChat(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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

cvartemplate_t chatCVars[] = {
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
ccmdtemplate_t chatCCmds[] = {
    {"chatcancel",      "",     CCmdMsgAction},
    {"chatcomplete",    "",     CCmdMsgAction},
    {"chatdelete",      "",     CCmdMsgAction},
    {"chatsendmacro",   NULL,   CCmdMsgAction},
    {"beginchat",       NULL,   CCmdMsgAction},
    {NULL}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

uiwidget_chat_t chatWidgets[DDMAXPLAYERS];

// CODE --------------------------------------------------------------------

/**
 * Called during the PreInit of each game during start up.
 * Register Cvars and CCmds for the opperation/look of the (message) log.
 */
void Chat_Register(void)
{
    int i;

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
    int i;

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
    int i;

    // Create the chat widgets.
    for(i = 0; i < MAXPLAYERS; ++i)
    {
        Chat_Open(i, false);

        // Create the input buffers.
        HUlib_initText(&chatWidgets[i].buffer, 0, 0, &chatWidgets[i].active);
    }
}

void Chat_Open(int player, boolean open)
{
    assert(player >= 0 && player < DDMAXPLAYERS);
    {
    uiwidget_chat_t* chat = &chatWidgets[player];
    if(open)
    {
        chat->active = true;
        chat->to = player;

        HUlib_resetText(&chat->buffer);
        // Enable the chat binding class
        DD_Execute(true, "activatebcontext chat");
        return;
    }

    if(chat->active)
    {
        chat->active = false;
        // Disable the chat binding class
        DD_Execute(true, "deactivatebcontext chat");
    }
    }
}

boolean Chat_Responder(event_t* ev)
{
    int player = CONSOLEPLAYER;
    uiwidget_chat_t* chat = &chatWidgets[player];
    boolean eatkey = false;
    unsigned char c;

    if(!chat->active)
        return false;

    if(ev->type != EV_KEY)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
    {
        chat->shiftDown = (ev->state == EVS_DOWN || ev->state == EVS_REPEAT);
        return false;
    }

    if(ev->state != EVS_DOWN)
        return false;

    c = (unsigned char) ev->data1;

    if(chat->shiftDown)
        c = shiftXForm[c];

    eatkey = HUlib_keyInText(&chat->buffer, c);

    return eatkey;
}

void Chat_Drawer(int player, float textAlpha, float iconAlpha,
    int* drawnWidth, int* drawnHeight)
{
    assert(player >= 0 && player < DDMAXPLAYERS);
    {
    uiwidget_chat_t* chat = &chatWidgets[player];
    hu_textline_t* l = &chat->buffer.l;
    char buf[HU_MAXLINELENGTH+1];
    const char* str;
    int xOffset = 0;
    byte textFlags;

    if(!*chat->buffer.on)
        return;

    FR_SetFont(FID(GF_FONTA));
    if(actualMapTime & 12)
    {
        dd_snprintf(buf, HU_MAXLINELENGTH+1, "%s_", chat->buffer.l.l);
        str = buf;
        if(cfg.msgAlign == 1)
            xOffset = FR_CharWidth('_')/2;
    }
    else
    {
        str = chat->buffer.l.l;
        if(cfg.msgAlign == 2)
            xOffset = -FR_CharWidth('_');
    }
    textFlags = DTF_ALIGN_TOP|DTF_NO_EFFECTS | ((cfg.msgAlign == 0)? DTF_ALIGN_LEFT : (cfg.msgAlign == 2)? DTF_ALIGN_RIGHT : 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_DrawText(str, xOffset, 0, FID(GF_FONTA), textFlags, .5f, 0, cfg.hudColor[CR], cfg.hudColor[CG], cfg.hudColor[CB], textAlpha, 0, 0, false);

    DGL_Disable(DGL_TEXTURE_2D);

    *drawnWidth = FR_TextWidth(chat->buffer.l.l, FID(GF_FONTA)) + FR_CharWidth('_');
    *drawnHeight = MAX_OF(FR_TextHeight(chat->buffer.l.l, FID(GF_FONTA)), FR_CharHeight('_'));
    }
}

/**
 * Sends a string to other player(s) as a chat message.
 */
static void sendMessage(int player, const char* msg)
{
    uiwidget_chat_t* chat = &chatWidgets[player];
    char buff[256];
    int i;

    if(chat->to == 0)
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
            M_StrCatQuoted(buff, msg, 256);
            DD_Execute(false, buff);
        }
    }
    else
    {   // Send to all of the destination color.
        chat->to -= 1;

        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame && cfg.playerColor[i] == chat->to)
            {
                if(!IS_NETGAME)
                {   // Send it locally.
                    D_NetMessageNoSound(i, msg);
                }
                else
                {
                    sprintf(buff, "chatNum %d ", i);
                    M_StrCatQuoted(buff, msg, 256);
                    DD_Execute(false, buff);
                }
            }
    }

#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        S_LocalSound(SFX_RADIO, 0);
    else
        S_LocalSound(SFX_TINK, 0);
#elif __JDOOM64__
    S_LocalSound(SFX_RADIO, 0);
#endif
}

boolean Chat_IsActive(int player)
{
    assert(player >= 0 && player < DDMAXPLAYERS);
    {
    uiwidget_chat_t* chat = &chatWidgets[player];
    return chat->active;
    }
}

/**
 * Sets the chat buffer to a chat macro string.
 */
static boolean sendMacro(int player, int num)
{
    if(num >= 0 && num < 9)
    {   // Leave chat mode and notify that it was sent.
        if(chatWidgets[player].active)
            Chat_Open(player, false);

        sendMessage(player, cfg.chatMacros[num]);
        return true;
    }

    return false;
}

/**
 * Handles controls (console commands) for the chat widget.
 */
D_CMD(MsgAction)
{
    int player = CONSOLEPLAYER;
    uiwidget_chat_t* chat = &chatWidgets[player];
    int toPlayer;

    if(G_GetGameAction() == GA_QUIT)
        return false;

    if(chat->active)
    {
        if(!stricmp(argv[0], "chatcomplete"))  // send the message
        {
            Chat_Open(player, false);
            if(chat->buffer.l.len)
            {
                sendMessage(player, chat->buffer.l.l);
            }
        }
        else if(!stricmp(argv[0], "chatcancel"))  // close chat
        {
            Chat_Open(player, false);
        }
        else if(!stricmp(argv[0], "chatdelete"))
        {
            HUlib_delCharFromText(&chat->buffer);
        }
    }

    if(!stricmp(argv[0], "chatsendmacro"))  // send a chat macro
    {
        int macroNum;

        if(argc < 2 || argc > 3)
        {
            Con_Message("Usage: %s (player) (macro number)\n", argv[0]);
            Con_Message("Send a chat macro to other player(s).\n"
                        "If (player) is omitted, the message will be sent to all players.\n");
            return true;
        }

        if(argc == 3)
        {
            toPlayer = atoi(argv[1]);
            if(toPlayer < 0 || toPlayer > 3)
            {
                // Bad destination.
                Con_Message("Invalid player number \"%i\". Should be 0-3\n", toPlayer);
                return false;
            }

            toPlayer = toPlayer + 1;
        }
        else
            toPlayer = 0;

        macroNum = atoi(((argc == 3)? argv[2] : argv[1]));
        if(!sendMacro(player, macroNum))
        {
            Con_Message("Invalid macro number\n");
            return false;
        }
    }
    else if(!stricmp(argv[0], "beginchat")) // begin chat mode
    {
        if(chat->active)
            return false;

        if(argc == 2)
        {
            toPlayer = atoi(argv[1]);
            if(toPlayer < 0 || toPlayer > 3)
            {
                // Bad destination.
                Con_Message("Invalid player number \"%i\". Should be 0-3\n", toPlayer);
                return false;
            }

            toPlayer = toPlayer + 1;
        }
        else
            toPlayer = 0;

        Chat_Open(toPlayer, true);
    }

    return true;
}
