/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2007 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/*
 * b_class.c: Binding Classs
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_misc.h"

#include "b_main.h"
#include "b_class.h"

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

static void B_UpdateDeviceStateAssociations(void)
{
    bclass_t*   bc;
    controlbinding_t* conBin;
    dbinding_t* db;
    int         i;
    
    I_ClearDeviceClassAssociations();
    
    // We need to iterate through all the device bindings in all class.
    for(i = 0; i < bindClassCount; ++i)
    {
        bc = bindClasses[i];
        // Skip inactive classes.
        if(!bc->active)
            continue;
        // All controls in the class.
        for(conBin = bc->controlBinds.next; conBin != &bc->controlBinds; 
            conBin = conBin->next)
        {
            for(i = 0; i < DDMAXPLAYERS; ++i)
            {
                // Associate all the device bindings.
                for(db = conBin->deviceBinds[i].next; db != &conBin->deviceBinds[i];
                    db = db->next)
                {
                    inputdev_t* dev = I_GetDevice(db->device, false);
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
                    }
                }                
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
    int         i;
    
    bclass_t* bc = M_Calloc(sizeof(bclass_t));
    bc->name = strdup(name);
    bc->active = false;
    B_InitCommandBindingList(&bc->commandBinds);
    B_InitControlBindingList(&bc->controlBinds);
    B_InsertClass(bc, 0);
    return bc;
}

void B_DestroyClass(bclass_t* bc)
{
    B_RemoveClass(bc);
    free(bc->name);
    B_DestroyCommandBindingList(&bc->commandBinds);
    B_DestroyControlBindingList(&bc->controlBinds);
    M_Free(bc);
}

void B_ActivateClass(bclass_t* bc, boolean doActivate)
{
    bc->active = doActivate;    
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
    free(conBin);
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
            if(B_TryCommandBinding(eb, event))
                return true;
        }
    }
    // Nobody used it.
    return false;
}
