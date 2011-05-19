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

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "hu_lib.h"

static boolean inited = false;
static uiwidgetid_t numWidgets;
static uiwidget_t* widgets;

static int numWidgetGroups;
static uiwidgetgroup_t* widgetGroups;

static ui_rendstate_t rs;
const ui_rendstate_t* uiRendState = &rs;

static uiwidget_t* toWidget(uiwidgetid_t id)
{
    if(id >= 0 && id < numWidgets)
        return &widgets[id];
    Con_Error("toWidget: Failed to locate widget for id %i.", id);
    exit(1); // Unreachable.
}

static uiwidgetgroup_t* groupForName(int name, boolean canCreate)
{
    int i;
    // Widget group names are unique.
    for(i = 0; i < numWidgetGroups; ++i)
    {
        uiwidgetgroup_t* group = &widgetGroups[i];
        if(group->name == name)
            return group;
    }
    if(!canCreate)
        return NULL;
    // Must allocate a new group.
    {
    uiwidgetgroup_t* group;
    widgetGroups = (uiwidgetgroup_t*) realloc(widgetGroups, sizeof(uiwidgetgroup_t) * ++numWidgetGroups);
    group = &widgetGroups[numWidgetGroups-1];
    group->name = name;
    group->widgetIdCount = 0;
    group->widgetIds = NULL;
    return group;
    }
}

static void drawWidget(uiwidget_t* obj, float alpha, float* drawnWidth, float* drawnHeight)
{
    int width = 0, height = 0;
    if(NULL != obj->drawer && alpha > 0)
    {
        rs.pageAlpha = alpha;
        obj->drawer(obj, 0, 0);
    }
    obj->dimensions(obj, &width, &height);
    *drawnWidth  = width;
    *drawnHeight = height;
}

static void clearWidgetGroups(void)
{
    if(0 == numWidgetGroups) return;
    { int i;
    for(i = 0; i < numWidgetGroups; ++i)
    {
        uiwidgetgroup_t* grp = &widgetGroups[i];
        free(grp->widgetIds);
    }}
    free(widgetGroups);
    widgetGroups = NULL;
    numWidgetGroups = 0;
}

static void clearWidgets(void)
{
    if(0 == numWidgets) return;

    free(widgets);
    widgets = NULL;
    numWidgets = 0;
}

void GUI_Init(void)
{
    if(inited) return;
    numWidgets = 0;
    widgets = NULL;
    numWidgetGroups = 0;
    widgetGroups = NULL;
    inited = true;
}

void GUI_Shutdown(void)
{
    if(!inited) return;
    clearWidgetGroups();
    clearWidgets();
    inited = false;
}

uiwidgetid_t GUI_CreateWidget(guiwidgettype_t type, int player, int hideId, gamefontid_t fontId,
    void (*dimensions) (uiwidget_t* obj, int* width, int* height),
    void (*drawer) (uiwidget_t* obj, int x, int y),
    void (*ticker) (uiwidget_t* obj), void* typedata)
{
    assert(inited);
    {
    uiwidget_t* obj;

    widgets = (uiwidget_t*) realloc(widgets, sizeof(uiwidget_t) * ++numWidgets);
    if(NULL == widgets)
        Con_Error("GUI_CreateWidget: Failed on allocation of %lu bytes for new widget.",
            (unsigned long) (sizeof(uiwidget_t) * numWidgets));

    obj = &widgets[numWidgets-1];
    obj->type = type;
    obj->player = player;
    obj->hideId = hideId;
    obj->fontId = fontId;
    obj->dimensions = dimensions;
    obj->drawer = drawer;
    obj->ticker = ticker;
    obj->typedata = typedata;
    return numWidgets-1;
    }
}

uiwidgetgroup_t* GUI_FindGroupForName(int name)
{
    assert(inited);
    return groupForName(name, false);
}

int GUI_CreateGroup(int name, short flags, int padding)
{
    assert(inited);
    {
    uiwidgetgroup_t* grp = groupForName(name, true);
    grp->flags = flags;
    grp->padding = padding;
    return name;
    }
}

void GUI_GroupAddWidget(uiwidgetgroup_t* grp, uiwidgetid_t id)
{
    assert(NULL != grp);
    {
    uiwidget_t* obj = toWidget(id);
    int i;
    assert(obj);

    // Ensure widget is not already in this grp.
    for(i = 0; i < grp->widgetIdCount; ++i)
        if(grp->widgetIds[i] == id)
            return; // Already present. Ignore.

    // Must add to this grp.
    grp->widgetIds = (uiwidgetid_t*) realloc(grp->widgetIds, sizeof(*grp->widgetIds) * ++grp->widgetIdCount);
    if(NULL == grp->widgetIds)
        Con_Error("GUI_GroupAddWidget: Failed on (re)allocation of %lu bytes for widget id list.",
            (unsigned long) (sizeof(*grp->widgetIds) * grp->widgetIdCount));

    grp->widgetIds[grp->widgetIdCount-1] = id;
    }
}

int GUI_GroupName(uiwidgetgroup_t* grp)
{
    assert(NULL != grp);
    return grp->name;
}

short GUI_GroupFlags(uiwidgetgroup_t* grp)
{
    assert(NULL != grp);
    return grp->flags;
}

void GUI_GroupSetFlags(uiwidgetgroup_t* grp, short flags)
{
    assert(NULL != grp);
    grp->flags = flags;
}

void GUI_DrawWidgets(uiwidgetgroup_t* grp, int inX, int inY, int availWidth,
    int availHeight, float alpha, int* rDrawnWidth, int* rDrawnHeight)
{
    assert(NULL != grp);
    {
    float x = inX, y = inY, drawnWidth = 0, drawnHeight = 0;
    int i, numDrawnWidgets = 0;

    if(rDrawnWidth)
        *rDrawnWidth = 0;
    if(rDrawnHeight)
        *rDrawnHeight = 0;

    if(availWidth == 0 || availHeight == 0)
        return;
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

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    for(i = 0; i < grp->widgetIdCount; ++i)
    {
        uiwidget_t* obj = toWidget(grp->widgetIds[i]);
        float wDrawnWidth = 0, wDrawnHeight = 0;

        if(obj->hideId != -1)
        {
            assert(obj->hideId >= 0 && obj->hideId < NUMHUDDISPLAYS);

            if(!cfg.hudShown[obj->hideId])
                continue;
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(x, y, 0);

        drawWidget(obj, alpha, &wDrawnWidth, &wDrawnHeight);
    
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-x, -y, 0);

        if(wDrawnWidth > 0 || wDrawnHeight > 0)
        {
            ++numDrawnWidgets;

            if(grp->flags & UWGF_RIGHTTOLEFT)
                x -= wDrawnWidth + grp->padding;
            else if(grp->flags & UWGF_LEFTTORIGHT)
                x += wDrawnWidth + grp->padding;

            if(grp->flags & UWGF_BOTTOMTOTOP)
                y -= wDrawnHeight + grp->padding;
            else if(grp->flags & UWGF_TOPTOBOTTOM)
                y += wDrawnHeight + grp->padding;

            if(grp->flags & (UWGF_LEFTTORIGHT|UWGF_RIGHTTOLEFT))
                drawnWidth += wDrawnWidth;
            else if(wDrawnWidth > drawnWidth)
                drawnWidth = wDrawnWidth;

            if(grp->flags & (UWGF_TOPTOBOTTOM|UWGF_BOTTOMTOTOP))
                drawnHeight += wDrawnHeight;
            else if(wDrawnHeight > drawnHeight)
                drawnHeight = wDrawnHeight;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    if(0 != numDrawnWidgets)
    {
        if(grp->flags & (UWGF_LEFTTORIGHT|UWGF_RIGHTTOLEFT))
            drawnWidth += (numDrawnWidgets-1) * grp->padding;

        if(grp->flags & (UWGF_TOPTOBOTTOM|UWGF_BOTTOMTOTOP))
            drawnHeight += (numDrawnWidgets-1) * grp->padding;
    }

    if(NULL != rDrawnWidth)  *rDrawnWidth  = drawnWidth;
    if(NULL != rDrawnHeight) *rDrawnHeight = drawnHeight;
    }
}

void GUI_TickWidgets(uiwidgetgroup_t* grp)
{
    assert(NULL != grp);
    if(!grp->widgetIdCount) return;
    { int i;
    for(i = 0; i < grp->widgetIdCount; ++i)
    {
        uiwidget_t* obj = toWidget(grp->widgetIds[i]);
        if(NULL != obj->ticker)
        {
            obj->ticker(obj);
        }
    }}
}
