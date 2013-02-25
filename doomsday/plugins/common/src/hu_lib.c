/**\file hu_lib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_chat.h"
#include "hu_lib.h"
#include "hu_log.h"
#include "hu_automap.h"

// @todo Remove external dependencies
#include "hu_menu.h" // For the menu sound ids.
extern int menuTime;
void Hu_MenuDrawFocusCursor(int x, int y, int focusObjectHeight, float alpha);

static boolean inited = false;

static int numWidgets;
static uiwidget_t* widgets;

static ui_rendstate_t uiRS;
const ui_rendstate_t* uiRendState = &uiRS;

// Menu (page) render state.
static mn_rendstate_t rs;
const mn_rendstate_t* mnRendState = &rs;

static patchid_t pSliderLeft;
static patchid_t pSliderRight;
static patchid_t pSliderMiddle;
static patchid_t pSliderHandle;
static patchid_t pEditLeft;
static patchid_t pEditRight;
static patchid_t pEditMiddle;

static void MNSlider_LoadResources(void);
static void MNEdit_LoadResources(void);

static mn_actioninfo_t* MNObject_FindActionInfoForId(mn_object_t* obj, mn_actionid_t id);

/**
 * Lookup the logical index of an object thought to be on this page.
 * @param obj  MNObject to lookup the index of.
 * @return  Index of the found object else @c -1.
 */
static int MNPage_FindObjectIndex(mn_page_t* page, mn_object_t* obj);

/**
 * Retrieve an object on this page by it's logical index.
 * @return  Found MNObject else fatal error.
 */
static mn_object_t* MNPage_ObjectByIndex(mn_page_t* page, int idx);

static void errorIfNotInited(const char* callerName)
{
    if(inited) return;
    Con_Error("%s: GUI module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

static void lerpColor(float* dst, const float* a, const float* b, float t, boolean rgbaMode)
{
    if(t <= 0)
    {
        dst[CR] = a[CR];
        dst[CG] = a[CG];
        dst[CB] = a[CB];
        if(rgbaMode) dst[CA] = a[CA];
        return;
    }
    if(t >= 1)
    {
        dst[CR] = b[CR];
        dst[CG] = b[CG];
        dst[CB] = b[CB];
        if(rgbaMode) dst[CA] = b[CA];
        return;
    }
    dst[CR] = (1 - t) * a[CR] + t * b[CR];
    dst[CG] = (1 - t) * a[CG] + t * b[CG];
    dst[CB] = (1 - t) * a[CB] + t * b[CB];
    if(rgbaMode) dst[CA] = (1 - t) * a[CA] + t * b[CA];
}

static uiwidgetid_t nextUnusedId(void)
{
    return (uiwidgetid_t)(numWidgets);
}

static uiwidget_t* allocateWidget(guiwidgettype_t type, uiwidgetid_t id, int player, void* typedata)
{
    uiwidget_t* obj;
    widgets = (uiwidget_t*) realloc(widgets, sizeof(*widgets) * ++numWidgets);
    if(!widgets) Con_Error("allocateWidget: Failed on (re)allocation of %lu bytes for new widget.", (unsigned long) (sizeof(*widgets) * numWidgets));

    obj = &widgets[numWidgets-1];
    memset(obj, 0, sizeof(*obj));
    obj->geometry = Rect_New();
    obj->type = type;
    obj->id = id;
    obj->player = player;

    switch(obj->type)
    {
    case GUI_GROUP: {
        guidata_group_t* grp = (guidata_group_t*)calloc(1, sizeof(*grp));
        if(!grp) Con_Error("allocateWidget: Failed on (re)allocation of %lu bytes for GUI_GROUP typedata.", (unsigned long) sizeof(*grp));

        obj->typedata = grp;
        break;
      }

    default:
        obj->typedata = typedata;
        break;
    }

    switch(obj->type)
    {
    case GUI_AUTOMAP: {
        guidata_automap_t* am = (guidata_automap_t*)obj->typedata;
        am->mcfg = ST_AutomapConfig();
        am->followPlayer = player;
        am->oldViewScale = 1;
        am->maxViewPositionDelta = 128;
        am->alpha = am->targetAlpha = am->oldAlpha = 0;
        /// Set initial geometry size.
        /// @todo Should not be necessary...
        Rect_SetWidthHeight(obj->geometry, SCREENWIDTH, SCREENHEIGHT);
        break;
      }
    default: break;
    }

    return obj;
}

static uiwidget_t* createWidget(guiwidgettype_t type, int player, int alignFlags,
    fontid_t fontId, float opacity, void (*updateGeometry) (uiwidget_t* obj),
    void (*drawer) (uiwidget_t* obj, const Point2Raw* offset),
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength), void* typedata)
{
    uiwidget_t* obj = allocateWidget(type, nextUnusedId(), player, typedata);
    assert(updateGeometry);
    obj->alignFlags = alignFlags;
    obj->font = fontId;
    obj->opacity = opacity;
    obj->updateGeometry = updateGeometry;
    obj->drawer = drawer;
    obj->ticker = ticker;
    return obj;
}

static void clearWidgets(void)
{
    int i;
    if(0 == numWidgets) return;

    for(i = 0; i < numWidgets; ++i)
    {
        uiwidget_t* obj = &widgets[i];
        if(obj->type == GUI_GROUP)
        {
            guidata_group_t* grp = (guidata_group_t*)obj->typedata;
            if(grp->widgetIds)
                free(grp->widgetIds);
            free(grp);
        }
        Rect_Delete(obj->geometry);
    }
    free(widgets);
    widgets = NULL;
    numWidgets = 0;
}

uiwidget_t* GUI_FindObjectById(uiwidgetid_t id)
{
    if(!inited) return NULL; // GUI not available.
    if(id >= 0)
    {
        int i;
        for(i = 0; i < numWidgets; ++i)
        {
            uiwidget_t* obj = &widgets[i];
            if(obj->id == id)
                return obj;
        }
    }
    return NULL;
}

uiwidget_t* GUI_MustFindObjectById(uiwidgetid_t id)
{
    uiwidget_t* obj = GUI_FindObjectById(id);
    if(!obj)
    {
        Con_Error("GUI_MustFindObjectById: Failed to locate object with id %i.", (int) id);
    }
    return obj;
}

void GUI_Register(void)
{
    UIAutomap_Register();
    UIChat_Register();
    UILog_Register();
}

void GUI_Init(void)
{
    if(inited) return;
    numWidgets = 0;
    widgets = NULL;
    UIChat_LoadMacros();

    inited = true;

    GUI_LoadResources();
}

void GUI_Shutdown(void)
{
    if(!inited) return;
    clearWidgets();
    inited = false;
}

void GUI_LoadResources(void)
{
    if(Get(DD_DEDICATED) || Get(DD_NOVIDEO)) return;

    UIAutomap_LoadResources();
    MNEdit_LoadResources();
    MNSlider_LoadResources();
}

void GUI_ReleaseResources(void)
{
    int i;

    if(Get(DD_DEDICATED) || Get(DD_NOVIDEO)) return;

    UIAutomap_ReleaseResources();

    for(i = 0; i < numWidgets; ++i)
    {
        uiwidget_t* ob = widgets + i;
        switch(ob->type)
        {
        case GUI_AUTOMAP: UIAutomap_Reset(ob); break;
        default: break;
        }
    }
}

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, int alignFlags,
    fontid_t fontId, float opacity,
    void (*updateGeometry) (uiwidget_t* obj), void (*drawer) (uiwidget_t* obj, const Point2Raw* offset),
    void (*ticker) (uiwidget_t* obj, timespan_t ticLength), void* typedata)
{
    uiwidget_t* obj;
    errorIfNotInited("GUI_CreateWidget");
    obj = createWidget(type, player, alignFlags, fontId, opacity, updateGeometry, drawer, ticker, typedata);
    return obj->id;
}

uiwidgetid_t GUI_CreateGroup(int groupFlags, int player, int alignFlags, order_t order, int padding)
{
    uiwidget_t* obj;
    guidata_group_t* grp;
    errorIfNotInited("GUI_CreateGroup");

    obj = createWidget(GUI_GROUP, player, alignFlags, 1, 0, UIGroup_UpdateGeometry, NULL, NULL, NULL);
    grp = (guidata_group_t*)obj->typedata;
    grp->flags = groupFlags;
    grp->order = order;
    grp->padding = padding;

    return obj->id;
}

void UIGroup_AddWidget(uiwidget_t* obj, uiwidget_t* other)
{
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    int i;
    assert(obj->type == GUI_GROUP);

    if(!other || other == obj)
    {
#if _DEBUG
        Con_Message("Warning: UIGroup::AddWidget: Attempt to add invalid widget %s, ignoring.", obj? "(this)" : "(null)");
#endif
        return;
    }

    // Ensure widget is not already in this grp.
    for(i = 0; i < grp->widgetIdCount; ++i)
    {
        if(grp->widgetIds[i] == other->id)
            return; // Already present. Ignore.
    }

    // Must add to this grp.
    grp->widgetIds = (uiwidgetid_t*) realloc(grp->widgetIds, sizeof(*grp->widgetIds) * ++grp->widgetIdCount);
    if(!grp->widgetIds) Con_Error("UIGroup::AddWidget: Failed on (re)allocation of %lu bytes for widget id list.", (unsigned long) (sizeof(*grp->widgetIds) * grp->widgetIdCount));

    grp->widgetIds[grp->widgetIdCount-1] = other->id;
}

int UIGroup_Flags(uiwidget_t* obj)
{
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    assert(obj->type == GUI_GROUP);

    return grp->flags;
}

void UIGroup_SetFlags(uiwidget_t* obj, int flags)
{
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    assert(obj->type == GUI_GROUP);

    grp->flags = flags;
}

static void applyAlignmentOffset(uiwidget_t* obj, int* x, int* y)
{
    assert(obj);
    if(x)
    {
        if(obj->alignFlags & ALIGN_RIGHT)
            *x += UIWidget_MaximumWidth(obj);
        else if(!(obj->alignFlags & ALIGN_LEFT))
            *x += UIWidget_MaximumWidth(obj)/2;
    }
    if(y)
    {
        if(obj->alignFlags & ALIGN_BOTTOM)
            *y += UIWidget_MaximumHeight(obj);
        else if(!(obj->alignFlags & ALIGN_TOP))
            *y += UIWidget_MaximumHeight(obj)/2;
    }
}

static void updateWidgetGeometry(uiwidget_t* obj)
{
    Rect_SetXY(obj->geometry, 0, 0);
    obj->updateGeometry(obj);

    if(Rect_Width(obj->geometry) <= 0 || Rect_Height(obj->geometry) <= 0) return;

    if(obj->alignFlags & ALIGN_RIGHT)
        Rect_SetX(obj->geometry, Rect_X(obj->geometry) - Rect_Width(obj->geometry));
    else if(!(obj->alignFlags & ALIGN_LEFT))
        Rect_SetX(obj->geometry, Rect_X(obj->geometry) - Rect_Width(obj->geometry)/2);

    if(obj->alignFlags & ALIGN_BOTTOM)
        Rect_SetY(obj->geometry, Rect_Y(obj->geometry) - Rect_Height(obj->geometry));
    else if(!(obj->alignFlags & ALIGN_TOP))
        Rect_SetY(obj->geometry, Rect_Y(obj->geometry) - Rect_Height(obj->geometry)/2);
}

void UIGroup_UpdateGeometry(uiwidget_t* obj)
{
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    int i, x, y;
    assert(obj && obj->type == GUI_GROUP);

    Rect_SetWidthHeight(obj->geometry, 0, 0);

    if(!grp->widgetIdCount) return;

    x = y = 0;
    applyAlignmentOffset(obj, &x, &y);

    for(i = 0; i < grp->widgetIdCount; ++i)
    {
        uiwidget_t* child = GUI_MustFindObjectById(grp->widgetIds[i]);
        const Rect* childGeometry;

        if(UIWidget_MaximumWidth(child) > 0 && UIWidget_MaximumHeight(child) > 0 &&
           UIWidget_Opacity(child) > 0)
        {
            updateWidgetGeometry(child);

            Rect_SetX(child->geometry, Rect_X(child->geometry) + x);
            Rect_SetY(child->geometry, Rect_Y(child->geometry) + y);

            childGeometry = UIWidget_Geometry(child);
            if(Rect_Width(childGeometry) > 0 && Rect_Height(childGeometry) > 0)
            {
                if(grp->order == ORDER_RIGHTTOLEFT)
                {
                    if(!(grp->flags & UWGF_VERTICAL))
                        x -= Rect_Width(childGeometry)  + grp->padding;
                    else
                        y -= Rect_Height(childGeometry) + grp->padding;
                }
                else if(grp->order == ORDER_LEFTTORIGHT)
                {
                    if(!(grp->flags & UWGF_VERTICAL))
                        x += Rect_Width(childGeometry)  + grp->padding;
                    else
                        y += Rect_Height(childGeometry) + grp->padding;
                }

                Rect_Unite(obj->geometry, childGeometry);
            }
        }
    }
}

#if _DEBUG
static void drawWidgetGeometry(uiwidget_t* obj)
{
    RectRaw geometry;
    assert(obj);
    Rect_Raw(obj->geometry, &geometry);

    DGL_Color3f(1, 1, 1);
    DGL_Begin(DGL_LINES);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x + geometry.size.width, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y + geometry.size.height);
        DGL_Vertex2f(geometry.origin.x, geometry.origin.y);
    DGL_End();
}

static void drawWidgetAvailableSpace(uiwidget_t* obj)
{
    assert(obj);
    DGL_Color4f(0, .4f, 0, .1f);
    DGL_DrawRectf2(Rect_X(obj->geometry), Rect_Y(obj->geometry), obj->maxSize.width, obj->maxSize.height);
}
#endif

static void drawWidget2(uiwidget_t* obj, const Point2Raw* offset)
{
    assert(obj);

/*#if _DEBUG
    drawWidgetAvailableSpace(obj);
#endif*/

    if(obj->drawer && obj->opacity > .0001f)
    {
        Point2Raw origin;
        Point2_Raw(Rect_Origin(obj->geometry), &origin);

        // Configure the page render state.
        uiRS.pageAlpha = obj->opacity;

        FR_PushAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin.x, origin.y, 0);

        // Do not pass a zero length offset.
        obj->drawer(obj, ((offset && (offset->x || offset->y))? offset : NULL));

        FR_PopAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin.x, -origin.y, 0);
    }

/*#if _DEBUG
    drawWidgetGeometry(obj);
#endif*/
}

static void drawWidget(uiwidget_t* obj, const Point2Raw* origin)
{
    assert(obj);

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin->x, origin->y, 0);
    }

    // First we draw ourself.
    drawWidget2(obj, NULL/*no origin offset*/);

    if(obj->type == GUI_GROUP)
    {
        // Now our children.
        guidata_group_t* grp = (guidata_group_t*)obj->typedata;
        int i;
        for(i = 0; i < grp->widgetIdCount; ++i)
        {
            uiwidget_t* child = GUI_MustFindObjectById(grp->widgetIds[i]);
            drawWidget(child, NULL/*no origin offset*/);
        }
    }

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin->x, -origin->y, 0);
    }
}

void GUI_DrawWidget(uiwidget_t* obj, const Point2Raw* offset)
{
    if(!obj) return;
    if(UIWidget_MaximumWidth(obj) < 1 || UIWidget_MaximumHeight(obj) < 1) return;
    if(UIWidget_Opacity(obj) <= 0) return;

    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    updateWidgetGeometry(obj);

    FR_PopAttrib();

    // Draw.
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    // Do not pass a zero length offset.
    drawWidget(obj, ((offset && (offset->x || offset->y))? offset : NULL));

    FR_PopAttrib();
}

void GUI_DrawWidgetXY(uiwidget_t* obj, int x, int y)
{
    Point2Raw origin;
    origin.x = x;
    origin.y = y;
    GUI_DrawWidget(obj, &origin);
}

void UIWidget_RunTic(uiwidget_t* obj, timespan_t ticLength)
{
    assert(obj);
    switch(obj->type)
    {
    case GUI_GROUP: {
        // First our children then self.
        guidata_group_t* grp = (guidata_group_t*)obj->typedata;
        int i;
        for(i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_RunTic(GUI_MustFindObjectById(grp->widgetIds[i]), ticLength);
        }
        // Fallthrough:
      }
    default:
        if(NULL != obj->ticker)
        {
            obj->ticker(obj, ticLength);
        }
        break;
    }
}

int UIWidget_Player(uiwidget_t* obj)
{
    assert(obj);
    return obj->player;
}

const Point2* UIWidget_Origin(uiwidget_t* obj)
{
    assert(obj);
    return Rect_Origin(obj->geometry);
}

const Rect* UIWidget_Geometry(uiwidget_t* obj)
{
    assert(obj);
    return obj->geometry;
}

int UIWidget_MaximumHeight(uiwidget_t* obj)
{
    assert(obj);
    return obj->maxSize.height;
}

const Size2Raw* UIWidget_MaximumSize(uiwidget_t* obj)
{
    assert(obj);
    return &obj->maxSize;
}

int UIWidget_MaximumWidth(uiwidget_t* obj)
{
    assert(obj);
    return obj->maxSize.width;
}

void UIWidget_SetMaximumHeight(uiwidget_t* obj, int height)
{
    assert(obj);
    if(obj->maxSize.height == height) return;
    obj->maxSize.height = height;

    if(obj->type == GUI_GROUP)
    {
        guidata_group_t* grp = (guidata_group_t*)obj->typedata;
        int i;
        for(i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumHeight(GUI_MustFindObjectById(grp->widgetIds[i]), height);
        }
    }
}

void UIWidget_SetMaximumSize(uiwidget_t* obj, const Size2Raw* size)
{
    assert(obj);
    if(obj->maxSize.width == size->width &&
       obj->maxSize.height == size->height) return;
    obj->maxSize.width = size->width;
    obj->maxSize.height = size->height;

    if(obj->type == GUI_GROUP)
    {
        guidata_group_t* grp = (guidata_group_t*)obj->typedata;
        int i;
        for(i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumSize(GUI_MustFindObjectById(grp->widgetIds[i]), size);
        }
    }
}

void UIWidget_SetMaximumWidth(uiwidget_t* obj, int width)
{
    assert(obj);
    if(obj->maxSize.width == width) return;
    obj->maxSize.width = width;

    if(obj->type == GUI_GROUP)
    {
        guidata_group_t* grp = (guidata_group_t*)obj->typedata;
        int i;
        for(i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumWidth(GUI_MustFindObjectById(grp->widgetIds[i]), width);
        }
    }
}

int UIWidget_Alignment(uiwidget_t* obj)
{
    assert(obj);
    return obj->alignFlags;
}

void UIWidget_SetAlignment(uiwidget_t* obj, int alignFlags)
{
    assert(obj);
    obj->alignFlags = alignFlags;
}

float UIWidget_Opacity(uiwidget_t* obj)
{
    assert(obj);
    return obj->opacity;
}

void UIWidget_SetOpacity(uiwidget_t* obj, float opacity)
{
    assert(obj);
    obj->opacity = opacity;
    if(obj->type == GUI_GROUP)
    {
        guidata_group_t* grp = (guidata_group_t*)obj->typedata;
        int i;
        for(i = 0; i < grp->widgetIdCount; ++i)
        {
            uiwidget_t* child = GUI_MustFindObjectById(grp->widgetIds[i]);
            UIWidget_SetOpacity(child, opacity);
        }
    }
}

static void MNSlider_LoadResources(void)
{
    pSliderLeft   = R_DeclarePatch(MNDATA_SLIDER_PATCH_LEFT);
    pSliderRight  = R_DeclarePatch(MNDATA_SLIDER_PATCH_RIGHT);
    pSliderMiddle = R_DeclarePatch(MNDATA_SLIDER_PATCH_MIDDLE);
    pSliderHandle = R_DeclarePatch(MNDATA_SLIDER_PATCH_HANDLE);
}

static void MNEdit_LoadResources(void)
{
#if defined(MNDATA_EDIT_BACKGROUND_PATCH_LEFT)
    pEditLeft   = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_LEFT);
#else
    pEditLeft   = 0;
#endif
#if defined(MNDATA_EDIT_BACKGROUND_PATCH_RIGHT)
    pEditRight  = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_RIGHT);
#else
    pEditRight  = 0;
#endif
    pEditMiddle = R_DeclarePatch(MNDATA_EDIT_BACKGROUND_PATCH_MIDDLE);
}

mn_object_t* MN_MustFindObjectOnPage(mn_page_t* page, int group, int flags)
{
    mn_object_t* obj = MNPage_FindObject(page, group, flags);
    if(!obj)
        Con_Error("MN_MustFindObjectOnPage: Failed to locate object in group #%i with flags %i on page %p.", group, flags, page);
    return obj;
}

short MN_MergeMenuEffectWithDrawTextFlags(short f)
{
    return ((~cfg.menuEffectFlags & DTF_NO_EFFECTS) | (f & ~DTF_NO_EFFECTS));
}

static void MN_DrawObject(mn_object_t* obj, const Point2Raw* offset)
{
    if(!obj) return;
    obj->drawer(obj, offset);
}

static void setupRenderStateForPageDrawing(mn_page_t* page, float alpha)
{
    int i;

    if(!page) return; // Huh?

    rs.pageAlpha = alpha;
    rs.textGlitter = cfg.menuTextGlitter;
    rs.textShadow = cfg.menuShadow;

    for(i = 0; i < MENU_FONT_COUNT; ++i)
    {
        rs.textFonts[i] = MNPage_PredefinedFont(page, i);
    }
    for(i = 0; i < MENU_COLOR_COUNT; ++i)
    {
        MNPage_PredefinedColor(page, i, rs.textColors[i]);
        rs.textColors[i][CA] = alpha; // For convenience.
    }

    // Configure the font renderer (assume state has already been pushed if necessary).
    FR_SetFont(rs.textFonts[0]);
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);
    FR_SetShadowStrength(rs.textShadow);
    FR_SetGlitterStrength(rs.textGlitter);
}

static void updatePageObjectGeometries(mn_page_t* page)
{
    int i;

    if(!page) return;

    // Update objects.
    for(i = 0; i < page->objectsCount; ++i)
    {
        mn_object_t* obj = &page->objects[i];

        if(MNObject_Type(obj) == MN_NONE) continue;

        FR_PushAttrib();
        if(obj->updateGeometry)
        {
            Rect_SetXY(obj->_geometry, 0, 0);
            obj->updateGeometry(obj, page);
        }
        FR_PopAttrib();
    }
}

/// @return  @c true iff this object is drawable (potentially visible).
boolean MNObject_IsDrawable(mn_object_t* ob)
{
    return !(MNObject_Type(ob) == MN_NONE || !ob->drawer || (MNObject_Flags(ob) & MNF_HIDDEN));
}

static void applyPageLayout(mn_page_t* page)
{
    int i, lineHeight, lineOffset;
    Point2Raw origin = { 0, 0 };

    if(!page) return;

    Rect_SetXY(page->geometry, 0, 0);
    Rect_SetWidthHeight(page->geometry, 0, 0);

    // Apply layout logic to this page.

    if(page->flags & MPF_LAYOUT_FIXED)
    {
        // This page uses a fixed layout.
        for(i = 0; i < page->objectsCount; ++i)
        {
            mn_object_t* ob = &page->objects[i];

            if(!MNObject_IsDrawable(ob)) continue;

            Rect_SetXY(ob->_geometry, ob->_origin.x, ob->_origin.y);
            Rect_Unite(page->geometry, ob->_geometry);
        }
        return;
    }

    // This page uses a dynamic layout.

    // Calculate leading/line offset.
    FR_SetFont(MNPage_PredefinedFont(page, MENU_FONT1));
    /// @kludge We cannot yet query line height from the font.
    lineHeight = FR_TextHeight("{case}WyQ");
    lineOffset = MAX_OF(1, .5f + lineHeight * .34f);
    // kludge end.

    for(i = 0; i < page->objectsCount;)
    {
        mn_object_t* ob = &page->objects[i];
        mn_object_t* nextOb = i+1 < page->objectsCount? &page->objects[i+1] : NULL;

        if(!MNObject_IsDrawable(ob))
        {
            // Proceed to the next object!
            i += 1;
            continue;
        }

        // If the object has a fixed position, we will ignore it while doing
        // dynamic layout.
        if(MNObject_Flags(ob) & MNF_POSITION_FIXED)
        {
            Rect_SetXY(ob->_geometry, ob->_origin.x, ob->_origin.y);
            Rect_Unite(page->geometry, ob->_geometry);

            // To the next object.
            i += 1;
            continue;
        }

        // An additional offset requested?
        if(MNObject_Flags(ob) & MNF_LAYOUT_OFFSET)
        {
            origin.x += ob->_origin.x;
            origin.y += ob->_origin.y;
        }

        Rect_SetXY(ob->_geometry, origin.x, origin.y);

        // Orient label plus button/inline-list/textual-slider pairs about a
        // vertical dividing line, with the label on the left, other object
        // on the right.
        // @todo Do not assume pairing, an object should designate it's pair.
        if(MNObject_Type(ob) == MN_TEXT && nextOb)
        {
            if(MNObject_IsDrawable(nextOb) &&
               (MNObject_Type(nextOb) == MN_BUTTON ||
                MNObject_Type(nextOb) == MN_LISTINLINE ||
                MNObject_Type(nextOb) == MN_COLORBOX ||
                MNObject_Type(nextOb) == MN_BINDINGS ||
                (MNObject_Type(nextOb) == MN_SLIDER && nextOb->drawer == MNSlider_TextualValueDrawer)))
            {
                const int margin = lineOffset * 2;
                RectRaw united;

                Rect_SetXY(nextOb->_geometry, margin + Rect_Width(ob->_geometry), origin.y);

                origin.y += Rect_United(ob->_geometry, nextOb->_geometry, &united)
                          ->size.height + lineOffset;

                Rect_UniteRaw(page->geometry, &united);

                // Extra spacing between object groups.
                if(i+2 < page->objectsCount &&
                   nextOb->_group != page->objects[i+2]._group)
                    origin.y += lineHeight;

                // Proceed to the next object!
                i += 2;
                continue;
            }
        }

        Rect_Unite(page->geometry, ob->_geometry);

        origin.y += Rect_Height(ob->_geometry) + lineOffset;

        // Extra spacing between object groups.
        if(nextOb && nextOb->_group != ob->_group)
            origin.y += lineHeight;

        // Proceed to the next object!
        i += 1;
    }
}

static void composeSubpageString(mn_page_t* page, char* buf, size_t bufSize)
{
    assert(page);
    if(!buf || 0 == bufSize) return;
    dd_snprintf(buf, bufSize, "Page %i/%i", 0, 0);
}

static void drawPageNavigation(mn_page_t* page, int x, int y)
{
    const int currentPage = 0;//(page->firstObject + page->numVisObjects/2) / page->numVisObjects + 1;
    const int totalPages  = 1;//(int)ceil((float)page->objectsCount/page->numVisObjects);
#if __JDOOM__ || __JDOOM64__
    char buf[1024];
#endif

    if(!page || totalPages <= 1) return;

#if __JDOOM__ || __JDOOM64__
    composeSubpageString(page, buf, 1024);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(FID(GF_FONTA));
    FR_SetColorv(cfg.menuTextColors[1]);
    FR_SetAlpha(mnRendState->pageAlpha);

    FR_DrawTextXY3(buf, x, y, ALIGN_TOP, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
#else
    DGL_Enable(DGL_TEXTURE_2D);
    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);

    GL_DrawPatchXY2( pInvPageLeft[currentPage == 0 || (menuTime & 8)], x - 144, y, ALIGN_RIGHT);
    GL_DrawPatchXY2(pInvPageRight[currentPage == totalPages-1 || (menuTime & 8)], x + 144, y, ALIGN_LEFT);

    DGL_Disable(DGL_TEXTURE_2D);
#endif
}

static void drawPageHeading(mn_page_t* page, const Point2Raw* offset)
{
    Point2Raw origin;

    if(!page) return;

    /// @kludge no title = no heading.
    if(Str_IsEmpty(&page->title)) return;
    // kludge end.

    origin.x = SCREENWIDTH/2;
    origin.y = (SCREENHEIGHT/2) - ((SCREENHEIGHT/2-5)/cfg.menuScale);
    if(offset)
    {
        origin.x += offset->x;
        origin.y += offset->y;
    }

    FR_PushAttrib();
    Hu_MenuDrawPageTitle(Str_Text(&page->title), origin.x, origin.y); origin.y += 16;
    drawPageNavigation(page, origin.x, origin.y);
    FR_PopAttrib();
}

void MN_DrawPage(mn_page_t* page, float alpha, boolean showFocusCursor)
{
    mn_object_t* focusObj;
    int i, focusObjHeight;
    Point2Raw cursorOrigin;

    if(!page) return;

    alpha = MINMAX_OF(0, alpha, 1);
    if(alpha <= .0001f) return;

    // Object geometry is determined from properties defined in the
    // render state, so configure render state before we begin.
    setupRenderStateForPageDrawing(page, alpha);

    // Update object geometry. We'll push the font renderer state because
    // updating geometry may require changing the current values.
    FR_PushAttrib();
    updatePageObjectGeometries(page);

    // Back to default page render state.
    FR_PopAttrib();

    // We can now layout the widgets of this page.
    applyPageLayout(page);

    // Determine the origin of the focus object (this dictates the page scroll location).
    focusObj = MNPage_FocusObject(page);
    if(focusObj && !MNObject_IsDrawable(focusObj))
        focusObj = NULL;

    // Are we focusing?
    if(focusObj)
    {
        focusObjHeight = Size2_Height(MNObject_Size(focusObj));

        // Determine the origin and dimensions of the cursor.
        // @todo Each object should define a focus origin...
        cursorOrigin.x = -1;
        cursorOrigin.y = Point2_Y(MNObject_Origin(focusObj));

        /// @kludge
        /// We cannot yet query the subobjects of the list for these values
        /// so we must calculate them ourselves, here.
        if(MN_LIST == MNObject_Type(focusObj) && (MNObject_Flags(focusObj) & MNF_ACTIVE) &&
           MNList_SelectionIsVisible(focusObj))
        {
            const mndata_list_t* list = (mndata_list_t*)focusObj->_typedata;

            FR_PushAttrib();
            FR_SetFont(MNPage_PredefinedFont(page, MNObject_Font(focusObj)));
            focusObjHeight = FR_CharHeight('A') * (1+MNDATA_LIST_LEADING);
            cursorOrigin.y += (list->selection - list->first) * focusObjHeight;
            FR_PopAttrib();
        }
        // kludge end
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(page->origin.x, page->origin.y, 0);

    // Apply page scroll?
    if(!(page->flags & MPF_NEVER_SCROLL) && focusObj)
    {
        RectRaw pageGeometry, viewRegion;
        Rect_Raw(page->geometry, &pageGeometry);

        // Determine available screen region for the page.
        viewRegion.origin.x = 0;
        viewRegion.origin.y = page->origin.y;
        viewRegion.size.width  = SCREENWIDTH;
        viewRegion.size.height = SCREENHEIGHT - 40/*arbitrary but enough for the help message*/;

        // Is scrolling in effect?
        if(pageGeometry.size.height > viewRegion.size.height)
        {
            const int minY = -viewRegion.origin.y/2 + viewRegion.size.height/2;
            if(cursorOrigin.y > minY)
            {
                const int scrollLimitY = pageGeometry.size.height - viewRegion.size.height/2;
                const int scrollOriginY = MIN_OF(cursorOrigin.y, scrollLimitY) - minY;
                DGL_Translatef(0, -scrollOriginY, 0);
            }
        }
    }

    // Draw child objects.
    for(i = 0; i < page->objectsCount; ++i)
    {
        mn_object_t* obj = &page->objects[i];
        RectRaw geometry;

        if(MNObject_Type(obj) == MN_NONE || !obj->drawer || (MNObject_Flags(obj) & MNF_HIDDEN))
            continue;

        Rect_Raw(MNObject_Geometry(obj), &geometry);

        FR_PushAttrib();
        MN_DrawObject(obj, &geometry.origin);
        FR_PopAttrib();
    }

    // How about a focus cursor?
    /// @todo cursor should be drawn on top of the page drawer.
    if(showFocusCursor && focusObj)
    {
        Hu_MenuDrawFocusCursor(cursorOrigin.x, cursorOrigin.y, focusObjHeight, alpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    drawPageHeading(page, NULL/*no offset*/);

    // The page has its own drawer.
    if(page->drawer)
    {
        FR_PushAttrib();
        page->drawer(page, &page->origin);
        FR_PopAttrib();
    }
}

static boolean MNActionInfo_IsActionExecuteable(mn_actioninfo_t* info)
{
    assert(info);
    return (info->callback != 0);
}

void MNPage_SetTitle(mn_page_t* page, const char* title)
{
    assert(page);
    Str_Set(&page->title, title? title : "");
}

void MNPage_SetX(mn_page_t* page, int x)
{
    assert(page);
    page->origin.x = x;
}

void MNPage_SetY(mn_page_t* page, int y)
{
    assert(page);
    page->origin.y = y;
}

void MNPage_SetPreviousPage(mn_page_t* page, mn_page_t* prevPage)
{
    assert(page);
    page->previous = prevPage;
}

mn_object_t* MNPage_FocusObject(mn_page_t* page)
{
    assert(page);
    if(0 == page->objectsCount || 0 > page->focus) return NULL;
    return &page->objects[page->focus];
}

void MNPage_ClearFocusObject(mn_page_t* page)
{
    mn_object_t* ob;
    int i;
    DENG_ASSERT(page);
    if(page->focus >= 0)
    {
        ob = &page->objects[page->focus];
        if(MNObject_Flags(ob) & MNF_ACTIVE)
        {
            return;
        }
    }
    page->focus = -1;
    ob = page->objects;
    for(i = 0; i < page->objectsCount; ++i, ob++)
    {
        MNObject_SetFlags(ob, FO_CLEAR, MNF_FOCUS);
    }
    MNPage_Refocus(page);
}

mn_object_t* MNPage_FindObject(mn_page_t* page, int group, int flags)
{
    mn_object_t* obj = page->objects;
    for(; MNObject_Type(obj) != MN_NONE; obj++)
    {
        if(MNObject_IsGroupMember(obj, group) && (MNObject_Flags(obj) & flags) == flags)
            return obj;
    }
    return NULL;
}

static int MNPage_FindObjectIndex(mn_page_t* page, mn_object_t* obj)
{
    int i;
    assert(page && obj);

    for(i = 0; i < page->objectsCount; ++i)
    {
        if(obj == page->objects+i)
            return i;
    }
    return -1; // Not found.
}

static mn_object_t* MNPage_ObjectByIndex(mn_page_t* page, int idx)
{
    assert(page);
    if(idx < 0 || idx >= page->objectsCount)
        Con_Error("MNPage::ObjectByIndex: Index #%i invalid for page %p.", idx, page);
    return page->objects + idx;
}

/// @pre @a ob is a child of @a page.
static void MNPage_GiveChildFocus(mn_page_t* page, mn_object_t* ob, boolean allowRefocus)
{
    assert(page && ob);

    if(!(0 > page->focus))
    {
        if(ob != page->objects + page->focus)
        {
            mn_object_t* oldFocusOb = page->objects + page->focus;
            if(MNObject_HasAction(oldFocusOb, MNA_FOCUSOUT))
            {
                MNObject_ExecAction(oldFocusOb, MNA_FOCUSOUT, NULL);
            }
            MNObject_SetFlags(oldFocusOb, FO_CLEAR, MNF_FOCUS);
        }
        else if(!allowRefocus)
        {
            return;
        }
    }

    page->focus = ob - page->objects;
    MNObject_SetFlags(ob, FO_SET, MNF_FOCUS);
    if(MNObject_HasAction(ob, MNA_FOCUS))
    {
        MNObject_ExecAction(ob, MNA_FOCUS, NULL);
    }
}

void MNPage_SetFocus(mn_page_t* page, mn_object_t* obj)
{
    int objIndex = MNPage_FindObjectIndex(page, obj);
    if(objIndex < 0)
    {
#if _DEBUG
        Con_Error("MNPage::Focus: Failed to determine index for object %p on page %p.", obj, page);
#endif
        return;
    }
    MNPage_GiveChildFocus(page, page->objects + objIndex, false);
}

void MNPage_Refocus(mn_page_t* page)
{
    DENG_ASSERT(page);

    // If we haven't yet visited this page then find the first focusable
    // object and select it.
    if(0 > page->focus)
    {
        int i, giveFocus = -1;

        // First look for a default focus object. There should only be one
        // but find the last with this flag...
        for(i = 0; i < page->objectsCount; ++i)
        {
            mn_object_t* ob = &page->objects[i];
            if((MNObject_Flags(ob) & MNF_DEFAULT) && !(MNObject_Flags(ob) & (MNF_DISABLED|MNF_NO_FOCUS)))
            {
                giveFocus = i;
            }
        }

        // No default focus? Find the first focusable object.
        if(-1 == giveFocus)
        for(i = 0; i < page->objectsCount; ++i)
        {
            mn_object_t* ob = &page->objects[i];
            if(!(MNObject_Flags(ob) & (MNF_DISABLED|MNF_NO_FOCUS)))
            {
                giveFocus = i;
                break;
            }
        }

        if(-1 != giveFocus)
        {
            MNPage_GiveChildFocus(page, page->objects + giveFocus, false);
        }
#if _DEBUG
        else
        {
            Con_Message("Warning: MNPage::Refocus: No focusable object on page.");
        }
#endif
    }
    else
    {
        // We've been here before; re-focus on the last focused object.
        MNPage_GiveChildFocus(page, page->objects + page->focus, true);
    }
}

void MNPage_Initialize(mn_page_t* page)
{
    mn_object_t* ob;
    int i;
    DENG_ASSERT(page);

    // Reset page timer.
    page->timer = 0;

    // (Re)init objects.
    for(i = 0, ob = page->objects; i < page->objectsCount; ++i, ob++)
    {
        // Reset object timer.
        ob->timer = 0;

        switch(MNObject_Type(ob))
        {
        case MN_BUTTON: {
            mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
            if(btn->staydownMode)
            {
                const boolean activate = (*(char*) ob->data1);
                MNObject_SetFlags(ob, (activate? FO_SET:FO_CLEAR), MNF_ACTIVE);
            }
            break; }

        case MN_LIST:
        case MN_LISTINLINE: {
            mndata_list_t* list = ob->_typedata;

            // Determine number of potentially visible items.
            list->numvis = list->count;
            if(list->selection >= 0)
            {
                if(list->selection < list->first)
                    list->first = list->selection;
                if(list->selection > list->first + list->numvis - 1)
                    list->first = list->selection - list->numvis + 1;
            }
            break; }

        default: break;
        }
    }

    if(0 == page->objectsCount)
    {
        // Presumably objects will be added later.
        return;
    }

    MNPage_Refocus(page);
}

void MNPage_Ticker(mn_page_t* page)
{
    mn_object_t* ob;
    int i;

    // Call the ticker of each object, unless they're hidden or paused.
    for(i = 0, ob = page->objects; i < page->objectsCount; ++i, ob++)
    {
        if((MNObject_Flags(ob) & MNF_PAUSED) || (MNObject_Flags(ob) & MNF_HIDDEN))
            continue;

        if(ob->ticker)
            ob->ticker(ob);

        // Advance object timer.
        ob->timer++;
    }

    page->timer++;
}

fontid_t MNPage_PredefinedFont(mn_page_t* page, mn_page_fontid_t id)
{
    assert(page);
    if(!VALID_MNPAGE_FONTID(id))
    {
#if _DEBUG
        Con_Error("MNPage::PredefinedFont: Invalid font id #%i.", (int)id);
        exit(1); // Unreachable.
#endif
        return 0; // Not a valid font id.
    }
    return page->fonts[id];
}

void MNPage_SetPredefinedFont(mn_page_t* page, mn_page_fontid_t id, fontid_t fontId)
{
    assert(page);
    if(!VALID_MNPAGE_FONTID(id))
    {
#if _DEBUG
        Con_Message("MNPage::SetPredefinedFont: Invalid font id #%i, ignoring.", id);
#endif
        return;
    }
    page->fonts[id] = fontId;
}

void MNPage_PredefinedColor(mn_page_t* page, mn_page_colorid_t id, float rgb[3])
{
    uint colorIndex;
    assert(page);

    if(!rgb)
    {
#if _DEBUG
        Con_Error("MNPage::PredefinedColor: Invalid 'rgb' reference.");
        exit(1); // Unreachable.
#endif
        return;
    }
    if(!VALID_MNPAGE_COLORID(id))
    {
#if _DEBUG
        Con_Error("MNPage::PredefinedColor: Invalid color id '%i'.", (int)id);
        exit(1); // Unreachable.
#endif
        rgb[CR] = rgb[CG] = rgb[CB] = 1;
        return;
    }

    colorIndex = page->colors[id];
    rgb[CR] = cfg.menuTextColors[colorIndex][CR];
    rgb[CG] = cfg.menuTextColors[colorIndex][CG];
    rgb[CB] = cfg.menuTextColors[colorIndex][CB];
}

int MNPage_Timer(mn_page_t* page)
{
    assert(page);
    return page->timer;
}

mn_obtype_e MNObject_Type(const mn_object_t* ob)
{
    assert(ob);
    return ob->_type;
}

mn_page_t* MNObject_Page(const mn_object_t* ob)
{
    assert(ob);
    return ob->_page;
}

int MNObject_Flags(const mn_object_t* ob)
{
    assert(ob);
    return ob->_flags;
}

const Rect* MNObject_Geometry(const mn_object_t* ob)
{
    assert(ob);
    return ob->_geometry;
}

const Point2* MNObject_Origin(const mn_object_t* ob)
{
    assert(ob);
    return Rect_Origin(ob->_geometry);
}

const Size2* MNObject_Size(const mn_object_t* ob)
{
    assert(ob);
    return Rect_Size(ob->_geometry);
}

const Point2Raw* MNObject_FixedOrigin(const mn_object_t* ob)
{
    assert(ob);
    return &ob->_origin;
}

int MNObject_FixedX(const mn_object_t* ob)
{
    assert(ob);
    return ob->_origin.x;
}

int MNObject_FixedY(const mn_object_t* ob)
{
    assert(ob);
    return ob->_origin.y;
}

mn_object_t* MNObject_SetFixedOrigin(mn_object_t* ob, const Point2Raw* origin)
{
    assert(ob);
    if(origin)
    {
        ob->_origin.x = origin->x;
        ob->_origin.y = origin->y;
    }
    return ob;
}

mn_object_t* MNObject_SetFixedX(mn_object_t* ob, int x)
{
    assert(ob);
    ob->_origin.x = x;
    return ob;
}

mn_object_t* MNObject_SetFixedY(mn_object_t* ob, int y)
{
    assert(ob);
    ob->_origin.y = y;
    return ob;
}

int MNObject_SetFlags(mn_object_t* obj, flagop_t op, int flags)
{
    assert(obj);
    switch(op)
    {
    case FO_CLEAR:  obj->_flags &= ~flags;  break;
    case FO_SET:    obj->_flags |= flags;   break;
    case FO_TOGGLE: obj->_flags ^= flags;   break;
    default:
        Con_Error("MNObject::SetFlags: Unknown op %i\n", op);
        exit(1); // Unreachable.
    }
    return obj->_flags;
}

int MNObject_Shortcut(mn_object_t* ob)
{
    assert(ob);
    return ob->_shortcut;
}

void MNObject_SetShortcut(mn_object_t* ob, int ddkey)
{
    assert(ob);
    if(isalnum(ddkey))
    {
        ob->_shortcut = tolower(ddkey);
    }
}

int MNObject_Font(mn_object_t* ob)
{
    assert(ob);
    return ob->_pageFontIdx;
}

int MNObject_Color(mn_object_t* ob)
{
    assert(ob);
    return ob->_pageColorIdx;
}

boolean MNObject_IsGroupMember(const mn_object_t* ob, int group)
{
    assert(ob);
    return (ob->_group == group);
}

int MNObject_DefaultCommandResponder(mn_object_t* ob, menucommand_e cmd)
{
    assert(ob);
    if(MCMD_SELECT == cmd && (ob->_flags & MNF_FOCUS) && !(ob->_flags & MNF_DISABLED))
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!(ob->_flags & MNF_ACTIVE))
        {
            ob->_flags |= MNF_ACTIVE;
            if(MNObject_HasAction(ob, MNA_ACTIVE))
            {
                MNObject_ExecAction(ob, MNA_ACTIVE, NULL);
            }
        }

        ob->_flags &= ~MNF_ACTIVE;
        if(MNObject_HasAction(ob, MNA_ACTIVEOUT))
        {
            MNObject_ExecAction(ob, MNA_ACTIVEOUT, NULL);
        }
        return true;
    }
    return false; // Not eaten.
}

static mn_actioninfo_t* MNObject_FindActionInfoForId(mn_object_t* ob, mn_actionid_t id)
{
    assert(ob);
    if(VALID_MNACTION(id))
    {
        return &ob->actions[id];
    }
    return NULL; // Not found.
}

const mn_actioninfo_t* MNObject_Action(mn_object_t* ob, mn_actionid_t id)
{
    return MNObject_FindActionInfoForId(ob, id);
}

boolean MNObject_HasAction(mn_object_t* ob, mn_actionid_t id)
{
    mn_actioninfo_t* info = MNObject_FindActionInfoForId(ob, id);
    return (info && MNActionInfo_IsActionExecuteable(info));
}

int MNObject_ExecAction(mn_object_t* ob, mn_actionid_t id, void* paramaters)
{
    mn_actioninfo_t* info = MNObject_FindActionInfoForId(ob, id);
    if(info && MNActionInfo_IsActionExecuteable(info))
    {
        return info->callback(ob, id, paramaters);
    }
#if _DEBUG
    Con_Error("MNObject::ExecAction: Attempt to execute non-existent action #%i on object %p.", (int) id, ob);
#endif
    /// @todo Need an error handling mechanic.
    return -1; // NOP
}

mn_object_t* MNRect_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNRect::New: Failed on allocation of %lu bytes for new MNRect.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_rect_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNRect::New: Failed on allocation of %lu bytes for mndata_rect_t.", (unsigned long) sizeof(mndata_rect_t));

    ob->_type = MN_RECT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNRect_Ticker;
    ob->drawer = MNRect_Drawer;
    ob->updateGeometry = MNRect_UpdateGeometry;

    return ob;
}

void MNRect_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_RECT);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNRect_Ticker(mn_object_t* ob)
{
    mndata_rect_t* rect = (mndata_rect_t*) ob->_typedata;
    assert(ob && ob->_type == MN_RECT);

    // Stub.
}

void MNRect_Drawer(mn_object_t* ob, const Point2Raw* origin)
{
    mndata_rect_t* rect = (mndata_rect_t*)ob->_typedata;
    assert(ob->_type == MN_RECT);

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin->x, origin->y, 0);
    }

    if(rect->patch)
    {
        DGL_SetPatch(rect->patch, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_Enable(DGL_TEXTURE_2D);
    }

    DGL_Color4f(1, 1, 1, mnRendState->pageAlpha);
    DGL_DrawRect2(0, 0, rect->dimensions.width, rect->dimensions.height);

    if(rect->patch)
    {
        DGL_Disable(DGL_TEXTURE_2D);
    }

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin->x, -origin->y, 0);
    }
}

void MNRect_UpdateGeometry(mn_object_t* ob, mn_page_t* page)
{
    mndata_rect_t* rct = (mndata_rect_t*)ob->_typedata;
    assert(ob->_type == MN_RECT);

    if(rct->dimensions.width == 0 && rct->dimensions.height == 0)
    {
        // Inherit dimensions from the patch.
        patchinfo_t info;
        if(R_GetPatchInfo(rct->patch, &info))
        {
            memcpy(&rct->dimensions, &info.geometry.size, sizeof(rct->dimensions));
        }
    }
    Rect_SetWidthHeight(ob->_geometry, rct->dimensions.width, rct->dimensions.height);
}

void MNRect_SetBackgroundPatch(mn_object_t* ob, patchid_t patch)
{
    mndata_rect_t* rect = (mndata_rect_t*)ob->_typedata;
    assert(ob->_type == MN_RECT);
    rect->patch = patch;
}

mn_object_t* MNText_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNText::New: Failed on allocation of %lu bytes for new MNText.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_text_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNText::New: Failed on allocation of %lu bytes for mndata_text_t.", (unsigned long) sizeof(mndata_text_t));

    ob->_type = MN_TEXT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNText_Ticker;
    ob->drawer = MNText_Drawer;
    ob->updateGeometry = MNText_UpdateGeometry;

    return ob;
}

void MNText_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_TEXT);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNText_Ticker(mn_object_t* ob)
{
    mndata_text_t* txt = (mndata_text_t*) ob->_typedata;
    assert(ob && ob->_type == MN_TEXT);

    // Stub.
}

void MNText_Drawer(mn_object_t* ob, const Point2Raw* origin)
{
    mndata_text_t* txt = (mndata_text_t*)ob->_typedata;
    fontid_t fontId = rs.textFonts[ob->_pageFontIdx];
    float color[4], t = (ob->_flags & MNF_FOCUS)? 1 : 0;
    assert(ob->_type == MN_TEXT);

    // Flash if focused?
    if((ob->_flags & MNF_FOCUS) && cfg.menuTextFlashSpeed > 0)
    {
        const float speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(MNPage_Timer(ob->_page) / (float)TICSPERSEC * speed * PI)) / 2;
    }

    lerpColor(color, rs.textColors[ob->_pageColorIdx], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    color[CA] = rs.textColors[ob->_pageColorIdx][CA];

    DGL_Color4f(1, 1, 1, color[CA]);
    FR_SetFont(fontId);
    FR_SetColorAndAlphav(color);

    if(txt->patch)
    {
        const char* replacement = NULL;
        if(!(txt->flags & MNTEXT_NO_ALTTEXT))
        {
            replacement = Hu_ChoosePatchReplacement2(cfg.menuPatchReplaceMode, *txt->patch, txt->text);
        }
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch3(*txt->patch, replacement, origin, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(txt->text, origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

void MNText_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    mndata_text_t* txt = (mndata_text_t*)obj->_typedata;
    Size2Raw size;
    assert(obj->_type == MN_TEXT);
    /// @todo What if patch replacement is disabled?
    if(txt->patch != 0)
    {
        patchinfo_t info;
        R_GetPatchInfo(*txt->patch, &info);
        Rect_SetWidthHeight(obj->_geometry, info.geometry.size.width, info.geometry.size.height);
        return;
    }
    FR_SetFont(MNPage_PredefinedFont(page, obj->_pageFontIdx));
    FR_TextSize(&size, txt->text);
    Rect_SetWidthHeight(obj->_geometry, size.width, size.height);
}

int MNText_SetFlags(mn_object_t* ob, flagop_t op, int flags)
{
    mndata_text_t* txt = (mndata_text_t*)ob->_typedata;
    assert(ob && ob->_type == MN_TEXT);
    switch(op)
    {
    case FO_CLEAR:  txt->flags &= ~flags;  break;
    case FO_SET:    txt->flags |= flags;   break;
    case FO_TOGGLE: txt->flags ^= flags;   break;
    default:
        Con_Error("MNText::SetFlags: Unknown op %i\n", op);
        exit(1); // Unreachable.
    }
    return ob->_flags;
}

mn_object_t* MNEdit_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNEdit::New: Failed on allocation of %lu bytes for new MNEdit.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_edit_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNEdit::New: Failed on allocation of %lu bytes for mndata_edit_t.", (unsigned long) sizeof(mndata_edit_t));

    ob->_type = MN_EDIT;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->drawer = MNEdit_Drawer;
    ob->ticker = MNEdit_Ticker;
    ob->updateGeometry = MNEdit_UpdateGeometry;
    ob->cmdResponder = MNEdit_CommandResponder;
    ob->responder = MNEdit_Responder;
    { mndata_edit_t* edit = (mndata_edit_t*) ob->_typedata;
    Str_Init(&edit->text);
    Str_Init(&edit->oldtext);
    }

    return ob;
}

void MNEdit_Delete(mn_object_t* ob)
{
    mndata_edit_t* edit = (mndata_edit_t*) ob->_typedata;
    assert(ob && ob->_type == MN_EDIT);
    Str_Free(&edit->text);
    Str_Free(&edit->oldtext);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNEdit_Ticker(mn_object_t* ob)
{
    mndata_edit_t* edit = (mndata_edit_t*) ob->_typedata;
    assert(ob && ob->_type == MN_EDIT);

    // Stub.
}

static void drawEditBackground(const mn_object_t* obj, int x, int y, int width, float alpha)
{
    patchinfo_t leftInfo, rightInfo, middleInfo;
    int leftOffset = 0, rightOffset = 0;

    DGL_Color4f(1, 1, 1, alpha);

    if(R_GetPatchInfo(pEditLeft, &leftInfo))
    {
        DGL_SetPatch(pEditLeft, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x, y, leftInfo.geometry.size.width, leftInfo.geometry.size.height);
        leftOffset = leftInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditRight, &rightInfo))
    {
        DGL_SetPatch(pEditRight, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + width - rightInfo.geometry.size.width, y, rightInfo.geometry.size.width, rightInfo.geometry.size.height);
        rightOffset = rightInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditMiddle, &middleInfo))
    {
        DGL_SetPatch(pEditMiddle, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x + leftOffset, y, width - leftOffset - rightOffset, middleInfo.geometry.size.height, middleInfo.geometry.size.width, middleInfo.geometry.size.height);
    }
}

void MNEdit_Drawer(mn_object_t* ob, const Point2Raw* _origin)
{
    const mndata_edit_t* edit = (mndata_edit_t*) ob->_typedata;
    fontid_t fontId = rs.textFonts[ob->_pageFontIdx];
    float light = 1, textAlpha = rs.pageAlpha;
    uint numVisCharacters;
    const char* string = 0;
    Point2Raw origin;
    assert(ob->_type == MN_EDIT);

    origin.x = _origin->x + MNDATA_EDIT_OFFSET_X;
    origin.y = _origin->y + MNDATA_EDIT_OFFSET_Y;

    if(!Str_IsEmpty(&edit->text))
    {
        string = Str_Text(&edit->text);
    }
    else if(!((ob->_flags & MNF_ACTIVE) && (ob->_flags & MNF_FOCUS)))
    {
        string = edit->emptyString;
        light *= .5f;
        textAlpha = rs.pageAlpha * .75f;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(fontId);

    numVisCharacters = string? strlen(string) : 0;
    if(edit->maxVisibleChars > 0 && edit->maxVisibleChars < numVisCharacters)
        numVisCharacters = edit->maxVisibleChars;

    drawEditBackground(ob, origin.x + MNDATA_EDIT_BACKGROUND_OFFSET_X,
                           origin.y + MNDATA_EDIT_BACKGROUND_OFFSET_Y,
                       Rect_Width(ob->_geometry), rs.pageAlpha);

    if(string)
    {
        float color[4], t = 0;

        // Flash if focused?
        if(!(ob->_flags & MNF_ACTIVE) && (ob->_flags & MNF_FOCUS) && cfg.menuTextFlashSpeed > 0)
        {
            const float speed = cfg.menuTextFlashSpeed / 2.f;
            t = (1 + sin(MNPage_Timer(ob->_page) / (float)TICSPERSEC * speed * PI)) / 2;
        }

        lerpColor(color, cfg.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX], cfg.menuTextFlashColor, t, false/*rgb mode*/);
        color[CA] = textAlpha;

        // Light the text.
        color[CR] *= light; color[CG] *= light; color[CB] *= light;

        // Draw the text:
        FR_SetColorAndAlphav(color);
        FR_DrawText3(string, &origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

        // Are we drawing a cursor?
        if((ob->_flags & MNF_ACTIVE) && (ob->_flags & MNF_FOCUS) && (menuTime & 8) &&
           (!edit->maxLength || (unsigned)Str_Length(&edit->text) < edit->maxLength))
        {
            origin.x += FR_TextWidth(string);
            FR_DrawChar3('_', &origin, ALIGN_TOPLEFT,  MN_MergeMenuEffectWithDrawTextFlags(0));
        }
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

int MNEdit_CommandResponder(mn_object_t* ob, menucommand_e cmd)
{
    mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    assert(ob->_type == MN_EDIT);

    switch(cmd)
    {
    case MCMD_SELECT:
        if(!(ob->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            ob->_flags |= MNF_ACTIVE;
            ob->timer = 0;
            // Store a copy of the present text value so we can restore it.
            Str_Copy(&edit->oldtext, &edit->text);
            if(MNObject_HasAction(ob, MNA_ACTIVE))
            {
                MNObject_ExecAction(ob, MNA_ACTIVE, NULL);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            Str_Copy(&edit->oldtext, &edit->text);
            ob->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(ob, MNA_ACTIVEOUT))
            {
                MNObject_ExecAction(ob, MNA_ACTIVEOUT, NULL);
            }
        }
        return true;
    case MCMD_NAV_OUT:
        if(ob->_flags & MNF_ACTIVE)
        {
            Str_Copy(&edit->text, &edit->oldtext);
            ob->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(ob, MNA_CLOSE))
            {
                MNObject_ExecAction(ob, MNA_CLOSE, NULL);
            }
            return true;
        }
        break;
    default: break;
    }
    return false; // Not eaten.
}

uint MNEdit_MaxLength(mn_object_t* ob)
{
    mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    assert(ob->_type == MN_EDIT);
    return edit->maxLength;
}

void MNEdit_SetMaxLength(mn_object_t* ob, uint newMaxLength)
{
    mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    assert(ob->_type == MN_EDIT);
    if(newMaxLength < edit->maxLength)
    {
        Str_Truncate(&edit->text, newMaxLength);
        Str_Truncate(&edit->oldtext, newMaxLength);
    }
    edit->maxLength = newMaxLength;
}

const ddstring_t* MNEdit_Text(mn_object_t* ob)
{
    mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    assert(ob->_type == MN_EDIT);
    return &edit->text;
}

void MNEdit_SetText(mn_object_t* ob, int flags, const char* string)
{
    mndata_edit_t* edit = (mndata_edit_t*)ob->_typedata;
    assert(ob && ob->_type == MN_EDIT);

    if(!edit->maxLength)
    {
        Str_Set(&edit->text, string);
    }
    else
    {
        Str_Clear(&edit->text);
        Str_PartAppend(&edit->text, string, 0, edit->maxLength);
    }

    if(flags & MNEDIT_STF_REPLACEOLD)
    {
        Str_Copy(&edit->oldtext, &edit->text);
    }
    if(!(flags & MNEDIT_STF_NO_ACTION) && MNObject_HasAction(ob, MNA_MODIFIED))
    {
        MNObject_ExecAction(ob, MNA_MODIFIED, NULL);
    }
}

/**
 * Responds to alphanumeric input for edit fields.
 */
int MNEdit_Responder(mn_object_t* ob, event_t* ev)
{
    mndata_edit_t* edit = (mndata_edit_t*) ob->_typedata;
    int ch = -1;
    assert(ob && ob->_type == MN_EDIT);

    if(!(ob->_flags & MNF_ACTIVE) || ev->type != EV_KEY)
        return false;

    if(DDKEY_RSHIFT == ev->data1)
    {
        shiftdown = (EVS_DOWN == ev->state || EVS_REPEAT == ev->state);
        return true;
    }

    if(!(EVS_DOWN == ev->state || EVS_REPEAT == ev->state))
        return false;

    if(DDKEY_BACKSPACE == ev->data1)
    {
        if(!Str_IsEmpty(&edit->text))
        {
            Str_Truncate(&edit->text, Str_Length(&edit->text)-1);
            if(MNObject_HasAction(ob, MNA_MODIFIED))
            {
                MNObject_ExecAction(ob, MNA_MODIFIED, NULL);
            }
        }
        return true;
    }

    ch = ev->data1;
    if(ch >= ' ' && ch <= 'z')
    {
        if(shiftdown)
            ch = shiftXForm[ch];

        // Filter out nasty characters.
        if(ch == '%')
            return true;

        if(!edit->maxLength || (unsigned)Str_Length(&edit->text) < edit->maxLength)
        {
            Str_AppendChar(&edit->text, ch);
            if(MNObject_HasAction(ob, MNA_MODIFIED))
            {
                MNObject_ExecAction(ob, MNA_MODIFIED, NULL);
            }
        }
        return true;
    }

    return false;
}

void MNEdit_UpdateGeometry(mn_object_t* ob, mn_page_t* page)
{
    // @todo calculate visible dimensions properly.
    assert(ob);
    Rect_SetWidthHeight(ob->_geometry, 170, 14);
}

mn_object_t* MNList_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNList::New: Failed on allocation of %lu bytes for new MNList.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNList::New: Failed on allocation of %lu bytes for mndata_list_t.", (unsigned long) sizeof(mndata_list_t));

    ob->_type = MN_LIST;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNList_Ticker;
    ob->drawer = MNList_Drawer;
    ob->updateGeometry = MNList_UpdateGeometry;
    ob->cmdResponder = MNList_CommandResponder;

    return ob;
}

void MNList_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_LIST);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNList_Ticker(mn_object_t* ob)
{
    mndata_list_t* list = (mndata_list_t*) ob->_typedata;
    assert(ob && ob->_type == MN_LIST);

    // Stub.
}

void MNList_Drawer(mn_object_t* ob, const Point2Raw* _origin)
{
    const mndata_list_t* list = (mndata_list_t*)ob->_typedata;
    const boolean flashSelection = ((ob->_flags & MNF_ACTIVE) && MNList_SelectionIsVisible(ob));
    const float* color = rs.textColors[ob->_pageColorIdx];
    float dimColor[4], flashColor[4], t = flashSelection? 1 : 0;
    Point2Raw origin;
    assert(ob->_type == MN_LIST);

    if(flashSelection && cfg.menuTextFlashSpeed > 0)
    {
        const float speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(MNPage_Timer(ob->_page) / (float)TICSPERSEC * speed * PI)) / 2;
    }

    lerpColor(flashColor, rs.textColors[ob->_pageColorIdx], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    flashColor[CA] = color[CA];

    memcpy(dimColor, color, sizeof(dimColor));
    dimColor[CR] *= MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CG] *= MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CB] *= MNDATA_LIST_NONSELECTION_LIGHT;

    if(list->first < list->count && list->numvis > 0)
    {
        int i = list->first;

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(rs.textFonts[ob->_pageFontIdx]);

        origin.x = _origin->x;
        origin.y = _origin->y;
        do
        {
            const mndata_listitem_t* item = &((const mndata_listitem_t*) list->items)[i];

            if(list->selection == i)
            {
                if(flashSelection)
                    FR_SetColorAndAlphav(flashColor);
                else
                    FR_SetColorAndAlphav(color);
            }
            else
            {
                FR_SetColorAndAlphav(dimColor);
            }

            FR_DrawText3(item->text, &origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
            origin.y += FR_TextHeight(item->text) * (1+MNDATA_LIST_LEADING);
        } while(++i < list->count && i < list->first + list->numvis);

        DGL_Disable(DGL_TEXTURE_2D);
    }
}

int MNList_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    assert(obj->_type == MN_LIST);

    switch(cmd)
    {
    case MCMD_NAV_DOWN:
    case MCMD_NAV_UP:
        if(obj->_flags & MNF_ACTIVE)
        {
            int oldSelection = list->selection;
            if(MCMD_NAV_DOWN == cmd)
            {
                if(list->selection < list->count - 1)
                    ++list->selection;
            }
            else
            {
                if(list->selection > 0)
                    --list->selection;
            }

            if(list->selection != oldSelection)
            {
                S_LocalSound(cmd == MCMD_NAV_DOWN? SFX_MENU_NAV_DOWN : SFX_MENU_NAV_UP, NULL);
                if(MNObject_HasAction(obj, MNA_MODIFIED))
                {
                    MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
                }
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_NAV_OUT:
        if(obj->_flags & MNF_ACTIVE)
        {
            S_LocalSound(SFX_MENU_CANCEL, NULL);
            obj->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_CLOSE))
            {
                MNObject_ExecAction(obj, MNA_CLOSE, NULL);
            }
            return true;
        }
        return false; // Not eaten.

    case MCMD_SELECT:
        if(!(obj->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            obj->_flags |= MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVE))
            {
                MNObject_ExecAction(obj, MNA_ACTIVE, NULL);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            obj->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVEOUT))
            {
                MNObject_ExecAction(obj, MNA_ACTIVEOUT, NULL);
            }
        }
        return true;

    default:
        return false; // Not eaten.
    }
}

int MNList_Selection(mn_object_t* obj)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    assert(obj && (obj->_type == MN_LIST || obj->_type == MN_LISTINLINE));
    return list->selection;
}

boolean MNList_SelectionIsVisible(mn_object_t* obj)
{
    const mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    assert(obj && (obj->_type == MN_LIST || obj->_type == MN_LISTINLINE));
    return (list->selection >= list->first && list->selection < list->first + list->numvis);
}

int MNList_ItemData(const mn_object_t* obj, int index)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    mndata_listitem_t* item;

    assert(obj && (obj->_type == MN_LIST || obj->_type == MN_LISTINLINE));
    if(index < 0 || index >= list->count) return 0;

    item = &((mndata_listitem_t*) list->items)[index];
    return item->data;
}

int MNList_FindItem(const mn_object_t* obj, int dataValue)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    int i;
    assert(obj && (obj->_type == MN_LIST || obj->_type == MN_LISTINLINE));

    for(i = 0; i < list->count; ++i)
    {
        mndata_listitem_t* item = &((mndata_listitem_t*) list->items)[i];
        if(list->mask)
        {
            if((dataValue & list->mask) == item->data)
                return i;
        }
        else if(dataValue == item->data)
        {
            return i;
        }
    }
    return -1;
}

boolean MNList_SelectItem(mn_object_t* obj, int flags, int itemIndex)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    int oldSelection = list->selection;
    assert(obj && (obj->_type == MN_LIST || obj->_type == MN_LISTINLINE));

    if(0 > itemIndex || itemIndex >= list->count) return false;

    list->selection = itemIndex;
    if(list->selection == oldSelection) return false;

    if(!(flags & MNLIST_SIF_NO_ACTION) && MNObject_HasAction(obj, MNA_MODIFIED))
    {
        MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
    }
    return true;
}

boolean MNList_SelectItemByValue(mn_object_t* obj, int flags, int dataValue)
{
    return MNList_SelectItem(obj, flags, MNList_FindItem(obj, dataValue));
}

mn_object_t* MNListInline_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNListInline::New: Failed on allocation of %lu bytes for new MNListInline.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_list_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNListInline::New: Failed on allocation of %lu bytes for mndata_list_t.", (unsigned long) sizeof(mndata_list_t));

    ob->_type = MN_LISTINLINE;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNListInline_Ticker;
    ob->drawer = MNListInline_Drawer;
    ob->updateGeometry = MNListInline_UpdateGeometry;
    ob->cmdResponder = MNListInline_CommandResponder;

    return ob;
}

void MNListInline_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_LISTINLINE);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNListInline_Ticker(mn_object_t* ob)
{
    mndata_list_t* rect = (mndata_list_t*) ob->_typedata;
    assert(ob && ob->_type == MN_LISTINLINE);

    // Stub.
}

void MNListInline_Drawer(mn_object_t* obj, const Point2Raw* origin)
{
    const mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    const mndata_listitem_t* item = ((const mndata_listitem_t*)list->items) + list->selection;
    assert(obj->_type == MN_LISTINLINE);

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(rs.textFonts[obj->_pageFontIdx]);
    FR_SetColorAndAlphav(rs.textColors[obj->_pageColorIdx]);
    FR_DrawText3(item->text, origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);
}

int MNListInline_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    assert(obj->_type == MN_LISTINLINE);

    switch(cmd)
    {
    case MCMD_SELECT: // Treat as @c MCMD_NAV_RIGHT
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        int oldSelection = list->selection;

        if(MCMD_NAV_LEFT == cmd)
        {
            if(list->selection > 0)
                --list->selection;
            else
                list->selection = list->count - 1;
        }
        else
        {
            if(list->selection < list->count - 1)
                ++list->selection;
            else
                list->selection = 0;
        }

        // Adjust the first visible item.
        list->first = list->selection;

        if(oldSelection != list->selection)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            if(MNObject_HasAction(obj, MNA_MODIFIED))
            {
                MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
            }
        }
        return true;
      }
    default:
        return false; // Not eaten.
    }
}

void MNList_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    RectRaw itemGeometry = { 0, 0 };
    int i;
    assert(obj->_type == MN_LIST);

    Rect_SetWidthHeight(obj->_geometry, 0, 0);

    FR_SetFont(MNPage_PredefinedFont(page, obj->_pageFontIdx));
    for(i = 0; i < list->count; ++i)
    {
        mndata_listitem_t* item = &((mndata_listitem_t*)list->items)[i];

        FR_TextSize(&itemGeometry.size, item->text);
        if(i != list->count-1)
            itemGeometry.size.height *= 1 + MNDATA_LIST_LEADING;

        Rect_UniteRaw(obj->_geometry, &itemGeometry);

        itemGeometry.origin.y += itemGeometry.size.height;
    }
}

void MNListInline_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    mndata_listitem_t* item = ((mndata_listitem_t*) list->items) + list->selection;
    Size2Raw size;
    assert(obj->_type == MN_LISTINLINE);

    FR_SetFont(MNPage_PredefinedFont(page, obj->_pageFontIdx));
    FR_TextSize(&size, item->text);
    Rect_SetWidthHeight(obj->_geometry, size.width, size.height);
}

mn_object_t* MNButton_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNButton::New: Failed on allocation of %lu bytes for new MNButton.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_button_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNButton:New: Failed on allocation of %lu bytes for mndata_button_t.", (unsigned long) sizeof(mndata_button_t));

    ob->_type = MN_BUTTON;
    ob->_pageFontIdx = MENU_FONT2;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNButton_Ticker;
    ob->drawer = MNButton_Drawer;
    ob->updateGeometry = MNButton_UpdateGeometry;
    ob->cmdResponder = MNButton_CommandResponder;

    return ob;
}

void MNButton_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_BUTTON);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNButton_Ticker(mn_object_t* ob)
{
    mndata_button_t* btn = (mndata_button_t*) ob->_typedata;
    assert(ob && ob->_type == MN_BUTTON);

    // Stub.
}

void MNButton_Drawer(mn_object_t* ob, const Point2Raw* origin)
{
    mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    //int dis   = (obj->_flags & MNF_DISABLED) != 0;
    //int act   = (obj->_flags & MNF_ACTIVE)   != 0;
    //int click = (obj->_flags & MNF_CLICKED)  != 0;
    //boolean down = act || click;
    const fontid_t fontId = rs.textFonts[ob->_pageFontIdx];
    float color[4], t = (ob->_flags & MNF_FOCUS)? 1 : 0;
    assert(ob->_type == MN_BUTTON);

    // Flash if focused?
    if((ob->_flags & MNF_FOCUS) && cfg.menuTextFlashSpeed > 0)
    {
        const float speed = cfg.menuTextFlashSpeed / 2.f;
        t = (1 + sin(MNPage_Timer(ob->_page) / (float)TICSPERSEC * speed * PI)) / 2;
    }

    lerpColor(color, rs.textColors[ob->_pageColorIdx], cfg.menuTextFlashColor, t, false/*rgb mode*/);
    color[CA] = rs.textColors[ob->_pageColorIdx][CA];

    FR_SetFont(fontId);
    FR_SetColorAndAlphav(color);
    DGL_Color4f(1, 1, 1, color[CA]);

    if(btn->patch)
    {
        const char* replacement = NULL;
        if(!(btn->flags & MNBUTTON_NO_ALTTEXT))
        {
            replacement = Hu_ChoosePatchReplacement2(cfg.menuPatchReplaceMode, *btn->patch, btn->text);
        }
        DGL_Enable(DGL_TEXTURE_2D);
        WI_DrawPatch3(*btn->patch, replacement, origin, ALIGN_TOPLEFT, 0, MN_MergeMenuEffectWithDrawTextFlags(0));
        DGL_Disable(DGL_TEXTURE_2D);
        return;
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_DrawText3(btn->text, origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
    DGL_Disable(DGL_TEXTURE_2D);
}

int MNButton_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
    assert(obj->_type == MN_BUTTON);

    if(cmd == MCMD_SELECT)
    {
        boolean justActivated = false;
        if(!(obj->_flags & MNF_ACTIVE))
        {
            justActivated = true;
            if(btn->staydownMode)
                S_LocalSound(SFX_MENU_CYCLE, NULL);

            obj->_flags |= MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVE))
            {
                MNObject_ExecAction(obj, MNA_ACTIVE, NULL);
            }
        }

        if(!btn->staydownMode)
        {
            // We are not going to receive an "up event" so action that now.
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            obj->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVEOUT))
            {
                MNObject_ExecAction(obj, MNA_ACTIVEOUT, NULL);
            }
        }
        else
        {
            // Stay-down buttons change state.
            if(!justActivated)
                obj->_flags ^= MNF_ACTIVE;

            if(obj->data1)
            {
                void* data = obj->data1;

                *((char*)data) = (obj->_flags & MNF_ACTIVE) != 0;
                if(MNObject_HasAction(obj, MNA_MODIFIED))
                {
                    MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
                }
            }

            if(!justActivated && !(obj->_flags & MNF_ACTIVE))
            {
                S_LocalSound(SFX_MENU_CYCLE, NULL);
                if(MNObject_HasAction(obj, MNA_ACTIVEOUT))
                {
                    MNObject_ExecAction(obj, MNA_ACTIVEOUT, NULL);
                }
            }
        }

        obj->timer = 0;
        return true;
    }

    return false; // Not eaten.
}

void MNButton_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
    //int dis = (obj->_flags & MNF_DISABLED) != 0;
    //int act = (obj->_flags & MNF_ACTIVE)   != 0;
    //int click = (obj->_flags & MNF_CLICKED) != 0;
    //boolean down = act || click;
    const char* text = btn->text;
    Size2Raw size;

    // @todo What if patch replacement is disabled?
    if(btn->patch)
    {
        if(!(btn->flags & MNBUTTON_NO_ALTTEXT))
        {
            // Use the replacement string?
            text = Hu_ChoosePatchReplacement2(cfg.menuPatchReplaceMode, *btn->patch, btn->text);
        }

        if(!text || !text[0])
        {
            // Use the original patch.
            patchinfo_t info;
            R_GetPatchInfo(*btn->patch, &info);
            Rect_SetWidthHeight(obj->_geometry, info.geometry.size.width,
                                                info.geometry.size.height);
            return;
        }
    }

    FR_SetFont(MNPage_PredefinedFont(page, obj->_pageFontIdx));
    FR_TextSize(&size, text);

    Rect_SetWidthHeight(obj->_geometry, size.width, size.height);
}

int MNButton_SetFlags(mn_object_t* ob, flagop_t op, int flags)
{
    mndata_button_t* btn = (mndata_button_t*)ob->_typedata;
    assert(ob && ob->_type == MN_BUTTON);
    switch(op)
    {
    case FO_CLEAR:  btn->flags &= ~flags;  break;
    case FO_SET:    btn->flags |= flags;   break;
    case FO_TOGGLE: btn->flags ^= flags;   break;
    default:
        Con_Error("MNButton::SetFlags: Unknown op %i\n", op);
        exit(1); // Unreachable.
    }
    return btn->flags;
}

mn_object_t* MNColorBox_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNColorBox::New: Failed on allocation of %lu bytes for new MNList.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_colorbox_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNColorBox::New: Failed on allocation of %lu bytes for mndata_colorbox_t.", (unsigned long) sizeof(mndata_colorbox_t));

    ob->_type = MN_COLORBOX;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNColorBox_Ticker;
    ob->drawer = MNColorBox_Drawer;
    ob->updateGeometry = MNColorBox_UpdateGeometry;
    ob->cmdResponder = MNColorBox_CommandResponder;

    return ob;
}

void MNColorBox_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_COLORBOX);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNColorBox_Ticker(mn_object_t* ob)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*) ob->_typedata;
    assert(ob && ob->_type == MN_COLORBOX);

    // Stub.
}

void MNColorBox_Drawer(mn_object_t* obj, const Point2Raw* offset)
{
    const mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    patchinfo_t t, b, l, r, tl, tr, br, bl;
    const int up = 1;
    int x, y, w, h;
    assert(obj->_type == MN_COLORBOX && offset);

    R_GetPatchInfo(borderPatches[0], &t);
    R_GetPatchInfo(borderPatches[2], &b);
    R_GetPatchInfo(borderPatches[3], &l);
    R_GetPatchInfo(borderPatches[1], &r);
    R_GetPatchInfo(borderPatches[4], &tl);
    R_GetPatchInfo(borderPatches[5], &tr);
    R_GetPatchInfo(borderPatches[6], &br);
    R_GetPatchInfo(borderPatches[7], &bl);

    x = offset->x;
    y = offset->y;
    w = cbox->width;
    h = cbox->height;

    if(t.id || tl.id || tr.id)
    {
        int height = 0;
        if(t.id)  height = t.geometry.size.height;
        if(tl.id) height = MAX_OF(height, tl.geometry.size.height);
        if(tr.id) height = MAX_OF(height, tr.geometry.size.height);

        y += height;
    }

    if(l.id || tl.id || bl.id)
    {
        int width = 0;
        if(l.id)  width = l.geometry.size.width;
        if(tl.id) width = MAX_OF(width, tl.geometry.size.width);
        if(bl.id) width = MAX_OF(width, bl.geometry.size.width);

        x += width;
    }

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_SetMaterialUI(P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString(borderGraphics[0])), DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectf2Tiled(x, y, w, h, 64, 64);

    // Top
    if(t.id)
    {
        DGL_SetPatch(t.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x, y - t.geometry.size.height, w, t.geometry.size.height, up * t.geometry.size.width, up * t.geometry.size.height);
    }

    // Bottom
    if(b.id)
    {
        DGL_SetPatch(b.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x, y + h, w, b.geometry.size.height, up * b.geometry.size.width, up * b.geometry.size.height);
    }

    // Left
    if(l.id)
    {
        DGL_SetPatch(l.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x - l.geometry.size.width, y, l.geometry.size.width, h, up * l.geometry.size.width, up * l.geometry.size.height);
    }

    // Right
    if(r.id)
    {
        DGL_SetPatch(r.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectf2Tiled(x + w, y, r.geometry.size.width, h, up * r.geometry.size.width, up * r.geometry.size.height);
    }

    // Top Left
    if(tl.id)
    {
        DGL_SetPatch(tl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x - tl.geometry.size.width, y - tl.geometry.size.height, tl.geometry.size.width, tl.geometry.size.height);
    }

    // Top Right
    if(tr.id)
    {
        DGL_SetPatch(tr.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + w, y - tr.geometry.size.height, tr.geometry.size.width, tr.geometry.size.height);
    }

    // Bottom Right
    if(br.id)
    {
        DGL_SetPatch(br.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x + w, y + h, br.geometry.size.width, br.geometry.size.height);
    }

    // Bottom Left
    if(bl.id)
    {
        DGL_SetPatch(bl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRectf2(x - bl.geometry.size.width, y + h, bl.geometry.size.width, bl.geometry.size.height);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_SetNoMaterial();
    DGL_DrawRectf2Color(x, y, w, h, cbox->r, cbox->g, cbox->b, cbox->a * rs.pageAlpha);
}

int MNColorBox_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    //mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    assert(obj->_type == MN_COLORBOX);

    switch(cmd)
    {
    case MCMD_SELECT:
        if(!(obj->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            obj->_flags |= MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVE))
            {
                MNObject_ExecAction(obj, MNA_ACTIVE, NULL);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            obj->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVEOUT))
            {
                MNObject_ExecAction(obj, MNA_ACTIVEOUT, NULL);
            }
        }
        return true;
    default:
        return false; // Not eaten.
    }
}

void MNColorBox_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    patchinfo_t info;
    assert(obj->_type == MN_COLORBOX);

    Rect_SetWidthHeight(obj->_geometry, cbox->width, cbox->height);

    // Add bottom border?
    if(R_GetPatchInfo(borderPatches[2], &info))
    {
        info.geometry.size.width = cbox->width;
        info.geometry.origin.y = cbox->height;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    // Add right border?
    if(R_GetPatchInfo(borderPatches[1], &info))
    {
        info.geometry.size.height = cbox->height;
        info.geometry.origin.x = cbox->width;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    // Add top border?
    if(R_GetPatchInfo(borderPatches[0], &info))
    {
        info.geometry.size.width = cbox->width;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    // Add left border?
    if(R_GetPatchInfo(borderPatches[3], &info))
    {
        info.geometry.size.height = cbox->height;
        info.geometry.origin.x = -info.geometry.size.width;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    // Add top-left corner?
    if(R_GetPatchInfo(borderPatches[4], &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    // Add top-right corner?
    if(R_GetPatchInfo(borderPatches[5], &info))
    {
        info.geometry.origin.x = cbox->width;
        info.geometry.origin.y = -info.geometry.size.height;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    // Add bottom-right corner?
    if(R_GetPatchInfo(borderPatches[6], &info))
    {
        info.geometry.origin.x = cbox->width;
        info.geometry.origin.y = cbox->height;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    // Add bottom-left corner?
    if(R_GetPatchInfo(borderPatches[7], &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        info.geometry.origin.y = cbox->height;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }
}

boolean MNColorBox_RGBAMode(mn_object_t* obj)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    assert(obj->_type == MN_COLORBOX);
    return cbox->rgbaMode;
}

float MNColorBox_Redf(const mn_object_t* obj)
{
    const mndata_colorbox_t* cbox = (const mndata_colorbox_t*)obj->_typedata;
    assert(obj->_type == MN_COLORBOX);
    return cbox->r;
}

float MNColorBox_Greenf(const mn_object_t* obj)
{
    const mndata_colorbox_t* cbox = (const mndata_colorbox_t*)obj->_typedata;
    assert(obj->_type == MN_COLORBOX);
    return cbox->g;
}

float MNColorBox_Bluef(const mn_object_t* obj)
{
    const mndata_colorbox_t* cbox = (const mndata_colorbox_t*)obj->_typedata;
    assert(obj->_type == MN_COLORBOX);
    return cbox->b;
}

float MNColorBox_Alphaf(const mn_object_t* obj)
{
    const mndata_colorbox_t* cbox = (const mndata_colorbox_t*) obj->_typedata;
    assert(obj->_type == MN_COLORBOX);
    return (cbox->rgbaMode? cbox->a : 1.0f);
}

boolean MNColorBox_SetRedf(mn_object_t* obj, int flags, float red)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    float oldRed = cbox->r;
    assert(obj->_type == MN_COLORBOX);

    cbox->r = red;
    if(cbox->r != oldRed)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && MNObject_HasAction(obj, MNA_MODIFIED))
        {
            MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
        }
        return true;
    }
    return false;
}

boolean MNColorBox_SetGreenf(mn_object_t* obj, int flags, float green)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    float oldGreen = cbox->g;
    assert(obj->_type == MN_COLORBOX);

    cbox->g = green;
    if(cbox->g != oldGreen)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && MNObject_HasAction(obj, MNA_MODIFIED))
        {
            MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
        }
        return true;
    }
    return false;
}

boolean MNColorBox_SetBluef(mn_object_t* obj, int flags, float blue)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    float oldBlue = cbox->b;
    assert(obj->_type == MN_COLORBOX);

    cbox->b = blue;
    if(cbox->b != oldBlue)
    {
        if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && MNObject_HasAction(obj, MNA_MODIFIED))
        {
            MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
        }
        return true;
    }
    return false;
}

boolean MNColorBox_SetAlphaf(mn_object_t* obj, int flags, float alpha)
{
    mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    assert(obj->_type == MN_COLORBOX);

    if(cbox->rgbaMode)
    {
        float oldAlpha = cbox->a;
        cbox->a = alpha;
        if(cbox->a != oldAlpha)
        {
            if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && MNObject_HasAction(obj, MNA_MODIFIED))
            {
                MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
            }
            return true;
        }
    }
    return false;
}

boolean MNColorBox_SetColor4f(mn_object_t* obj, int flags, float red, float green,
    float blue, float alpha)
{
    //mndata_colorbox_t* cbox = (mndata_colorbox_t*)obj->_typedata;
    int setComps = 0, setCompFlags = (flags | MNCOLORBOX_SCF_NO_ACTION);
    assert(obj->_type == MN_COLORBOX);

    if(MNColorBox_SetRedf(  obj, setCompFlags, red))   setComps |= 0x1;
    if(MNColorBox_SetGreenf(obj, setCompFlags, green)) setComps |= 0x2;
    if(MNColorBox_SetBluef( obj, setCompFlags, blue))  setComps |= 0x4;
    if(MNColorBox_SetAlphaf(obj, setCompFlags, alpha)) setComps |= 0x8;

    if(0 == setComps) return false;

    if(!(flags & MNCOLORBOX_SCF_NO_ACTION) && MNObject_HasAction(obj, MNA_MODIFIED))
    {
        MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
    }
    return true;
}

boolean MNColorBox_SetColor4fv(mn_object_t* obj, int flags, float rgba[4])
{
    if(!rgba) return false;
    return MNColorBox_SetColor4f(obj, flags, rgba[CR], rgba[CG], rgba[CB], rgba[CA]);
}

boolean MNColorBox_CopyColor(mn_object_t* obj, int flags, const mn_object_t* other)
{
    assert(obj->_type == MN_COLORBOX);
    if(!other)
    {
#if _DEBUG
        Con_Error("MNColorBox::CopyColor: Called with invalid 'other' argument.");
#endif
        return false;
    }
    return MNColorBox_SetColor4f(obj, flags, MNColorBox_Redf(other),
                                             MNColorBox_Greenf(other),
                                             MNColorBox_Bluef(other),
                                             MNColorBox_Alphaf(other));
}

mn_object_t* MNSlider_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNSlider::New: Failed on allocation of %lu bytes for new MNSlider.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_slider_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNSlider::New: Failed on allocation of %lu bytes for mndata_slider_t.", (unsigned long) sizeof(mndata_slider_t));

    ob->_type = MN_SLIDER;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNSlider_Ticker;
    ob->drawer = MNSlider_Drawer;
    ob->updateGeometry = MNSlider_UpdateGeometry;
    ob->cmdResponder = MNSlider_CommandResponder;

    return ob;
}

void MNSlider_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_SLIDER);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

float MNSlider_Value(const mn_object_t* obj)
{
    const mndata_slider_t* sldr = (const mndata_slider_t*)obj->_typedata;
    assert(obj->_type == MN_SLIDER);

    if(sldr->floatMode)
    {
        return sldr->value;
    }
    return (int) (sldr->value + (sldr->value > 0? .5f : -.5f));
}

void MNSlider_SetValue(mn_object_t* obj, int flags, float value)
{
    mndata_slider_t* sldr = (mndata_slider_t*)obj->_typedata;
    assert(obj->_type == MN_SLIDER);

    if(sldr->floatMode)
        sldr->value = value;
    else
        sldr->value = (int) (value + (value > 0? + .5f : -.5f));
}

int MNSlider_ThumbPos(const mn_object_t* obj)
{
#define WIDTH           (middleInfo.geometry.size.width)

    mndata_slider_t* data = (mndata_slider_t*)obj->_typedata;
    float range = data->max - data->min, useVal;
    patchinfo_t middleInfo;
    assert(obj->_type == MN_SLIDER);

    if(!R_GetPatchInfo(pSliderMiddle, &middleInfo)) return 0;

    if(!range)
        range = 1; // Should never happen.
    useVal = MNSlider_Value(obj) - data->min;
    //return obj->x + UI_BAR_BORDER + butw + useVal / range * (obj->w - UI_BAR_BORDER * 2 - butw * 3);
    return useVal / range * MNDATA_SLIDER_SLOTS * WIDTH;

#undef WIDTH
}

void MNSlider_Ticker(mn_object_t* ob)
{
    mndata_slider_t* sld = (mndata_slider_t*) ob->_typedata;
    assert(ob && ob->_type == MN_SLIDER);

    // Stub.
}

void MNSlider_Drawer(mn_object_t* obj, const Point2Raw* origin)
{
#define WIDTH                   (middleInfo.geometry.size.width)
#define HEIGHT                  (middleInfo.geometry.size.height)

    //const mndata_slider_t* sldr = (mndata_slider_t*)obj->_typedata;
    float x, y;// float range = sldr->max - sldr->min;
    patchinfo_t middleInfo, leftInfo;
    assert(obj->_type == MN_SLIDER && origin);

    if(!R_GetPatchInfo(pSliderMiddle, &middleInfo)) return;
    if(!R_GetPatchInfo(pSliderLeft, &leftInfo)) return;
    if(WIDTH <= 0 || HEIGHT <= 0) return;

    x = origin->x + MNDATA_SLIDER_SCALE * (MNDATA_SLIDER_OFFSET_X + leftInfo.geometry.size.width);
    y = origin->y + MNDATA_SLIDER_SCALE * (MNDATA_SLIDER_OFFSET_Y);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(x, y, 0);
    DGL_Scalef(MNDATA_SLIDER_SCALE, MNDATA_SLIDER_SCALE, 1);

    DGL_Enable(DGL_TEXTURE_2D);

    if(cfg.menuShadow > 0)
    {
        float from[2], to[2];
        from[0] = 2;
        from[1] = 1+HEIGHT/2;
        to[0] = (MNDATA_SLIDER_SLOTS * WIDTH) - 2;
        to[1] = 1+HEIGHT/2;
        M_DrawGlowBar(from, to, HEIGHT*1.1f, true, true, true, 0, 0, 0, rs.pageAlpha * rs.textShadow);
    }

    DGL_Color4f(1, 1, 1, rs.pageAlpha);

    GL_DrawPatchXY3(pSliderLeft, 0, 0, ALIGN_TOPRIGHT, DPF_NO_OFFSETX);
    GL_DrawPatchXY(pSliderRight, MNDATA_SLIDER_SLOTS * WIDTH, 0);

    DGL_SetPatch(pSliderMiddle, DGL_REPEAT, DGL_REPEAT);
    DGL_DrawRectf2Tiled(0, middleInfo.geometry.origin.y, MNDATA_SLIDER_SLOTS * WIDTH, HEIGHT, middleInfo.geometry.size.width, middleInfo.geometry.size.height);

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    GL_DrawPatchXY3(pSliderHandle, MNSlider_ThumbPos(obj), 1, ALIGN_TOP, DPF_NO_OFFSET);

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

#undef HEIGHT
#undef WIDTH
}

int MNSlider_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    mndata_slider_t* sldr = (mndata_slider_t*)obj->_typedata;
    assert(obj->_type == MN_SLIDER);

    switch(cmd)
    {
    case MCMD_NAV_LEFT:
    case MCMD_NAV_RIGHT: {
        float oldvalue = sldr->value;

        if(MCMD_NAV_LEFT == cmd)
        {
            sldr->value -= sldr->step;
            if(sldr->value < sldr->min)
                sldr->value = sldr->min;
        }
        else
        {
            sldr->value += sldr->step;
            if(sldr->value > sldr->max)
                sldr->value = sldr->max;
        }

        // Did the value change?
        if(oldvalue != sldr->value)
        {
            S_LocalSound(SFX_MENU_SLIDER_MOVE, NULL);
            if(MNObject_HasAction(obj, MNA_MODIFIED))
            {
                MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
            }
        }
        return true;
      }
    default:
        return false; // Not eaten.
    }
}

static __inline boolean valueIsOne(float value, boolean floatMode)
{
    if(floatMode)
    {
        return INRANGE_OF(1, value, .0001f);
    }
    return (value > 0 && 1 == (int)(value + .5f));
}

static char* composeTextualValue(float value, boolean floatMode, int precision,
    size_t bufSize, char* buf)
{
    assert(0 != bufSize && buf);
    precision = MAX_OF(0, precision);
    if(floatMode && !valueIsOne(value, floatMode))
    {
        dd_snprintf(buf, bufSize, "%.*f", precision, value);
    }
    else
    {
        dd_snprintf(buf, bufSize, "%.*i", precision, (int)value);
    }
    return buf;
}

static char* composeValueString(float value, float defaultValue, boolean floatMode,
    int precision, const char* defaultString, const char* templateString,
    const char* onethSuffix, const char* nthSuffix, size_t bufSize, char* buf)
{
    const boolean haveTemplateString = (templateString && templateString[0]);
    const boolean haveDefaultString  = (defaultString && defaultString[0]);
    const boolean haveOnethSuffix    = (onethSuffix && onethSuffix[0]);
    const boolean haveNthSuffix      = (nthSuffix && nthSuffix[0]);
    const char* suffix = NULL;
    char textualValue[11];
    assert(0 != bufSize && buf);

    // Is the default-value-string in use?
    if(haveDefaultString && INRANGE_OF(value, defaultValue, .0001f))
    {
        strncpy(buf, defaultString, bufSize);
        buf[bufSize] = '\0';
        return buf;
    }

    composeTextualValue(value, floatMode, precision, 10, textualValue);

    // Choose a suffix.
    if(haveOnethSuffix && valueIsOne(value, floatMode))
    {
        suffix = onethSuffix;
    }
    else if(haveNthSuffix)
    {
        suffix = nthSuffix;
    }
    else
    {
        suffix = "";
    }

    // Are we substituting the textual value into a template?
    if(haveTemplateString)
    {
        const char* c, *beginSubstring = NULL;
        ddstring_t compStr;

        // Reserve a conservative amount of storage, we assume the caller
        // knows best and take the value given as the output buffer size.
        Str_Init(&compStr);
        Str_Reserve(&compStr, bufSize);

        // Composite the final string.
        beginSubstring = templateString;
        for(c = beginSubstring; *c; c++)
        {
            if(c[0] == '%' && c[1] == '1')
            {
                Str_PartAppend(&compStr, beginSubstring, 0, c - beginSubstring);
                Str_Appendf(&compStr, "%s%s", textualValue, suffix);
                // Next substring will begin from here.
                beginSubstring = c + 2;
                c += 1;
            }
        }
        // Anything remaining?
        if(beginSubstring != c)
            Str_Append(&compStr, beginSubstring);

        strncpy(buf, Str_Text(&compStr), bufSize);
        buf[bufSize] = '\0';
        Str_Free(&compStr);
    }
    else
    {
        dd_snprintf(buf, bufSize, "%s%s", textualValue, suffix);
    }

    return buf;
}

void MNSlider_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    int middleWidth;
    patchinfo_t info;
    if(!R_GetPatchInfo(pSliderMiddle, &info)) return;

    middleWidth = info.geometry.size.width * MNDATA_SLIDER_SLOTS;
    Rect_SetWidthHeight(obj->_geometry, middleWidth, info.geometry.size.height);

    if(R_GetPatchInfo(pSliderLeft, &info))
    {
        info.geometry.origin.x = -info.geometry.size.width;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    if(R_GetPatchInfo(pSliderRight, &info))
    {
        info.geometry.origin.x += middleWidth;
        Rect_UniteRaw(obj->_geometry, &info.geometry);
    }

    Rect_SetWidthHeight(obj->_geometry, .5f + Rect_Width(obj->_geometry)  * MNDATA_SLIDER_SCALE,
                                        .5f + Rect_Height(obj->_geometry) * MNDATA_SLIDER_SCALE);
}

void MNSlider_TextualValueDrawer(mn_object_t* obj, const Point2Raw* origin)
{
    const mndata_slider_t* sldr = (mndata_slider_t*)obj->_typedata;
    const float value = MINMAX_OF(sldr->min, sldr->value, sldr->max);
    char textualValue[41];
    const char* str = composeValueString(value, 0, sldr->floatMode, 0,
        sldr->data2, sldr->data3, sldr->data4, sldr->data5, 40, textualValue);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(origin->x, origin->y, 0);

    DGL_Enable(DGL_TEXTURE_2D);

    FR_SetFont(rs.textFonts[obj->_pageFontIdx]);
    FR_SetColorAndAlphav(rs.textColors[obj->_pageColorIdx]);
    FR_DrawTextXY3(str, 0, 0, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_Translatef(-origin->x, -origin->y, 0);
}

void MNSlider_TextualValueUpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    mndata_slider_t* sldr = (mndata_slider_t*)obj->_typedata;
    const fontid_t font = MNPage_PredefinedFont(page, obj->_pageFontIdx);
    const float value = MINMAX_OF(sldr->min, sldr->value, sldr->max);
    char textualValue[41];
    const char* str = composeValueString(value, 0, sldr->floatMode, 0,
        sldr->data2, sldr->data3, sldr->data4, sldr->data5, 40, textualValue);
    Size2Raw size;

    FR_SetFont(font);
    FR_TextSize(&size, str);

    Rect_SetWidthHeight(obj->_geometry, size.width, size.height);
}

mn_object_t* MNMobjPreview_New(void)
{
    mn_object_t* ob = Z_Calloc(sizeof(*ob), PU_GAMESTATIC, 0);
    if(!ob) Con_Error("MNMobjPreview::New: Failed on allocation of %lu bytes for new MNMobjPreview.", (unsigned long) sizeof(*ob));
    ob->_typedata = Z_Calloc(sizeof(mndata_mobjpreview_t), PU_GAMESTATIC, 0);
    if(!ob->_typedata) Con_Error("MNMobjPreview::New: Failed on allocation of %lu bytes for mndata_mobjpreview_t.", (unsigned long) sizeof(mndata_mobjpreview_t));

    ob->_type = MN_MOBJPREVIEW;
    ob->_pageFontIdx = MENU_FONT1;
    ob->_pageColorIdx = MENU_COLOR1;
    ob->ticker = MNMobjPreview_Ticker;
    ob->updateGeometry = MNMobjPreview_UpdateGeometry;
    ob->drawer = MNMobjPreview_Drawer;

    return ob;
}

void MNMobjPreview_Delete(mn_object_t* ob)
{
    assert(ob && ob->_type == MN_MOBJPREVIEW);
    Z_Free(ob->_typedata);
    Z_Free(ob);
}

void MNMobjPreview_Ticker(mn_object_t* ob)
{
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*) ob->_typedata;
    assert(ob && ob->_type == MN_MOBJPREVIEW);

    // Stub.
}

static void findSpriteForMobjType(int mobjType, spritetype_e* sprite, int* frame)
{
    mobjinfo_t* info;
    int stateNum;
    assert(mobjType >= MT_FIRST && mobjType < NUMMOBJTYPES && sprite && frame);

    info = &MOBJINFO[mobjType];
    stateNum = info->states[SN_SPAWN];
    *sprite = STATES[stateNum].sprite;
    *frame = ((menuTime >> 3) & 3);
}

void MNMobjPreview_SetMobjType(mn_object_t* obj, int mobjType)
{
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*)obj->_typedata;
    assert(obj->_type == MN_MOBJPREVIEW);

    mop->mobjType = mobjType;
}

void MNMobjPreview_SetPlayerClass(mn_object_t* obj, int plrClass)
{
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*)obj->_typedata;
    assert(obj->_type == MN_MOBJPREVIEW);

    mop->plrClass = plrClass;
}

void MNMobjPreview_SetTranslationClass(mn_object_t* obj, int tClass)
{
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*)obj->_typedata;
    assert(obj->_type == MN_MOBJPREVIEW);

    mop->tClass = tClass;
}

void MNMobjPreview_SetTranslationMap(mn_object_t* obj, int tMap)
{
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*)obj->_typedata;
    assert(obj->_type == MN_MOBJPREVIEW);

    mop->tMap = tMap;
}

/// @todo We can do better - the engine should be able to render this visual for us.
void MNMobjPreview_Drawer(mn_object_t* ob, const Point2Raw* offset)
{
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*)ob->_typedata;
    int tClass, tMap, spriteFrame;
    spritetype_e sprite;
    spriteinfo_t info;
    float s, t, scale;
    Point2Raw origin;
    Size2Raw size;

    assert(ob->_type == MN_MOBJPREVIEW);

    if(MT_NONE == mop->mobjType) return;

    findSpriteForMobjType(mop->mobjType, &sprite, &spriteFrame);
    if(!R_GetSpriteInfo(sprite, spriteFrame, &info)) return;

    origin.x = info.geometry.origin.x;
    origin.y = info.geometry.origin.y;
    size.width  = info.geometry.size.width;
    size.height = info.geometry.size.height;

    scale = (size.height > size.width? (float)MNDATA_MOBJPREVIEW_HEIGHT / size.height :
                                       (float)MNDATA_MOBJPREVIEW_WIDTH  / size.width);

    s = info.texCoord[0];
    t = info.texCoord[1];

    tClass = mop->tClass;
    tMap = mop->tMap;
    // Are we cycling the translation map?
    if(tMap == NUMPLAYERCOLORS)
    {
        tMap = menuTime / 5 % NUMPLAYERCOLORS;
    }
#if __JHEXEN__
    if(gameMode == hexen_v10)
    {
        // Cycle through the four available colors.
        if(mop->tMap == NUMPLAYERCOLORS) tMap = menuTime / 5 % 4;
    }
    if(mop->plrClass >= PCLASS_FIGHTER)
    {
        R_GetTranslation(mop->plrClass, tMap, &tClass, &tMap);
    }
#endif

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_Translatef(offset->x, offset->y, 0);
    DGL_Scalef(scale, scale, 1);
    // Translate origin to the top left.
    DGL_Translatef(-origin.x, -origin.y, 0);

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_SetPSprite2(info.material, tClass, tMap);
    DGL_Color4f(1, 1, 1, rs.pageAlpha);

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(0, 0);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(size.width, 0);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(size.width, size.height);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(0, size.height);
    DGL_End();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    DGL_Disable(DGL_TEXTURE_2D);
}

void MNMobjPreview_UpdateGeometry(mn_object_t* ob, mn_page_t* page)
{
    // @todo calculate visible dimensions properly!
    assert(ob && ob->_type == MN_MOBJPREVIEW);
    Rect_SetWidthHeight(ob->_geometry, MNDATA_MOBJPREVIEW_WIDTH, MNDATA_MOBJPREVIEW_HEIGHT);
}
