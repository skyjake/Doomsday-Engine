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

// CODE --------------------------------------------------------------------

void HUlib_init(void)
{
    // Nothing to do...
}

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

static int drawWidget(const uiwidget_t* w, int player, float textAlpha, float iconAlpha)
{
    int drawnWidth;

    if(w->scale != 1)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PushMatrix();
        DGL_Scalef(w->scale, w->scale, 1);
    }

    drawnWidth = w->draw(player, textAlpha, iconAlpha);

    if(w->scale != 1)
    {
        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_PopMatrix();
    }

    return drawnWidth;
}

void UI_DrawWidgets(const uiwidget_t* widgets, size_t numWidgets, int x, int y,
    int player, float textAlpha, float iconAlpha, hotloc_t hotspot)
{
    size_t i;

    if(!numWidgets || !(iconAlpha > 0))
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    for(i = 0; i < numWidgets; ++i)
    {
        const uiwidget_t* w = &widgets[i];
        int drawnWidth;

        if(w->id != -1)
        {
            assert(w->id >= 0 && w->id < NUMHUDDISPLAYS);

             if(!cfg.hudShown[w->id])
                continue;
        }

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(x, y, 0);

        drawnWidth = drawWidget(w, player, w->textAlpha? *w->textAlpha : textAlpha, w->iconAlpha? *w->iconAlpha : iconAlpha);

        DGL_MatrixMode(DGL_MODELVIEW);
        DGL_Translatef(-x, -y, 0);

        if(drawnWidth > 0)
        {
#if __JDOOM__ || __JDOOM64__
            switch(hotspot)
            {
            case HOT_TLEFT:
            case HOT_BLEFT:
            case HOT_B: x += drawnWidth; break;

            case HOT_TRIGHT:
            case HOT_BRIGHT: x -= drawnWidth; break;

            default: break;
            }
#elif __JHERETIC__
            switch(hotspot)
            {
            case HOT_TLEFT: x += drawnWidth + 2; break;

            case HOT_BLEFT:
            case HOT_B: y -= drawnWidth + 2; break;

            case HOT_TRIGHT:
            case HOT_BRIGHT: x -= drawnWidth + 2; break;

            default: break;
            }
#elif __JHEXEN__
            switch(hotspot)
            {
            case HOT_TLEFT: y += drawnWidth + 2; break;
            case HOT_LEFT: x += drawnWidth + 2; break;

            case HOT_BLEFT:
            case HOT_B: y -= drawnWidth + 2; break;

            case HOT_TRIGHT:
            case HOT_BRIGHT: x -= drawnWidth + 2; break;

            default: break;
            }
#endif
        }
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}
