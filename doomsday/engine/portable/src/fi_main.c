/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * The "In Fine" finale sequence system.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_defs.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_network.h"
#include "de_audio.h"
#include "de_infine.h"
#include "de_misc.h"

#include "finaleinterpreter.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(StartFinale);
D_CMD(StopFinale);

fidata_text_t*      P_CreateText(fi_objectid_t id, const char* name);
void                P_DestroyText(fidata_text_t* text);

fidata_pic_t*       P_CreatePic(fi_objectid_t id, const char* name);
void                P_DestroyPic(fidata_pic_t* pic);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static boolean active = false; // Interpreter active for the current finaleStack top.

// Allow stretching to fill the screen at near 4:3 aspect ratios?
static byte noStretch = false;

// Finale script/page collection.
static uint finaleStackSize;
static finaleinterpreter_t** finaleStack;

// Global Finale object store.
static fi_object_collection_t objects;

// CODE --------------------------------------------------------------------

/**
 * Called during pre-init to register cvars and ccmds for the finale system.
 */
void FI_Register(void)
{
    C_VAR_BYTE("finale-nostretch",  &noStretch, 0, 0, 1);

    C_CMD("startfinale",    "s",    StartFinale);
    C_CMD("startinf",       "s",    StartFinale);
    C_CMD("stopfinale",     "",     StopFinale);
    C_CMD("stopinf",        "",     StopFinale);
}

static void useColor(animator_t *color, int components)
{
    if(components == 3)
    {
        glColor3f(color[0].value, color[1].value, color[2].value);
    }
    else if(components == 4)
    {
        glColor4f(color[0].value, color[1].value, color[2].value, color[3].value);
    }
}

static void objectSetName(fi_object_t* obj, const char* name)
{
    dd_snprintf(obj->name, FI_NAME_MAX_LENGTH, "%s", name);
}

static void thinkObjectsInScope(fi_object_collection_t* c)
{
    uint i;
    for(i = 0; i < c->num; ++i)
    {
        fi_object_t* obj = c->vector[i];
        switch(obj->type)
        {
        case FI_PIC:    FIData_PicThink((fidata_pic_t*)obj);    break;
        case FI_TEXT:   FIData_TextThink((fidata_text_t*)obj);  break;
        default: break;
        }
    }
}

static void drawObjectsInScope2(fi_object_collection_t* c, fi_obtype_e type, float picOffsetX, float picOffsetY)
{
    static const vec3_t worldOrigin = { 0, 0, 0 };
    uint i;
    for(i = 0; i < c->num; ++i)
    {
        fi_object_t* obj = c->vector[i];
        if(obj->type != type)
            continue;
        switch(obj->type)
        {
        case FI_PIC:
            {
            vec3_t offset;
            V3_Set(offset, worldOrigin[VX]+picOffsetX, worldOrigin[VY]+picOffsetY, worldOrigin[VZ]);
            FIData_PicDraw((fidata_pic_t*)obj, offset);
            break;
            }
        case FI_TEXT:   FIData_TextDraw((fidata_text_t*)obj, worldOrigin); break;
        default: break;
        }
    }
}

static void drawObjectsInScope(fi_object_collection_t* c, float picXOffset, float picYOffset)
{
    // Images first, then text.
    drawObjectsInScope2(c, FI_PIC, picXOffset, picYOffset);
    drawObjectsInScope2(c, FI_TEXT, 0, 0);
}

static uint objectsToIndex(fi_object_collection_t* c, fi_object_t* obj)
{
    if(obj)
    {
        uint i;
        for(i = 0; i < c->num; ++i)
        {
            fi_object_t* other = c->vector[i];
            if(other == obj)
                return i+1;
        }
    }
    return 0;
}

static __inline boolean objectsIsPresent(fi_object_collection_t* c, fi_object_t* obj)
{
    return objectsToIndex(c, obj) != 0;
}

/**
 * Does not check if the object already exists in this scope. Assumes the caller
 * knows what they are doing.
 */
static fi_object_t* objectsAdd(fi_object_collection_t* c, fi_object_t* obj)
{
    c->vector = Z_Realloc(c->vector, sizeof(*c->vector) * ++c->num, PU_STATIC);
    c->vector[c->num-1] = obj;
    return obj;
}

/**
 * Assumes there is at most one reference to the obj in the scope and that the
 * caller knows what they are doing.
 */
static fi_object_t* objectsRemove(fi_object_collection_t* c, fi_object_t* obj)
{
    uint idx;
    if((idx = objectsToIndex(c, obj)))
    {
        idx -= 1; // Indices are 1-based.

        if(idx != c->num-1)
            memmove(&c->vector[idx], &c->vector[idx+1], sizeof(*c->vector) * (c->num-idx));

        if(c->num > 1)
        {
            c->vector = Z_Realloc(c->vector, sizeof(*c->vector) * --c->num, PU_STATIC);
        }
        else
        {
            Z_Free(c->vector);
            c->vector = 0;
            c->num = 0;
        }
    }
    return obj;
}

static void objectsEmpty(fi_object_collection_t* c)
{
    if(c->num)
    {
        uint i;
        for(i = 0; i < c->num; ++i)
        {
            fi_object_t* obj = c->vector[i];
            switch(obj->type)
            {
            case FI_PIC: P_DestroyPic((fidata_pic_t*)obj); break;
            case FI_TEXT: P_DestroyText((fidata_text_t*)obj); break;
            default:
                Con_Error("InFine: Unknown object type %i in objectsEmpty.", (int)obj->type);
            }
        }
        Z_Free(c->vector);
    }
    c->vector = 0;
    c->num = 0;
}

static fi_object_t* objectsById(fi_object_collection_t* c, fi_objectid_t id)
{
    if(id != 0)
    {
        uint i;
        for(i = 0; i < c->num; ++i)
        {
            fi_object_t* obj = c->vector[i];
            if(obj->id == id)
                return obj;
        }
    }
    return NULL;
}

/**
 * @return                  A new (unused) unique object id.
 */
static fi_objectid_t objectsUniqueId(fi_object_collection_t* c)
{
    fi_objectid_t id = 0;
    while(objectsById(c, ++id));
    return id;
}

/**
 * Clear the specified page to the default, blank state.
 */
static void pageClear(fi_page_t* p)
{
    p->timer = 0;
    p->bgMaterial = NULL; // No background material.

    if(p->_objects.vector)
    {
        Z_Free(p->_objects.vector);
    }
    p->_objects.vector = 0;
    p->_objects.num = 0;

    AnimatorVector4_Init(p->filter, 0, 0, 0, 0);
    AnimatorVector2_Init(p->imgOffset, 0, 0);  
    AnimatorVector4_Init(p->bgColor, 1, 1, 1, 0);
    {uint i;
    for(i = 0; i < 9; ++i)
    {
        AnimatorVector3_Init(p->textColor[i], 1, 1, 1);
    }}
}

static fi_page_t* newPage(void)
{
    fi_page_t* p = Z_Calloc(sizeof(*p), PU_STATIC, 0);
    pageClear(p);
    return p;
}

static __inline finaleinterpreter_t* stackTop(void)
{
    return (finaleStackSize == 0? 0 : finaleStack[finaleStackSize-1]);
}

static finaleinterpreter_t* stackPush(finaleinterpreter_t* fi)
{
    finaleStack = Z_Realloc(finaleStack, sizeof(*finaleStack) * ++finaleStackSize, PU_STATIC);
    finaleStack[finaleStackSize-1] = fi;
    return fi;
}

static boolean stackPop(void)
{
    finaleinterpreter_t* fi = stackTop();

    if(!fi)
    {
#ifdef _DEBUG
Con_Printf("InFine: Attempt to pop empty stack\n");
#endif
        return false;
    }

    P_DestroyFinaleInterpreter(fi);

    // Should we go back to NULL?
    if(finaleStackSize > 1)
    {   // Return to previous state.
        finaleStack = Z_Realloc(finaleStack, sizeof(*finaleStack) * --finaleStackSize, PU_STATIC);
        active = true;
    }
    else
    {
        Z_Free(finaleStack); finaleStack = 0;
        finaleStackSize = 0;
        active = false;
    }

    return active;
}

/**
 * Stop playing the script and go to next game state.
 */
static void scriptTerminate(finaleinterpreter_t* fi)
{
    if(!active)
        return;
    if(!FinaleInterpreter_CanSkip(fi))
        return;

    // Should we go back to NULL?
    if(finaleStackSize > 1)
    {   // Return to previous state.
        finaleStack = Z_Realloc(finaleStack, sizeof(*finaleStack) * --finaleStackSize, PU_STATIC);
        active = true;
    }
    else
    {
        Z_Free(finaleStack); finaleStack = 0;
        finaleStackSize = 0;
        active = false;
    }

    P_DestroyFinaleInterpreter(fi);
}

static void rotate(float angle)
{
    // Counter the VGA aspect ratio.
    glScalef(1, 200.0f / 240.0f, 1);
    glRotatef(angle, 0, 0, 1);
    glScalef(1, 240.0f / 200.0f, 1);
}

/**
 * Reset the entire InFine state stack.
 */
static void doReset(void)
{
    finaleinterpreter_t* fi;
    if((fi = stackTop()) && active)
    {
        // The state is suspended when the PlayDemo command is used.
        // Being suspended means that InFine is currently not active, but
        // will be restored at a later time.
        if(FinaleInterpreter_IsSuspended(fi))
            return;

        // Pop all the states.
        while(stackPop());
    }
}

static void picFrameDeleteXImage(fidata_pic_frame_t* f)
{
    DGL_DeleteTextures(1, (DGLuint*)&f->texRef.tex);
    f->texRef.tex = 0;
}

static fidata_pic_frame_t* createPicFrame(int type, int tics, void* texRef, short sound, boolean flagFlipH)
{
    fidata_pic_frame_t* f = (fidata_pic_frame_t*) Z_Malloc(sizeof(*f), PU_STATIC, 0);
    f->flags.flip = flagFlipH;
    f->type = type;
    f->tics = tics;
    switch(f->type)
    {
    case PFT_MATERIAL:  f->texRef.material = ((material_t*)texRef);     break;
    case PFT_PATCH:     f->texRef.patch = *((patchid_t*)texRef);    break;
    case PFT_RAW:       f->texRef.lump  = *((lumpnum_t*)texRef);    break;
    case PFT_XIMAGE:    f->texRef.tex = *((DGLuint*)texRef);        break;
    default:
        Con_Error("Error - InFine: unknown frame type %i.", (int)type);
    }
    f->sound = sound;
    return f;
}

static void destroyPicFrame(fidata_pic_frame_t* f)
{
    if(f->type == PFT_XIMAGE)
        picFrameDeleteXImage(f);
    Z_Free(f);
}

static fidata_pic_frame_t* picAddFrame(fidata_pic_t* p, fidata_pic_frame_t* f)
{
    p->frames = Z_Realloc(p->frames, sizeof(*p->frames) * ++p->numFrames, PU_STATIC);
    p->frames[p->numFrames-1] = f;
    return f;
}

static void picRotationOrigin(const fidata_pic_t* p, uint frame, float center[3])
{
    if(p->numFrames && frame < p->numFrames)
    {
        fidata_pic_frame_t* f = p->frames[frame];
        switch(f->type)
        {
        case PFT_PATCH:
            {
            patchinfo_t info;
            if(R_GetPatchInfo(f->texRef.patch, &info))
            {
                /// \fixme what about extraOffset?
                center[VX] = info.width / 2 - info.offset;
                center[VY] = info.height / 2 - info.topOffset;
                center[VZ] = 0;
            }
            else
            {
                center[VX] = center[VY] = center[VZ] = 0;
            }
            break;
            }
        case PFT_RAW:
        case PFT_XIMAGE:
            center[VX] = SCREENWIDTH/2;
            center[VY] = SCREENHEIGHT/2;
            center[VZ] = 0;
            break;
        case PFT_MATERIAL:
            center[VX] = center[VY] = center[VZ] = 0;
            break;
        }
    }
    else
    {
        center[VX] = center[VY] = .5f;
        center[VZ] = 0;
    }

    center[VX] *= p->scale[VX].value;
    center[VY] *= p->scale[VY].value;
    center[VZ] *= p->scale[VZ].value;
}

void FIObject_Destructor(fi_object_t* obj)
{
    assert(obj);
    // Destroy all references to this object in all scopes.
    if(finaleStackSize)
    {
        uint i;
        for(i = 0; i < finaleStackSize; ++i)
        {
            finaleinterpreter_t* fi = finaleStack[i];
            FIPage_RemoveObject(fi->_page, obj);
        }
    }
    objectsRemove(&objects, obj);
    Z_Free(obj);
}

fidata_pic_t* P_CreatePic(fi_objectid_t id, const char* name)
{
    fidata_pic_t* p = Z_Calloc(sizeof(*p), PU_STATIC, 0);

    p->id = id;
    p->type = FI_PIC;
    objectSetName((fi_object_t*)p, name);
    AnimatorVector4_Init(p->color, 1, 1, 1, 1);
    AnimatorVector3_Init(p->scale, 1, 1, 1);

    FIData_PicClearAnimation(p);
    return p;
}

void P_DestroyPic(fidata_pic_t* pic)
{
    assert(pic);
    FIData_PicClearAnimation(pic);
    // Call parent destructor.
    FIObject_Destructor((fi_object_t*)pic);
}

fidata_text_t* P_CreateText(fi_objectid_t id, const char* name)
{
#define LEADING             (11.f/7-1)

    fidata_text_t* t = Z_Calloc(sizeof(*t), PU_STATIC, 0);

    // Initialize it.
    t->id = id;
    t->type = FI_TEXT;
    t->textFlags = DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS;
    objectSetName((fi_object_t*)t, name);
    AnimatorVector4_Init(t->color, 1, 1, 1, 1);
    AnimatorVector3_Init(t->scale, 1, 1, 1);

    t->wait = 3;
    t->font = R_CompositeFontNumForName("a");
    t->lineheight = LEADING;

    return t;

#undef LEADING
}

void P_DestroyText(fidata_text_t* text)
{
    assert(text);
    if(text->text)
    {   // Free the memory allocated for the text string.
        Z_Free(text->text);
        text->text = NULL;
    }
    // Call parent destructor.
    FIObject_Destructor((fi_object_t*)text);
}

void FIObject_Think(fi_object_t* obj)
{
    assert(obj);

    AnimatorVector3_Think(obj->pos);
    AnimatorVector3_Think(obj->scale);
    Animator_Think(&obj->angle);
}

boolean FI_Active(void)
{
    finaleinterpreter_t* fi;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Active: Not initialized yet!\n");
#endif
        return false;
    }
    if((fi = stackTop()))
    {
        return active;
    }
    return false;
}

void FI_Init(void)
{
    if(inited)
        return; // Already been here.
    memset(&objects, 0, sizeof(objects));
    finaleStackSize = 0;
    finaleStack = 0;

    inited = true;
}

void FI_Shutdown(void)
{
    if(!inited)
        return; // Huh?

    if(finaleStackSize)
    {
        uint i;
        for(i = 0; i < finaleStackSize; ++i)
        {
            P_DestroyFinaleInterpreter(finaleStack[i]);
        }
        Z_Free(finaleStack);
    }
    finaleStack = 0;
    finaleStackSize = 0;

    // Garbage collection.
    objectsEmpty(&objects);
    active = false;
    inited = false;
}

boolean FI_CmdExecuted(void)
{
    finaleinterpreter_t* fi;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_CmdExecuted: Not initialized yet!\n");
#endif
        return false;
    }
    if((fi = stackTop()))
    {
        return FinaleInterpreter_CommandExecuted(fi);
    }
    return false;
}

void FI_Reset(void)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Reset: Not initialized yet!\n");
#endif
        return;
    }
    doReset();
}

fi_page_t* FI_NewPage(void)
{
    return newPage();
}

void FI_DeletePage(fi_page_t* p)
{
    if(!p) Con_Error("FI_DeletePage: Invalid page.");
    pageClear(p);
    Z_Free(p);
}

/**
 * Start playing the given script.
 */
boolean FI_ScriptBegin(const char* scriptSrc, finale_mode_t mode, int gameState, void* extraData)
{
    finaleinterpreter_t* fi;

    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptBegin: Not initialized yet!\n");
#endif
        return false;
    }
    if(!scriptSrc || !scriptSrc[0])
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptBegin: Warning, attempt to play empty script (mode=%i).\n", (int)mode);
#endif
        return false;
    }

    if(mode == FIMODE_LOCAL && isDedicated)
    {
        // Dedicated servers don't play local scripts.
#ifdef _DEBUG
        Con_Printf("Finale Begin: No local scripts in dedicated mode.\n");
#endif
        return false;
    }

#ifdef _DEBUG
    Con_Printf("Finale Begin: mode=%i '%.30s'\n", (int)mode, scriptSrc);
#endif

    // Init InFine state.
    fi = stackPush(P_CreateFinaleInterpreter());
    FinaleInterpreter_LoadScript(fi, mode, scriptSrc, gameState, extraData);
    active = true;

    return true;
}

void FI_ScriptTerminate(void)
{
    finaleinterpreter_t* fi;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptTerminate: Not initialized yet!\n");
#endif
        return;
    }
    if((fi = stackTop()) && active)
    {
        FinaleInterpreter_AllowSkip(fi, true);
        scriptTerminate(fi);
    }
}

fi_object_t* FI_Object(fi_objectid_t id)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Object: Not initialized yet!\n");
#endif
        return 0;
    }
    return objectsById(&objects, id);
}

void* FI_ScriptExtraData(void)
{
    finaleinterpreter_t* fi;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_ScriptGetExtraData: Not initialized yet!\n");
#endif
        return 0;
    }
    if((fi = stackTop()))
    {
        return FinaleInterpreter_ExtraData(fi);
    }
    return 0;
}

fi_object_t* FI_NewObject(fi_obtype_e type, const char* name)
{
    fi_object_t* obj;
    // Allocate and attach another.
    switch(type)
    {
    case FI_TEXT: obj = (fi_object_t*) P_CreateText(objectsUniqueId(&objects), name); break;
    case FI_PIC:  obj = (fi_object_t*) P_CreatePic(objectsUniqueId(&objects), name); break;
    default:
        Con_Error("FI_NewObject: Unknown type %i.", type);
    }
    return objectsAdd(&objects, obj);
}

void FI_DeleteObject(fi_object_t* obj)
{
    assert(obj);
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_DeleteObject: Not initialized yet!\n");
#endif
        return;
    }
    switch(obj->type)
    {
    case FI_PIC:    P_DestroyPic((fidata_pic_t*)obj);   break;
    case FI_TEXT:   P_DestroyText((fidata_text_t*)obj); break;
    default:
        Con_Error("FI_DeleteObject: Invalid type %i.", (int) obj->type);
    }
}

void FIPage_RunTic(fi_page_t* p)
{
    if(!p) Con_Error("FIPage_RunTic: Invalid page.");

    p->timer++;

    thinkObjectsInScope(&p->_objects);

    // Interpolateable values.
    AnimatorVector4_Think(p->bgColor);
    AnimatorVector2_Think(p->imgOffset);
    AnimatorVector4_Think(p->filter);
    {uint i;
    for(i = 0; i < 9; ++i)
        AnimatorVector3_Think(p->textColor[i]);
    }
}

boolean FIPage_HasObject(fi_page_t* p, fi_object_t* obj)
{
    if(!p) Con_Error("FIPage_HasObject: Invalid page.");
    return objectsIsPresent(&p->_objects, obj);
}

fi_object_t* FIPage_AddObject(fi_page_t* p, fi_object_t* obj)
{
    if(!p) Con_Error("FIPage_AddObject: Invalid page.");
    if(obj && !objectsIsPresent(&p->_objects, obj))
    {
        return objectsAdd(&p->_objects, obj);
    }
    return obj;
}

fi_object_t* FIPage_RemoveObject(fi_page_t* p, fi_object_t* obj)
{
    if(!p) Con_Error("FIPage_RemoveObject: Invalid page.");
    if(obj && objectsIsPresent(&p->_objects, obj))
    {
        return objectsRemove(&p->_objects, obj);
    }
    return obj;
}

void FIPage_SetBackground(fi_page_t* p, material_t* mat)
{
    if(!p) Con_Error("FIPage_SetBackground: Invalid page.");
    p->bgMaterial = mat;
}

void FIPage_SetBackgroundColor(fi_page_t* p, float red, float green, float blue, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundColor: Invalid page.");
    AnimatorVector3_Set(p->bgColor, red, green, blue, steps);
}

void FIPage_SetBackgroundColorAndAlpha(fi_page_t* p, float red, float green, float blue, float alpha, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundColorAndAlpha: Invalid page.");
    AnimatorVector4_Set(p->bgColor, red, green, blue, alpha, steps);
}

void FIPage_SetImageOffsetX(fi_page_t* p, float x, int steps)
{
    if(!p) Con_Error("FIPage_SetImageOffsetX: Invalid page.");
    Animator_Set(&p->imgOffset[0], x, steps);
}

void FIPage_SetImageOffsetY(fi_page_t* p, float y, int steps)
{
    if(!p) Con_Error("FIPage_SetImageOffsetY: Invalid page.");
    Animator_Set(&p->imgOffset[1], y, steps);
}

void FIPage_SetImageOffsetXY(fi_page_t* p, float x, float y, int steps)
{
    if(!p) Con_Error("FIPage_SetImageOffsetXY: Invalid page.");
    AnimatorVector2_Set(p->imgOffset, x, y, steps);
}

void FIPage_SetFilterColorAndAlpha(fi_page_t* p, float red, float green, float blue, float alpha, int steps)
{
    if(!p) Con_Error("FIPage_SetFilterColorAndAlpha: Invalid page.");
    AnimatorVector4_Set(p->filter, red, green, blue, alpha, steps);
}

void FIPage_SetPredefinedColor(fi_page_t* p, uint idx, float red, float green, float blue, int steps)
{
    if(!p) Con_Error("FIPage_SetPredefinedColor: Invalid page.");
    AnimatorVector3_Set(p->textColor[idx], red, green, blue, steps);
}

void FI_Ticker(timespan_t ticLength)
{
    // Always tic.
    R_TextTicker(ticLength);

    /*if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Ticker: Not initialized yet!\n");
#endif
        return;
    }*/

    if(!M_CheckTrigger(&sharedFixedTrigger, ticLength))
        return;

    // A new 'sharp' tick has begun.
    {finaleinterpreter_t* fi;
    if((fi = stackTop()) && active)
    {
        if(!FinaleInterpreter_IsSuspended(fi))
            FIPage_RunTic(fi->_page);

        if(FinaleInterpreter_RunTic(fi))
        {   // The script has ended!
            if(!active)
                return;

            scriptTerminate(fi);
        }
    }}
}

/**
 * The user has requested a skip. Returns true if the skip was done.
 */
int FI_SkipRequest(void)
{
    finaleinterpreter_t* fi;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_SkipRequest: Not initialized yet!\n");
#endif
        return false;
    }
    if((fi = stackTop()))
    {
        return FinaleInterpreter_Skip(fi);
    }
    return false;
}

/**
 * @return              @c true, if the event should open the menu.
 */
boolean FI_IsMenuTrigger(void)
{
    finaleinterpreter_t* fi;
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_IsMenuTrigger: Not initialized yet!\n");
#endif
        return false;
    }
    if((fi = stackTop()) && active)
        return FinaleInterpreter_IsMenuTrigger(fi);
    return false;
}

int FI_Responder(ddevent_t* ev)
{
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Responder: Not initialized yet!\n");
#endif
        return false;
    }
    {finaleinterpreter_t* fi;
    if((fi = stackTop()) && active)
        return FinaleInterpreter_Responder(fi, ev);
    }
    return false;
}

/**
 * Drawing is the most complex task here.
 */
void FI_Drawer(void)
{
    finaleinterpreter_t* fi;
    fi_page_t* page;

    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_Drawer: Not initialized yet!\n");
#endif
        return;
    }

    fi = stackTop();
    if(!fi || !active)
        return;

    // Don't draw anything until we are sure the script has started.
    if(!FinaleInterpreter_CommandExecuted(fi))
        return;
    // Don't draw anything if we are playing a demo.
    if(FinaleInterpreter_IsSuspended(fi))
        return;
    page = fi->_page;

    // First, draw the background.
    if(page->bgMaterial)
    {
        useColor(page->bgColor, 4);
        DGL_SetMaterial(page->bgMaterial);
        DGL_DrawRectTiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);
    }
    else if(page->bgColor[3].value > 0)
    {
        // Just clear the screen, then.
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, SCREENWIDTH, SCREENHEIGHT, page->bgColor[0].value, page->bgColor[1].value, page->bgColor[2].value, page->bgColor[3].value);
        DGL_Enable(DGL_TEXTURING);
    }

    // Now lets go into 3D mode for drawing the page objects.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // The 3D projection matrix.
    {float oldFOV = fieldOfView; /// \kludge not pretty.
    fieldOfView = 90;
    GL_ProjectionMatrix();
    fieldOfView = oldFOV;}

    // Configure the coordinate space to put the objects at screen depth.
    glScalef(1, -SCREENWIDTH/SCREENHEIGHT, 1); // This is the aspect correction.
    glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);
    glTranslatef(-SCREENWIDTH/2, -SCREENHEIGHT/2, .05f/*equal to glNearClip*/);

    // Clear Z buffer (prevent the objects being clipped by nearby polygons).
    glClear(GL_DEPTH_BUFFER_BIT);

    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);

    drawObjectsInScope(&page->_objects, -page->imgOffset[0].value, -page->imgOffset[1].value);

    // Restore original matrices and state: back to normal 2D.
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_CULL_FACE);
    // Back from wireframe mode?
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Filter on top of everything. Only draw if necessary.
    if(page->filter[3].value > 0)
    {
        DGL_Disable(DGL_TEXTURING);
        useColor(page->filter, 4);
        glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(SCREENWIDTH, 0);
            glVertex2f(SCREENWIDTH, SCREENHEIGHT);
            glVertex2f(0, SCREENHEIGHT);
        glEnd();
        DGL_Enable(DGL_TEXTURING);
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void FIData_PicThink(fidata_pic_t* p)
{
    assert(p);

    // Call parent thinker.
    FIObject_Think((fi_object_t*)p);

    AnimatorVector4_Think(p->color);
    AnimatorVector4_Think(p->otherColor);
    AnimatorVector4_Think(p->edgeColor);
    AnimatorVector4_Think(p->otherEdgeColor);

    if(!(p->numFrames > 1))
        return;

    // If animating, decrease the sequence timer.
    if(p->frames[p->curFrame]->tics > 0)
    {
        if(--p->tics <= 0)
        {
            fidata_pic_frame_t* f;
            // Advance the sequence position. k = next pos.
            int next = p->curFrame + 1;

            if(next == p->numFrames)
            {   // This is the end.
                p->animComplete = true;

                // Stop the sequence?
                if(p->flags.looping)
                    next = 0; // Rewind back to beginning.
                else // Yes.
                    p->frames[next = p->curFrame]->tics = 0;
            }

            // Advance to the next pos.
            f = p->frames[p->curFrame = next];
            p->tics = f->tics;

            // Play a sound?
            if(f->sound > 0)
                S_LocalSound(f->sound, NULL);
        }
    }
}

static void drawRect(fidata_pic_t* p, uint frame, float angle, const float worldOffset[3])
{
    assert(p->numFrames && frame < p->numFrames);
    {
    float mid[3];
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : NULL);

    assert(f->type == PFT_MATERIAL);

    mid[VX] = mid[VY] = mid[VZ] = 0;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(p->pos[0].value + worldOffset[VX], p->pos[1].value + worldOffset[VY], p->pos[2].value);
    glTranslatef(mid[VX], mid[VY], mid[VZ]);

    // Counter the VGA aspect ratio.
    if(p->angle.value != 0)
    {
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(p->angle.value, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    // Move to origin.
    glTranslatef(-mid[VX], -mid[VY], -mid[VZ]);
    glScalef((p->numFrames && p->frames[p->curFrame]->flags.flip ? -1 : 1) * p->scale[0].value, p->scale[1].value, p->scale[2].value);

    {
    float offset[2] = { 0, 0 }, scale[2] = { 1, 1 }, color[4], bottomColor[4];
    int magMode = DGL_LINEAR, width = 1, height = 1;
    DGLuint tex = 0;
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : NULL);
    material_t* mat;

    if((mat = f->texRef.material))
    {
        material_load_params_t params;
        material_snapshot_t ms;
        surface_t suf;

        suf.header.type = DMU_SURFACE; /// \fixme: perhaps use the dummy object system?
        suf.owner = 0;
        suf.decorations = 0;
        suf.numDecorations = 0;
        suf.flags = suf.oldFlags = (f->flags.flip? DDSUF_MATERIAL_FLIPH : 0);
        suf.inFlags = SUIF_PVIS|SUIF_BLEND;
        suf.material = mat;
        suf.normal[VX] = suf.oldNormal[VX] = 0;
        suf.normal[VY] = suf.oldNormal[VY] = 0;
        suf.normal[VZ] = suf.oldNormal[VZ] = 1; // toward the viewer.
        suf.offset[0] = suf.visOffset[0] = suf.oldOffset[0][0] = suf.oldOffset[1][0] = worldOffset[VX];
        suf.offset[1] = suf.visOffset[1] = suf.oldOffset[0][1] = suf.oldOffset[1][1] = worldOffset[VY];
        suf.visOffsetDelta[0] = suf.visOffsetDelta[1] = 0;
        suf.rgba[CR] = p->color[0].value;
        suf.rgba[CG] = p->color[1].value;
        suf.rgba[CB] = p->color[2].value;
        suf.rgba[CA] = p->color[3].value;
        suf.blendMode = BM_NORMAL;

        memset(&params, 0, sizeof(params));
        params.pSprite = false;
        params.tex.border = 0; // Need to allow for repeating.
        Materials_Prepare(&ms, suf.material, (suf.inFlags & SUIF_BLEND), &params);

        {int i;
        for(i = 0; i < 4; ++i)
            color[i] = bottomColor[i] = suf.rgba[i];
        }

        if(ms.units[MTU_PRIMARY].texInst)
        {
            tex = ms.units[MTU_PRIMARY].texInst->id;
            magMode = ms.units[MTU_PRIMARY].magMode;
            offset[0] = ms.units[MTU_PRIMARY].offset[0];
            offset[1] = ms.units[MTU_PRIMARY].offset[1];
            scale[0] = 1;//ms.units[MTU_PRIMARY].scale[0];
            scale[1] = 1;//ms.units[MTU_PRIMARY].scale[1];
            color[CA] *= ms.units[MTU_PRIMARY].alpha;
            bottomColor[CA] *= ms.units[MTU_PRIMARY].alpha;
            width = ms.width;
            height = ms.height;
        }
    }

    // The fill.
    if(tex)
    {
        /// \fixme: do not override the value taken from the Material snapshot.
        magMode = (filterUI ? GL_LINEAR : GL_NEAREST);

        GL_BindTexture(tex, magMode);

        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glTranslatef(offset[0], offset[1], 0);
        glScalef(scale[0], scale[1], 0);
    }
    else
        DGL_Disable(DGL_TEXTURING);

    glBegin(GL_QUADS);
        glColor4fv(color);
        glTexCoord2f(0, 0);
        glVertex2f(0, 0);

        glTexCoord2f(1, 0);
        glVertex2f(0+width, 0);

        glColor4fv(bottomColor);
        glTexCoord2f(1, 1);
        glVertex2f(0+width, 0+height);

        glTexCoord2f(0, 1);
        glVertex2f(0, 0+height);
    glEnd();

    if(tex)
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }
    else
    {
        DGL_Enable(DGL_TEXTURING);
    }
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

static __inline boolean useRect(const fidata_pic_t* p, uint frame)
{
    fidata_pic_frame_t* f;
    if(!p->numFrames)
        return false;
    if(frame >= p->numFrames)
        return true;
    f = p->frames[frame];
    if(f->type == PFT_MATERIAL)
        return true;
    return false;
}

/**
 * Vertex layout:
 *
 * 0 - 1
 * | / |
 * 2 - 3
 */
static size_t buildGeometry(DGLuint tex, const float rgba[4], const float rgba2[4],
    boolean flagTexFlip, rvertex_t** verts, rcolor_t** colors, rtexcoord_t** coords)
{
    static rvertex_t rvertices[4];
    static rcolor_t rcolors[4];
    static rtexcoord_t rcoords[4];

    V3_Set(rvertices[0].pos, 0, 0, 0);
    V3_Set(rvertices[1].pos, 1, 0, 0);
    V3_Set(rvertices[2].pos, 0, 1, 0);
    V3_Set(rvertices[3].pos, 1, 1, 0);

    if(tex)
    {
        V2_Set(rcoords[0].st, (flagTexFlip? 1:0), 0);
        V2_Set(rcoords[1].st, (flagTexFlip? 0:1), 0);
        V2_Set(rcoords[2].st, (flagTexFlip? 1:0), 1);
        V2_Set(rcoords[3].st, (flagTexFlip? 0:1), 1);
    }

    V4_Copy(rcolors[0].rgba, rgba);
    V4_Copy(rcolors[1].rgba, rgba);
    V4_Copy(rcolors[2].rgba, rgba2);
    V4_Copy(rcolors[3].rgba, rgba2);

    *verts = rvertices;
    *coords = (tex!=0? rcoords : 0);
    *colors = rcolors;
    return 4;
}

static void drawGeometry(DGLuint tex, size_t numVerts, const rvertex_t* verts,
    const rcolor_t* colors, const rtexcoord_t* coords)
{
    glBindTexture(GL_TEXTURE_2D, tex);
    if(tex)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (filterUI ? GL_LINEAR : GL_NEAREST));
    }
    else
        DGL_Disable(DGL_TEXTURING);

    glBegin(GL_TRIANGLE_STRIP);
    {size_t i;
    for(i = 0; i < numVerts; ++i)
    {
        if(coords) glTexCoord2fv(coords[i].st);
        if(colors) glColor4fv(colors[i].rgba);
        glVertex3fv(verts[i].pos);
    }}
    glEnd();

    if(!tex)
        DGL_Enable(DGL_TEXTURING);
}

static void drawPicFrame(fidata_pic_t* p, uint frame, const float _origin[3],
    const float scale[3], const float rgba[4], const float rgba2[4], float angle,
    const float worldOffset[3])
{
    if(useRect(p, frame))
    {
        drawRect(p, frame, angle, worldOffset);
        return;
    }

    {
    vec3_t offset = { 0, 0, 0 }, dimensions, origin, originOffset, center;
    boolean showEdges = true, flipTextureS = false;
    DGLuint tex = 0;
    size_t numVerts;
    rvertex_t* rvertices;
    rcolor_t* rcolors;
    rtexcoord_t* rcoords;

    if(p->numFrames)
    {
        fidata_pic_frame_t* f = p->frames[frame];
        patchtex_t* patch;
        rawtex_t* rawTex;

        flipTextureS = (f->flags.flip != 0);
        showEdges = false;

        if(f->type == PFT_RAW && (rawTex = R_GetRawTex(f->texRef.lump)))
        {   
            tex = GL_PrepareRawTex(rawTex);
            V3_Set(offset, SCREENWIDTH/2, SCREENHEIGHT/2, 0);
            V3_Set(dimensions, rawTex->width, rawTex->height, 0);
        }
        else if(f->type == PFT_XIMAGE)
        {
            tex = (DGLuint)f->texRef.tex;
            V3_Set(offset, SCREENWIDTH/2, SCREENHEIGHT/2, 0);
            V3_Set(dimensions, 1, 1, 0); /// \fixme.
        }
        /*else if(f->type == PFT_MATERIAL)
        {
            V3_Set(dimensions, 1, 1, 0);
        }*/
        else if(f->type == PFT_PATCH && (patch = R_FindPatchTex(f->texRef.patch)))
        {
            tex = (renderTextures==1? GL_PreparePatch(patch) : 0);
            V3_Set(offset, patch->offX, patch->offY, 0);
            /// \todo need to decide what if any significance what depth will mean here.
            V3_Set(dimensions, patch->width, patch->height, 0);
        }
    }

    // If we've not chosen a texture by now set some defaults.
    if(!tex)
    {
        V3_Set(dimensions, 1, 1, 1);
    }

    V3_Set(center, dimensions[VX] / 2, dimensions[VY] / 2, dimensions[VZ] / 2);

    V3_Sum(origin, _origin, center);
    V3_Subtract(origin, origin, offset);
    V3_Sum(origin, origin, worldOffset);

    V3_Subtract(originOffset, offset, center);              
    offset[VX] *= scale[VX]; offset[VY] *= scale[VY]; offset[VZ] *= scale[VZ];
    V3_Sum(originOffset, originOffset, offset);

    numVerts = buildGeometry(tex, rgba, rgba2, flipTextureS, &rvertices, &rcolors, &rcoords);

    // Setup the transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Move to the object origin.
    glTranslatef(origin[VX], origin[VY], origin[VZ]);

    if(angle != 0)
    {
        // With rotation we must counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(angle, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    // Translate to the object center.
    glTranslatef(originOffset[VX], originOffset[VY], originOffset[VZ]);
    glScalef(scale[VX] * dimensions[VX], scale[VY] * dimensions[VY], scale[VZ] * dimensions[VZ]);

    drawGeometry(tex, numVerts, rvertices, rcolors, rcoords);

    if(showEdges)
    {
        // The edges never have a texture.
        DGL_Disable(DGL_TEXTURING);

        glBegin(GL_LINES);
            useColor(p->edgeColor, 4);
            glVertex2f(0, 0);
            glVertex2f(1, 0);
            glVertex2f(1, 0);

            useColor(p->otherEdgeColor, 4);
            glVertex2f(1, 1);
            glVertex2f(1, 1);
            glVertex2f(0, 1);
            glVertex2f(0, 1);

            useColor(p->edgeColor, 4);
            glVertex2f(0, 0);
        glEnd();
        
        DGL_Enable(DGL_TEXTURING);
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

void FIData_PicDraw(fidata_pic_t* p, const float worldOffset[3])
{
    assert(p);
    {
    vec3_t scale, origin;
    vec4_t rgba, rgba2;

    // Fully transparent pics will not be drawn.
    if(!(p->color[CA].value > 0))
        return;

    V3_Set(origin, p->pos[VX].value, p->pos[VY].value, p->pos[VZ].value);
    V3_Set(scale, p->scale[VX].value, p->scale[VY].value, p->scale[VZ].value);
    V4_Set(rgba, p->color[CR].value, p->color[CG].value, p->color[CB].value, p->color[CA].value);
    if(p->numFrames == 0)
        V4_Set(rgba2, p->otherColor[CR].value, p->otherColor[CG].value, p->otherColor[CB].value, p->otherColor[CA].value);

    drawPicFrame(p, p->curFrame, origin, scale, rgba, (p->numFrames==0? rgba2 : rgba), p->angle.value, worldOffset);
    }
}

uint FIData_PicAppendFrame(fidata_pic_t* p, int type, int tics, void* texRef, short sound,
    boolean flagFlipH)
{
    assert(p);
    picAddFrame(p, createPicFrame(type, tics, texRef, sound, flagFlipH));
    return p->numFrames-1;
}

void FIData_PicClearAnimation(fidata_pic_t* p)
{
    assert(p);
    if(p->frames)
    {
        uint i;
        for(i = 0; i < p->numFrames; ++i)
            destroyPicFrame(p->frames[i]);
        Z_Free(p->frames);
    }
    p->flags.looping = false; // Yeah?
    p->frames = 0;
    p->numFrames = 0;
    p->curFrame = 0;
    p->animComplete = true;
}

void FIData_TextThink(fidata_text_t* t)
{
    assert(t);

    // Call parent thinker.
    FIObject_Think((fi_object_t*)t);

    AnimatorVector4_Think(t->color);

    if(t->wait)
    {
        if(--t->timer <= 0)
        {
            t->timer = t->wait;
            t->cursorPos++;
        }
    }

    if(t->scrollWait)
    {
        if(--t->scrollTimer <= 0)
        {
            t->scrollTimer = t->scrollWait;
            t->pos[1].target -= 1;
            t->pos[1].steps = t->scrollWait;
        }
    }

    // Is the text object fully visible?
    t->animComplete = (!t->wait || t->cursorPos >= FIData_TextLength(t));
}

static int textLineWidth(const char* text, compositefontid_t font)
{
    int width = 0;

    for(; *text; text++)
    {
        if(*text == '\\')
        {
            if(!*++text)
                break;
            if(*text == 'n')
                break;
            if(*text >= '0' && *text <= '9')
                continue;
            if(*text == 'w' || *text == 'W' || *text == 'p' || *text == 'P')
                continue;
        }
        width += GL_CharWidth(*text, font);
    }

    return width;
}

void FIData_TextDraw(fidata_text_t* tex, const float offset[3])
{
    assert(tex);
    {
    finaleinterpreter_t* fi = stackTop();
    int cnt, x = 0, y = 0;
    int ch, linew = -1;
    char* ptr;

    if(!tex->text)
        return;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glTranslatef(tex->pos[0].value + offset[VX], tex->pos[1].value + offset[VY], tex->pos[2].value + offset[VZ]);

    rotate(tex->angle.value);
    glScalef(tex->scale[0].value, tex->scale[1].value, tex->scale[2].value);

    // Draw it.
    // Set color zero (the normal color).
    useColor(tex->color, 4);
    for(cnt = 0, ptr = tex->text; *ptr && (!tex->wait || cnt < tex->cursorPos); ptr++)
    {
        if(linew < 0)
            linew = textLineWidth(ptr, tex->font);

        ch = *ptr;
        if(*ptr == '\\') // Escape?
        {
            if(!*++ptr)
                break;

            // Change of color.
            if(*ptr >= '0' && *ptr <= '9')
            {
                animatorvector3_t* color;
                uint colorIdx = *ptr - '0';

                if(!colorIdx)
                    color = (animatorvector3_t*) &tex->color; // Use the default color.
                else
                    color = &fi->_page->textColor[colorIdx-1];

                glColor4f((*color)[0].value, (*color)[1].value, (*color)[2].value, tex->color[3].value);
                continue;
            }

            // 'w' = half a second wait, 'W' = second's wait
            if(*ptr == 'w' || *ptr == 'W') // Wait?
            {
                if(tex->wait)
                    cnt += (int) ((float)TICRATE / tex->wait / (*ptr == 'w' ? 2 : 1));
                continue;
            }

            // 'p' = 5 second wait, 'P' = 10 second wait
            if(*ptr == 'p' || *ptr == 'P') // Longer pause?
            {
                if(tex->wait)
                    cnt += (int) ((float)TICRATE / tex->wait * (*ptr == 'p' ? 5 : 10));
                continue;
            }

            if(*ptr == 'n' || *ptr == 'N') // Newline?
            {
                x = 0;
                y += GL_CharHeight('A', tex->font) * (1+tex->lineheight);
                linew = -1;
                cnt++; // Include newlines in the wait count.
                continue;
            }

            if(*ptr == '_')
                ch = ' ';
        }

        // Let's do Y-clipping (in case of tall text blocks).
        if(tex->scale[1].value * y + tex->pos[1].value >= -tex->scale[1].value * tex->lineheight &&
           tex->scale[1].value * y + tex->pos[1].value < SCREENHEIGHT)
        {
            GL_DrawChar2(ch, (tex->textFlags & DTF_ALIGN_LEFT) ? x : x - linew / 2, y, tex->font);
            x += GL_CharWidth(ch, tex->font);
        }

        cnt++; // Actual character drawn.
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

/**
 * @return                  The length as a counter.
 */
int FIData_TextLength(fidata_text_t* tex)
{
    assert(tex);
    {
    int cnt = 0;
    if(tex->text)
    {
        float secondLen = (tex->wait ? TICRATE / tex->wait : 0);
        const char* ptr;
        for(ptr = tex->text; *ptr; ptr++)
        {
            if(*ptr == '\\') // Escape?
            {
                if(!*++ptr)
                    break;
                if(*ptr == 'w')
                    cnt += secondLen / 2;
                if(*ptr == 'W')
                    cnt += secondLen;
                if(*ptr == 'p')
                    cnt += 5 * secondLen;
                if(*ptr == 'P')
                    cnt += 10 * secondLen;
                if((*ptr >= '0' && *ptr <= '9') || *ptr == 'n' || *ptr == 'N')
                    continue;
            }

            cnt++; // An actual character.
        }
    }
    return cnt;
    }
}

void FIData_TextCopy(fidata_text_t* t, const char* str)
{
    assert(t);
    assert(str && str[0]);
    {
    size_t len = strlen(str) + 1;
    if(t->text)
        Z_Free(t->text); // Free any previous text.
    t->text = Z_Malloc(len, PU_STATIC, 0);
    memcpy(t->text, str, len);
    }
}

D_CMD(StartFinale)
{
    finale_mode_t mode = (FI_Active()? FIMODE_OVERLAY : FIMODE_LOCAL); //(G_GetGameState() == GS_MAP? FIMODE_OVERLAY : FIMODE_LOCAL);
    char* script;

    if(FI_Active())
        return false;

    if(!Def_Get(DD_DEF_FINALE, argv[1], &script))
    {
        Con_Printf("Script '%s' is not defined.\n", argv[1]);
        return false;
    }

    FI_ScriptBegin(script, mode, gx.FI_GetGameState(), 0);
    return true;
}

D_CMD(StopFinale)
{
    FI_ScriptTerminate();
    return true;
}
