/** @file hu_lib.cpp  HUD widget library.
 *
 * @authors Copyright © 2005-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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

#include "common.h"

#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <de/memory.h>

#include "hu_chat.h"
#include "hu_lib.h"
#include "hu_log.h"
#include "hu_automap.h"

/// @todo remove me
#include "menu/widgets/lineeditwidget.h"
#include "menu/widgets/sliderwidget.h"

using namespace de;
using namespace common;

static dd_bool inited = false;

static int numWidgets;
static uiwidget_t *widgets;

static ui_rendstate_t uiRS;
const ui_rendstate_t *uiRendState = &uiRS;

static void errorIfNotInited(char const *callerName)
{
    if(inited) return;
    Con_Error("%s: GUI module is not presently initialized.", callerName);
    // Unreachable. Prevents static analysers from getting rather confused, poor things.
    exit(1);
}

void lerpColor(float *dst, float const *a, float const *b, float t, dd_bool rgbaMode)
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

static uiwidgetid_t nextUnusedId()
{
    return uiwidgetid_t(numWidgets);
}

static uiwidget_t *allocateWidget(guiwidgettype_t type, uiwidgetid_t id, int player, void *typedata)
{
    widgets = (uiwidget_t *) M_Realloc(widgets, sizeof(*widgets) * ++numWidgets);

    uiwidget_t *ob = &widgets[numWidgets-1];

    de::zapPtr(ob);
    ob->geometry = Rect_New();
    ob->type     = type;
    ob->id       = id;
    ob->player   = player;

    switch(ob->type)
    {
    case GUI_GROUP: {
        ob->typedata = (guidata_group_t *) M_Calloc(sizeof(guidata_group_t));
        break; }

    default:
        ob->typedata = typedata;
        break;
    }

    switch(ob->type)
    {
    case GUI_AUTOMAP: {
        guidata_automap_t *am = (guidata_automap_t*)ob->typedata;
        am->mcfg                 = ST_AutomapConfig();
        am->followPlayer         = player;
        am->oldViewScale         = 1;
        am->maxViewPositionDelta = 128;
        am->alpha = am->targetAlpha = am->oldAlpha = 0;

        /// Set initial geometry size.
        /// @todo Should not be necessary...
        Rect_SetWidthHeight(ob->geometry, SCREENWIDTH, SCREENHEIGHT);
        break; }

    default: break;
    }

    return ob;
}

static uiwidget_t *createWidget(guiwidgettype_t type, int player, int alignFlags,
    fontid_t fontId, float opacity, void (*updateGeometry) (uiwidget_t *ob),
    void (*drawer) (uiwidget_t *ob, Point2Raw const *offset),
    void (*ticker) (uiwidget_t *ob, timespan_t ticLength), void *typedata)
{
    DENG2_ASSERT(updateGeometry != 0);

    uiwidget_t *ob = allocateWidget(type, nextUnusedId(), player, typedata);

    ob->alignFlags     = alignFlags;
    ob->font           = fontId;
    ob->opacity        = opacity;
    ob->updateGeometry = updateGeometry;
    ob->drawer         = drawer;
    ob->ticker         = ticker;

    return ob;
}

static void clearWidgets()
{
    if(!numWidgets) return;

    for(int i = 0; i < numWidgets; ++i)
    {
        uiwidget_t *ob = &widgets[i];
        if(ob->type == GUI_GROUP)
        {
            guidata_group_t *grp = (guidata_group_t *)ob->typedata;
            M_Free(grp->widgetIds);
            M_Free(grp);
        }
        Rect_Delete(ob->geometry);
    }
    M_Free(widgets); widgets = 0;
    numWidgets = 0;
}

uiwidget_t *GUI_FindObjectById(uiwidgetid_t id)
{
    if(!inited) return 0; // GUI not available.
    if(id >= 0)
    {
        for(int i = 0; i < numWidgets; ++i)
        {
            uiwidget_t *ob = &widgets[i];
            if(ob->id == id)
            {
                return ob;
            }
        }
    }
    return 0; // Not found.
}

uiwidget_t *GUI_MustFindObjectById(uiwidgetid_t id)
{
    uiwidget_t *ob = GUI_FindObjectById(id);
    if(!ob)
    {
        Con_Error("GUI_MustFindObjectById: Failed to locate object with id %i.", (int) id);
    }
    return ob;
}

void GUI_Register()
{
    UIAutomap_Register();
    UIChat_Register();
    UILog_Register();
}

void GUI_Init()
{
    if(inited) return;

    numWidgets = 0;
    widgets    = 0;
    UIChat_LoadMacros();

    inited = true;

    GUI_LoadResources();
}

void GUI_Shutdown()
{
    if(!inited) return;
    clearWidgets();
    inited = false;
}

void GUI_LoadResources()
{
    if(Get(DD_DEDICATED) || Get(DD_NOVIDEO)) return;

    UIAutomap_LoadResources();
    menu::LineEditWidget::loadResources();
    menu::SliderWidget::loadResources();
}

void GUI_ReleaseResources()
{
    if(Get(DD_DEDICATED) || Get(DD_NOVIDEO)) return;

    UIAutomap_ReleaseResources();

    for(int i = 0; i < numWidgets; ++i)
    {
        uiwidget_t *ob = widgets + i;
        switch(ob->type)
        {
        case GUI_AUTOMAP: UIAutomap_Reset(ob); break;
        default: break;
        }
    }
}

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, int alignFlags,
    fontid_t fontId, float opacity,
    void (*updateGeometry) (uiwidget_t *ob), void (*drawer) (uiwidget_t *ob, Point2Raw const *offset),
    void (*ticker) (uiwidget_t *ob, timespan_t ticLength), void *typedata)
{
    errorIfNotInited("GUI_CreateWidget");
    uiwidget_t *ob = createWidget(type, player, alignFlags, fontId, opacity, updateGeometry, drawer, ticker, typedata);
    return ob->id;
}

uiwidgetid_t GUI_CreateGroup(int groupFlags, int player, int alignFlags, order_t order, int padding)
{
    errorIfNotInited("GUI_CreateGroup");

    uiwidget_t *ob = createWidget(GUI_GROUP, player, alignFlags, 1, 0, UIGroup_UpdateGeometry, NULL, NULL, NULL);
    guidata_group_t *grp = (guidata_group_t *)ob->typedata;
    grp->flags   = groupFlags;
    grp->order   = order;
    grp->padding = padding;

    return ob->id;
}

void UIGroup_AddWidget(uiwidget_t *ob, uiwidget_t *other)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);

    guidata_group_t *grp = (guidata_group_t *)ob->typedata;

    if(!other || other == ob)
    {
        DENG2_ASSERT(!"UIGroup::AddWidget: Attempt to add invalid widget");
        return;
    }

    // Ensure widget is not already in this grp.
    for(int i = 0; i < grp->widgetIdCount; ++i)
    {
        if(grp->widgetIds[i] == other->id)
            return; // Already present. Ignore.
    }

    // Must add to this grp.
    grp->widgetIds = (uiwidgetid_t *) M_Realloc(grp->widgetIds, sizeof(*grp->widgetIds) * ++grp->widgetIdCount);
    grp->widgetIds[grp->widgetIdCount - 1] = other->id;
}

int UIGroup_Flags(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);
    return static_cast<guidata_group_t *>(ob->typedata)->flags;
}

void UIGroup_SetFlags(uiwidget_t *ob, int flags)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);
    static_cast<guidata_group_t *>(ob->typedata)->flags = flags;
}

static void applyAlignmentOffset(uiwidget_t *ob, int *x, int *y)
{
    DENG2_ASSERT(ob != 0);

    if(x)
    {
        if(ob->alignFlags & ALIGN_RIGHT)
            *x += UIWidget_MaximumWidth(ob);
        else if(!(ob->alignFlags & ALIGN_LEFT))
            *x += UIWidget_MaximumWidth(ob)/2;
    }
    if(y)
    {
        if(ob->alignFlags & ALIGN_BOTTOM)
            *y += UIWidget_MaximumHeight(ob);
        else if(!(ob->alignFlags & ALIGN_TOP))
            *y += UIWidget_MaximumHeight(ob)/2;
    }
}

static void updateWidgetGeometry(uiwidget_t *ob)
{
    Rect_SetXY(ob->geometry, 0, 0);
    ob->updateGeometry(ob);

    if(Rect_Width(ob->geometry) <= 0 || Rect_Height(ob->geometry) <= 0) return;

    if(ob->alignFlags & ALIGN_RIGHT)
        Rect_SetX(ob->geometry, Rect_X(ob->geometry) - Rect_Width(ob->geometry));
    else if(!(ob->alignFlags & ALIGN_LEFT))
        Rect_SetX(ob->geometry, Rect_X(ob->geometry) - Rect_Width(ob->geometry)/2);

    if(ob->alignFlags & ALIGN_BOTTOM)
        Rect_SetY(ob->geometry, Rect_Y(ob->geometry) - Rect_Height(ob->geometry));
    else if(!(ob->alignFlags & ALIGN_TOP))
        Rect_SetY(ob->geometry, Rect_Y(ob->geometry) - Rect_Height(ob->geometry)/2);
}

void UIGroup_UpdateGeometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0 && ob->type == GUI_GROUP);

    guidata_group_t *grp = (guidata_group_t*)ob->typedata;

    Rect_SetWidthHeight(ob->geometry, 0, 0);

    if(!grp->widgetIdCount) return;

    int x = 0, y = 0;
    applyAlignmentOffset(ob, &x, &y);

    for(int i = 0; i < grp->widgetIdCount; ++i)
    {
        uiwidget_t *child = GUI_MustFindObjectById(grp->widgetIds[i]);
        Rect const *childGeometry;

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

                Rect_Unite(ob->geometry, childGeometry);
            }
        }
    }
}

#if 0 //def DENG2_DEBUG
static void drawWidgetGeometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);

    RectRaw geometry;
    Rect_Raw(ob->geometry, &geometry);

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

static void drawWidgetAvailableSpace(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    DGL_Color4f(0, .4f, 0, .1f);
    DGL_DrawRectf2(Rect_X(ob->geometry), Rect_Y(ob->geometry), ob->maxSize.width, ob->maxSize.height);
}
#endif

static void drawWidget2(uiwidget_t *ob, Point2Raw const *offset)
{
    DENG2_ASSERT(ob != 0);

/*#if _DEBUG
    drawWidgetAvailableSpace(ob);
#endif*/

    if(ob->drawer && ob->opacity > .0001f)
    {
        Point2Raw origin;
        Point2_Raw(Rect_Origin(ob->geometry), &origin);

        // Configure the page render state.
        uiRS.pageAlpha = ob->opacity;

        FR_PushAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin.x, origin.y, 0);

        // Do not pass a zero length offset.
        ob->drawer(ob, ((offset && (offset->x || offset->y))? offset : NULL));

        FR_PopAttrib();

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin.x, -origin.y, 0);
    }

/*#if _DEBUG
    drawWidgetGeometry(ob);
#endif*/
}

static void drawWidget(uiwidget_t *ob, Point2Raw const *origin)
{
    DENG2_ASSERT(ob != 0);

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(origin->x, origin->y, 0);
    }

    // First we draw ourself.
    drawWidget2(ob, NULL/*no origin offset*/);

    if(ob->type == GUI_GROUP)
    {
        // Now our children.
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            uiwidget_t *child = GUI_MustFindObjectById(grp->widgetIds[i]);
            drawWidget(child, NULL/*no origin offset*/);
        }
    }

    if(origin)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-origin->x, -origin->y, 0);
    }
}

void GUI_DrawWidget(uiwidget_t *ob, Point2Raw const *offset)
{
    if(!ob) return;
    if(UIWidget_MaximumWidth(ob) < 1 || UIWidget_MaximumHeight(ob) < 1) return;
    if(UIWidget_Opacity(ob) <= 0) return;

    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    updateWidgetGeometry(ob);

    FR_PopAttrib();

    // Draw.
    FR_PushAttrib();
    FR_LoadDefaultAttrib();
    FR_SetLeading(0);

    // Do not pass a zero length offset.
    drawWidget(ob, ((offset && (offset->x || offset->y))? offset : NULL));

    FR_PopAttrib();
}

void GUI_DrawWidgetXY(uiwidget_t *ob, int x, int y)
{
    Point2Raw origin;
    origin.x = x;
    origin.y = y;
    GUI_DrawWidget(ob, &origin);
}

void UIWidget_RunTic(uiwidget_t *ob, timespan_t ticLength)
{
    DENG2_ASSERT(ob != 0);
    switch(ob->type)
    {
    case GUI_GROUP: {
        // First our children then self.
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_RunTic(GUI_MustFindObjectById(grp->widgetIds[i]), ticLength);
        }
        /* Fallthrough*/ }

    default:
        if(ob->ticker)
        {
            ob->ticker(ob, ticLength);
        }
        break;
    }
}

int UIWidget_Player(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->player;
}

Point2 const *UIWidget_Origin(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return Rect_Origin(ob->geometry);
}

Rect const *UIWidget_Geometry(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->geometry;
}

int UIWidget_MaximumHeight(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->maxSize.height;
}

Size2Raw const *UIWidget_MaximumSize(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return &ob->maxSize;
}

int UIWidget_MaximumWidth(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->maxSize.width;
}

void UIWidget_SetMaximumHeight(uiwidget_t *ob, int height)
{
    DENG2_ASSERT(ob != 0);

    if(ob->maxSize.height == height) return;
    ob->maxSize.height = height;

    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumHeight(GUI_MustFindObjectById(grp->widgetIds[i]), height);
        }
    }
}

void UIWidget_SetMaximumSize(uiwidget_t *ob, Size2Raw const *size)
{
    DENG2_ASSERT(ob != 0);
    if(ob->maxSize.width == size->width &&
       ob->maxSize.height == size->height) return;
    ob->maxSize.width = size->width;
    ob->maxSize.height = size->height;

    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumSize(GUI_MustFindObjectById(grp->widgetIds[i]), size);
        }
    }
}

void UIWidget_SetMaximumWidth(uiwidget_t *ob, int width)
{
    DENG2_ASSERT(ob != 0);
    if(ob->maxSize.width == width) return;
    ob->maxSize.width = width;

    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            UIWidget_SetMaximumWidth(GUI_MustFindObjectById(grp->widgetIds[i]), width);
        }
    }
}

int UIWidget_Alignment(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->alignFlags;
}

void UIWidget_SetAlignment(uiwidget_t *ob, int alignFlags)
{
    DENG2_ASSERT(ob != 0);
    ob->alignFlags = alignFlags;
}

float UIWidget_Opacity(uiwidget_t *ob)
{
    DENG2_ASSERT(ob != 0);
    return ob->opacity;
}

void UIWidget_SetOpacity(uiwidget_t *ob, float opacity)
{
    DENG2_ASSERT(ob != 0);
    ob->opacity = opacity;
    if(ob->type == GUI_GROUP)
    {
        guidata_group_t *grp = (guidata_group_t *)ob->typedata;
        for(int i = 0; i < grp->widgetIdCount; ++i)
        {
            uiwidget_t *child = GUI_MustFindObjectById(grp->widgetIds[i]);
            UIWidget_SetOpacity(child, opacity);
        }
    }
}
