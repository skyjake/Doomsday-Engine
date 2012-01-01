/**\file hu_lib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
extern int menuFlashCounter;
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
        const int winWidth  = Get(DD_WINDOW_WIDTH);
        const int winHeight = Get(DD_WINDOW_HEIGHT);
        am->mcfg = ST_AutomapConfig();
        am->followPlayer = player;
        am->oldViewScale = 1;
        am->maxViewPositionDelta = 128;
        am->alpha = am->targetAlpha = am->oldAlpha = 0;
        obj->geometry.origin.x = 0;
        obj->geometry.origin.y = 0;
        obj->geometry.size.width  = winWidth;
        obj->geometry.size.height = winHeight;
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
    }
    free(widgets);
    widgets = NULL;
    numWidgets = 0;
}

uiwidget_t* GUI_FindObjectById(uiwidgetid_t id)
{
    errorIfNotInited("GUI_FindObjectById");
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
    if(Get(DD_DEDICATED) || Get(DD_NOVIDEO)) return;

    UIAutomap_ReleaseResources();
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

uiwidgetid_t GUI_CreateGroup(int groupFlags, int player, int alignFlags, int padding)
{
    uiwidget_t* obj;
    guidata_group_t* grp;
    errorIfNotInited("GUI_CreateGroup");

    obj = createWidget(GUI_GROUP, player, alignFlags, 1, 0, UIGroup_UpdateGeometry, NULL, NULL, NULL);
    grp = (guidata_group_t*)obj->typedata;
    grp->flags = groupFlags;
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
        Con_Message("Warning:UIGroup::AddWidget: Attempt to add invalid widget %s, ignoring.\n", obj? "(this)" : "(null)");
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
    obj->geometry.origin.x = obj->geometry.origin.y = 0;
    obj->updateGeometry(obj);

    if(obj->geometry.size.width <= 0 || obj->geometry.size.height <= 0) return;

    if(obj->alignFlags & ALIGN_RIGHT)
        obj->geometry.origin.x -= obj->geometry.size.width;
    else if(!(obj->alignFlags & ALIGN_LEFT))
        obj->geometry.origin.x -= obj->geometry.size.width/2;

    if(obj->alignFlags & ALIGN_BOTTOM)
        obj->geometry.origin.y -= obj->geometry.size.height;
    else if(!(obj->alignFlags & ALIGN_TOP))
        obj->geometry.origin.y -= obj->geometry.size.height/2;
}

void UIGroup_UpdateGeometry(uiwidget_t* obj)
{
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    int i, x, y, numVisibleChildren = 0;
    assert(obj && obj->type == GUI_GROUP);

    obj->geometry.size.width = obj->geometry.size.height = 0;

    if(!grp->widgetIdCount) return;

    x = y = 0;
    applyAlignmentOffset(obj, &x, &y);

    for(i = 0; i < grp->widgetIdCount; ++i)
    {
        uiwidget_t* child = GUI_MustFindObjectById(grp->widgetIds[i]);
        const RectRaw* childGeometry;

        if(UIWidget_MaximumWidth(child) > 0 && UIWidget_MaximumHeight(child) > 0 &&
           UIWidget_Opacity(child) > 0)
        {
            updateWidgetGeometry(child);
            child->geometry.origin.x += x;
            child->geometry.origin.y += y;

            childGeometry = UIWidget_Geometry(child);

            if(childGeometry->size.width > 0 && childGeometry->size.height > 0)
            {
                numVisibleChildren++;

                if(grp->flags & UWGF_RIGHTTOLEFT)
                {
                    if(!(grp->flags & UWGF_VERTICAL))
                        x -= childGeometry->size.width  + grp->padding;
                    else
                        y -= childGeometry->size.height + grp->padding;
                }
                else if(grp->flags & UWGF_LEFTTORIGHT)
                {
                    if(!(grp->flags & UWGF_VERTICAL))
                        x += childGeometry->size.width  + grp->padding;
                    else
                        y += childGeometry->size.height + grp->padding;
                }

                if(grp->flags & (UWGF_LEFTTORIGHT|UWGF_RIGHTTOLEFT))
                {
                    if(!(grp->flags & UWGF_VERTICAL))
                    {
                        obj->geometry.size.width  += childGeometry->size.width;
                        if(childGeometry->size.height > obj->geometry.size.height)
                            obj->geometry.size.height = childGeometry->size.height;
                    }
                    else
                    {
                        if(childGeometry->size.width  > obj->geometry.size.width)
                            obj->geometry.size.width  = childGeometry->size.width;
                        obj->geometry.size.height += childGeometry->size.height;
                    }
                }
                else
                {
                    if(childGeometry->size.width  > obj->geometry.size.width)
                        obj->geometry.size.width  = childGeometry->size.width;

                    if(childGeometry->size.height > obj->geometry.size.height)
                        obj->geometry.size.height = childGeometry->size.height;
                }
            }
        }
    }

    if(0 != numVisibleChildren && (grp->flags & (UWGF_LEFTTORIGHT|UWGF_RIGHTTOLEFT)))
    {
        if(!(grp->flags & UWGF_VERTICAL))
            obj->geometry.size.width  += (numVisibleChildren-1) * grp->padding;
        else
            obj->geometry.size.height += (numVisibleChildren-1) * grp->padding;
    }
}

#if _DEBUG
static void drawWidgetGeometry(uiwidget_t* obj)
{
    assert(obj);
    DGL_Color3f(1, 1, 1);
    DGL_Begin(DGL_LINES);
        DGL_Vertex2f(obj->geometry.origin.x, obj->geometry.origin.y);
        DGL_Vertex2f(obj->geometry.origin.x + obj->geometry.size.width, obj->geometry.origin.y);
        DGL_Vertex2f(obj->geometry.origin.x + obj->geometry.size.width, obj->geometry.origin.y);
        DGL_Vertex2f(obj->geometry.origin.x + obj->geometry.size.width, obj->geometry.origin.y + obj->geometry.size.height);
        DGL_Vertex2f(obj->geometry.origin.x + obj->geometry.size.width, obj->geometry.origin.y + obj->geometry.size.height);
        DGL_Vertex2f(obj->geometry.origin.x, obj->geometry.origin.y + obj->geometry.size.height);
        DGL_Vertex2f(obj->geometry.origin.x, obj->geometry.origin.y + obj->geometry.size.height);
        DGL_Vertex2f(obj->geometry.origin.x, obj->geometry.origin.y);
    DGL_End();
}

static void drawWidgetAvailableSpace(uiwidget_t* obj)
{
    assert(obj);
    DGL_Color4f(0, .4f, 0, .1f);
    DGL_DrawRect2(obj->geometry.origin.x, obj->geometry.origin.y, obj->maxSize.width, obj->maxSize.height);
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
        // Configure the page render state.
        /// \todo Initial font renderer setup.
        uiRS.pageAlpha = obj->opacity;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(obj->geometry.origin.x, obj->geometry.origin.y, 0);

        // Do not pass a zero length offset.
        obj->drawer(obj, ((offset && (offset->x || offset->y))? offset : NULL));

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-obj->geometry.origin.x, -obj->geometry.origin.y, 0);
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

const Point2Raw* UIWidget_Origin(uiwidget_t* obj)
{
    assert(obj);
    return &obj->geometry.origin;
}

const RectRaw* UIWidget_Geometry(uiwidget_t* obj)
{
    assert(obj);
    return &obj->geometry;
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

int MN_CountObjects(mn_object_t* list)
{
    int count;
    for(count = 0; MNObject_Type(list) != MN_NONE; list++, count++);
    return count;
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
            obj->updateGeometry(obj, page);
        }
        FR_PopAttrib();
    }
}

/// @return  @c true iff this object is drawable (potentially visible).
boolean MNObject_IsDrawable(mn_object_t* obj)
{
    return !(MNObject_Type(obj) == MN_NONE || !obj->drawer || (MNObject_Flags(obj) & MNF_HIDDEN));
}

static void applyPageLayout(mn_page_t* page)
{
    int i, lineHeight, lineOffset;
    Point2Raw origin = { 0, 0 };

    if(!page) return;

    // Calculate leading/line offset.
    FR_SetFont(MNPage_PredefinedFont(page, MENU_FONT1));
    lineHeight = FR_CharHeight('Q');
    lineOffset = MAX_OF(1, .5f + lineHeight * .08f);

    Rect_SetXY(page->geometry, 0, 0);
    Rect_SetWidthHeight(page->geometry, 0, 0);

    // Apply layout logic to this page.
    for(i = 0; i < page->objectsCount;)
    {
        mn_object_t* obj = &page->objects[i];
        mn_object_t* nextObj = i+1 < page->objectsCount? &page->objects[i+1] : NULL;

        if(!MNObject_IsDrawable(obj))
        {
            // Proceed to the next object!
            i += 1;
            continue;
        }

        Rect_SetXY(obj->_geometry, origin.x, origin.y);

        // Orient label plus button/inline-list/textual-slider pairs about a
        // vertical dividing line, with the label on the left, other object
        // on the right.
        // \todo Do not assume pairing, an object should designate it's pair.
        if(MNObject_Type(obj) == MN_TEXT && nextObj)
        {
            if(MNObject_IsDrawable(nextObj) &&
               (MNObject_Type(nextObj) == MN_BUTTON ||
                MNObject_Type(nextObj) == MN_LISTINLINE ||
                MNObject_Type(nextObj) == MN_COLORBOX ||
                MNObject_Type(nextObj) == MN_BINDINGS ||
                (MNObject_Type(nextObj) == MN_SLIDER && nextObj->drawer == MNSlider_TextualValueDrawer)))
            {
                const int margin = lineOffset * 2;
                RectRaw united;

                Rect_SetXY(nextObj->_geometry, margin + Rect_Width(obj->_geometry), origin.y);

                origin.y += Rect_United(obj->_geometry, nextObj->_geometry, &united)
                          ->size.height + lineOffset;

                Rect_UniteRaw(page->geometry, &united);

                // Extra spacing between object groups.
                if(i+2 < page->objectsCount &&
                   nextObj->_group != page->objects[i+2]._group)
                    origin.y += lineHeight;

                // Proceed to the next object!
                i += 2;
                continue;
            }
        }

        Rect_Unite(page->geometry, obj->_geometry);

        origin.y += Rect_Height(obj->_geometry) + lineOffset;

        // Extra spacing between object groups.
        if(nextObj && nextObj->_group != obj->_group)
            origin.y += lineHeight;

        // Proceed to the next object!
        i += 1;
    }
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
        // \todo Each object should define a focus origin...
        cursorOrigin.x = 0;
        cursorOrigin.y = Point2_Y(MNObject_Origin(focusObj));

        /// \kludge
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
    if(focusObj)
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

    // Finally, the focus cursor?
    /// \todo cursor should be drawn on top of the page drawer.
    if(showFocusCursor && focusObj)
    {
        Hu_MenuDrawFocusCursor(cursorOrigin.x, cursorOrigin.y, focusObjHeight, alpha);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

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

mn_object_t* MNPage_FocusObject(mn_page_t* page)
{
    assert(page);
    if(0 == page->objectsCount || 0 > page->focus) return NULL;
    return &page->objects[page->focus];
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

/// \assume @a obj is a child of @a page.
static void MNPage_GiveChildFocus(mn_page_t* page, mn_object_t* obj, boolean allowRefocus)
{
    mn_object_t* oldFocusObj;
    assert(page && obj);

    if(!(0 > page->focus))
    {
        if(obj != page->objects + page->focus)
        {
            oldFocusObj = page->objects + page->focus;
            if(MNObject_HasAction(oldFocusObj, MNA_FOCUSOUT))
            {
                MNObject_ExecAction(oldFocusObj, MNA_FOCUSOUT, NULL);
            }
            MNObject_SetFlags(oldFocusObj, FO_CLEAR, MNF_FOCUS);
        }
        else if(!allowRefocus)
        {
            return;
        }
    }

    page->focus = obj - page->objects;
    MNObject_SetFlags(obj, FO_SET, MNF_FOCUS);
    if(MNObject_HasAction(obj, MNA_FOCUS))
    {
        MNObject_ExecAction(obj, MNA_FOCUS, NULL);
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

void MNPage_Initialize(mn_page_t* page)
{
    mn_object_t* obj;
    int i;
    assert(page);

    // (Re)init objects.
    for(i = 0, obj = page->objects; i < page->objectsCount; ++i, obj++)
    {
        switch(MNObject_Type(obj))
        {
        case MN_TEXT:
        case MN_MOBJPREVIEW:
            MNObject_SetFlags(obj, FO_SET, MNF_NO_FOCUS);
            break;
        case MN_BUTTON: {
            mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
            if(btn->text && (PTR2INT(btn->text) > 0 && PTR2INT(btn->text) < NUMTEXT))
            {
                btn->text = GET_TXT(PTR2INT(btn->text));
                MNObject_SetShortcut(obj, btn->text[0]);
            }

            if(btn->staydownMode)
            {
                const boolean activate = (*(char*) obj->data1);
                MNObject_SetFlags(obj, (activate? FO_SET:FO_CLEAR), MNF_ACTIVE);
            }
            break;
          }
        case MN_EDIT: {
            mndata_edit_t* edit = (mndata_edit_t*) obj->_typedata;
            if(edit->emptyString && (PTR2INT(edit->emptyString) > 0 && PTR2INT(edit->emptyString) < NUMTEXT))
            {
                edit->emptyString = GET_TXT(PTR2INT(edit->emptyString));
            }

            break;
          }
        case MN_LIST:
        case MN_LISTINLINE: {
            mndata_list_t* list = obj->_typedata;
            int j;
            for(j = 0; j < list->count; ++j)
            {
                mndata_listitem_t* item = &((mndata_listitem_t*)list->items)[j];
                if(item->text && (PTR2INT(item->text) > 0 && PTR2INT(item->text) < NUMTEXT))
                {
                    item->text = GET_TXT(PTR2INT(item->text));
                }
            }

            // Determine number of potentially visible items.
            list->numvis = list->count;
            if(list->selection >= 0)
            {
                if(list->selection < list->first)
                    list->first = list->selection;
                if(list->selection > list->first + list->numvis - 1)
                    list->first = list->selection - list->numvis + 1;
            }
            break;
          }
        case MN_COLORBOX: {
            mndata_colorbox_t* cbox = (mndata_colorbox_t*) obj->_typedata;
            if(!cbox->rgbaMode)
                cbox->a = 1.f;
            if(0 >= cbox->width)
                cbox->width = MNDATA_COLORBOX_WIDTH;
            if(0 >= cbox->height)
                cbox->height = MNDATA_COLORBOX_HEIGHT;
            break;
          }
        default: break;
        }
    }

    if(0 == page->objectsCount)
    {
        // Presumably objects will be added later.
        return;
    }

    // If we haven't yet visited this page then find the first focusable
    // object and select it.
    if(0 > page->focus)
    {
        int i, giveFocus = -1;

        // First look for a default focus object. There should only be one
        // but find the last with this flag...
        for(i = 0; i < page->objectsCount; ++i)
        {
            mn_object_t* obj = &page->objects[i];
            if((MNObject_Flags(obj) & MNF_DEFAULT) && !(MNObject_Flags(obj) & (MNF_DISABLED|MNF_NO_FOCUS)))
            {
                giveFocus = i;
            }
        }

        // No default focus? Find the first focusable object.
        if(-1 == giveFocus)
        for(i = 0; i < page->objectsCount; ++i)
        {
            mn_object_t* obj = &page->objects[i];
            if(!(MNObject_Flags(obj) & (MNF_DISABLED|MNF_NO_FOCUS)))
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
            Con_Message("Warning:MNPage::Initialize: No focusable object on page.\n");
        }
#endif
    }
    else
    {
        // We've been here before; re-focus on the last focused object.
        MNPage_GiveChildFocus(page, page->objects + page->focus, true);
    }
}

fontid_t MNPage_PredefinedFont(mn_page_t* page, mn_page_fontid_t id)
{
    assert(page);
    if(!VALID_MNPAGE_FONTID(id))
    {
#if _DEBUG
        Con_Error("MNPage::PredefinedFont: Invalid font id '%i'.", (int)id);
        exit(1); // Unreachable.
#endif
        return 0; // Not a valid font id.
    }
    return page->fonts[id];
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

mn_obtype_e MNObject_Type(const mn_object_t* obj)
{
    assert(obj);
    return obj->_type;
}

int MNObject_Flags(const mn_object_t* obj)
{
    assert(obj);
    return obj->_flags;
}

const Rect* MNObject_Geometry(const mn_object_t* obj)
{
    assert(obj);
    return obj->_geometry;
}

const Point2* MNObject_Origin(const mn_object_t* obj)
{
    assert(obj);
    return Rect_Origin(obj->_geometry);
}

const Size2* MNObject_Size(const mn_object_t* obj)
{
    assert(obj);
    return Rect_Size(obj->_geometry);
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

int MNObject_Shortcut(mn_object_t* obj)
{
    assert(obj);
    return obj->_shortcut;
}

void MNObject_SetShortcut(mn_object_t* obj, int ddkey)
{
    assert(obj);
    if(isalnum(ddkey))
    {
        obj->_shortcut = tolower(ddkey);
    }
}

int MNObject_Font(mn_object_t* obj)
{
    assert(obj);
    return obj->_pageFontIdx;
}

int MNObject_Color(mn_object_t* obj)
{
    assert(obj);
    return obj->_pageColorIdx;
}

boolean MNObject_IsGroupMember(const mn_object_t* obj, int group)
{
    assert(obj);
    return (obj->_group == group);
}

int MNObject_DefaultCommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    assert(obj);
    if(MCMD_SELECT == cmd && (obj->_flags & MNF_FOCUS) && !(obj->_flags & MNF_DISABLED))
    {
        S_LocalSound(SFX_MENU_ACCEPT, NULL);
        if(!(obj->_flags & MNF_ACTIVE))
        {
            obj->_flags |= MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVE))
            {
                MNObject_ExecAction(obj, MNA_ACTIVE, NULL);
            }
        }

        obj->_flags &= ~MNF_ACTIVE;
        if(MNObject_HasAction(obj, MNA_ACTIVEOUT))
        {
            MNObject_ExecAction(obj, MNA_ACTIVEOUT, NULL);
        }
        return true;
    }
    return false; // Not eaten.
}

static mn_actioninfo_t* MNObject_FindActionInfoForId(mn_object_t* obj, mn_actionid_t id)
{
    assert(obj);
    if(VALID_MNACTION(id))
    {
        return &obj->actions[id];
    }
    return NULL; // Not found.
}

const mn_actioninfo_t* MNObject_Action(mn_object_t* obj, mn_actionid_t id)
{
    return MNObject_FindActionInfoForId(obj, id);
}

boolean MNObject_HasAction(mn_object_t* obj, mn_actionid_t id)
{
    mn_actioninfo_t* info = MNObject_FindActionInfoForId(obj, id);
    return (info && MNActionInfo_IsActionExecuteable(info));
}

int MNObject_ExecAction(mn_object_t* obj, mn_actionid_t id, void* paramaters)
{
    mn_actioninfo_t* info = MNObject_FindActionInfoForId(obj, id);
    if(info && MNActionInfo_IsActionExecuteable(info))
    {
        return info->callback(obj, id, paramaters);
    }
#if _DEBUG
    Con_Error("MNObject::ExecAction: Attempt to execute non-existent action #%i on object %p.", (int) id, obj);
#endif
    /// \fixme Need an error handling mechanic.
    return -1; // NOP
}

void MNText_Drawer(mn_object_t* obj, const Point2Raw* origin)
{
    mndata_text_t* txt = (mndata_text_t*)obj->_typedata;
    fontid_t fontId = rs.textFonts[obj->_pageFontIdx];
    float color[4];
    assert(obj->_type == MN_TEXT);

    memcpy(color, rs.textColors[obj->_pageColorIdx], sizeof(color));

    // Flash the focused object?
    if(obj->_flags & MNF_FOCUS)
    {
        float t = (menuFlashCounter <= 50? menuFlashCounter / 50.0f : (100 - menuFlashCounter) / 50.0f);
        color[CR] *= t; color[CG] *= t; color[CB] *= t;
        color[CR] += cfg.menuTextFlashColor[CR] * (1 - t);
        color[CG] += cfg.menuTextFlashColor[CG] * (1 - t);
        color[CB] += cfg.menuTextFlashColor[CB] * (1 - t);
    }

    DGL_Color4f(1, 1, 1, color[CA]);
    FR_SetFont(fontId);
    FR_SetColorAndAlphav(color);

    if(txt->patch)
    {
        const char* replacement = NULL;
        if(!(obj->_flags & MNF_NO_ALTTEXT))
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
    // @fixme What if patch replacement is disabled?
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

static void drawEditBackground(const mn_object_t* obj, int x, int y, int width, float alpha)
{
    patchinfo_t leftInfo, rightInfo, middleInfo;
    int leftOffset = 0, rightOffset = 0;

    DGL_Color4f(1, 1, 1, alpha);

    if(R_GetPatchInfo(pEditLeft, &leftInfo))
    {
        DGL_SetPatch(pEditLeft, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect2(x, y, leftInfo.geometry.size.width, leftInfo.geometry.size.height);
        leftOffset = leftInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditRight, &rightInfo))
    {
        DGL_SetPatch(pEditRight, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect2(x + width - rightInfo.geometry.size.width, y, rightInfo.geometry.size.width, rightInfo.geometry.size.height);
        rightOffset = rightInfo.geometry.size.width;
    }

    if(R_GetPatchInfo(pEditMiddle, &middleInfo))
    {
        DGL_SetPatch(pEditMiddle, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x + leftOffset, y, width - leftOffset - rightOffset, middleInfo.geometry.size.height, middleInfo.geometry.size.width, middleInfo.geometry.size.height);
    }
}

void MNEdit_Drawer(mn_object_t* obj, const Point2Raw* _origin)
{
    const mndata_edit_t* edit = (mndata_edit_t*) obj->_typedata;
    fontid_t fontId = rs.textFonts[obj->_pageFontIdx];
    char buf[MNDATA_EDIT_TEXT_MAX_LENGTH+1];
    float light = 1, textAlpha = rs.pageAlpha;
    int width, numVisCharacters;
    const char* string;
    Point2Raw origin;
    assert(obj->_type == MN_EDIT);

    origin.x = _origin->x + MNDATA_EDIT_OFFSET_X;
    origin.y = _origin->y + MNDATA_EDIT_OFFSET_Y;

    if((obj->_flags & MNF_ACTIVE) && (obj->_flags & MNF_FOCUS))
    {
        if((menuTime & 8) && strlen(edit->text) < MNDATA_EDIT_TEXT_MAX_LENGTH)
        {
            dd_snprintf(buf, MNDATA_EDIT_TEXT_MAX_LENGTH+1, "%s_", edit->text);
            string = buf;
        }
        else
            string = edit->text;
    }
    else
    {
        if(edit->text[0])
        {
            string = edit->text;
        }
        else
        {
            string = edit->emptyString;
            light *= .5f;
            textAlpha = rs.pageAlpha * .75f;
        }
    }

    DGL_Enable(DGL_TEXTURE_2D);
    FR_SetFont(fontId);

    if(edit->maxVisibleChars > 0)
        numVisCharacters = MIN_OF(edit->maxVisibleChars, MNDATA_EDIT_TEXT_MAX_LENGTH);
    else
        numVisCharacters = MNDATA_EDIT_TEXT_MAX_LENGTH;
    width = numVisCharacters * FR_CharWidth('_') + 20;
    drawEditBackground(obj, origin.x + MNDATA_EDIT_BACKGROUND_OFFSET_X,
                            origin.y + MNDATA_EDIT_BACKGROUND_OFFSET_Y, width, rs.pageAlpha);

    if(string)
    {
        float color[4];
        color[CR] = cfg.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX][CR];
        color[CG] = cfg.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX][CG];
        color[CB] = cfg.menuTextColors[MNDATA_EDIT_TEXT_COLORIDX][CB];

        if(obj->_flags & MNF_FOCUS)
        {
            float t;

            if(obj->_flags & MNF_ACTIVE)
            {
                t = 0;
            }
            else
            {
                t = (menuFlashCounter <= 50? (menuFlashCounter / 50.0f) :
                                             ((100 - menuFlashCounter) / 50.0f));
            }

            color[CR] *= t;
            color[CG] *= t;
            color[CB] *= t;

            color[CR] += cfg.menuTextFlashColor[CR] * (1 - t);
            color[CG] += cfg.menuTextFlashColor[CG] * (1 - t);
            color[CB] += cfg.menuTextFlashColor[CB] * (1 - t);
        }
        color[CA] = textAlpha;

        color[CR] *= light;
        color[CG] *= light;
        color[CB] *= light;

        FR_SetColorAndAlphav(color);
        FR_DrawText3(string, &origin, ALIGN_TOPLEFT, MN_MergeMenuEffectWithDrawTextFlags(0));
    }

    DGL_Disable(DGL_TEXTURE_2D);
}

int MNEdit_CommandResponder(mn_object_t* obj, menucommand_e cmd)
{
    mndata_edit_t* edit = (mndata_edit_t*)obj->_typedata;
    assert(obj->_type == MN_EDIT);

    switch(cmd)
    {
    case MCMD_SELECT:
        if(!(obj->_flags & MNF_ACTIVE))
        {
            S_LocalSound(SFX_MENU_CYCLE, NULL);
            obj->_flags |= MNF_ACTIVE;
            // Store a copy of the present text value so we can restore it.
            memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));
            if(MNObject_HasAction(obj, MNA_ACTIVE))
            {
                MNObject_ExecAction(obj, MNA_ACTIVE, NULL);
            }
        }
        else
        {
            S_LocalSound(SFX_MENU_ACCEPT, NULL);
            memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));
            obj->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_ACTIVEOUT))
            {
                MNObject_ExecAction(obj, MNA_ACTIVEOUT, NULL);
            }
        }
        return true;
    case MCMD_NAV_OUT:
        if(obj->_flags & MNF_ACTIVE)
        {
            memcpy(edit->text, edit->oldtext, sizeof(edit->text));
            obj->_flags &= ~MNF_ACTIVE;
            if(MNObject_HasAction(obj, MNA_CLOSE))
            {
                MNObject_ExecAction(obj, MNA_CLOSE, NULL);
            }
            return true;
        }
        break;
    default: break;
    }
    return false; // Not eaten.
}

const char* MNEdit_Text(mn_object_t* obj)
{
    mndata_edit_t* edit = (mndata_edit_t*)obj->_typedata;
    assert(obj->_type == MN_EDIT);
    return edit->text;
}

void MNEdit_SetText(mn_object_t* obj, int flags, const char* string)
{
    mndata_edit_t* edit = (mndata_edit_t*)obj->_typedata;
    assert(obj && obj->_type == MN_EDIT);

    //strncpy(edit->ptr, Con_GetString(edit->data), edit->maxLen);
    dd_snprintf(edit->text, MNDATA_EDIT_TEXT_MAX_LENGTH, "%s", string);
    if(flags & MNEDIT_STF_REPLACEOLD)
    {
        memcpy(edit->oldtext, edit->text, sizeof(edit->oldtext));
    }
    if(!(flags & MNEDIT_STF_NO_ACTION) && MNObject_HasAction(obj, MNA_MODIFIED))
    {
        MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
    }
}

/**
 * Responds to alphanumeric input for edit fields.
 */
int MNEdit_Responder(mn_object_t* obj, event_t* ev)
{
    mndata_edit_t* edit = (mndata_edit_t*) obj->_typedata;
    int ch = -1;
    char* ptr;
    assert(obj && obj->_type == MN_EDIT);

    if(!(obj->_flags & MNF_ACTIVE) || ev->type != EV_KEY)
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
        size_t len = strlen(edit->text);
        if(0 != len)
        {
            edit->text[len - 1] = '\0';
            if(MNObject_HasAction(obj, MNA_MODIFIED))
            {
                MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
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

        if(strlen(edit->text) < MNDATA_EDIT_TEXT_MAX_LENGTH)
        {
            ptr = edit->text + strlen(edit->text);
            ptr[0] = ch;
            ptr[1] = '\0';
            if(MNObject_HasAction(obj, MNA_MODIFIED))
            {
                MNObject_ExecAction(obj, MNA_MODIFIED, NULL);
            }
        }
        return true;
    }

    return false;
}

void MNEdit_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    // @fixme calculate visible dimensions properly.
    assert(obj);
    Rect_SetWidthHeight(obj->_geometry, 170, 14);
}

void MNList_Drawer(mn_object_t* obj, const Point2Raw* _origin)
{
    const mndata_list_t* list = (mndata_list_t*)obj->_typedata;
    const boolean flashSelection = ((obj->_flags & MNF_ACTIVE) && MNList_SelectionIsVisible(obj));
    const float* color = rs.textColors[obj->_pageColorIdx];
    float dimColor[4], flashColor[4];
    Point2Raw origin;
    assert(obj->_type == MN_LIST);

    if(flashSelection)
    {
        float t = (menuFlashCounter <= 50? menuFlashCounter / 50.0f : (100 - menuFlashCounter) / 50.0f);
        flashColor[CR] = color[CR] * t + cfg.menuTextFlashColor[CR] * (1 - t);
        flashColor[CG] = color[CG] * t + cfg.menuTextFlashColor[CG] * (1 - t);
        flashColor[CB] = color[CB] * t + cfg.menuTextFlashColor[CB] * (1 - t);
        flashColor[CA] = color[CA];
    }

    dimColor[CR] = color[CR] * MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CG] = color[CG] * MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CB] = color[CB] * MNDATA_LIST_NONSELECTION_LIGHT;
    dimColor[CA] = color[CA];

    if(list->first < list->count && list->numvis > 0)
    {
        int i = list->first;

        DGL_Enable(DGL_TEXTURE_2D);
        FR_SetFont(rs.textFonts[obj->_pageFontIdx]);

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

void MNButton_Drawer(mn_object_t* obj, const Point2Raw* origin)
{
    mndata_button_t* btn = (mndata_button_t*)obj->_typedata;
    //int dis   = (obj->_flags & MNF_DISABLED) != 0;
    //int act   = (obj->_flags & MNF_ACTIVE)   != 0;
    //int click = (obj->_flags & MNF_CLICKED)  != 0;
    //boolean down = act || click;
    const fontid_t fontId = rs.textFonts[obj->_pageFontIdx];
    float color[4];
    assert(obj->_type == MN_BUTTON);

    memcpy(color, rs.textColors[obj->_pageColorIdx], sizeof(color));

    // Flash the focused object?
    if(obj->_flags & MNF_FOCUS)
    {
        float t = (menuFlashCounter <= 50? menuFlashCounter / 50.0f : (100 - menuFlashCounter) / 50.0f);
        color[CR] *= t; color[CG] *= t; color[CB] *= t;
        color[CR] += cfg.menuTextFlashColor[CR] * (1 - t);
        color[CG] += cfg.menuTextFlashColor[CG] * (1 - t);
        color[CB] += cfg.menuTextFlashColor[CB] * (1 - t);
    }

    FR_SetFont(fontId);
    FR_SetColorAndAlphav(color);
    DGL_Color4f(1, 1, 1, color[CA]);

    if(btn->patch)
    {
        const char* replacement = NULL;
        if(!(obj->_flags & MNF_NO_ALTTEXT))
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

    if(MCMD_SELECT == cmd)
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
            S_LocalSound(SFX_MENU_CYCLE, NULL);

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

    // @fixme What if patch replacement is disabled?
    if(btn->patch)
    {
        if(!(obj->_flags & MNF_NO_ALTTEXT))
        {
            // Use the replacement string?
            text = Hu_ChoosePatchReplacement2(cfg.menuPatchReplaceMode, *btn->patch, btn->text);
        }

        if(!text || text[0])
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

    DGL_SetMaterialUI(P_ToPtr(DMU_MATERIAL, Materials_ResolveUriCString(borderGraphics[0])));
    DGL_DrawRectTiled(x, y, w, h, 64, 64);

    // Top
    if(t.id)
    {
        DGL_SetPatch(t.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x, y - t.geometry.size.height, w, t.geometry.size.height, up * t.geometry.size.width, up * t.geometry.size.height);
    }

    // Bottom
    if(b.id)
    {
        DGL_SetPatch(b.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x, y + h, w, b.geometry.size.height, up * b.geometry.size.width, up * b.geometry.size.height);
    }

    // Left
    if(l.id)
    {
        DGL_SetPatch(l.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x - l.geometry.size.width, y, l.geometry.size.width, h, up * l.geometry.size.width, up * l.geometry.size.height);
    }

    // Right
    if(r.id)
    {
        DGL_SetPatch(r.id, DGL_REPEAT, DGL_REPEAT);
        DGL_DrawRectTiled(x + w, y, r.geometry.size.width, h, up * r.geometry.size.width, up * r.geometry.size.height);
    }

    // Top Left
    if(tl.id)
    {
        DGL_SetPatch(tl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect2(x - tl.geometry.size.width, y - tl.geometry.size.height, tl.geometry.size.width, tl.geometry.size.height);
    }

    // Top Right
    if(tr.id)
    {
        DGL_SetPatch(tr.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect2(x + w, y - tr.geometry.size.height, tr.geometry.size.width, tr.geometry.size.height);
    }

    // Bottom Right
    if(br.id)
    {
        DGL_SetPatch(br.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect2(x + w, y + h, br.geometry.size.width, br.geometry.size.height);
    }

    // Bottom Left
    if(bl.id)
    {
        DGL_SetPatch(bl.id, DGL_CLAMP_TO_EDGE, DGL_CLAMP_TO_EDGE);
        DGL_DrawRect2(x - bl.geometry.size.width, y + h, bl.geometry.size.width, bl.geometry.size.height);
    }

    DGL_Disable(DGL_TEXTURE_2D);

    DGL_SetNoMaterial();
    DGL_DrawRectColor(x, y, w, h, cbox->r, cbox->g, cbox->b, cbox->a * rs.pageAlpha);
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
    DGL_DrawRectTiled(0, middleInfo.geometry.origin.y, MNDATA_SLIDER_SLOTS * WIDTH, HEIGHT, middleInfo.geometry.size.width, middleInfo.geometry.size.height);

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
        info.geometry.origin.x = -info.geometry.size.width;
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

/// \todo We can do better - the engine should be able to render this visual for us.
void MNMobjPreview_Drawer(mn_object_t* obj, const Point2Raw* origin)
{
    mndata_mobjpreview_t* mop = (mndata_mobjpreview_t*)obj->_typedata;
    float x, y, w, h, s, t, scale;
    int tClass, tMap, spriteFrame;
    spritetype_e sprite;
    spriteinfo_t info;
    assert(obj->_type == MN_MOBJPREVIEW);

    if(MT_NONE == mop->mobjType) return;

    findSpriteForMobjType(mop->mobjType, &sprite, &spriteFrame);
    if(!R_GetSpriteInfo(sprite, spriteFrame, &info)) return;

    x = origin->x;
    y = origin->y;
    w = info.geometry.size.width;
    h = info.geometry.size.height;
    scale = (h > w? MNDATA_MOBJPREVIEW_HEIGHT / h : MNDATA_MOBJPREVIEW_WIDTH / w);
    w *= scale;
    h *= scale;

    x += MNDATA_MOBJPREVIEW_WIDTH/2 - info.geometry.size.width/2 * scale;
    y += MNDATA_MOBJPREVIEW_HEIGHT  - info.geometry.size.height  * scale;

    tClass = mop->tClass;
    tMap = mop->tMap;
    // Are we cycling the translation map?
    if(tMap == NUMPLAYERCOLORS)
        tMap = menuTime / 5 % NUMPLAYERCOLORS;
#if __JHEXEN__
    if(mop->plrClass >= PCLASS_FIGHTER)
        R_GetTranslation(mop->plrClass, tMap, &tClass, &tMap);
#endif

    DGL_Enable(DGL_TEXTURE_2D);
    DGL_SetPSprite2(info.material, tClass, tMap);

    s = info.texCoord[0];
    t = info.texCoord[1];

    DGL_Color4f(1, 1, 1, rs.pageAlpha);
    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0 * s, 0);
        DGL_Vertex2f(x, y);

        DGL_TexCoord2f(0, 1 * s, 0);
        DGL_Vertex2f(x + w, y);

        DGL_TexCoord2f(0, 1 * s, t);
        DGL_Vertex2f(x + w, y + h);

        DGL_TexCoord2f(0, 0 * s, t);
        DGL_Vertex2f(x, y + h);
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);
}

void MNMobjPreview_UpdateGeometry(mn_object_t* obj, mn_page_t* page)
{
    // @fixme calculate visible dimensions properly!
    assert(obj && obj->_type == MN_MOBJPREVIEW);
    Rect_SetWidthHeight(obj->_geometry, MNDATA_MOBJPREVIEW_WIDTH, MNDATA_MOBJPREVIEW_HEIGHT);
}
