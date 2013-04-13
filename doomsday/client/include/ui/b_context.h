/** @file b_context.h
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

/**
 * Bindings Contexts.
 */

#ifndef LIBDENG_BIND_CONTEXT_H
#define LIBDENG_BIND_CONTEXT_H

#include "dd_share.h"
#include "b_command.h"
#include "b_device.h"

typedef struct controlbinding_s {
    struct controlbinding_s* next;
    struct controlbinding_s* prev;
    int             bid; // Unique identifier.
    int             control; // Identifier of the player control.
    dbinding_t      deviceBinds[DDMAXPLAYERS]; // Separate bindings for each local player.
} controlbinding_t;

// Binding Context Flags:
#define BCF_ACTIVE              0x01 // Context is only used when it is active.
#define BCF_PROTECTED           0x02 // Context cannot be (de)activated by plugins.
#define BCF_ACQUIRE_KEYBOARD    0x04 // Context has acquired all keyboard states, unless
                                     // higher-priority contexts override it.
#define BCF_ACQUIRE_ALL         0x08 // Context will acquire all unacquired states.

typedef struct bcontext_s {
    char*           name; // Name of the binding context.
    byte            flags;
    evbinding_t     commandBinds; // List of command bindings.
    controlbinding_t controlBinds;
    int           (*ddFallbackResponder)(const ddevent_t* ddev);
    int           (*fallbackResponder)(event_t* event); // event_t
} bcontext_t;

#ifdef __cplusplus
extern "C" {
#endif

void            B_UpdateDeviceStateAssociations(void);
bcontext_t*     B_NewContext(const char* name);
void            B_DestroyAllContexts(void);
void            B_ActivateContext(bcontext_t* bc, boolean doActivate);
void            B_AcquireKeyboard(bcontext_t* bc, boolean doAcquire);
void            B_AcquireAll(bcontext_t* bc, boolean doAcquire);
void            B_SetContextFallbackForDDEvents(const char* name, int (*ddResponderFunc)(const ddevent_t*));
bcontext_t*     B_ContextByPos(int pos);
bcontext_t*     B_ContextByName(const char* name);
int             B_ContextCount(void);
int             B_GetContextPos(bcontext_t* bc);
void            B_ReorderContext(bcontext_t* bc, int pos);
void            B_ClearContext(bcontext_t* bc);
void            B_DestroyContext(bcontext_t* bc);
controlbinding_t* B_FindControlBinding(bcontext_t* bc, int control);
controlbinding_t* B_GetControlBinding(bcontext_t* bc, int control);
void            B_DestroyControlBinding(controlbinding_t* conBin);
void            B_InitControlBindingList(controlbinding_t* listRoot);
void            B_DestroyControlBindingList(controlbinding_t* listRoot);
boolean         B_DeleteBinding(bcontext_t* bc, int bid);
de::Action     *B_ActionForEvent(ddevent_t const *event);
boolean         B_FindMatchingBinding(bcontext_t* bc, evbinding_t* match1, dbinding_t* match2,
                                      evbinding_t** evResult, dbinding_t** dResult);
void            B_PrintContexts(void);
void            B_PrintAllBindings(void);
void            B_WriteContextToFile(const bcontext_t* bc, FILE* file);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_BIND_CONTEXT_H */
