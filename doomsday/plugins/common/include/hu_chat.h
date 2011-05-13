/**\file hu_chat.h
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

/**
 * Player chat widget.
 */

#ifndef LIBCOMMON_HUD_CHAT_H
#define LIBCOMMON_HUD_CHAT_H

#include "doomsday.h"

#define UICHAT_INPUTBUFFER_MAXLENGTH    (160)

typedef struct {
    char text[UICHAT_INPUTBUFFER_MAXLENGTH+1];
    int length; /// Current length of text.
    boolean shiftDown;
} uidata_chat_inputbuffer_t;

/**
 * @defgroup uiChatFlags  UIChat Flags.
 * @{
 */
#define UICF_ACTIVE             0x1
/**@}*/

/**
 * UIChat. UI widget for composing player/team chat messages.
 *
 * Terminology:
 *   "Destination":
 *      @c 0= All players (i.e., a "global" message).
 *      @c 1...NUMTEAMS= Any players on team number (n+1).
 */
typedef struct {
    int flags; /// @see uiChatFlags
    int destination;
    uidata_chat_inputbuffer_t buffer;
} uidata_chat_t;

/**
 * Change the "active" state of this. When activating the message target destination
 * is initialized for "global" messaging and the input buffer is cleared.
 *
 * @return  @c true if the active state changed.
 */
boolean UIChat_Activate(uidata_chat_t* chat, boolean yes);
boolean UIChat_IsActive(uidata_chat_t* chat);

int UIChat_Destination(uidata_chat_t* chat);
void UIChat_SetDestination(uidata_chat_t* chat, int destination);

/// @return  @c true if the shift modifier state changed.
boolean UIChat_SetShiftModifier(uidata_chat_t* chat, boolean on);

boolean UIChat_AppendCharacter(uidata_chat_t* chat, char ch);
void UIChat_DeleteLastCharacter(uidata_chat_t* chat);
void UIChat_Clear(uidata_chat_t* chat);

/// @return  A pointer to an immutable copy of the current contents of the input buffer.
const char* UIChat_Text(uidata_chat_t* chat);

/// @return  Current length of the input buffer in characters including terminating '\0'. 
size_t UIChat_TextLength(uidata_chat_t* chat);

/// @return  @c true= Current input buffer is empty.
boolean UIChat_TextIsEmpty(uidata_chat_t* chat);

/// Register the console variables and commands of this module.
void Chat_Register(void);

/// Initialize this module.
void Chat_Init(void);

/// Shutdown this module.
void Chat_Shutdown(void);

/// Load resources for this module (chat macro strings etc...).
void Chat_LoadResources(void);

/// To be called when the player enters the game to complete initialization
/// of that player's chat widget(s).
void Chat_Start(int player);

void Chat_Open(int player, boolean open);

boolean Chat_IsActive(int player);

int Chat_Responder(int player, event_t* ev);

void Chat_Drawer(int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight);

D_CMD(ChatOpen);
D_CMD(ChatAction);
D_CMD(ChatSendMacro);

#endif /* LIBCOMMON_HUD_CHAT_H */
