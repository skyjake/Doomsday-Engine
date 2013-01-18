/** @file ui2_main.cpp UI Widgets
 * @ingroup ui
 *
 * @authors Copyright &copy; 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "de_console.h"
#include "de_ui.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_resource.h"

#include "gl/sys_opengl.h" // TODO: get rid of this

#include <de/memory.h>
#include <de/memoryzone.h>
#include <cstring> // memcpy, memmove

#include "resource/materialsnapshot.h"

using namespace de;

fidata_text_t *P_CreateText(fi_objectid_t id, char const *name, fontid_t fontNum);
void P_DestroyText(fidata_text_t *text);

fidata_pic_t *P_CreatePic(fi_objectid_t id, char const *name);
void P_DestroyPic(fidata_pic_t* pic);

static boolean inited = false;
static uint numPages;
static fi_page_t **pages;

/// Global object store.
static fi_object_collection_t objects;

static fi_page_t *pagesAdd(fi_page_t *p)
{
    pages = (fi_page_t **) Z_Realloc(pages, sizeof(*pages) * ++numPages, PU_APPSTATIC);
    pages[numPages-1] = p;
    return p;
}

static fi_page_t *pagesRemove(fi_page_t *p)
{
    for(uint i = 0; i < numPages; ++i)
    {
        if(pages[i] != p) continue;

        if(i != numPages-1)
        {
            std::memmove(&pages[i], &pages[i+1], sizeof(*pages) * (numPages-i));
        }

        if(numPages > 1)
        {
            pages = (fi_page_t **) Z_Realloc(pages, sizeof(*pages) * --numPages, PU_APPSTATIC);
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
static void pageClear(fi_page_t *p)
{
    p->_timer = 0;
    p->flags.showBackground = true; /// Draw background by default.
    p->_bg.material = 0; // No background material.

    if(p->_objects.vector)
    {
        Z_Free(p->_objects.vector); p->_objects.vector = 0;
    }
    p->_objects.size = 0;

    AnimatorVector3_Init(p->_offset, 0, 0, 0);
    AnimatorVector4_Init(p->_bg.topColor, 1, 1, 1, 0);
    AnimatorVector4_Init(p->_bg.bottomColor, 1, 1, 1, 0);
    AnimatorVector4_Init(p->_filter, 0, 0, 0, 0);
    memset(p->_preFont, 0, sizeof(p->_preFont));

    for(uint i = 0; i < FIPAGE_NUM_PREDEFINED_COLORS; ++i)
    {
        AnimatorVector3_Init(p->_preColor[i], 1, 1, 1);
    }
}

static fi_page_t *newPage(fi_page_t *prevPage)
{
    fi_page_t *p = (fi_page_t *) Z_Malloc(sizeof(*p), PU_APPSTATIC, 0);
    p->flags.hidden = p->flags.paused = p->flags.showBackground = 0;
    p->_objects.vector = 0;
    p->drawer = FIPage_Drawer;
    p->ticker = FIPage_Ticker;
    p->previous = prevPage;
    pageClear(p);
    return p;
}

static void objectsThink(fi_object_collection_t *c)
{
    for(uint i = 0; i < c->size; ++i)
    {
        fi_object_t *obj = c->vector[i];
        obj->thinker(obj);
    }
}

static void objectsDraw(fi_object_collection_t *c, fi_obtype_e type,
    float const worldOrigin[3])
{
    for(uint i = 0; i < c->size; ++i)
    {
        fi_object_t *obj = c->vector[i];
        if(type != FI_NONE && obj->type != type) continue;
        obj->drawer(obj, worldOrigin);
    }
}

static uint objectsToIndex(fi_object_collection_t *c, fi_object_t *obj)
{
    if(obj)
    {
        for(uint i = 0; i < c->size; ++i)
        {
            fi_object_t *other = c->vector[i];
            if(other == obj)
                return i + 1; // 1-based index.
        }
    }
    return 0;
}

static inline bool objectsIsPresent(fi_object_collection_t *c, fi_object_t *obj)
{
    return objectsToIndex(c, obj) != 0;
}

/**
 * @note Does not check if the object already exists in this collection.
 */
static fi_object_t *objectsAdd(fi_object_collection_t *c, fi_object_t *obj)
{
    c->vector = (fi_object_t **) Z_Realloc(c->vector, sizeof(*c->vector) * ++c->size, PU_APPSTATIC);
    c->vector[c->size-1] = obj;
    return obj;
}

/**
 * @pre There is at most one reference to the object in this collection.
 */
static fi_object_t *objectsRemove(fi_object_collection_t *c, fi_object_t *obj)
{
    uint idx;
    if((idx = objectsToIndex(c, obj)))
    {
        idx -= 1; // Indices are 1-based.

        if(idx != c->size-1)
        {
            std::memmove(&c->vector[idx], &c->vector[idx+1], sizeof(*c->vector) * (c->size-idx));
        }

        if(c->size > 1)
        {
            c->vector = (fi_object_t **) Z_Realloc(c->vector, sizeof(*c->vector) * --c->size, PU_APPSTATIC);
        }
        else
        {
            Z_Free(c->vector); c->vector = 0;
            c->size = 0;
        }
    }
    return obj;
}

static void objectsEmpty(fi_object_collection_t *c)
{
    if(c->size)
    {
        for(uint i = 0; i < c->size; ++i)
        {
            fi_object_t *obj = c->vector[i];
            switch(obj->type)
            {
            case FI_PIC:    P_DestroyPic((fidata_pic_t *)obj);   break;
            case FI_TEXT:   P_DestroyText((fidata_text_t *)obj); break;
            default:
                Con_Error("InFine: Unknown object type %i in objectsEmpty.", int(obj->type));
            }
        }
        Z_Free(c->vector);
    }
    c->vector = 0;
    c->size = 0;
}

static fi_object_t *objectsById(fi_object_collection_t *c, fi_objectid_t id)
{
    if(id != 0)
    {
        for(uint i = 0; i < c->size; ++i)
        {
            fi_object_t *obj = c->vector[i];
            if(obj->id == id)
                return obj;
        }
    }
    return 0;
}

/**
 * @return  A new (unused) unique object id.
 */
static fi_objectid_t objectsUniqueId(fi_object_collection_t *c)
{
    fi_objectid_t id = 0;
    while(objectsById(c, ++id)) {}
    return id;
}

static void picFrameDeleteXImage(fidata_pic_frame_t *f)
{
#ifdef __CLIENT__
    DGL_DeleteTextures(1, (DGLuint *)&f->texRef.tex);
#endif
    f->texRef.tex = 0;
}

static fidata_pic_frame_t *createPicFrame(fi_pic_type_t type, int tics, void *texRef, short sound, boolean flagFlipH)
{
    fidata_pic_frame_t * f = (fidata_pic_frame_t *) Z_Malloc(sizeof(*f), PU_APPSTATIC, 0);
    f->flags.flip = flagFlipH;
    f->type = type;
    f->tics = tics;
    switch(f->type)
    {
    case PFT_MATERIAL:  f->texRef.material =  ((material_t *)texRef); break;
    case PFT_PATCH:     f->texRef.patch    = *((patchid_t *)texRef);  break;
    case PFT_RAW:       f->texRef.lumpNum  = *((lumpnum_t *)texRef);  break;
    case PFT_XIMAGE:    f->texRef.tex      = *((DGLuint *)texRef);    break;
    default:
        Con_Error("createPicFrame: unknown frame type %i.", int(type));
    }
    f->sound = sound;
    return f;
}

static void destroyPicFrame(fidata_pic_frame_t *f)
{
    if(f->type == PFT_XIMAGE)
        picFrameDeleteXImage(f);
    Z_Free(f);
}

static fidata_pic_frame_t* picAddFrame(fidata_pic_t *p, fidata_pic_frame_t *f)
{
    p->frames = (fidata_pic_frame_t **) Z_Realloc(p->frames, sizeof(*p->frames) * ++p->numFrames, PU_APPSTATIC);
    p->frames[p->numFrames-1] = f;
    return f;
}

static void objectSetName(fi_object_t *obj, char const *name)
{
    dd_snprintf(obj->name, FI_NAME_MAX_LENGTH, "%s", name);
}

void UI_Init(void)
{
    // Already been here?
    if(inited) return;

    std::memset(&objects, 0, sizeof(objects));
    pages = 0; numPages = 0;

    inited = true;
}

void UI_Shutdown(void)
{
    if(!inited) return;

    // Garbage collection.
    objectsEmpty(&objects);
    if(numPages)
    {
        for(uint i = 0; i < numPages; ++i)
        {
            fi_page_t *p = pages[i];
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
#ifdef __CLIENT__
    // Always tic.
    FR_Ticker(ticLength);
#endif

    if(!inited) return;

    // All pages tic unless paused.
    for(uint i = 0; i < numPages; ++i)
    {
        fi_page_t *page = pages[i];
        page->ticker(page, ticLength);
    }
}

void FIObject_Delete(fi_object_t *obj)
{
    DENG_ASSERT(obj);
    // Destroy all references to this object on all pages.
    for(uint i = 0; i < numPages; ++i)
    {
        FIPage_RemoveObject(pages[i], obj);
    }
    objectsRemove(&objects, obj);
    Z_Free(obj);
}

fidata_pic_t *P_CreatePic(fi_objectid_t id, char const *name)
{
    fidata_pic_t *p = (fidata_pic_t *) Z_Calloc(sizeof(*p), PU_APPSTATIC, 0);

    p->type = FI_PIC;
    p->drawer = FIData_PicDraw;
    p->thinker = FIData_PicThink;
    p->id = id;
    p->flags.looping = false;
    p->animComplete = true;
    objectSetName((fi_object_t *)p, name);
    AnimatorVector4_Init(p->color, 1, 1, 1, 1);
    AnimatorVector3_Init(p->scale, 1, 1, 1);

    FIData_PicClearAnimation((fi_object_t *)p);
    return p;
}

void P_DestroyPic(fidata_pic_t *pic)
{
    DENG_ASSERT(pic);
    FIData_PicClearAnimation((fi_object_t *)pic);
    // Call parent destructor.
    FIObject_Delete((fi_object_t *)pic);
}

fidata_text_t *P_CreateText(fi_objectid_t id, char const *name, fontid_t fontNum)
{
#define LEADING             (11.f/7-1)

    fidata_text_t *t = (fidata_text_t *) Z_Calloc(sizeof(*t), PU_APPSTATIC, 0);

    t->type = FI_TEXT;
    t->drawer = FIData_TextDraw;
    t->thinker = FIData_TextThink;
    t->id = id;
    t->flags.looping = false;
    t->animComplete = true;
    t->alignFlags = ALIGN_TOPLEFT;
    t->textFlags = DTF_ONLY_SHADOW;
    objectSetName((fi_object_t*)t, name);
    t->pageColor = 0; // Do not use a predefined color by default.
    AnimatorVector4_Init(t->color, 1, 1, 1, 1);
    AnimatorVector3_Init(t->scale, 1, 1, 1);

    t->wait = 3;
    t->fontNum = fontNum;
    t->lineHeight = LEADING;

    return t;

#undef LEADING
}

void P_DestroyText(fidata_text_t *text)
{
    DENG_ASSERT(text);
    if(text->text)
    {
        Z_Free(text->text); text->text = 0;
    }
    // Call parent destructor.
    FIObject_Delete((fi_object_t *)text);
}

void FIObject_Think(fi_object_t *obj)
{
    DENG_ASSERT(obj);
    AnimatorVector3_Think(obj->pos);
    AnimatorVector3_Think(obj->scale);
    Animator_Think(&obj->angle);
}

struct fi_page_s *FIObject_Page(struct fi_object_s *obj)
{
    DENG_ASSERT(obj);
    return obj->page;
}

void FIObject_SetPage(struct fi_object_s *obj, struct fi_page_s *page)
{
    DENG_ASSERT(obj);
    obj->page = page;
}

fi_page_t *FI_NewPage(fi_page_t *prevPage)
{
    return pagesAdd(newPage(prevPage));
}

void FI_DeletePage(fi_page_t *p)
{
    if(!p) Con_Error("FI_DeletePage: Invalid page.");

    pageClear(p);
    pagesRemove(p);
    for(uint i = 0; i < numPages; ++i)
    {
        fi_page_t *other = pages[i];
        if(other->previous == p)
            other->previous = 0;
    }
    Z_Free(p);
}

fi_object_t *FI_Object(fi_objectid_t id)
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

fi_object_t *FI_NewObject(fi_obtype_e type, char const *name)
{
    fi_object_t *obj;
    switch(type)
    {
    case FI_TEXT: obj = (fi_object_t *) P_CreateText(objectsUniqueId(&objects), name, 0);   break;
    case FI_PIC:  obj = (fi_object_t *) P_CreatePic(objectsUniqueId(&objects), name);       break;
    default:
        Con_Error("FI_NewObject: Unknown type %i.", type);
        exit(1); // Unreachable.
    }
    return objectsAdd(&objects, obj);
}

void FI_DeleteObject(fi_object_t *obj)
{
    DENG_ASSERT(obj);
    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("FI_DeleteObject: Not initialized yet!\n");
#endif
        return;
    }
    switch(obj->type)
    {
    case FI_PIC:    P_DestroyPic((fidata_pic_t *)obj);   break;
    case FI_TEXT:   P_DestroyText((fidata_text_t *)obj); break;
    default:
        Con_Error("FI_DeleteObject: Invalid type %i.", int(obj->type));
    }
}

static void useColor(animator_t const *color, int components)
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

static void drawPageBackground(fi_page_t *p, float x, float y, float width, float height,
    float light, float alpha)
{
#ifdef __CLIENT__
    vec3f_t topColor, bottomColor;
    float topAlpha, bottomAlpha;

    V3f_Set(topColor,    p->_bg.topColor   [0].value * light, p->_bg.topColor   [1].value * light, p->_bg.topColor   [2].value * light);
    topAlpha = p->_bg.topColor[3].value * alpha;

    V3f_Set(bottomColor, p->_bg.bottomColor[0].value * light, p->_bg.bottomColor[1].value * light, p->_bg.bottomColor[2].value * light);
    bottomAlpha = p->_bg.bottomColor[3].value * alpha;

    if(topAlpha <= 0 && bottomAlpha <= 0) return;

    if(p->_bg.material)
    {
        MaterialVariantSpec const &spec =
            App_Materials()->variantSpecForContext(MC_UI, 0, 0, 0, 0, GL_REPEAT, GL_REPEAT,
                                                   0, 1, 0, false, false, false, false);
        MaterialSnapshot const &ms = App_Materials()->prepare(*p->_bg.material, spec, true);

        GL_BindTexture(reinterpret_cast<texturevariant_s *>(&ms.texture(MTU_PRIMARY)));
        glEnable(GL_TEXTURE_2D);
    }

    if(p->_bg.material || topAlpha < 1.0 || bottomAlpha < 1.0)
    {
        GL_BlendMode(BM_NORMAL);
    }
    else
    {
        glDisable(GL_BLEND);
    }

    GL_DrawRectf2TextureColor(x, y, width, height, 64, 64, topColor, topAlpha, bottomColor, bottomAlpha);

    GL_SetNoTexture();
    glEnable(GL_BLEND);
#endif
}

void FIPage_Drawer(fi_page_t *p)
{
#ifdef __CLIENT__
    if(!p) Con_Error("FIPage_Drawer: Invalid page.");

    if(p->flags.hidden) return;

    // First, draw the background.
    if(p->flags.showBackground)
        drawPageBackground(p, 0, 0, SCREENWIDTH, SCREENHEIGHT, 1.0, 1.0);

    // Now lets go into 3D mode for drawing the p objects.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    //glLoadIdentity();

    GL_SetMultisample(true);

    // The 3D projection matrix.
    // We're assuming pixels are squares.
    /*float aspect = theWindow->width / (float) theWindow->height;
    yfov = 2 * RAD2DEG(atan(tan(DEG2RAD(90) / 2) / aspect));
    GL_InfinitePerspective(yfov, aspect, .05f);*/

    // We need a left-handed yflipped coordinate system.
    //glScalef(1, -1, -1);

    // Clear Z buffer (prevent the objects being clipped by nearby polygons).
    glClear(GL_DEPTH_BUFFER_BIT);

    if(renderWireframe > 1)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    //glEnable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);

    vec3f_t worldOrigin;
    V3f_Set(worldOrigin, /*-SCREENWIDTH/2*/ - p->_offset[VX].value,
                         /*-SCREENHEIGHT/2*/ - p->_offset[VY].value,
                         0/*.05f - p->_offset[VZ].value*/);
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

    // Restore original matrices and state: back to normal 2D.
    glDisable(GL_ALPHA_TEST);
    //glDisable(GL_CULL_FACE);
    // Back from wireframe mode?
    if(renderWireframe > 1)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Filter on top of everything. Only draw if necessary.
    if(p->_filter[3].value > 0)
    {
        GL_DrawRectf2Color(0, 0, SCREENWIDTH, SCREENHEIGHT, p->_filter[0].value, p->_filter[1].value, p->_filter[2].value, p->_filter[3].value);
    }

    GL_SetMultisample(false);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
#endif
}

void FIPage_MakeVisible(fi_page_t *p, boolean yes)
{
    if(!p) Con_Error("FIPage_MakeVisible: Invalid page.");
    p->flags.hidden = !yes;
}

void FIPage_Pause(fi_page_t *p, boolean yes)
{
    if(!p) Con_Error("FIPage_Pause: Invalid page.");
    p->flags.paused = yes;
}

void FIPage_Ticker(fi_page_t *p, timespan_t /*ticLength*/)
{
    if(!p) Con_Error("FIPage_Ticker: Invalid page.");

    if(!DD_IsSharpTick()) return;

    // A new 'sharp' tick has begun.
    p->_timer++;

    objectsThink(&p->_objects);

    AnimatorVector3_Think(p->_offset);
    AnimatorVector4_Think(p->_bg.topColor);
    AnimatorVector4_Think(p->_bg.bottomColor);
    AnimatorVector4_Think(p->_filter);
    for(uint i = 0; i < FIPAGE_NUM_PREDEFINED_COLORS; ++i)
    {
        AnimatorVector3_Think(p->_preColor[i]);
    }
}

boolean FIPage_HasObject(fi_page_t *p, fi_object_t *obj)
{
    if(!p) Con_Error("FIPage_HasObject: Invalid page.");
    return objectsIsPresent(&p->_objects, obj);
}

fi_object_t *FIPage_AddObject(fi_page_t *p, fi_object_t *obj)
{
    if(!p) Con_Error("FIPage_AddObject: Invalid page.");
    if(obj && !objectsIsPresent(&p->_objects, obj))
    {
        FIObject_SetPage(obj, p);
        return objectsAdd(&p->_objects, obj);
    }
    return obj;
}

fi_object_t *FIPage_RemoveObject(fi_page_t *p, fi_object_t *obj)
{
    if(!p) Con_Error("FIPage_RemoveObject: Invalid page.");
    if(obj && objectsIsPresent(&p->_objects, obj))
    {
        FIObject_SetPage(obj, NULL);
        return objectsRemove(&p->_objects, obj);
    }
    return obj;
}

material_t *FIPage_BackgroundMaterial(fi_page_t *p)
{
    if(!p) Con_Error("FIPage_BackgroundMaterial: Invalid page.");
    return p->_bg.material;
}

void FIPage_SetBackgroundMaterial(fi_page_t *p, material_t *mat)
{
    if(!p) Con_Error("FIPage_SetBackgroundMaterial: Invalid page.");
    p->_bg.material = mat;
}

void FIPage_SetBackgroundTopColor(fi_page_t *p, float red, float green, float blue, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundTopColor: Invalid page.");
    AnimatorVector3_Set(p->_bg.topColor, red, green, blue, steps);
}

void FIPage_SetBackgroundTopColorAndAlpha(fi_page_t *p, float red, float green, float blue, float alpha, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundTopColorAndAlpha: Invalid page.");
    AnimatorVector4_Set(p->_bg.topColor, red, green, blue, alpha, steps);
}

void FIPage_SetBackgroundBottomColor(fi_page_t *p, float red, float green, float blue, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundBottomColor: Invalid page.");
    AnimatorVector3_Set(p->_bg.bottomColor, red, green, blue, steps);
}

void FIPage_SetBackgroundBottomColorAndAlpha(fi_page_t *p, float red, float green, float blue, float alpha, int steps)
{
    if(!p) Con_Error("FIPage_SetBackgroundBottomColorAndAlpha: Invalid page.");
    AnimatorVector4_Set(p->_bg.bottomColor, red, green, blue, alpha, steps);
}

void FIPage_SetOffsetX(fi_page_t *p, float x, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetX: Invalid page.");
    Animator_Set(&p->_offset[VX], x, steps);
}

void FIPage_SetOffsetY(fi_page_t *p, float y, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetY: Invalid page.");
    Animator_Set(&p->_offset[VY], y, steps);
}

void FIPage_SetOffsetZ(fi_page_t *p, float y, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetY: Invalid page.");
    Animator_Set(&p->_offset[VZ], y, steps);
}

void FIPage_SetOffsetXYZ(fi_page_t *p, float x, float y, float z, int steps)
{
    if(!p) Con_Error("FIPage_SetOffsetXYZ: Invalid page.");
    AnimatorVector3_Set(p->_offset, x, y, z, steps);
}

void FIPage_SetFilterColorAndAlpha(fi_page_t *p, float red, float green, float blue, float alpha, int steps)
{
    if(!p) Con_Error("FIPage_SetFilterColorAndAlpha: Invalid page.");
    AnimatorVector4_Set(p->_filter, red, green, blue, alpha, steps);
}

void FIPage_SetPredefinedColor(fi_page_t *p, uint idx, float red, float green, float blue, int steps)
{
    if(!p) Con_Error("FIPage_SetPredefinedColor: Invalid page.");
    if(!VALID_FIPAGE_PREDEFINED_COLOR(idx)) Con_Error("FIPage_SetPredefinedColor: Invalid color id %u.", idx);
    AnimatorVector3_Set(p->_preColor[idx], red, green, blue, steps);
}

animatorvector3_t const *FIPage_PredefinedColor(fi_page_t *p, uint idx)
{
    if(!p) Con_Error("FIPage_PredefinedColor: Invalid page.");
    if(!VALID_FIPAGE_PREDEFINED_COLOR(idx)) Con_Error("FIPage_PredefinedColor: Invalid color id %u.", idx);
    return (animatorvector3_t const *) &p->_preColor[idx];
}

void FIPage_SetPredefinedFont(fi_page_t *p, uint idx, fontid_t fontNum)
{
    if(!p) Con_Error("FIPage_SetPredefinedFont: Invalid page.");
    if(!VALID_FIPAGE_PREDEFINED_FONT(idx)) Con_Error("FIPage_SetPredefinedFont: Invalid font id %u.", idx);
    p->_preFont[idx] = fontNum;
}

fontid_t FIPage_PredefinedFont(fi_page_t *p, uint idx)
{
    if(!p) Con_Error("FIPage_PredefinedFont: Invalid page.");
    if(!VALID_FIPAGE_PREDEFINED_FONT(idx)) Con_Error("FIPage_PredefinedFont: Invalid font id %u.", idx);
    return p->_preFont[idx];
}

#if 0
static void setupModelParamsForFIObject(rendmodelparams_t *params,
    char const *modelId, float const worldOffset[3])
{
    float pos[] = { SCREENWIDTH/2, SCREENHEIGHT/2, 0 };
    modeldef_t* mf = Models_Definition(modelId);
    if(!mf) return;

    params->mf = mf;
    params->origin[VX] = worldOffset[VX] + pos[VX];
    params->origin[VY] = worldOffset[VZ] + pos[VZ];
    params->origin[VZ] = worldOffset[VY] + pos[VY];
    params->distance = -10; /// @todo inherit depth.
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
     * @todo This additional scale multiplier is necessary for the model
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
    //borderedprojectionstate_t borderedProjection;
    //boolean bordered;

    if(!inited)
    {
#ifdef _DEBUG
        Con_Printf("UI2_Drawer: Not initialized yet!\n");
#endif
        return;
    }

    /// @todo need to refactor.
    /*bordered = (FI_ScriptActive() && FI_ScriptCmdExecuted());
    if(bordered)
    {
        // Draw using the special bordered projection.
        GL_ConfigureBorderedProjection(&borderedProjection);
        GL_BeginBorderedProjection(&borderedProjection);
    }*/

    for(uint i = 0; i < numPages; ++i)
    {
        fi_page_t *page = pages[i];
        page->drawer(page);
    }

    //if(bordered)
    //    GL_EndBorderedProjection(&borderedProjection);
}

void FIData_PicThink(fi_object_t *obj)
{
    fidata_pic_t *p = (fidata_pic_t *)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicThink: Not a FI_PIC.");

    // Call parent thinker.
    FIObject_Think(obj);

    AnimatorVector4_Think(p->color);
    AnimatorVector4_Think(p->otherColor);
    AnimatorVector4_Think(p->edgeColor);
    AnimatorVector4_Think(p->otherEdgeColor);

    if(!(p->numFrames > 1)) return;

    // If animating, decrease the sequence timer.
    if(p->frames[p->curFrame]->tics > 0)
    {
        if(--p->tics <= 0)
        {
            fidata_pic_frame_t *f;
            // Advance the sequence position. k = next pos.
            uint next = p->curFrame + 1;

            if(next == p->numFrames)
            {
                // This is the end.
                p->animComplete = true;

                // Stop the sequence?
                if(p->flags.looping)
                {
                    next = 0; // Rewind back to beginning.
                }
                else // Yes.
                {
                    p->frames[next = p->curFrame]->tics = 0;
                }
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

/**
 * Vertex layout:
 *
 * 0 - 1
 * | / |
 * 2 - 3
 */
static size_t buildGeometry(float const /*dimensions*/[3], boolean flipTextureS,
    float const rgba[4], float const rgba2[4], rvertex_t **verts,
    ColorRawf **colors, rtexcoord_t **coords)
{
    static rvertex_t rvertices[4];
    static ColorRawf rcolors[4];
    static rtexcoord_t rcoords[4];

    V3f_Set(rvertices[0].pos, 0, 0, 0);
    V3f_Set(rvertices[1].pos, 1, 0, 0);
    V3f_Set(rvertices[2].pos, 0, 1, 0);
    V3f_Set(rvertices[3].pos, 1, 1, 0);

    V2f_Set(rcoords[0].st, (flipTextureS? 1:0), 0);
    V2f_Set(rcoords[1].st, (flipTextureS? 0:1), 0);
    V2f_Set(rcoords[2].st, (flipTextureS? 1:0), 1);
    V2f_Set(rcoords[3].st, (flipTextureS? 0:1), 1);

    V4f_Copy(rcolors[0].rgba, rgba);
    V4f_Copy(rcolors[1].rgba, rgba);
    V4f_Copy(rcolors[2].rgba, rgba2);
    V4f_Copy(rcolors[3].rgba, rgba2);

    *verts = rvertices;
    *coords = rcoords;
    *colors = rcolors;
    return 4;
}

static void drawGeometry(size_t numVerts, rvertex_t const *verts,
    ColorRawf const *colors, rtexcoord_t const *coords)
{
    glBegin(GL_TRIANGLE_STRIP);
    for(size_t i = 0; i < numVerts; ++i)
    {
        if(coords) glTexCoord2fv(coords[i].st);
        if(colors) glColor4fv(colors[i].rgba);
        glVertex3fv(verts[i].pos);
    }
    glEnd();
}

static void drawPicFrame(fidata_pic_t *p, uint frame, float const _origin[3],
    float /*const*/ scale[3], float const rgba[4], float const rgba2[4], float angle,
    float const worldOffset[3])
{
#ifdef __CLIENT__
    vec3f_t offset = { 0, 0, 0 }, dimensions, origin, originOffset, center;
    vec2f_t texScale = { 1, 1 };
    vec2f_t rotateCenter = { .5f, .5f };
    boolean showEdges = true, flipTextureS = false;
    boolean mustPopTextureMatrix = false;
    boolean textureEnabled = false;
    size_t numVerts;
    rvertex_t *rvertices;
    ColorRawf *rcolors;
    rtexcoord_t *rcoords;

    if(p->numFrames)
    {
        /// @todo Optimize: Texture/Material searches should be NOT be done here -ds
        fidata_pic_frame_t *f = p->frames[frame];

        flipTextureS = (f->flags.flip != 0);
        showEdges = false;

        switch(f->type)
        {
        case PFT_RAW: {
            rawtex_t *rawTex = R_GetRawTex(f->texRef.lumpNum);
            if(rawTex)
            {
                DGLuint glName = GL_PrepareRawTexture(rawTex);
                V3f_Set(offset, 0, 0, 0);
                // Raw images are always considered to have a logical size of 320x200
                // even though the actual texture resolution may be different.
                V3f_Set(dimensions, 320 /*rawTex->width*/, 200 /*rawTex->height*/, 0);
                // Rotation occurs around the center of the screen.
                V2f_Set(rotateCenter, 160, 100);
                GL_BindTextureUnmanaged(glName, (filterUI ? GL_LINEAR : GL_NEAREST));
                if(glName)
                {
                    glEnable(GL_TEXTURE_2D);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                    textureEnabled = true;
                }
            }
            break; }

        case PFT_XIMAGE:
            V3f_Set(offset, 0, 0, 0);
            V3f_Set(dimensions, 1, 1, 0);
            V2f_Set(rotateCenter, .5f, .5f);
            GL_BindTextureUnmanaged(f->texRef.tex, (filterUI ? GL_LINEAR : GL_NEAREST));
            if(f->texRef.tex)
            {
                glEnable(GL_TEXTURE_2D);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                textureEnabled = true;
            }
            break;

        case PFT_MATERIAL: {
            material_t *mat = f->texRef.material;
            if(mat)
            {
                MaterialVariantSpec const &spec =
                    App_Materials()->variantSpecForContext(MC_UI, 0, 0, 0, 0, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE,
                                                           0, -3, 0, false, false, false, false);
                MaterialSnapshot const &ms = App_Materials()->prepare(*mat, spec, true);

                GL_BindTexture(reinterpret_cast<texturevariant_s *>(&ms.texture(MTU_PRIMARY)));
                glEnable(GL_TEXTURE_2D);
                textureEnabled = true;

                texturevariantspecification_t const *texSpec = ms.texture(MTU_PRIMARY).spec();

                /// @todo Utilize *all* properties of the Material.
                V3f_Set(dimensions,
                        ms.dimensions().width()  + TS_GENERAL(texSpec)->border*2,
                        ms.dimensions().height() + TS_GENERAL(texSpec)->border*2, 0);
                V2f_Set(rotateCenter, dimensions[VX]/2, dimensions[VY]/2);
                ms.texture(MTU_PRIMARY).coords(&texScale[VX], &texScale[VY]);

                Texture const &texture = ms.texture(MTU_PRIMARY).generalCase();
                de::Uri uri = texture.manifest().composeUri();
                if(!uri.scheme().compareWithoutCase("Sprites"))
                {
                    V3f_Set(offset, texture.origin().x(), texture.origin().y(), 0);
                }
                else
                {
                    V3f_Set(offset, 0, 0, 0);
                }
            }
            break; }

        case PFT_PATCH: {
            Texture *texture = App_Textures()->scheme("Patches").findByUniqueId(f->texRef.patch).texture();
            if(texture)
            {
                GL_BindTexture(GL_PreparePatchTexture(reinterpret_cast<texture_s *>(texture)));
                glEnable(GL_TEXTURE_2D);
                textureEnabled = true;

                V3f_Set(offset, texture->origin().x(), texture->origin().y(), 0);
                V3f_Set(dimensions, texture->width(), texture->height(), 0);
                V2f_Set(rotateCenter, dimensions[VX]/2, dimensions[VY]/2);
            }
            break; }

        default:
            Con_Error("drawPicFrame: Invalid FI_PIC frame type %i.", int(f->type));
        }
    }

    // If we've not chosen a texture by now set some defaults.
    /// @todo This is some seriously funky logic... refactor or remove.
    if(!textureEnabled)
    {
        V3f_Copy(dimensions, scale);
        V3f_Set(scale, 1, 1, 1);
        V2f_Set(rotateCenter, dimensions[VX] / 2, dimensions[VY] / 2);
    }

    V3f_Set(center, dimensions[VX] / 2, dimensions[VY] / 2, dimensions[VZ] / 2);

    V3f_Sum(origin, _origin, center);
    V3f_Subtract(origin, origin, offset);
    V3f_Sum(origin, origin, worldOffset);

    V3f_Subtract(originOffset, offset, center);
    offset[VX] *= scale[VX]; offset[VY] *= scale[VY]; offset[VZ] *= scale[VZ];
    V3f_Sum(originOffset, originOffset, offset);

    numVerts = buildGeometry(dimensions, flipTextureS, rgba, rgba2, &rvertices, &rcolors, &rcoords);

    // Setup the transformation.
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    //glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);

    // Move to the object origin.
    glTranslatef(origin[VX], origin[VY], origin[VZ]);

    // Translate to the object center.
    /// @todo Remove this; just go to origin directly. Rotation origin is
    /// now separately in 'rotateCenter'. -jk
    glTranslatef(originOffset[VX], originOffset[VY], originOffset[VZ]);

    glScalef(scale[VX], scale[VY], scale[VZ]);

    if(angle != 0)
    {
        glTranslatef(rotateCenter[VX], rotateCenter[VY], 0);

        // With rotation we must counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(angle, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);

        glTranslatef(-rotateCenter[VX], -rotateCenter[VY], 0);
    }

    glMatrixMode(GL_MODELVIEW);
    // Scale up our unit-geometry to the desired dimensions.
    glScalef(dimensions[VX], dimensions[VY], dimensions[VZ]);

    if(texScale[0] != 1 || texScale[1] != 1)
    {
        glMatrixMode(GL_TEXTURE);
        glPushMatrix();
        glScalef(texScale[0], texScale[1], 1);
        mustPopTextureMatrix = true;
    }

    drawGeometry(numVerts, rvertices, rcolors, rcoords);

    GL_SetNoTexture();

    if(mustPopTextureMatrix)
    {
        glMatrixMode(GL_TEXTURE);
        glPopMatrix();
    }

    if(showEdges)
    {
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
    }

    // Restore original transformation.
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
#endif
}

void FIData_PicDraw(fi_object_t *obj, const float offset[3])
{
    fidata_pic_t *p = (fidata_pic_t *)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicDraw: Not a FI_PIC.");

    // Fully transparent pics will not be drawn.
    if(!(p->color[CA].value > 0)) return;

    vec3f_t scale, origin;
    V3f_Set(origin, p->pos[VX].value, p->pos[VY].value, p->pos[VZ].value);
    V3f_Set(scale, p->scale[VX].value, p->scale[VY].value, p->scale[VZ].value);

    vec4f_t rgba, rgba2;
    V4f_Set(rgba, p->color[CR].value, p->color[CG].value, p->color[CB].value, p->color[CA].value);
    if(p->numFrames == 0)
        V4f_Set(rgba2, p->otherColor[CR].value, p->otherColor[CG].value, p->otherColor[CB].value, p->otherColor[CA].value);

    drawPicFrame(p, p->curFrame, origin, scale, rgba, (p->numFrames==0? rgba2 : rgba), p->angle.value, offset);
}

uint FIData_PicAppendFrame(fi_object_t *obj, fi_pic_type_t type, int tics, void *texRef, short sound,
    boolean flagFlipH)
{
    fidata_pic_t * p = (fidata_pic_t *)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicAppendFrame: Not a FI_PIC.");
    picAddFrame(p, createPicFrame(type, tics, texRef, sound, flagFlipH));
    return p->numFrames-1;
}

void FIData_PicClearAnimation(fi_object_t *obj)
{
    fidata_pic_t *p = (fidata_pic_t *)obj;
    if(!obj || obj->type != FI_PIC) Con_Error("FIData_PicClearAnimation: Not a FI_PIC.");
    if(p->frames)
    {
        for(uint i = 0; i < p->numFrames; ++i)
        {
            destroyPicFrame(p->frames[i]);
        }
        Z_Free(p->frames);
    }
    p->flags.looping = false; // Yeah?
    p->frames = 0;
    p->numFrames = 0;
    p->curFrame = 0;
    p->animComplete = true;
}

void FIData_TextAccelerate(fi_object_t *obj)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextSkipCursorToEnd: Not a FI_TEXT.");

    // Fill in the rest very quickly.
    t->wait = -10;
}

void FIData_TextThink(fi_object_t *obj)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextThink: Not a FI_TEXT.");

    // Call parent thinker.
    FIObject_Think(obj);

    AnimatorVector4_Think(t->color);

    if(t->wait)
    {
        if(--t->timer <= 0)
        {
            if(t->wait > 0)
            {
                // Positive wait: move cursor one position, wait again.
                t->cursorPos++;
                t->timer = t->wait;
            }
            else
            {
                // Negative wait: move cursor several positions, don't wait.
                t->cursorPos += ABS(t->wait);
                t->timer = 1;
            }
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
    t->animComplete = (!t->wait || t->cursorPos >= FIData_TextLength((fi_object_t *)t));
}

static int textLineWidth(char const *text)
{
    int width = 0;

#ifdef __CLIENT__
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
        width += FR_CharWidth(*text);
    }
#endif

    return width;
}

void FIData_TextDraw(fi_object_t *obj, const float offset[3])
{
#ifdef __CLIENT__
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextDraw: Not a FI_TEXT.");

    if(!t->text) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    //glScalef(.1f/SCREENWIDTH, .1f/SCREENWIDTH, 1);
    glTranslatef(t->pos[0].value + offset[VX], t->pos[1].value + offset[VY], t->pos[2].value + offset[VZ]);

    if(t->angle.value != 0)
    {
        // Counter the VGA aspect ratio.
        glScalef(1, 200.0f / 240.0f, 1);
        glRotatef(t->angle.value, 0, 0, 1);
        glScalef(1, 240.0f / 200.0f, 1);
    }

    glScalef(t->scale[0].value, t->scale[1].value, t->scale[2].value);
    glEnable(GL_TEXTURE_2D);

    FR_SetFont(t->fontNum);

    // Set the normal color.
    animatorvector3_t const *color;
    if(t->pageColor == 0)
        color = (const animatorvector3_t*)&t->color;
    else
        color = FIPage_PredefinedColor(FIObject_Page(obj), t->pageColor-1);
    FR_SetColor((*color)[CR].value, (*color)[CG].value, (*color)[CB].value);
    FR_SetAlpha(t->color[CA].value);

    int x = 0, y = 0, ch, linew = -1;
    char *ptr = t->text;
    for(size_t cnt = 0; *ptr && (!t->wait || cnt < t->cursorPos); ptr++)
    {
        if(linew < 0)
            linew = textLineWidth(ptr);

        ch = *ptr;
        if(*ptr == '\\') // Escape?
        {
            if(!*++ptr)
                break;

            // Change of color?
            if(*ptr >= '0' && *ptr <= '9')
            {
                uint colorIdx = *ptr - '0';
                if(colorIdx == 0)
                    color = (animatorvector3_t const *)&t->color;
                else
                    color = FIPage_PredefinedColor(FIObject_Page(obj), colorIdx-1);
                FR_SetColor((*color)[CR].value, (*color)[CG].value, (*color)[CB].value);
                FR_SetAlpha(t->color[CA].value);
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
                y += FR_CharHeight('A') * (1+t->lineHeight);
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
            FR_DrawCharXY(ch, (t->alignFlags & ALIGN_LEFT) ? x : x - linew / 2, y);
            x += FR_CharWidth(ch);
        }

        ++cnt;
    }

    glDisable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
#endif
}

size_t FIData_TextLength(fi_object_t *obj)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextLength: Not a FI_TEXT.");

    size_t cnt = 0;
    if(t->text)
    {
        float secondLen = (t->wait ? TICRATE / t->wait : 0);
        char const *ptr;
        for(ptr = t->text; *ptr; ptr++)
        {
            if(*ptr == '\\') // Escape?
            {
                if(!*++ptr) break;

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

void FIData_TextCopy(fi_object_t *obj, char const *str)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextCopy: Not a FI_TEXT.");

    if(t->text)
    {
        Z_Free(t->text); t->text = 0;
    }

    if(str && str[0])
    {
        size_t len = strlen(str) + 1;
        t->text = (char *) Z_Malloc(len, PU_APPSTATIC, 0);
        memcpy(t->text, str, len);
    }
}

void FIData_TextSetFont(fi_object_t *obj, fontid_t fontNum)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextSetFont: Not a FI_TEXT.");
    if(fontNum != 0)
        t->fontNum = fontNum;
}

void FIData_TextSetColor(struct fi_object_s *obj, float red, float green, float blue, int steps)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextSetColor: Not a FI_TEXT.");
    AnimatorVector3_Set(*((animatorvector3_t*)t->color), red, green, blue, steps);
    t->pageColor = 0;
}

void FIData_TextSetAlpha(struct fi_object_s *obj, float alpha, int steps)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextSetAlpha: Not a FI_TEXT.");
    Animator_Set(&t->color[CA], alpha, steps);
}

void FIData_TextSetColorAndAlpha(struct fi_object_s *obj, float red, float green, float blue,
    float alpha, int steps)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextSetColorAndAlpha: Not a FI_TEXT.");
    AnimatorVector4_Set(t->color, red, green, blue, alpha, steps);
    t->pageColor = 0;
}

void FIData_TextSetPreColor(fi_object_t *obj, uint id)
{
    fidata_text_t *t = (fidata_text_t *)obj;
    if(!obj || obj->type != FI_TEXT) Con_Error("FIData_TextSetPreColor: Not a FI_TEXT.");
    if(id >= FIPAGE_NUM_PREDEFINED_COLORS) Con_Error("FIData_TextSetPreColor: Invalid color id %u.", id);
    t->pageColor = id;
}
