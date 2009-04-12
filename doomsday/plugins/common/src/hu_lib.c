/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2005-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

#include <ctype.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "hu_lib.h"
#include "../../../engine/portable/include/r_draw.h"

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

void HUlib_initTextLine(hu_textline_t *t, int x, int y, dpatch_t *f, int sc)
{
    t->x = x;
    t->y = y;
    t->f = f;
    t->sc = sc;
    HUlib_clearTextLine(t);
}

boolean HUlib_addCharToTextLine(hu_textline_t *t, char ch)
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

void HUlib_drawTextLine(hu_textline_t *l, boolean drawcursor)
{
    int         i, w, x;
    unsigned char c;

    DGL_Color3fv(PLRPROFILE.hud.color);

    x = l->x;
    for(i = 0; i < l->len; ++i)
    {
        c = toupper(l->l[i]);
        if(c != ' ' && c >= l->sc && c <= '_')
        {
            w = l->f[c - l->sc].width;
            if(x + w > SCREENWIDTH)
                break;
            GL_DrawPatch_CS(x, l->y, l->f[c - l->sc].lump);
            x += w;
        }
        else
        {
            x += 4;
            if(x >= SCREENWIDTH)
                break;
        }
    }

    // Draw the cursor if requested.
    if(drawcursor && x + l->f['_' - l->sc].width <= SCREENWIDTH)
        GL_DrawPatch_CS(x, l->y, l->f['_' - l->sc].lump);
}

/**
 * Sorta called by HU_Erase and just better darn get things straight.
 */
void HUlib_eraseTextLine(hu_textline_t *l)
{
    if(l->needsupdate)
        l->needsupdate--;
}

void HUlib_initSText(hu_stext_t *s, int x, int y, int h, dpatch_t *font,
                     int startchar, boolean *on)
{
    int         i;

    s->h = h;
    s->on = on;
    s->laston = true;
    s->cl = 0;

    for(i = 0; i < h; ++i)
    {
        HUlib_initTextLine(&s->l[i], x, y - i * (font[0].height + 1),
                           font, startchar);
    }
}

void HUlib_addLineToSText(hu_stext_t *s)
{
    int         i;

    // Add a clear line.
    if(++s->cl == s->h)
        s->cl = 0;

    HUlib_clearTextLine(&s->l[s->cl]);

    // Everything needs updating.
    for(i = 0; i < s->h; ++i)
    {
        s->l[i].needsupdate = 4;
    }
}

void HUlib_addMessageToSText(hu_stext_t *s, char *prefix, char *msg)
{
    HUlib_addLineToSText(s);

    if(prefix)
    {
        while(*prefix)
            HUlib_addCharToTextLine(&s->l[s->cl], *(prefix++));
    }

    while(*msg)
        HUlib_addCharToTextLine(&s->l[s->cl], *(msg++));
}

void HUlib_drawSText(hu_stext_t *s)
{
    int         i, idx;
    hu_textline_t *l;

    if(!*s->on)
        return; // If not on, don't draw.

    // Draw everything.
    for(i = 0; i < s->h; ++i)
    {
        idx = s->cl - i;
        if(idx < 0)
            idx += s->h; // Handle queue of lines.

        l = &s->l[idx];

        HUlib_drawTextLine(l, false);
    }
}

void HUlib_eraseSText(hu_stext_t *s)
{
    int         i;

    for(i = 0; i < s->h; ++i)
    {
        if(s->laston && !*s->on)
            s->l[i].needsupdate = 4;
        HUlib_eraseTextLine(&s->l[i]);
    }

    s->laston = *s->on;
}

void HUlib_initIText(hu_itext_t *it, int x, int y, dpatch_t *font,
                     int startchar, boolean *on)
{
    it->lm = 0; // Default left margin is start of text.
    it->on = on;
    it->laston = true;

    HUlib_initTextLine(&it->l, x, y, font, startchar);
}

/**
 * Adheres to the left margin restriction
 */
void HUlib_delCharFromIText(hu_itext_t *it)
{
    if(it->l.len != it->lm)
        HUlib_delCharFromTextLine(&it->l);
}

void HUlib_eraseLineFromIText(hu_itext_t *it)
{
    while(it->lm != it->l.len)
        HUlib_delCharFromTextLine(&it->l);
}

/**
 * Resets left margin as well.
 */
void HUlib_resetIText(hu_itext_t *it)
{
    it->lm = 0;
    HUlib_clearTextLine(&it->l);
}

void HUlib_addPrefixToIText(hu_itext_t *it, char *str)
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
boolean HUlib_keyInIText(hu_itext_t *it, unsigned char ch)
{
    if(ch >= ' ' && ch <= '_')
    {
        HUlib_addCharToTextLine(&it->l, (char) ch);
        return true;
    }

    return false;
}

void HUlib_drawIText(hu_itext_t *it)
{
    hu_textline_t *l = &it->l;

    if(!*it->on)
        return;

    HUlib_drawTextLine(l, true);
}

void HUlib_eraseIText(hu_itext_t * it)
{
    if(it->laston && !*it->on)
        it->l.needsupdate = 4;

    HUlib_eraseTextLine(&it->l);
    it->laston = *it->on;
}
