/** @file hu_chat.h  Player chat widget.
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

#ifndef LIBCOMMON_HUD_CHAT_H
#define LIBCOMMON_HUD_CHAT_H

#include "doomsday.h"
#include "hu_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Register the console variables and commands of this module.
void UIChat_Register(void);

void UIChat_LoadMacros(void);

#ifdef __cplusplus
} // extern "C"
#endif

/**
 * UIChat. UI widget for composing player/team chat messages.
 *
 * Terminology:
 *   "Destination":
 *      @c 0= All players (i.e., a "global" message).
 *      @c 1...NUMTEAMS= Any players on team number (n+1).
 */
#define UICHAT_INPUTBUFFER_MAXLENGTH    (160)

typedef struct {
    char text[UICHAT_INPUTBUFFER_MAXLENGTH+1];
    int length; /// Current length of text.
    dd_bool shiftDown;
} guidata_chat_inputbuffer_t;

/**
 * @defgroup uiChatFlags  UIChat Flags.
 * @{
 */
#define UICF_ACTIVE             0x1
/**@}*/

typedef struct {
    int flags; /// @ref uiChatFlags
    int destination;
    guidata_chat_inputbuffer_t buffer;
} guidata_chat_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Possibly respond to input event @a ev.
 * @return  Non-zero if the event was eaten.
 */
int UIChat_Responder(uiwidget_t *ob, event_t *ev);

/**
 * Possible respond to menu command @a cmd.
 * @return  Non-zero if the command was eaten.
 */
int UIChat_CommandResponder(uiwidget_t *ob, menucommand_e cmd);

/// Draw this widget.
void UIChat_Drawer(uiwidget_t *ob, Point2Raw const *offset);

/// Calculate the "physical" dimensions of this widget in fixed-pixels.
void UIChat_UpdateGeometry(uiwidget_t *ob);

/**
 * Change the "active" state of this. When activating the message target
 * destination is initialized for "global" messaging and the input buffer
 * is cleared.
 *
 * @post The "chat" binding context is (de)activated as necessary.
 *
 * @return  @c true if the active state changed.
 */
dd_bool UIChat_Activate(uiwidget_t *ob, dd_bool yes);

dd_bool UIChat_IsActive(uiwidget_t *ob);

/// @return  Current chat destination number.
int UIChat_Destination(uiwidget_t *ob);

/// Change the current chat @a destination.
void UIChat_SetDestination(uiwidget_t *ob, int destination);

/// @return  @c true if the shift modifier state changed.
dd_bool UIChat_SetShiftModifier(uiwidget_t *ob, dd_bool on);

dd_bool UIChat_AppendCharacter(uiwidget_t *ob, char ch);

void UIChat_DeleteLastCharacter(uiwidget_t *ob);

/// Clear the chat input buffer.
void UIChat_Clear(uiwidget_t *ob);

/// @return  A pointer to an immutable copy of the current contents of the input buffer.
char const *UIChat_Text(uiwidget_t *ob);

/// @return  Current length of the input buffer in characters including terminating '\0'.
size_t UIChat_TextLength(uiwidget_t *ob);

/// @return  @c true= Current input buffer is empty.
dd_bool UIChat_TextIsEmpty(uiwidget_t *ob);

/**
 * Given a macro identifier load the associated macro replacing the current contents
 * of the input buffer.
 * @return  @c true= success.
 */
dd_bool UIChat_LoadMacro(uiwidget_t *ob, int macroId);

/**
 * Given a macro identifier lookup the associated macro string.
 * @return  Pointer to the macro string if found else @c NULL.
 */
char const *UIChat_FindMacro(uiwidget_t *ob, int macroId);

/// @return  Parsed chat destination number from @a str or @c -1 if invalid.
int UIChat_ParseDestination(char const *str);

/// @return  Parsed chat macro identifier from @a str or @c -1 if invalid.
int UIChat_ParseMacroId(char const *str);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBCOMMON_HUD_CHAT_H
