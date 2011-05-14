/**\file hu_chat.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include <assert.h>
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

#include "p_tick.h"
#include "d_net.h"
#include "g_common.h"

#include "hu_chat.h"

cvartemplate_t chatCVars[] = {
    { "chat-macro0", 0, CVT_CHARPTR, &cfg.chatMacros[0], 0, 0 },
    { "chat-macro1", 0, CVT_CHARPTR, &cfg.chatMacros[1], 0, 0 },
    { "chat-macro2", 0, CVT_CHARPTR, &cfg.chatMacros[2], 0, 0 },
    { "chat-macro3", 0, CVT_CHARPTR, &cfg.chatMacros[3], 0, 0 },
    { "chat-macro4", 0, CVT_CHARPTR, &cfg.chatMacros[4], 0, 0 },
    { "chat-macro5", 0, CVT_CHARPTR, &cfg.chatMacros[5], 0, 0 },
    { "chat-macro6", 0, CVT_CHARPTR, &cfg.chatMacros[6], 0, 0 },
    { "chat-macro7", 0, CVT_CHARPTR, &cfg.chatMacros[7], 0, 0 },
    { "chat-macro8", 0, CVT_CHARPTR, &cfg.chatMacros[8], 0, 0 },
    { "chat-macro9", 0, CVT_CHARPTR, &cfg.chatMacros[9], 0, 0 },
    { "chat-beep",   0, CVT_BYTE,    &cfg.chatBeep, 0, 1 },
    { NULL }
};

ccmdtemplate_t chatCCmds[] = {
    { "beginchat",       NULL,   CCmdChatOpen },
    { "chatcancel",      "",     CCmdChatAction },
    { "chatcomplete",    "",     CCmdChatAction },
    { "chatdelete",      "",     CCmdChatAction },
    { "chatsendmacro",   NULL,   CCmdChatSendMacro },
    { NULL }
};

static uidata_chat_t chatWidgets[DDMAXPLAYERS];

/// @return  UIChat widget associated to local @a player else fatal error.
static uidata_chat_t* widgetForLocalPlayer(int player)
{
    if(player >= 0 && player < DDMAXPLAYERS)
        return chatWidgets + player;
    Con_Error("widgetForLocalPlayer: Invalid player #%i.", player);
    exit(1); // Unreachable.
}

void Chat_Register(void)
{
    int i;
    for(i = 0; chatCVars[i].name; ++i)
        Con_AddVariable(chatCVars + i);
    for(i = 0; chatCCmds[i].name; ++i)
        Con_AddCommand(chatCCmds + i);
}

static void clearInputBuffer(uidata_chat_t* chat)
{
    assert(NULL != chat);
    chat->buffer.length = 0;
    chat->buffer.text[0] = '\0';
}

boolean UIChat_IsActive(uidata_chat_t* chat)
{
    assert(NULL != chat);
    return (0 != (chat->flags & UICF_ACTIVE));
}

boolean UIChat_Activate(uidata_chat_t* chat, boolean yes)
{
    assert(NULL != chat);
    {
    boolean oldActive = (chat->flags & UICF_ACTIVE) != 0;
    if(chat->flags & UICF_ACTIVE)
    {
        if(!yes)
        {
            chat->flags &= ~UICF_ACTIVE;
        }
    }
    else if(yes)
    {
        chat->flags |= UICF_ACTIVE;
        // Default destinition is "global".
        UIChat_SetDestination(chat, 0);
        UIChat_Clear(chat);
    }
    return oldActive != ((chat->flags & UICF_ACTIVE) != 0);
    }
}

int UIChat_Destination(uidata_chat_t* chat)
{
    assert(NULL != chat);
    return chat->destination;
}

void UIChat_SetDestination(uidata_chat_t* chat, int destination)
{
    assert(NULL != chat);
    if(destination < 0 || destination > NUMTEAMS) return;
    chat->destination = destination;
}

boolean UIChat_SetShiftModifier(uidata_chat_t* chat, boolean on)
{
    assert(NULL != chat);
    {
    boolean oldShiftDown = chat->buffer.shiftDown;
    chat->buffer.shiftDown = on;
    return oldShiftDown != chat->buffer.shiftDown;
    }
}

boolean UIChat_AppendCharacter(uidata_chat_t* chat, char ch)
{
    assert(NULL != chat);

    if(chat->buffer.length == UICHAT_INPUTBUFFER_MAXLENGTH)
    {
        return false;
    }

    if(chat->buffer.shiftDown)
    {
        ch = shiftXForm[ch];
    }

    if(ch < ' ' || ch > 'z')
        return false;

    chat->buffer.text[chat->buffer.length++] = ch;
    chat->buffer.text[chat->buffer.length] = '\0';
    return true;
}

void UIChat_DeleteLastCharacter(uidata_chat_t* chat)
{
    assert(NULL != chat);
    if(0 == chat->buffer.length) return;
    chat->buffer.text[--chat->buffer.length] = 0;
}

void UIChat_Clear(uidata_chat_t* chat)
{
    clearInputBuffer(chat);
}

const char* UIChat_Text(uidata_chat_t* chat)
{
    assert(NULL != chat);
    return chat->buffer.text;
}

size_t UIChat_TextLength(uidata_chat_t* chat)
{
    assert(NULL != chat);
    return chat->buffer.length;
}

boolean UIChat_TextIsEmpty(uidata_chat_t* chat)
{
    return (0 == UIChat_TextLength(chat));
}

static void playMessageSentSound(void)
{
#if __JDOOM__
    if(gameModeBits & GM_ANY_DOOM2)
        S_LocalSound(SFX_RADIO, 0);
    else
        S_LocalSound(SFX_TINK, 0);
#elif __JDOOM64__
    S_LocalSound(SFX_RADIO, 0);
#endif
}

static void sendMessage(int player, int destination, const char* msg)
{
    char buff[256];

    if(destination == 0)
    {   // Send the message to the other players explicitly,
        if(!IS_NETGAME)
        {   // Send it locally.
            int i;
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
    {   // Send to all on the same team (team = color).
        int i;
        destination -= 1;

        for(i = 0; i < MAXPLAYERS; ++i)
            if(players[i].plr->inGame && cfg.playerColor[i] == destination)
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
    playMessageSentSound();
}

static int parseDestination(const char* str)
{
    if(str && str[0])
    {
        int dest = atoi(str);
        if(dest >= 0 && dest <= NUMTEAMS)
        {
            return dest;
        }
    }
    return -1;
}

static int parseMacroId(const char* str)
{
    if(str && str[0])
    {
        int id = atoi(str);
        if(id >= 0 && id < 9)
        {
            return id;
        }
    }
    return -1;
}

void Chat_Init(void)
{
    Chat_LoadResources();
}

void Chat_Shutdown(void)
{
    // Stub.
}

void Chat_LoadResources(void)
{
    // Retrieve the chat macro strings if not already set.
    int i;
    for(i = 0; i < 10; ++i)
    {
        if(cfg.chatMacros[i]) continue;
        cfg.chatMacros[i] = GET_TXT(TXT_HUSTR_CHATMACRO0 + i);
    }
}

void Chat_Open(int player, boolean open)
{
    uidata_chat_t* chat = widgetForLocalPlayer(player);
    if(UIChat_Activate(chat, open))
    {
        DD_Executef(true, "%s chat", UIChat_IsActive(chat)? "activatebcontext" : "deactivatebcontext");
    }
}

void Chat_Start(int player)
{
    // Stub.
}

int Chat_Responder(int player, event_t* ev)
{
    uidata_chat_t* chat = widgetForLocalPlayer(player);

    if(!UIChat_IsActive(chat))
        return false;

    if(ev->type != EV_KEY)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
    {
        UIChat_SetShiftModifier(chat, (ev->state == EVS_DOWN || ev->state == EVS_REPEAT));
        return false; // Never eaten.
    }

    if(!(ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        return false;

    if(ev->data1 == DDKEY_BACKSPACE)
    {
        UIChat_DeleteLastCharacter(chat);
        return true;
    }

    return UIChat_AppendCharacter(chat, (char)ev->data1);
}

static void drawWidget(uidata_chat_t* chat, float textAlpha, float iconAlpha)
{
    assert(NULL != chat);
    {
    const char* text = UIChat_Text(chat);
    char buf[UICHAT_INPUTBUFFER_MAXLENGTH+1];
    short textFlags = DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS;
    int xOffset, textWidth, cursorWidth;

    FR_SetFont(FID(GF_FONTA));
    textWidth = FR_TextWidth(text, FID(GF_FONTA));
    cursorWidth = FR_CharWidth('_');

    if(cfg.msgAlign == 0)
        xOffset = 0;
    else if(cfg.msgAlign == 1)
        xOffset = -(textWidth + cursorWidth)/2;
    else if(cfg.msgAlign == 2)
        xOffset = -(textWidth + cursorWidth);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText(text, xOffset, 0, FID(GF_FONTA), textFlags, .5f, 0, cfg.hudColor[CR], cfg.hudColor[CG], cfg.hudColor[CB], textAlpha, 0, 0, false);
    if(actualMapTime & 12)
    {
        DGL_Color4f(cfg.hudColor[CR], cfg.hudColor[CG], cfg.hudColor[CB], textAlpha);
        FR_DrawChar('_', xOffset + textWidth, 0);
    }
    DGL_Disable(DGL_TEXTURE_2D);
    }
}

static void calcWidgetDimensions(uidata_chat_t* chat, int* width, int* height)
{
    assert(NULL != chat);
    {
    const char* text = UIChat_Text(chat);
    FR_SetFont(FID(GF_FONTA));
    if(NULL != width)  *width  = FR_TextWidth(text, FID(GF_FONTA)) + FR_CharWidth('_');
    if(NULL != height) *height = MAX_OF(FR_TextHeight(text, FID(GF_FONTA)), FR_CharHeight('_'));
    }
}

void Chat_Drawer(int player, float textAlpha, float iconAlpha)
{
    uidata_chat_t* chat = widgetForLocalPlayer(player);

    if(!UIChat_IsActive(chat))
        return;

    drawWidget(chat, textAlpha, iconAlpha);
}

void Chat_Dimensions(int player, int* width, int* height)
{
    uidata_chat_t* chat = widgetForLocalPlayer(player);

    if(NULL != width)  *width  = 0;
    if(NULL != height) *height = 0;

    if(!UIChat_IsActive(chat))
        return;

    calcWidgetDimensions(chat, width, height);
}

boolean Chat_IsActive(int player)
{
    return UIChat_IsActive(widgetForLocalPlayer(player));
}

D_CMD(ChatOpen)
{
    int player = CONSOLEPLAYER, destination = 0;

    if(G_GetGameAction() == GA_QUIT)
        return false;

    Chat_Open(player, true);

    if(argc == 2)
    {
        destination = parseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0...%i).\n", destination, NUMTEAMS);
            return false;
        }
    }
    UIChat_SetDestination(widgetForLocalPlayer(player), destination);

    return true;
}

D_CMD(ChatSendMacro)
{
    int player = CONSOLEPLAYER, macroId, destination = 0;

    if(G_GetGameAction() == GA_QUIT)
        return false;

    if(argc < 2 || argc > 3)
    {
        Con_Message("Usage: %s (team) (macro number)\n", argv[0]);
        Con_Message("Send a chat macro to other player(s).\n"
                    "If (team) is omitted, the message will be sent to all players.\n");
        return true;
    }

    if(argc == 3)
    {
        destination = parseDestination(argv[1]);
        if(destination < 0)
        {
            Con_Message("Invalid team number #%i (valid range: 0...%i).\n", destination, NUMTEAMS);
            return false;
        }
    }

    macroId = parseMacroId(argc == 3? argv[2] : argv[1]);
    if(-1 == macroId)
    {
        Con_Message("Invalid macro id\n");
        return false;
    }

    sendMessage(player, destination, cfg.chatMacros[macroId]);
    Chat_Open(player, false);
    return true;
}

D_CMD(ChatAction)
{
    int player = CONSOLEPLAYER;
    const char* cmd = argv[0] + 4;

    if(G_GetGameAction() == GA_QUIT) return false;
    if(!Chat_IsActive(player)) return false;

    if(!stricmp(cmd, "complete")) // Send the message.
    {
        uidata_chat_t* chat = widgetForLocalPlayer(player);
        if(!UIChat_TextIsEmpty(chat))
        {
            sendMessage(player, UIChat_Destination(chat), UIChat_Text(chat));
        }
        Chat_Open(player, false);
    }
    else if(!stricmp(cmd, "cancel")) // Close chat.
    {
        Chat_Open(player, false);
    }
    else if(!stricmp(cmd, "delete"))
    {
        UIChat_DeleteLastCharacter(widgetForLocalPlayer(player));
    }
    return true;
}
