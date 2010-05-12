/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * hu_lib.c: Heads-up text and input code.
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean inited = false;
static uiwidgetid_t numWidgets;
static uiwidget_t* widgets;

static int numWidgetGroups;
static uiwidgetgroup_t* widgetGroups;

// CODE --------------------------------------------------------------------

void HUlib_clearTextLine(hu_textline_t *t)
{
    t->len = 0;
    t->l[0] = 0;
    t->needsupdate = true;
}

void HUlib_initTextLine(hu_textline_t *t, int x, int y)
{
    t->x = x;
    t->y = y;
    HUlib_clearTextLine(t);
}

boolean HUlib_addCharToTextLine(hu_textline_t* t, char ch)
{
    if(t->len == HU_MAXLINELENGTH)
        return false;

    t->l[t->len++] = ch;
    t->l[t->len] = 0;
    t->needsupdate = 4;
    return true;
}

boolean HUlib_delCharFromTextLine(hu_textline_t *t)
{
    if(!t->len)
        return false;

    t->l[--t->len] = 0;
    t->needsupdate = 4;
    return true;
}

void HUlib_drawTextLine(hu_textline_t* l, gamefontid_t font,
                        boolean drawcursor)
{
    HUlib_drawTextLine2(l->x, l->y, l->l, l->len, font, drawcursor);
}

/**
 * Sorta called by HU_Erase and just better darn get things straight.
 */
void HUlib_eraseTextLine(hu_textline_t* l)
{
    if(l->needsupdate)
        l->needsupdate--;
}

void HUlib_initText(hu_text_t* it, int x, int y, boolean* on)
{
    it->lm = 0; // Default left margin is start of text.
    it->on = on;
    it->laston = true;

    HUlib_initTextLine(&it->l, x, y);
}

/**
 * Adheres to the left margin restriction
 */
void HUlib_delCharFromText(hu_text_t* it)
{
    if(it->l.len != it->lm)
        HUlib_delCharFromTextLine(&it->l);
}

void HUlib_eraseLineFromText(hu_text_t* it)
{
    while(it->lm != it->l.len)
        HUlib_delCharFromTextLine(&it->l);
}

/**
 * Resets left margin as well.
 */
void HUlib_resetText(hu_text_t* it)
{
    it->lm = 0;
    HUlib_clearTextLine(&it->l);
}

void HUlib_addPrefixToText(hu_text_t* it, char* str)
{
    while(*str)
        HUlib_addCharToTextLine(&it->l, *(str++));

    it->lm = it->l.len;
}

/**
 * Wrapper function for handling general keyed input.
 *
 * @return              @c true, if it ate the key.
 */
boolean HUlib_keyInText(hu_text_t* it, unsigned char ch)
{
    if(ch >= ' ' && ch <= 'z')
    {
        HUlib_addCharToTextLine(&it->l, (char) ch);
        return true;
    }

    return false;
}

void HUlib_drawText(hu_text_t* it, gamefontid_t font)
{
    hu_textline_t*		l = &it->l;

    if(!*it->on)
        return;

    HUlib_drawTextLine(l, font, true);
}

void HUlib_eraseText(hu_text_t* it)
{
    if(it->laston && !*it->on)
        it->l.needsupdate = 4;

    HUlib_eraseTextLine(&it->l);
    it->laston = *it->on;
}

static __inline uiwidget_t* toWidget(uiwidgetid_t id)
{
    assert(id < numWidgets);
    return &widgets[id];
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
        return 0;
    // Must allocate a new group.
    {
    uiwidgetgroup_t* group;
    widgetGroups = (uiwidgetgroup_t*) realloc(widgetGroups, sizeof(uiwidgetgroup_t) * ++numWidgetGroups);
    group = &widgetGroups[numWidgetGroups-1];
    group->name = name;
    group->num = 0;
    group->widgetIds = 0;
    return group;
    }
}

static void drawWidget(const uiwidget_t* w, short flags, float alpha,
    float* drawnWidth, float* drawnHeight)
{
    float textAlpha = (flags & UWF_OVERRIDE_ALPHA)? alpha : w->textAlpha? alpha * *w->textAlpha : alpha;
    float iconAlpha = (flags & UWF_OVERRIDE_ALPHA)? alpha : w->iconAlpha? alpha * *w->iconAlpha : alpha;
    int width, height;
    float scale = 1;
    boolean scaled = false;

    if(w->scale || w->extraScale != 1)
    {
        scale = (w->scale? *w->scale : 1) * w->extraScale;
        if(scale != 1)
        {
            DGL_MatrixMode(DGL_MODELVIEW);
            DGL_PushMatrix();
            DGL_Scalef(scale, scale, 1);
            scaled = true;
        }
    }

    w->draw(w->player, textAlpha, iconAlpha, &width, &height);
    *drawnWidth = width;
    *drawnHeight = height;

    if(scaled)
    {
        *drawnWidth *= scale;
        *drawnHeight *= scale;

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }
}

static void clearWidgetGroups(void)
{
    if(numWidgetGroups)
    {
        int i;
        for(i = 0; i < numWidgetGroups; ++i)
        {
            uiwidgetgroup_t* grp = &widgetGroups[i];
            free(grp->widgetIds);
        }
        free(widgetGroups);
    }
    widgetGroups = 0;
    numWidgetGroups = 0;
}

static void clearWidgets(void)
{
    if(numWidgets)
    {
        free(widgets);
    }
    widgets = 0;
    numWidgets = 0;
}

void GUI_Init(void)
{
    if(inited)
        return;
    numWidgets = 0;
    widgets = 0;
    numWidgetGroups = 0;
    widgetGroups = 0;
    inited = true;
}

void GUI_Shutdown(void)
{
    if(!inited)
        return;
    clearWidgetGroups();
    clearWidgets();
    inited = false;
}

uiwidgetid_t GUI_CreateWidget(int player, int id, float* scale, float extraScale,
    void (*draw) (int player, float textAlpha, float iconAlpha, int* drawnWidth, int* drawnHeight),
    float* textAlpha, float *iconAlpha)
{
    uiwidget_t* w;
    assert(inited);
    widgets = realloc(widgets, sizeof(uiwidget_t) * ++numWidgets);
    w = &widgets[numWidgets-1];
    w->player = player;
    w->id = id;
    w->scale = scale;
    w->extraScale = extraScale;
    w->draw = draw;
    w->textAlpha = textAlpha;
    w->iconAlpha = iconAlpha;
    return numWidgets-1;
}

int GUI_CreateWidgetGroup(int name, short flags, int padding)
{
    assert(inited);
    {
    uiwidgetgroup_t* group = groupForName(name, true);
    group->flags = flags;
    group->padding = padding;
    return name;
    }
}

void GUI_GroupAddWidget(int name, uiwidgetid_t id)
{
    assert(inited);
    {
    uiwidgetgroup_t* group;
    uiwidget_t* w = toWidget(id);
    uiwidgetid_t i;

    // Ensure this is a known widget id.
    assert(w);
    // Ensure this is a known group name.
    group = groupForName(name, false);
    if(!group)
    {
        Con_Message("GUI_GroupAddWidget: Failed adding widget %u, group %i unknown.\n", id, name);
        return;
    }

    // Ensure widget is not already in this group.
    for(i = 0; i < group->num; ++i)
        if(group->widgetIds[i] == id)
            return; // Already present. Ignore.

    // Must add to this group.
    group->widgetIds = (uiwidgetid_t*) realloc(group->widgetIds, sizeof(uiwidgetid_t) * ++group->num);
    group->widgetIds[group->num-1] = id;
    }
}

short GUI_GroupFlags(int name)
{
    assert(inited);
    {
    const uiwidgetgroup_t* group = groupForName(name, false);
    // Ensure this is a known group name.
    assert(group);
    return group->flags;
    }
}

void GUI_GroupSetFlags(int name, short flags)
{
    assert(inited);
    {
    uiwidgetgroup_t* group = groupForName(name, false);
    // Ensure this is a known group name.
    assert(group);
    group->flags = flags;
    }
}

void GUI_DrawWidgets(int group, byte flags, int inX, int inY, int availWidth,
    int availHeight, float alpha, int* rDrawnWidth, int* rDrawnHeight)
{
    assert(inited);
    {
    const uiwidgetgroup_t* grp;
    float x = inX, y = inY, drawnWidth = 0, drawnHeight = 0;
    size_t i, numDrawnWidgets = 0;

    if(rDrawnWidth)
        *rDrawnWidth = 0;
    if(rDrawnHeight)
        *rDrawnHeight = 0;

    if(!(alpha > 0) || availWidth == 0 || availHeight == 0)
        return;
    grp = groupForName(group, false);
    if(!grp || !grp->num)
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

    for(i = 0; i < grp->num; ++i)
    {
        const uiwidget_t* w = toWidget(grp->widgetIds[i]);
        float wDrawnWidth = 0, wDrawnHeight = 0;

        if(w->id != -1)
        {
            assert(w->id >= 0 && w->id < NUMHUDDISPLAYS);

            if(!cfg.hudShown[w->id])
                continue;
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(x, y, 0);

        drawWidget(w, flags, alpha, &wDrawnWidth, &wDrawnHeight);
    
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-x, -y, 0);

        if(wDrawnWidth > 0 || wDrawnHeight > 0)
        {
            numDrawnWidgets++;

            if(grp->flags & UWGF_RIGHT2LEFT)
                x -= wDrawnWidth + grp->padding;
            else if(grp->flags & UWGF_LEFT2RIGHT)
                x += wDrawnWidth + grp->padding;

            if(grp->flags & UWGF_BOTTOM2TOP)
                y -= wDrawnHeight + grp->padding;
            else if(grp->flags & UWGF_TOP2BOTTOM)
                y += wDrawnHeight + grp->padding;

            if(grp->flags & (UWGF_LEFT2RIGHT|UWGF_RIGHT2LEFT))
                drawnWidth += wDrawnWidth;
            else if(wDrawnWidth > drawnWidth)
                drawnWidth = wDrawnWidth;

            if(grp->flags & (UWGF_TOP2BOTTOM|UWGF_BOTTOM2TOP))
                drawnHeight += wDrawnHeight;
            else if(wDrawnHeight > drawnHeight)
                drawnHeight = wDrawnHeight;
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    if(numDrawnWidgets)
    {
        if(grp->flags & (UWGF_LEFT2RIGHT|UWGF_RIGHT2LEFT))
            drawnWidth += (numDrawnWidgets-1) * grp->padding;

        if(grp->flags & (UWGF_TOP2BOTTOM|UWGF_BOTTOM2TOP))
            drawnHeight += (numDrawnWidgets-1) * grp->padding;
    }

    if(rDrawnWidth)
        *rDrawnWidth = drawnWidth;
    if(rDrawnHeight)
        *rDrawnHeight = drawnHeight;
    }
}
