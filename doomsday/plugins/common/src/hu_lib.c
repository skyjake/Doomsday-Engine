/**\file hu_lib.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include "hu_lib.h"

static boolean inited = false;

static int numWidgets;
static uiwidget_t* widgets;

static ui_rendstate_t rs;
const ui_rendstate_t* uiRendState = &rs;

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

static uiwidget_t* allocateWidget(guiwidgettype_t type, uiwidgetid_t id, void* typedata)
{
    uiwidget_t* obj;
    widgets = (uiwidget_t*) realloc(widgets, sizeof(*widgets) * ++numWidgets);
    if(NULL == widgets)
        Con_Error("allocateWidget: Failed on (re)allocation of %lu bytes for new widget.",
            (unsigned long) (sizeof(*widgets) * numWidgets));
    obj = &widgets[numWidgets-1];
    memset(obj, 0, sizeof(*obj));
    obj->type = type;
    obj->id = id;
    switch(obj->type)
    {
    case GUI_GROUP: {
        guidata_group_t* grp = (guidata_group_t*)calloc(1, sizeof(*grp));
        if(NULL == grp)
            Con_Error("allocateWidget: Failed on (re)allocation of %lu bytes for GUI_GROUP typedata.",
                (unsigned long) sizeof(*grp));
        obj->typedata = grp;
        break;
      }
    default:
        obj->typedata = typedata;
        break;
    }
    return obj;
}

static uiwidget_t* createWidget(guiwidgettype_t type, int player, int hideId, gamefontid_t fontId,
    void (*dimensions) (uiwidget_t* obj, int* width, int* height),
    void (*drawer) (uiwidget_t* obj, int x, int y),
    void (*ticker) (uiwidget_t* obj), void* typedata)
{
    uiwidget_t* obj = allocateWidget(type, nextUnusedId(), typedata);
    obj->player = player;
    obj->hideId = hideId;
    obj->fontId = fontId;
    obj->dimensions = dimensions;
    obj->drawer = drawer;
    obj->ticker = ticker;
    return obj;
}

static void clearWidgets(void)
{
    if(0 == numWidgets) return;

    { int i;
    for(i = 0; i < numWidgets; ++i)
    {
        uiwidget_t* obj = &widgets[i];
        if(obj->type == GUI_GROUP)
            free(obj->typedata);
    }}
    free(widgets);
    widgets = NULL;
    numWidgets = 0;
}

uiwidget_t* GUI_FindObjectById(uiwidgetid_t id)
{
    errorIfNotInited("GUI_FindObjectById");
    { int i;
    for(i = 0; i < numWidgets; ++i)
    {
        uiwidget_t* obj = &widgets[i];
        if(obj->id == id)
            return obj;
    }}
    return NULL;
}

uiwidget_t* GUI_MustFindObjectById(uiwidgetid_t id)
{
    uiwidget_t* obj = GUI_FindObjectById(id);
    if(NULL == obj)
        Con_Error("GUI_MustFindObjectById: Failed to locate object with id %i.", (int) id);
    return obj;
}

void GUI_Init(void)
{
    if(inited) return;
    numWidgets = 0;
    widgets = NULL;
    inited = true;
}

void GUI_Shutdown(void)
{
    if(!inited) return;
    clearWidgets();
    inited = false;
}

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, int hideId, gamefontid_t fontId,
    void (*dimensions) (uiwidget_t* obj, int* width, int* height),
    void (*drawer) (uiwidget_t* obj, int x, int y),
    void (*ticker) (uiwidget_t* obj), void* typedata)
{
    uiwidget_t* obj;
    errorIfNotInited("GUI_CreateWidget");
    obj = createWidget(type, player, hideId, fontId, dimensions, drawer, ticker, typedata);
    return obj->id;
}

uiwidgetid_t GUI_CreateGroup(int player, short flags, int padding)
{
    uiwidget_t* obj;
    guidata_group_t* grp;
    errorIfNotInited("GUI_CreateGroup");
    obj = createWidget(GUI_GROUP, player, -1, 0, NULL, NULL, NULL, NULL);
    grp = (guidata_group_t*)obj->typedata;
    grp->flags = flags;
    grp->padding = padding;
    return obj->id;
}

void UIGroup_AddWidget(uiwidget_t* obj, uiwidget_t* other)
{
    assert(NULL != obj && obj->type == GUI_GROUP);
    {
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;

    if(NULL == other)
        return;

    // Ensure widget is not already in this grp.
    { int i;
    for(i = 0; i < grp->widgetIdCount; ++i)
        if(grp->widgetIds[i] == other->id)
            return; // Already present. Ignore.
    }

    // Must add to this grp.
    grp->widgetIds = (uiwidgetid_t*) realloc(grp->widgetIds, sizeof(*grp->widgetIds) * ++grp->widgetIdCount);
    if(NULL == grp->widgetIds)
        Con_Error("UIGroup::AddWidget: Failed on (re)allocation of %lu bytes for widget id list.",
            (unsigned long) (sizeof(*grp->widgetIds) * grp->widgetIdCount));

    grp->widgetIds[grp->widgetIdCount-1] = other->id;
    }
}

short UIGroup_Flags(uiwidget_t* obj)
{
    assert(NULL != obj && obj->type == GUI_GROUP);
    {
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    return grp->flags;
    }
}

void UIGroup_SetFlags(uiwidget_t* obj, short flags)
{
    assert(NULL != obj && obj->type == GUI_GROUP);
    {
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    grp->flags = flags;
    }
}

static void drawWidget(uiwidget_t* obj, int x, int y, float alpha, int* drawnWidth, int* drawnHeight)
{
    int width = 0, height = 0;
    if(NULL != obj->drawer && alpha > 0)
    {
        rs.pageAlpha = alpha;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(x, y, 0);
    
        obj->drawer(obj, 0, 0);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-x, -y, 0);
    }
    if(NULL != obj->dimensions)
    {
        obj->dimensions(obj, &width, &height);
    }
    *drawnWidth  = width;
    *drawnHeight = height;
}

static void drawChildWidgets(uiwidget_t* obj, int x, int y, int availWidth,
    int availHeight, float alpha, int* drawnWidth, int* drawnHeight)
{
    assert(NULL != obj && obj->type == GUI_GROUP && NULL != drawnWidth && NULL != drawnHeight);
    {
    guidata_group_t* grp = (guidata_group_t*)obj->typedata;
    int i, numDrawnWidgets = 0;

    if(!grp->widgetIdCount)
        return;

    if(grp->flags & UWGF_ALIGN_RIGHT)
        x += availWidth;
    else if(!(grp->flags & UWGF_ALIGN_LEFT))
        x += availWidth/2;

    if(grp->flags & UWGF_ALIGN_BOTTOM)
        y += availHeight;
    else if(!(grp->flags & UWGF_ALIGN_TOP))
        y += availHeight/2;

    for(i = 0; i < grp->widgetIdCount; ++i)
    {
        uiwidget_t* child = GUI_MustFindObjectById(grp->widgetIds[i]);
        int width = 0, height = 0;

        if(child->hideId != -1)
        {
            assert(child->hideId >= 0 && child->hideId < NUMHUDDISPLAYS);

            if(!cfg.hudShown[child->hideId])
                continue;
        }

        GUI_DrawWidget(child, x, y, availWidth, availHeight, alpha, &width, &height);
    
        if(width > 0 || height > 0)
        {
            ++numDrawnWidgets;

            if(grp->flags & UWGF_RIGHTTOLEFT)
            {
                if(!(grp->flags & UWGF_VERTICAL))
                    x -= width  + grp->padding;
                else
                    y -= height + grp->padding;
            }
            else if(grp->flags & UWGF_LEFTTORIGHT)
            {
                if(!(grp->flags & UWGF_VERTICAL))
                    x += width  + grp->padding;
                else
                    y += height + grp->padding;
            }

            if(grp->flags & (UWGF_LEFTTORIGHT|UWGF_RIGHTTOLEFT))
            {
                if(!(grp->flags & UWGF_VERTICAL))
                    *drawnWidth  += width;
                else
                    *drawnHeight += height;
            }
            else
            {
                if(width  > *drawnWidth)
                    *drawnWidth  = width;

                if(height > *drawnHeight)
                    *drawnHeight = height;
            }
        }
    }

    if(0 != numDrawnWidgets && (grp->flags & (UWGF_LEFTTORIGHT|UWGF_RIGHTTOLEFT)))
    {
        if(!(grp->flags & UWGF_VERTICAL))
            *drawnWidth  += (numDrawnWidgets-1) * grp->padding;
        else
            *drawnHeight += (numDrawnWidgets-1) * grp->padding;
    }
    }
}

void GUI_DrawWidget(uiwidget_t* obj, int x, int y, int availWidth,
    int availHeight, float alpha, int* drawnWidth, int* drawnHeight)
{
    assert(NULL != obj);
    {
    int width = 0, height = 0;

    if(NULL != drawnWidth)  *drawnWidth = 0;
    if(NULL != drawnHeight) *drawnHeight = 0;

    if(availWidth == 0 || availHeight == 0)
        return;

    // First we draw ourself.
    drawWidget(obj, x, y, alpha, &width, &height);

    if(obj->type == GUI_GROUP)
    {
        // Now our children.
        int cWidth, cHeight;
        drawChildWidgets(obj, x, y, availWidth, availHeight, alpha, &cWidth, &cHeight);
        if(cWidth  > width)
            width = cWidth;
        if(cHeight > height)
            height = cHeight;
    }

    if(NULL != drawnWidth)  *drawnWidth  = width;
    if(NULL != drawnHeight) *drawnHeight = height;
    }
}

void GUI_TickWidget(uiwidget_t* obj)
{
    assert(NULL != obj);
    switch(obj->type)
    {
    case GUI_GROUP: {
        // First our children then self.
        guidata_group_t* grp = (guidata_group_t*)obj->typedata;
        int i;
        for(i = 0; i < grp->widgetIdCount; ++i)
        {
            GUI_TickWidget(GUI_MustFindObjectById(grp->widgetIds[i]));
        }
        // Fallthrough:
      }
    default:
        if(NULL != obj->ticker)
        {
            obj->ticker(obj);
        }
        break;
    }
}
