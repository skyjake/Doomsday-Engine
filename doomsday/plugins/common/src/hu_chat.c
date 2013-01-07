/**\file hu_chat.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

void UIChat_Register(void)
{
    int i;
    for(i = 0; chatCVars[i].path; ++i)
        Con_AddVariable(chatCVars + i);
}

void UIChat_LoadMacros(void)
{
    // Retrieve the chat macro strings if not already set.
    int i;
    for(i = 0; i < 10; ++i)
    {
        if(cfg.chatMacros[i]) continue;
        cfg.chatMacros[i] = GET_TXT(TXT_HUSTR_CHATMACRO0 + i);
    }
}

static void clearInputBuffer(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    chat->buffer.length = 0;
    chat->buffer.text[0] = '\0';
    }
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

    if(NULL == msg || !msg[0])
        return;

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

boolean UIChat_IsActive(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    return (0 != (chat->flags & UICF_ACTIVE));
    }
}

boolean UIChat_Activate(uiwidget_t* obj, boolean yes)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
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
        UIChat_SetDestination(obj, 0);
        UIChat_Clear(obj);
    }
    if(oldActive != ((chat->flags & UICF_ACTIVE) != 0))
    {
        DD_Executef(true, "%s chat", UIChat_IsActive(obj)? "activatebcontext" : "deactivatebcontext");
        return true;
    }
    return false;
    }
}

int UIChat_Destination(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    return chat->destination;
    }
}

void UIChat_SetDestination(uiwidget_t* obj, int destination)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    if(destination < 0 || destination > NUMTEAMS) return;
    chat->destination = destination;
    }
}

boolean UIChat_SetShiftModifier(uiwidget_t* obj, boolean on)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    boolean oldShiftDown = chat->buffer.shiftDown;
    chat->buffer.shiftDown = on;
    return oldShiftDown != chat->buffer.shiftDown;
    }
}

boolean UIChat_AppendCharacter(uiwidget_t* obj, char ch)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;

    if(chat->buffer.length == UICHAT_INPUTBUFFER_MAXLENGTH)
    {
        return false;
    }

    if(ch < ' ' || ch > 'z')
        return false;

    if(chat->buffer.shiftDown)
    {
        ch = shiftXForm[(unsigned)ch];
    }

    chat->buffer.text[chat->buffer.length++] = ch;
    chat->buffer.text[chat->buffer.length] = '\0';
    return true;
    }
}

void UIChat_DeleteLastCharacter(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    if(0 == chat->buffer.length) return;
    chat->buffer.text[--chat->buffer.length] = 0;
    }
}

void UIChat_Clear(uiwidget_t* obj)
{
    clearInputBuffer(obj);
}

const char* UIChat_Text(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    return chat->buffer.text;
    }
}

size_t UIChat_TextLength(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    return chat->buffer.length;
    }
}

boolean UIChat_TextIsEmpty(uiwidget_t* obj)
{
    return (0 == UIChat_TextLength(obj));
}

const char* UIChat_FindMacro(uiwidget_t* obj, int macroId)
{
    if(macroId < 0 || macroId >= 10) return NULL;
    return cfg.chatMacros[macroId];
}

boolean UIChat_LoadMacro(uiwidget_t* obj, int macroId)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    {
    guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    const char* macro = UIChat_FindMacro(obj, macroId);
    if(NULL != macro)
    {
        strncpy(chat->buffer.text, macro, UICHAT_INPUTBUFFER_MAXLENGTH);
        chat->buffer.text[UICHAT_INPUTBUFFER_MAXLENGTH] = '\0';
        chat->buffer.length = strlen(chat->buffer.text);
    }
    return false;
    }
}

int UIChat_Responder(uiwidget_t* obj, event_t* ev)
{
    assert(NULL != obj && obj->type == GUI_CHAT);

    if(!UIChat_IsActive(obj))
        return false;

    if(ev->type != EV_KEY)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
    {
        UIChat_SetShiftModifier(obj, (ev->state == EVS_DOWN || ev->state == EVS_REPEAT));
        return false; // Never eaten.
    }

    if(!(ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        return false;

    if(ev->data1 == DDKEY_BACKSPACE)
    {
        UIChat_DeleteLastCharacter(obj);
        return true;
    }

    return UIChat_AppendCharacter(obj, (char)ev->data1);
}

int UIChat_CommandResponder(uiwidget_t* obj, menucommand_e cmd)
{
    assert(NULL != obj && obj->type == GUI_CHAT);
    if(!UIChat_IsActive(obj)) return false;

    switch(cmd)
    {
    case MCMD_SELECT:  // Send the message.
        if(!UIChat_TextIsEmpty(obj))
        {
            sendMessage(UIWidget_Player(obj), UIChat_Destination(obj), UIChat_Text(obj));
        }
        UIChat_Activate(obj, false);
        return true;
    case MCMD_CLOSE:
    case MCMD_NAV_OUT: // Close chat.
        UIChat_Activate(obj, false);
        return true;
    case MCMD_DELETE:
        UIChat_DeleteLastCharacter(obj);
        return true;
    default:
        return false;
    }
}

void UIChat_Drawer(uiwidget_t* obj, const Point2Raw* offset)
{
    //guidata_chat_t* chat = (guidata_chat_t*)obj->typedata;
    const float textAlpha = uiRendState->pageAlpha * cfg.hudColor[3];
    //const float iconAlpha = uiRendState->pageAlpha * cfg.hudIconAlpha;
    const char* text = UIChat_Text(obj);
    int xOffset, textWidth, cursorWidth;
    assert(obj->type == GUI_CHAT);

    if(!UIChat_IsActive(obj)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.msgScale, cfg.msgScale, 1);

    FR_SetFont(obj->font);
    FR_SetColorAndAlpha(cfg.hudColor[CR], cfg.hudColor[CG], cfg.hudColor[CB], textAlpha);

    textWidth = FR_TextWidth(text);
    cursorWidth = FR_CharWidth('_');

    if(cfg.msgAlign == 1)
        xOffset = -(textWidth + cursorWidth)/2;
    else if(cfg.msgAlign == 2)
        xOffset = -(textWidth + cursorWidth);
    else
        xOffset = 0;

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawTextXY(text, xOffset, 0);
    if(actualMapTime & 12)
    {
        FR_DrawCharXY('_', xOffset + textWidth, 0);
    }
    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

void UIChat_UpdateGeometry(uiwidget_t* obj)
{
    const char* text = UIChat_Text(obj);
    assert(obj->type == GUI_CHAT);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!UIChat_IsActive(obj)) return;

    FR_SetFont(obj->font);
    Rect_SetWidthHeight(obj->geometry, cfg.msgScale * (FR_TextWidth(text) + FR_CharWidth('_')),
                                       cfg.msgScale * (MAX_OF(FR_TextHeight(text), FR_CharHeight('_'))));
}

int UIChat_ParseDestination(const char* str)
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

int UIChat_ParseMacroId(const char* str)
{
    if(str && str[0])
    {
        int id = atoi(str);
        if(id >= 0 && id <= 9)
        {
            return id;
        }
    }
    return -1;
}
