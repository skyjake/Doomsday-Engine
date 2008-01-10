/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * b_class.c: Binding Classs
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_misc.h"

#include "b_main.h"
#include "b_class.h"
#include "p_control.h"

#include <string.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int bindClassCount;
static bclass_t** bindClasses;

// CODE --------------------------------------------------------------------

/**
 * Destroy all binding classes and the bindings within the classes.
 * Called at shutdown time.
 */
void B_DestroyAllClasses(void)
{
    int     i;

    for(i = 0; i < bindClassCount; ++i)
    {
        B_DestroyClass(bindClasses[i]);
    }
    M_Free(bindClasses);
    bindClasses = 0;
    bindClassCount = 0;
}

/**
 * Marks all device states with the highest-priority binding class to which they have
 * a connection via device bindings. This ensures that if a high-priority class is
 * using a particular device state, lower-priority classes will not be using the
 * same state for their own controls.
 *
 * Called automatically whenever a class is activated or deactivated.
 */
void B_UpdateDeviceStateAssociations(void)
{
    int             i;
    uint            k;
    bclass_t       *bc;
    evbinding_t    *eb;
    controlbinding_t* conBin;
    dbinding_t     *db;

    I_ClearDeviceClassAssociations();

    // We need to iterate through all the device bindings in all class.
    for(i = 0; i < bindClassCount; ++i)
    {
        bc = bindClasses[i];
        // Skip inactive classes.
        if(!bc->active)
            continue;

        // Mark all event bindings in the class.
        for(eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
        {
            inputdev_t* dev = I_GetDevice(eb->device, false);
            switch(eb->type)
            {
            case E_TOGGLE:
                if(!dev->keys[eb->id].bClass)
                    dev->keys[eb->id].bClass = bc;
                break;

            case E_AXIS:
                if(!dev->axes[eb->id].bClass)
                    dev->axes[eb->id].bClass = bc;
                break;

            case E_ANGLE:
                if(!dev->hats[eb->id].bClass)
                    dev->hats[eb->id].bClass = bc;
                break;

            default:
                Con_Error("B_UpdateDeviceStateAssociations: Invalid value, "
                          "eb->type = %i.", (int) eb->type);
                break;
            }
        }

        // All controls in the class.
        for(conBin = bc->controlBinds.next; conBin != &bc->controlBinds;
            conBin = conBin->next)
        {
            for(k = 0; k < DDMAXPLAYERS; ++k)
            {
                // Associate all the device bindings.
                for(db = conBin->deviceBinds[k].next; db != &conBin->deviceBinds[k];
                    db = db->next)
                {
                    inputdev_t     *dev = I_GetDevice(db->device, false);
                    switch(db->type)
                    {
                    case CBD_TOGGLE:
                        if(!dev->keys[db->id].bClass)
                            dev->keys[db->id].bClass = bc;
                        break;

                    case CBD_AXIS:
                        if(!dev->axes[db->id].bClass)
                            dev->axes[db->id].bClass = bc;
                        break;

                    case CBD_ANGLE:
                        if(!dev->hats[db->id].bClass)
                            dev->hats[db->id].bClass = bc;
                        break;

                    default:
                        Con_Error("B_UpdateDeviceStateAssociations: Invalid value, "
                                  "db->type = %i.", (int) db->type);
                        break;
                    }
                }
            }
        }

        // If the class have made a broad device acquisition, mark all relevant states.
        if(bc->acquireKeyboard)
        {
            inputdev_t     *dev = I_GetDevice(IDEV_KEYBOARD, false);
            for(k = 0; k < dev->numKeys; ++k)
            {
                if(!dev->keys[k].bClass)
                    dev->keys[k].bClass = bc;
            }
        }
    }
}

static void B_SetClassCount(int count)
{
    bindClasses = realloc(bindClasses, sizeof(bclass_t*) * count);
    bindClassCount = count;
}

static void B_InsertClass(bclass_t* bc, int where)
{
    B_SetClassCount(bindClassCount + 1);
    memmove(&bindClasses[where + 1], &bindClasses[where], sizeof(bclass_t*) *
            (bindClassCount - 1 - where));
    bindClasses[where] = bc;
}

static void B_RemoveClass(bclass_t* bc)
{
    int where = B_GetClassPos(bc);
    if(where >= 0)
    {
        memmove(&bindClasses[where], &bindClasses[where + 1],
                sizeof(bclass_t*) * (bindClassCount - 1 - where));
        B_SetClassCount(bindClassCount - 1);
    }
}

/**
 * Creates a new binding class. The new class has the highest priority of all existing
 * classes, and is inactive.
 */
bclass_t* B_NewClass(const char* name)
{
    bclass_t* bc = M_Calloc(sizeof(bclass_t));

    bc->name = strdup(name);
    B_InitCommandBindingList(&bc->commandBinds);
    B_InitControlBindingList(&bc->controlBinds);
    B_InsertClass(bc, 0);
    return bc;
}

void B_DestroyClass(bclass_t* bc)
{
    B_RemoveClass(bc);
    free(bc->name);
    B_ClearClass(bc);
    M_Free(bc);
}

void B_ClearClass(bclass_t* bc)
{
    B_DestroyCommandBindingList(&bc->commandBinds);
    B_DestroyControlBindingList(&bc->controlBinds);
}

void B_ActivateClass(bclass_t* bc, boolean doActivate)
{
    if(!bc)
        return;

    bc->active = doActivate;
    B_UpdateDeviceStateAssociations();
}

void B_AcquireKeyboard(bclass_t* bc, boolean doAcquire)
{
    bc->acquireKeyboard = doAcquire;
    B_UpdateDeviceStateAssociations();
}

bclass_t* B_ClassByName(const char* name)
{
    int     i;

    for(i = 0; i < bindClassCount; ++i)
    {
        if(!strcasecmp(name, bindClasses[i]->name))
            return bindClasses[i];
    }
    return NULL;
}

bclass_t* B_ClassByPos(int pos)
{
    if(pos < 0 || pos >= bindClassCount)
        return NULL;
    return bindClasses[pos];
}

int B_ClassCount(void)
{
    return bindClassCount;
}

int B_GetClassPos(bclass_t* bc)
{
    int     i;

    for(i = 0; i < bindClassCount; ++i)
    {
        if(bindClasses[i] == bc)
            return i;
    }
    return -1;
}

void B_ReorderClass(bclass_t* bc, int pos)
{
    B_RemoveClass(bc);
    B_InsertClass(bc, pos);
}

controlbinding_t* B_NewControlBinding(bclass_t* bc)
{
    int     i;

    controlbinding_t* conBin = M_Calloc(sizeof(controlbinding_t));
    conBin->bid = B_NewIdentifier();
    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        B_InitDeviceBindingList(&conBin->deviceBinds[i]);
    }

    // Link it in.
    conBin->next = &bc->controlBinds;
    conBin->prev = bc->controlBinds.prev;
    bc->controlBinds.prev->next = conBin;
    bc->controlBinds.prev = conBin;

    return conBin;
}

controlbinding_t* B_GetControlBinding(bclass_t* bc, int control)
{
    controlbinding_t* i;

    for(i = bc->controlBinds.next; i != &bc->controlBinds; i = i->next)
    {
        if(i->control == control)
            return i;
    }
    // Create a new one.
    i = B_NewControlBinding(bc);
    i->control = control;
    return i;
}

void B_DestroyControlBinding(controlbinding_t* conBin)
{
    int     i;

    assert(conBin->bid != 0);

    // Unlink first, if linked.
    if(conBin->prev)
    {
        conBin->prev->next = conBin->next;
        conBin->next->prev = conBin->prev;
    }

    for(i = 0; i < DDMAXPLAYERS; ++i)
    {
        B_DestroyDeviceBindingList(&conBin->deviceBinds[i]);
    }
    M_Free(conBin);
}

void B_InitControlBindingList(controlbinding_t* listRoot)
{
    memset(listRoot, 0, sizeof(*listRoot));
    listRoot->next = listRoot->prev = listRoot;
}

void B_DestroyControlBindingList(controlbinding_t* listRoot)
{
    while(listRoot->next != listRoot)
    {
        B_DestroyControlBinding(listRoot->next);
    }
}

/**
 * @return  @c true, if the binding was found and deleted.
 */
boolean B_DeleteBinding(bclass_t* bc, int bid)
{
    evbinding_t* eb = 0;
    controlbinding_t* conBin = 0;
    dbinding_t* db = 0;
    int         i;

    // Check if it one of the command bindings.
    for(eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
    {
        if(eb->bid == bid)
        {
            B_DestroyCommandBinding(eb);
            return true;
        }
    }

    // How about one fo the control bindings?
    for(conBin = bc->controlBinds.next; conBin != &bc->controlBinds; conBin = conBin->next)
    {
        if(conBin->bid == bid)
        {
            B_DestroyControlBinding(conBin);
            return true;
        }

        // It may also be a device binding.
        for(i = 0; i < DDMAXPLAYERS; ++i)
        {
            for(db = conBin->deviceBinds[i].next; db != &conBin->deviceBinds[i]; db = db->next)
            {
                if(db->bid == bid)
                {
                    B_DestroyDeviceBinding(db);
                    return true;
                }
            }
        }
    }

    return false;
}

boolean B_TryEvent(ddevent_t* event)
{
    int     i;
    evbinding_t* eb;

    for(i = 0; i < bindClassCount; ++i)
    {
        bclass_t* bc = bindClasses[i];

        if(!bc->active)
            continue;

        // See if the command bindings will have it.
        for(eb = bc->commandBinds.next; eb != &bc->commandBinds; eb = eb->next)
        {
            if(B_TryCommandBinding(eb, event, bc))
                return true;
        }
    }
    // Nobody used it.
    return false;
}

void B_PrintClasses(void)
{
    int         i;
    bclass_t*   bc;

    Con_Printf("%i binding classes defined:\n", bindClassCount);

    for(i = 0; i < bindClassCount; ++i)
    {
        bc = bindClasses[i];
        Con_Printf("[%3i] \"%s\" (%s)\n", i, bc->name, bc->active? "active" : "inactive");
    }
}

void B_PrintAllBindings(void)
{
    int         i;
    int         k;
    bclass_t*   bc;
    int         count;
    evbinding_t* e;
    controlbinding_t* c;
    dbinding_t* d;
    ddstring_t* str = Str_New();

    Con_Printf("%i binding classes defined.\n", bindClassCount);

#define BIDFORMAT   "[%3i]"
    for(i = 0; i < bindClassCount; ++i)
    {
        bc = bindClasses[i];

        Con_Printf("Class \"%s\" (%s):\n", bc->name, bc->active? "active" : "inactive");

        // Commands.
        for(count = 0, e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next, count++);
        if(count)
            Con_Printf("  %i event bindings:\n", count);
        for(e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
        {
            B_EventBindingToString(e, str);
            Con_Printf("  "BIDFORMAT" %s : %s\n", e->bid, Str_Text(str), e->command);
        }

        // Controls.
        for(count = 0, c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next, count++);
        if(count)
            Con_Printf("  %i control bindings.\n", count);
        for(c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
        {
            const char* controlName = P_PlayerControlById(c->control)->name;
            Con_Printf("  Control \"%s\" "BIDFORMAT":\n", controlName, c->bid);
            for(k = 0; k < DDMAXPLAYERS; ++k)
            {
                for(count = 0, d = c->deviceBinds[k].next; d != &c->deviceBinds[k];
                    d = d->next, count++);
                if(!count) continue;
                Con_Printf("    Local player %i has %i device bindings for \"%s\":\n",
                           k + 1, count, controlName);
                for(d = c->deviceBinds[k].next; d != &c->deviceBinds[k]; d = d->next)
                {
                    B_DeviceBindingToString(d, str);
                    Con_Printf("    "BIDFORMAT" %s\n", d->bid, Str_Text(str));
                }
            }
        }
    }
    Str_Free(str);
}

void B_WriteClassToFile(const bclass_t* bc, FILE* file)
{
    evbinding_t* e;
    controlbinding_t* c;
    dbinding_t* d;
    int         k;
    ddstring_t* str = Str_New();

    // Commands.
    for(e = bc->commandBinds.next; e != &bc->commandBinds; e = e->next)
    {
        B_EventBindingToString(e, str);
        fprintf(file, "bindevent \"%s:%s\" \"", bc->name, Str_Text(str));
        M_WriteTextEsc(file, e->command);
        fprintf(file, "\"\n");
    }

    // Controls.
    for(c = bc->controlBinds.next; c != &bc->controlBinds; c = c->next)
    {
        const char* controlName = P_PlayerControlById(c->control)->name;
        for(k = 0; k < DDMAXPLAYERS; ++k)
        {
            for(d = c->deviceBinds[k].next; d != &c->deviceBinds[k]; d = d->next)
            {
                B_DeviceBindingToString(d, str);
                fprintf(file, "bindcontrol local%i-%s \"%s\"\n", k + 1, controlName, Str_Text(str));
            }
        }
    }
    Str_Free(str);
}
