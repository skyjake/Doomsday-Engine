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

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_ui.h"
#include "de_render.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

fidata_text_t*      P_CreateText(fi_objectid_t id, const char* name);
void                P_DestroyText(fidata_text_t* text);

fidata_pic_t*       P_CreatePic(fi_objectid_t id, const char* name);
void                P_DestroyPic(fidata_pic_t* pic);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static uint numPages;
static fi_page_t** pages;

/// Global object store.
static fi_object_collection_t objects;

// CODE --------------------------------------------------------------------

static fi_page_t* pagesAdd(fi_page_t* p)
{
    pages = Z_Realloc(pages, sizeof(*pages) * ++numPages, PU_STATIC);
    pages[numPages-1] = p;
    return p;
}

static fi_page_t* pagesRemove(fi_page_t* p)
{
    uint i;
    for(i = 0; i < numPages; ++i)
    {
        if(pages[i] != p)
            continue;

        if(i != numPages-1)
            memmove(&pages[i], &pages[i+1], sizeof(*pages) * (numPages-i));

        if(numPages > 1)
        {
            pages = Z_Realloc(pages, sizeof(*pages) * --numPages, PU_STATIC);
        }
        else
        {
            Z_Free(pages); pages = 0;
            numPages = 0;
        }
        break;
    }
    return p;
}

/**
 * Clear the specified page to the default, blank state.
 */
static void pageClear(fi_page_t* p)
{
    p->_timer = 0;
    p->_bgMaterial = 0; // No background material.

    if(p->_objects.vector)
    {
        Z_Free(p->_objects.vector);
    }
    p->_objects.vector = 0;
    p->_objects.size = 0;

    AnimatorVector3_Init(p->_offset, 0, 0, 0);
    AnimatorVector4_Init(p->_bgColor, 1, 1, 1, 0);
    AnimatorVector4_Init(p->_filter, 0, 0, 0, 0);
    {uint i;
    for(i = 0; i < 9; ++i)
    {
        AnimatorVector3_Init(p->_textColor[i], 1, 1, 1);
    }}
}

static fi_page_t* newPage(fi_page_t* prevPage)
{
    fi_page_t* p = Z_Calloc(sizeof(*p), PU_STATIC, 0);
    p->drawer = FIPage_Drawer;
    p->ticker = FIPage_Ticker;
    p->previous = prevPage;
    pageClear(p);
    return p;
}

static void objectsThink(fi_object_collection_t* c)
{
    uint i;
    for(i = 0; i < c->size; ++i)
    {
        fi_object_t* obj = c->vector[i];
        obj->thinker(obj);
    }
}

static void objectsDraw(fi_object_collection_t* c, fi_obtype_e type,
    const float worldOrigin[3])
{
    uint i;
    for(i = 0; i < c->size; ++i)
    {
        fi_object_t* obj = c->vector[i];
        if(type != FI_NONE && obj->type != type)
            continue;
        obj->drawer(obj, worldOrigin);
    }
}

static uint objectsToIndex(fi_object_collection_t* c, fi_object_t* obj)
{
    if(obj)
    {
        uint i;
        for(i = 0; i < c->size; ++i)
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
 * \note Does not check if the object already exists in this collection.
 */
static fi_object_t* objectsAdd(fi_object_collection_t* c, fi_object_t* obj)
{
    c->vector = Z_Realloc(c->vector, sizeof(*c->vector) * ++c->size, PU_STATIC);
    c->vector[c->size-1] = obj;
    return obj;
}

/**
 * \assume There is at most one reference to the object in this collection.
 */
static fi_object_t* objectsRemove(fi_object_collection_t* c, fi_object_t* obj)
{
    uint idx;
    if((idx = objectsToIndex(c, obj)))
    {
        idx -= 1; // Indices are 1-based.

        if(idx != c->size-1)
            memmove(&c->vector[idx], &c->vector[idx+1], sizeof(*c->vector) * (c->size-idx));

        if(c->size > 1)
        {
            c->vector = Z_Realloc(c->vector, sizeof(*c->vector) * --c->size, PU_STATIC);
        }
        else
        {
            Z_Free(c->vector); c->vector = 0;
            c->size = 0;
        }
    }
    return obj;
}

static void objectsEmpty(fi_object_collection_t* c)
{
    if(c->size)
    {
        uint i;
        for(i = 0; i < c->size; ++i)
        {
            fi_object_t* obj = c->vector[i];
            switch(obj->type)
            {
            case FI_PIC:    P_DestroyPic((fidata_pic_t*)obj);   break;
            case FI_TEXT:   P_DestroyText((fidata_text_t*)obj); break;
            default:
                Con_Error("InFine: Unknown object type %i in objectsEmpty.", (int)obj->type);
            }
        }
        Z_Free(c->vector);
    }
    c->vector = 0;
    c->size = 0;
}

static fi_object_t* objectsById(fi_object_collection_t* c, fi_objectid_t id)
{
    if(id != 0)
    {
        uint i;
        for(i = 0; i < c->size; ++i)
        {
            fi_object_t* obj = c->vector[i];
            if(obj->id == id)
                return obj;
        }
    }
    return 0;
}

/**
 * @return  A new (unused) unique object id.
 */
static fi_objectid_t objectsUniqueId(fi_object_collection_t* c)
{
    fi_objectid_t id = 0;
    while(objectsById(c, ++id));
    return id;
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
    case PFT_MATERIAL:  f->texRef.material = ((material_t*)texRef); break;
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

static void objectSetName(fi_object_t* obj, const char* name)
{
    dd_snprintf(obj->name, FI_NAME_MAX_LENGTH, "%s", name);
}

void UI_Init(void)
{
    if(inited)
        return; // Already been here.
    memset(&objects, 0, sizeof(objects));
    pages = 0; numPages = 0;

    inited = true;
}

void UI_Shutdown(void)
{
    if(!inited)
        return; // Huh?

    // Garbage collection.
    objectsEmpty(&objects);
    if(numPages)
    {
        uint i;
        for(i = 0; i < numPages; ++i)
        {
            fi_page_t* p = pages[i];
            pageClear(p);
            Z_Free(p);
        }
        Z_Free(pages);
    }
    pages = 0; numPages = 0;

    inited = false;
}

void UI2_Ticker(timespan_t ticLength)
{
    // Always tic.
    R_TextTicker(ticLength);

    if(!inited)
        return;

    // All pages tic unless paused.
    { uint i;
    for(i = 0; i < numPages; ++i)
    {
        fi_page_t* page = pages[i];
        page->ticker(page, ticLength);
    }}
}

void FIObject_Destructor(fi_object_t* obj)
{
    assert(obj);
    // Destroy all references to this object on all pages.
    {uint i;
    for(i = 0; i < numPages; ++i)
        FIPage_RemoveObject(pages[i], obj);
    }
    objectsRemove(&objects, obj);
    Z_Free(obj);
}

fidata_pic_t* P_CreatePic(fi_objectid_t id, const char* name)
{
    fidata_pic_t* p = Z_Calloc(sizeof(*p), PU_STATIC, 0);

    p->type = FI_PIC;
    p->drawer = FIData_PicDraw;
    p->thinker = FIData_PicThink;
    p->id = id;
    p->flags.looping = false;
    p->animComplete = true;
    objectSetName((fi_object_t*)p, name);
    AnimatorVector4_Init(p->color, 1, 1, 1, 1);
    AnimatorVector3_Init(p->scale, 1, 1, 1);

    FIData_PicClearAnimation((fi_object_t*)p);
    return p;
}

void P_DestroyPic(fidata_pic_t* pic)
{
    assert(pic);
    FIData_PicClearAnimation((fi_object_t*)pic);
    // Call parent destructor.
    FIObject_Destructor((fi_object_t*)pic);
}

fidata_text_t* P_CreateText(fi_objectid_t id, const char* name)
{
#define LEADING             (11.f/7-1)

    fidata_text_t* t = Z_Calloc(sizeof(*t), PU_STATIC, 0);

    t->type = FI_TEXT;
    t->drawer = FIData_TextDraw;
    t->thinker = FIData_TextThink;
    t->id = id;
    t->flags.looping = false;
    t->animComplete = true;
    t->textFlags = DTF_ALIGN_TOPLEFT|DTF_NO_EFFECTS;
    objectSetName((fi_object_t*)t, name);
    AnimatorVector4_Init(t->color, 1, 1, 1, 1);
    AnimatorVector3_Init(t->scale, 1, 1, 1);

    t->wait = 3;
    t->font = R_CompositeFontNumForName("a");
    t->lineHeight = LEADING;

    return t;

#undef LEADING
}

void P_DestroyText(fidata_text_t* text)
{
    assert(text);
    if(text->text)
    {
        Z_Free(text->text); text->text = 0;
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

fi_page_t* FI_NewPage(fi_page_t* prevPage)
{
    return pagesAdd(newPage(prevPage));
}

void FI_DeletePage(fi_page_t* p)
{
    if(!p) Con_Error("FI_DeletePage: Invalid page.");
    pageClear(p);
    pagesRemove(p);
    { uint i;
    for(i = 0; i < numPages; ++i)
    {
        fi_page_t* other = pages[i];
        if(other->previous == p)
            other->previous = 0;
    }}
    Z_Free(p);
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

fi_object_t* FI_NewObject(fi_obtype_e type, const char* name)
{
    fi_object_t* obj;
    switch(type)
    {
    case FI_TEXT: obj = (fi_object_t*) P_CreateText(objectsUniqueId(&objects), name);   break;
    case FI_PIC:  obj = (fi_object_t*) P_CreatePic(objectsUniqueId(&objects), name);    break;
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

static void useColor(const animator_t* color, int components)
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

void FIPage_Drawer(fi_page_t* p)
{
    if(!p) Con_Error("FIPage_Drawer: Invalid page.");

    if(p->flags.hidden)
        return;

    // First, draw the background.
    if(p->_bgMaterial)
    {
        useColor(p->_bgColor, 4);
        DGL_SetMaterial(p->_bgMaterial);
        DGL_DrawRectTiled(0, 0, SCREENWIDTH, SCREENHEIGHT, 64, 64);
    }
    else if(p->_bgColor[3].value > 0)
    {
        // Just clear the screen, then.
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, SCREENWIDTH, SCREENHEIGHT, p->_bgColor[0].value, p->_bgColor[1].value, p->_bgColor[2].value, p->_bgColor[3].value);
        DGL_Enable(DGL_TEXTURING);
    }

    // Now lets go into 3D mode for drawing the p objects.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    GL_SetMultisample(true);

    // The 3D projection matrix.
    // We're assuming pixels are squares.
    {float aspect = theWindow->width / (float) theWindow->height;
    yfov = 2 * RAD2DEG(atan(tan(DEG2RAD(90) / 2) / aspect));
    GL_InfinitePerspective(yfov, aspect, .05f);}

    // We need a left-handed yflipped coordinate system.
    glScalef(1, -1, -1);

    // Clear Z buffer (prevent the objects being clipped by nearby polygons).
    glClear(GL_DEPTH_BUFFER_BIT);

    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glEnable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);

    {vec3_t worldOrigin;
    V3_Set(worldOrigin, -SCREENWIDTH/2 - p->_offset[VX].value, -SCREENHEIGHT/2 - p->_offset[VY].value, .05f - p->_offset[VZ].value);
    objectsDraw(&p->_objects, FI_NONE/* treated as 'any' */, worldOrigin);

    /*{rendmodelparams_t params;
    memset(&params, 0, sizeof(params));

    glEnable(GL_DEPTH_TEST);

    worldOrigin[VY] += 50.f / SCREENWIDTH * (40);
    worldOrigin[VZ] += 20; // Suitable default?
    setupModelParamsForFIObject(&params, "testmodel", worldOrigin);
    Rend_RenderModel(&params);

    worldOrigin[VX] -= 160.f / SCREENWIDTH * (40);
    setupModelParamsForFIObject(&params, "testmodel", worldOrigin);
    Rend_RenderModel(&params);

    worldOrigin[VX] += 320.f / SCREENWIDTH * (40);
    setupModelParamsForFIObject(&params, "testmodel", worldOrigin);
    Rend_RenderModel(&params);

    glDisable(GL_DEPTH_TEST);}*/
    }

    // Restore original matrices and state: back to normal 2D.
    glDisable(GL_ALPHA_TEST);
    //glDisable(GL_CULL_FACE);
    // Back from wireframe mode?
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Filter on top of everything. Only draw if necessary.
    if(p->_filter[3].value > 0)
    {
        DGL_Disable(DGL_TEXTURING);
        useColor(p->_filter, 4);
        glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(SCREENWIDTH, 0);
            glVertex2f(SCREENWIDTH, SCREENHEIGHT);
            glVertex2f(0, SCREENHEIGHT);
        glEnd();
        DGL_Enable(DGL_TEXTURING);
    }

    GL_SetMultisample(false);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void FIPage_MakeVisible(fi_page_t* p, boolean yes)
{
    if(!p) Con_Error("FIPage_MakeVisible: Invalid page.");
    p->flags.hidden = !yes;
}

void FIPage_Pause(fi_page_t* p, boolean yes)
{
    if(!p) Con_Error("FIPage_Pause: Invalid page.");
    p->flags.paused = yes;
}

void FIPage_Ticker(fi_page_t* p, timespan_t ticLength)
{
    if(!p) Con_Error("FIPage_Ticker: Invalid page.");

    if(!M_CheckTrigger(&sharedFixedTrigger, ticLength))
        return;
    // A new 'sharp' tick has begun.
    p->_timer++;

    objectsThink(&p->_objects);

    AnimatorVector3_Think(p->_offset);
    AnimatorVector4_Think(p->_bgColor);
    AnimatorVector4_Think(p->_filter);
    {uint i;
    for(i = 0; i < 9; ++i)
        AnimatorVector3_Think(p->_textColor[i]);
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

material_t* FIPage_Background(fi_page_t* p)
{
    if(!p) Con_Error("FIPage_Background: Invalid page.");
    return p->_bgMaterial;
}

void FIPage_SetBackground(fi_page_t* p, material_t* mat)
{
    if(!p) Con_Error("FIPage_SetBackground: Invalid page.");
    p->_bgMaterial = mat;
}

void FIPage_SetBackgroundColor(fi_page_t* p, float red, float green, float blue, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundColor: Invalid page.");
    AnimatorVector3_Set(p->_bgColor, red, green, blue, steps);
}

void FIPage_SetBackgroundColorAndAlpha(fi_page_t* p, float red, float green, float blue, float alpha, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundColorAndAlpha: Invalid page.");
    AnimatorVector4_Set(p->_bgColor, red, green, blue, alpha, steps);
}

void FIPage_SetOffsetX(fi_page_t* p, float x, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetX: Invalid page.");
    Animator_Set(&p->_offset[VX], x, steps);
}

void FIPage_SetOffsetY(fi_page_t* p, float y, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetY: Invalid page.");
    Animator_Set(&p->_offset[VY], y, steps);
}

void FIPage_SetOffsetZ(fi_page_t* p, float y, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetY: Invalid page.");
    Animator_Set(&p->_offset[VZ], y, steps);
}

void FIPage_SetOffsetXYZ(fi_page_t* p, float x, float y, float z, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetXYZ: Invalid page.");
    AnimatorVector3_Set(p->_offset, x, y, z, steps);
}

void FIPage_SetFilterColorAndAlpha(fi_page_t* p, float red, float green, float blue, float alpha, int steps)
{
    if(!p) Con_Error("FIPage_SetFilterColorAndAlpha: Invalid page.");
    AnimatorVector4_Set(p->_filter, red, green, blue, alpha, steps);
}

void FIPage_SetPredefinedColor(fi_page_t* p, uint idx, float red, float green, float blue, int steps)
{
    if(!p) Con_Error("FIPage_SetPredefinedColor: Invalid page.");
    AnimatorVector3_Set(p->_textColor[idx], red, green, blue, steps);
}

#if _DEBUG
static void setupModelParamsForFIObject(rendmodelparams_t* params, const char* modelId, const float worldOffset[3])
{
    float pos[] = { SCREENWIDTH/2, SCREENHEIGHT/2, 0 };
    modeldef_t* mf = R_CheckIDModelFor(modelId);

    if(!mf)
        return;

    params->mf = mf;
    params->center[VX] = worldOffset[VX] + pos[VX];
    params->center[VY] = worldOffset[VZ] + pos[VZ];
    params->center[VZ] = worldOffset[VY] + pos[VY];
    params->distance = -10; /// \fixme inherit depth.
    params->yawAngleOffset   = (SCREENWIDTH/2  - pos[VX]) * weaponOffsetScale + 90;
    params->pitchAngleOffset = (SCREENHEIGHT/2 - pos[VY]) * weaponOffsetScale * weaponOffsetScaleY / 1000.0f;
    params->yaw = params->yawAngleOffset + 180;
    params->pitch = params->yawAngleOffset + 90;
    params->shineYawOffset = -vang;
    params->shinePitchOffset = vpitch + 90;
    params->shinepspriteCoordSpace = true;
    params->ambientColor[CR] = params->ambientColor[CG] = params->ambientColor[CB] = 1;
    params->ambientColor[CA] = 1;
    /**
     * \fixme This additional scale multiplier is necessary for the model
     * to be drawn at a scale consistent with the other object types
     * (e.g., Model compared to Pic).
     *
     * Both terms are also present in the other object scale calcs and can
     * therefore be refactored away.
     */
    params->extraScale = .1f - (.05f * .05f);

    // Lets get it spinning so we can better see whats going on.
    params->yaw += rFrameCount;
}
#endif

void UI2_Drawer(void)
{
    borderedprojectionstate_t borderedProjection;
    boolean bordered;

    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("UI2_Drawer: Not initialized yet!\n");
#endif
        return;
    }

    /// \fixme need to refactor.
    bordered = true; //(FI_ScriptActive() && FI_ScriptCmdExecuted());
    if(bordered)
    {   // Draw using the special bordered projection.
        R_ConfigureBorderedProjection(&borderedProjection);
        R_BeginBorderedProjection(&borderedProjection);
    }

    {uint i;
    for(i = 0; i < numPages; ++i)
    {
        fi_page_t* page = pages[i];
        page->drawer(page);
    }}

    if(bordered)
        R_EndBorderedProjection(&borderedProjection);
}

void FIData_PicThink(fi_object_t* obj)
{
    fidata_pic_t* p = (fidata_pic_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicThink: Not a FI_PIC.");

    // Call parent thinker.
    FIObject_Think(obj);

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
                S_LocalSound(f->sound, 0);
        }
    }
}

static void drawRect(fidata_pic_t* p, uint frame, float angle, const float worldOffset[3])
{
    assert(p->numFrames && frame < p->numFrames);
    {
    float mid[3];
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : 0);

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
    fidata_pic_frame_t* f = (p->numFrames? p->frames[frame] : 0);
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
static size_t buildGeometry(const float dimensions[3], DGLuint tex, const float rgba[4],
    const float rgba2[4], boolean flagTexFlip, rvertex_t** verts, rcolor_t** colors,
    rtexcoord_t** coords)
{
    static rvertex_t rvertices[4];
    static rcolor_t rcolors[4];
    static rtexcoord_t rcoords[4];

    V3_Set(rvertices[0].pos, 0,              0,              0);
    V3_Set(rvertices[1].pos, dimensions[VX], 0,              0);
    V3_Set(rvertices[2].pos, 0,              dimensions[VY], 0);
    V3_Set(rvertices[3].pos, dimensions[VX], dimensions[VY], 0);

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
    /*const*/ float scale[3], const float rgba[4], const float rgba2[4], float angle,
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
            V3_Set(offset, 0, 0, 0);
            V3_Set(dimensions, rawTex->width, rawTex->height, 0);
        }
        else if(f->type == PFT_XIMAGE)
        {
            tex = (DGLuint)f->texRef.tex;
            V3_Set(offset, 0, 0, 0);
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
        V3_Copy(dimensions, scale);
        V3_Set(scale, 1, 1, 1);
    }

    V3_Set(center, dimensions[VX] / 2, dimensions[VY] / 2, dimensions[VZ] / 2);

    V3_Sum(origin, _origin, center);
    V3_Subtract(origin, origin, offset);
    V3_Sum(origin, origin, worldOffset);

    V3_Subtract(originOffset, offset, center);
    offset[VX] *= scale[VX]; offset[VY] *= scale[VY]; offset[VZ] *= scale[VZ];
    V3_Sum(originOffset, originOffset, offset);

    numVerts = buildGeometry(dimensions, tex, rgba, rgba2, flipTextureS, &rvertices, &rcolors, &rcoords);

    // Setup the transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);

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
    glScalef(scale[VX], scale[VY], scale[VZ]);

    drawGeometry(tex, numVerts, rvertices, rcolors, rcoords);

    if(showEdges)
    {
        // The edges never have a texture.
        DGL_Disable(DGL_TEXTURING);

        glBegin(GL_LINES);
            useColor(p->edgeColor, 4);
            glVertex2f(0, 0);
            glVertex2f(dimensions[VX], 0);
            glVertex2f(dimensions[VX], 0);

            useColor(p->otherEdgeColor, 4);
            glVertex2f(dimensions[VX], dimensions[VY]);
            glVertex2f(dimensions[VX], dimensions[VY]);
            glVertex2f(0, dimensions[VY]);
            glVertex2f(0, dimensions[VY]);

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

void FIData_PicDraw(fi_object_t* obj, const float offset[3])
{
    fidata_pic_t* p = (fidata_pic_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicDraw: Not a FI_PIC.");

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

    drawPicFrame(p, p->curFrame, origin, scale, rgba, (p->numFrames==0? rgba2 : rgba), p->angle.value, offset);
    }
}

uint FIData_PicAppendFrame(fi_object_t* obj, int type, int tics, void* texRef, short sound,
    boolean flagFlipH)
{
    fidata_pic_t* p = (fidata_pic_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicAppendFrame: Not a FI_PIC.");
    picAddFrame(p, createPicFrame(type, tics, texRef, sound, flagFlipH));
    return p->numFrames-1;
}

void FIData_PicClearAnimation(fi_object_t* obj)
{
    fidata_pic_t* p = (fidata_pic_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicClearAnimation: Not a FI_PIC.");
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

void FIData_TextThink(fi_object_t* obj)
{
    fidata_text_t* t = (fidata_text_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_TextThink: Not a FI_TEXT.");

    // Call parent thinker.
    FIObject_Think(obj);

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
    t->animComplete = (!t->wait || t->cursorPos >= FIData_TextLength((fi_object_t*)t));
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

void FIData_TextDraw(fi_object_t* obj, const float offset[3])
{
    fidata_text_t* t = (fidata_text_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_TextDraw: Not a FI_TEXT.");

    {
    int x = 0, y = 0;
    int ch, linew = -1;
    char* ptr;
    size_t cnt;

    if(!t->text)
        return;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);
    glTranslatef(t->pos[0].value + offset[VX], t->pos[1].value + offset[VY], t->pos[2].value + offset[VZ]);

    if(t->angle.value != 0)
    {
        // Counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(t->angle.value, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    glScalef(t->scale[0].value, t->scale[1].value, t->scale[2].value);

    // Draw it.
    // Set color zero (the normal color).
    useColor(t->color, 4);
    for(cnt = 0, ptr = t->text; *ptr && (!t->wait || cnt < t->cursorPos); ptr++)
    {
        if(linew < 0)
            linew = textLineWidth(ptr, t->font);

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

                //if(!colorIdx)
                {   // Use the default color.
                    color = (animatorvector3_t*) &t->color;
                }
                /*else
                {   /// \fixme disabled for now as this violates our ownership model.
                    finale_t* f = finalesById(stackTop());
                    color = &f->_interpreter->_pages[PAGE_TEXT]->_textColor[colorIdx-1];
                }*/

                glColor4f((*color)[0].value, (*color)[1].value, (*color)[2].value, t->color[3].value);
                continue;
            }

            // 'w' = half a second wait, 'W' = second'f wait
            if(*ptr == 'w' || *ptr == 'W') // Wait?
            {
                if(t->wait)
                    cnt += (int) ((float)TICRATE / t->wait / (*ptr == 'w' ? 2 : 1));
                continue;
            }

            // 'p' = 5 second wait, 'P' = 10 second wait
            if(*ptr == 'p' || *ptr == 'P') // Longer pause?
            {
                if(t->wait)
                    cnt += (int) ((float)TICRATE / t->wait * (*ptr == 'p' ? 5 : 10));
                continue;
            }

            if(*ptr == 'n' || *ptr == 'N') // Newline?
            {
                x = 0;
                y += GL_CharHeight('A', t->font) * (1+t->lineHeight);
                linew = -1;
                cnt++; // Include newlines in the wait count.
                continue;
            }

            if(*ptr == '_')
                ch = ' ';
        }

        // Let'f do Y-clipping (in case of tall text blocks).
        if(t->scale[1].value * y + t->pos[1].value >= -t->scale[1].value * t->lineHeight &&
           t->scale[1].value * y + t->pos[1].value < SCREENHEIGHT)
        {
            GL_DrawChar2(ch, (t->textFlags & DTF_ALIGN_LEFT) ? x : x - linew / 2, y, t->font);
            x += GL_CharWidth(ch, t->font);
        }

        cnt++; // Actual character drawn.
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    }
}

size_t FIData_TextLength(fi_object_t* obj)
{
    fidata_text_t* t = (fidata_text_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_TextLength: Not a FI_TEXT.");

    {
    size_t cnt = 0;
    if(t->text)
    {
        float secondLen = (t->wait ? TICRATE / t->wait : 0);
        const char* ptr;
        for(ptr = t->text; *ptr; ptr++)
        {
            if(*ptr == '\\') // Escape?
            {
                if(!*++ptr)
                    break;
                switch(*ptr)
                {
                case 'w':   cnt += secondLen / 2;   break;
                case 'W':   cnt += secondLen;       break;
                case 'p':   cnt += 5 * secondLen;   break;
                case 'P':   cnt += 10 * secondLen;  break;
                default:
                    if((*ptr >= '0' && *ptr <= '9') || *ptr == 'n' || *ptr == 'N')
                        continue;
                }
            }
            cnt++;
        }
    }
    return cnt;
    }
}

void FIData_TextCopy(fi_object_t* obj, const char* str)
{
    fidata_text_t* t = (fidata_text_t*)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_TextCopy: Not a FI_TEXT.");

    if(t->text)
    {
        Z_Free(t->text); t->text = 0;
    }

    if(str && str[0])
    {
        size_t len = strlen(str) + 1;
        t->text = Z_Malloc(len, PU_STATIC, 0);
        memcpy(t->text, str, len);
    }
}
