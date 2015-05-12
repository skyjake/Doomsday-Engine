/** @file hu_chat.cpp  Player chat widget.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1993-1996 id Software, Inc.
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "common.h"
#include "hu_chat.h"
#include <cstdio>
#include <cstring>
#include "p_tick.h"
#include "d_net.h"

using namespace de;

void UIChat_LoadMacros()
{
    // Retrieve the chat macro strings if not already set.
    for(int i = 0; i < 10; ++i)
    {
        if(cfg.common.chatMacros[i]) continue;
        cfg.common.chatMacros[i] = (char *) GET_TXT(TXT_HUSTR_CHATMACRO0 + i);
    }
}

static void clearInputBuffer(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;

    chat->buffer.length = 0;
    chat->buffer.text[0] = '\0';
}

static void playMessageSentSound()
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

static void sendMessage(int /*player*/, int destination, char const *msg)
{
    if(!msg || !msg[0]) return;

    if(destination == 0)
    {
        // Send the message to the other players explicitly,
        if(!IS_NETGAME)
        {
            // Send it locally.
            for(int i = 0; i < MAXPLAYERS; ++i)
            {
                D_NetMessageNoSound(i, msg);
            }
        }
        else
        {
            char buf[256]; strcpy(buf, "chat ");
            M_StrCatQuoted(buf, msg, 256);
            DD_Execute(false, buf);
        }
    }
    else
    {
        // Send to all on the same team (team = color).
        destination -= 1;

        for(int i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame && cfg.playerColor[i] == destination)
            {
                if(!IS_NETGAME)
                {
                    // Send it locally.
                    D_NetMessageNoSound(i, msg);
                }
                else
                {
                    char buf[256]; sprintf(buf, "chatNum %d ", i);
                    M_StrCatQuoted(buf, msg, 256);
                    DD_Execute(false, buf);
                }
            }
        }
    }

    playMessageSentSound();
}

dd_bool UIChat_IsActive(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    return (0 != (chat->flags & UICF_ACTIVE));
}

dd_bool UIChat_Activate(uiwidget_t *ob, dd_bool yes)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    dd_bool oldActive = (chat->flags & UICF_ACTIVE) != 0;

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
        UIChat_SetDestination(ob, 0);
        UIChat_Clear(ob);
    }

    if(oldActive != ((chat->flags & UICF_ACTIVE) != 0))
    {
        DD_Executef(true, "%s chat", UIChat_IsActive(ob)? "activatebcontext" : "deactivatebcontext");
        return true;
    }

    return false;
}

int UIChat_Destination(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    return chat->destination;
}

void UIChat_SetDestination(uiwidget_t *ob, int destination)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    if(destination < 0 || destination > NUMTEAMS) return;
    chat->destination = destination;
}

dd_bool UIChat_SetShiftModifier(uiwidget_t *ob, dd_bool on)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    dd_bool oldShiftDown = chat->buffer.shiftDown;
    chat->buffer.shiftDown = on;
    return oldShiftDown != chat->buffer.shiftDown;
}

dd_bool UIChat_AppendCharacter(uiwidget_t *ob, char ch)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;

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

void UIChat_DeleteLastCharacter(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    if(!chat->buffer.length) return;
    chat->buffer.text[--chat->buffer.length] = 0;
}

void UIChat_Clear(uiwidget_t *ob)
{
    clearInputBuffer(ob);
}

char const *UIChat_Text(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    return chat->buffer.text;
}

size_t UIChat_TextLength(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    return chat->buffer.length;
}

dd_bool UIChat_TextIsEmpty(uiwidget_t *ob)
{
    return (0 == UIChat_TextLength(ob));
}

char const *UIChat_FindMacro(uiwidget_t * /*ob*/, int macroId)
{
    if(macroId < 0 || macroId >= 10) return 0;
    return cfg.common.chatMacros[macroId];
}

dd_bool UIChat_LoadMacro(uiwidget_t *ob, int macroId)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);
    guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    if(char const *macro = UIChat_FindMacro(ob, macroId))
    {
        strncpy(chat->buffer.text, macro, UICHAT_INPUTBUFFER_MAXLENGTH);
        chat->buffer.text[UICHAT_INPUTBUFFER_MAXLENGTH] = '\0';
        chat->buffer.length = strlen(chat->buffer.text);
    }
    return false;
}

int UIChat_Responder(uiwidget_t *ob, event_t *ev)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);

    if(!UIChat_IsActive(ob))
        return false;

    if(ev->type != EV_KEY)
        return false;

    if(ev->data1 == DDKEY_RSHIFT)
    {
        UIChat_SetShiftModifier(ob, (ev->state == EVS_DOWN || ev->state == EVS_REPEAT));
        return false; // Never eaten.
    }

    if(!(ev->state == EVS_DOWN || ev->state == EVS_REPEAT))
        return false;

    if(ev->data1 == DDKEY_BACKSPACE)
    {
        UIChat_DeleteLastCharacter(ob);
        return true;
    }

    return UIChat_AppendCharacter(ob, (char)ev->data1);
}

int UIChat_CommandResponder(uiwidget_t *ob, menucommand_e cmd)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_CHAT);

    if(!UIChat_IsActive(ob)) return false;

    switch(cmd)
    {
    case MCMD_SELECT:  // Send the message.
        if(!UIChat_TextIsEmpty(ob))
        {
            sendMessage(UIWidget_Player(ob), UIChat_Destination(ob), UIChat_Text(ob));
        }
        UIChat_Activate(ob, false);
        return true;

    case MCMD_CLOSE:
    case MCMD_NAV_OUT: // Close chat.
        UIChat_Activate(ob, false);
        return true;

    case MCMD_DELETE:
        UIChat_DeleteLastCharacter(ob);
        return true;

    default: return false;
    }
}

void UIChat_Drawer(uiwidget_t *ob, Point2Raw const *offset)
{
    DENG2_ASSERT(ob->type == GUI_CHAT);

    //guidata_chat_t *chat = (guidata_chat_t *)ob->typedata;
    float const textAlpha = uiRendState->pageAlpha * cfg.common.hudColor[3];
    //float const iconAlpha = uiRendState->pageAlpha * cfg.common.hudIconAlpha;
    char const *text = UIChat_Text(ob);

    if(!UIChat_IsActive(ob)) return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    if(offset) DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(cfg.common.msgScale, cfg.common.msgScale, 1);

    FR_SetFont(ob->font);
    FR_SetColorAndAlpha(cfg.common.hudColor[CR], cfg.common.hudColor[CG], cfg.common.hudColor[CB], textAlpha);

    int const textWidth  = FR_TextWidth(text);
    int const cursorWidth = FR_CharWidth('_');

    int xOffset = 0;
    if(cfg.common.msgAlign == 1)
        xOffset = -(textWidth + cursorWidth)/2;
    else if(cfg.common.msgAlign == 2)
        xOffset = -(textWidth + cursorWidth);

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

void UIChat_UpdateGeometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob->type == GUI_CHAT);

    char const *text = UIChat_Text(ob);

    Rect_SetWidthHeight(ob->geometry, 0, 0);

    if(!UIChat_IsActive(ob)) return;

    FR_SetFont(ob->font);
    Rect_SetWidthHeight(ob->geometry, cfg.common.msgScale * (FR_TextWidth(text) + FR_CharWidth('_')),
                                      cfg.common.msgScale * (de::max(FR_TextHeight(text), FR_CharHeight('_'))));
}

int UIChat_ParseDestination(char const *str)
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

int UIChat_ParseMacroId(char const *str)
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

void UIChat_Register()
{
    C_VAR_CHARPTR("chat-macro0", &cfg.common.chatMacros[0], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro1", &cfg.common.chatMacros[1], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro2", &cfg.common.chatMacros[2], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro3", &cfg.common.chatMacros[3], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro4", &cfg.common.chatMacros[4], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro5", &cfg.common.chatMacros[5], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro6", &cfg.common.chatMacros[6], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro7", &cfg.common.chatMacros[7], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro8", &cfg.common.chatMacros[8], 0, 0, 0);
    C_VAR_CHARPTR("chat-macro9", &cfg.common.chatMacros[9], 0, 0, 0);
    C_VAR_BYTE   ("chat-beep",   &cfg.common.chatBeep,      0, 0, 1);
}
